#!/usr/bin/env python3
"""
ca65 -> vbcc (vasm6502_oldstyle) assembly converter for x16clib.

This translates the MECHANICAL parts of a src_ca65 module into the syntax
of vasm6502_oldstyle, the assembler vbcc's 6502 backend drives. It does
NOT translate the `_x16_*` C entry points: those depend on the calling
convention, which is completely different, and a plausible-looking
automatic translation would be silently wrong.

Instead it leaves cc65's runtime symbols (popa, popax) exactly as they
are.  vasm has no such symbols, so any shim that has not been rewritten by
hand fails at LINK time with an undefined symbol.  That is the point: the
failure is loud and early.  See tools/ca65_to_llvm.py for the sibling that
does the same job for llvm-mos.

What it translates:

    .segment "CODE"     -> section text
    .segment "RODATA"   -> section rodata
    .segment "DATA"     -> section data
    .segment "BSS"      -> section bss
    .segment "ZEROPAGE" -> section zpage
    .export / .exportzp -> global
    .import / .importzp -> zpage <name>   (zeropage extern: keeps the
                                            2-byte addressing form) OR a
                                            plain extern note for code
                                            symbols vlink resolves by name
    .res N              -> reserve N
    .byte / .word / .dword -> byte / word / long
    .ifndef X / .endif  -> ifndef X / endif
    .if / .else / .elseif  -> ifne / else / elseif   (nonzero == true)
    .fatal "msg"        -> fail "msg"
    .macro n a,b / .endmacro -> n macro / endm   (named args -> \\1 \\2 ...)
    @label / @label:    -> .label   (vasm scopes '.' locals to the last
                                      global label, exactly like ca65's @)
    :+ / :- / :         -> unnamed labels via +/- (vasm supports these)

What it deliberately keeps:

    - the leading underscore on C symbols.  vbcc decorates C identifiers
      with a leading '_' (x16_foo in C is _x16_foo in asm), UNLIKE
      llvm-mos.  So `_x16_foo` stays `_x16_foo`.
    - popa / popax, so unrewritten shims fail at link.

Usage:  python tools/ca65_to_vbcc.py src_ca65/util/math.s src_vbcc/util/math.s

The output path is an argument rather than stdout on purpose: Windows
Python rewrites '\\n' to '\\r\\n' on a text stream, and a converted file
full of CRLFs silently defeats every later exact-match edit.
"""

import re
import sys

SEGMENTS = {
    'CODE':     'section text',
    'RODATA':   'section rodata',
    'DATA':     'section data',
    'BSS':      'section bss',
    'ZEROPAGE': 'section zpage',
}

RE_LABEL     = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')
RE_CHEAP_DEF = re.compile(r'^@([A-Za-z0-9_]+):')
RE_CHEAP_REF = re.compile(r'@([A-Za-z0-9_]+)')
RE_SEGMENT   = re.compile(r'^(\s*)\.segment\s+"([A-Z]+)"')
RE_EXPORT    = re.compile(r'^(\s*)\.export(zp)?\s+(.*)$')
RE_IMPORT    = re.compile(r'^(\s*)\.import(zp)?\s+(.*)$')
RE_MACRO     = re.compile(r'^(\s*)\.macro\s+(\w+)\s*(.*)$')

# KERNAL virtual registers r0..r15 live at $02..$21.  vbcc's backend names
# its own pseudo-registers r0..r15 and the linker places r0 at $02 -- the
# SAME physical bytes.  So the library's PLAIN `rN = $xx` definitions
# would clash with vbcc's built-in rN and are dropped.
#
# But vbcc provides ONLY the byte names rN; it does NOT provide the
# KERNAL's 16-bit low/high halves rNL/rNH (r0L=$02, r0H=$03, ...), which
# the library's internal routines use to marshal KERNAL arguments. Those
# MUST survive -- so this matches only `rN =` with no L/H suffix (the `=`
# right after the digits), leaving rNL/rNH definitions in place.
RE_KERNAL_REG_DEF = re.compile(r'^\s*r(\d|1[0-5])\s*=\s*\$[0-9A-Fa-f]+')


def split_code_comment(line):
    """Split off a trailing `;` comment, ignoring `;` inside a string."""
    in_str = False
    for i, ch in enumerate(line):
        if ch == '"':
            in_str = not in_str
        elif ch == ';' and not in_str:
            return line[:i], line[i:]
    return line, ''


def convert(text, drop_kernal_regs=False):
    out = []
    macro_params = []        # named parameters of the macro currently open

    for raw in text.split('\n'):
        code, comment = split_code_comment(raw)

        # A vasm ';' comment is fine, but keep everything after the first ';'.
        # Rewrite the whole-line constructs first.

        # Drop the redundant KERNAL r0..r15 definitions in const_zp.inc.
        if drop_kernal_regs and RE_KERNAL_REG_DEF.match(code):
            out.append('; (r-reg dropped: vbcc provides r0..r15)' + comment)
            continue

        m = RE_SEGMENT.match(code)
        if m:
            indent, seg = m.groups()
            if seg not in SEGMENTS:
                raise SystemExit(f'unknown segment: {seg}')
            tail = ('  ' + comment) if comment.strip() else ''
            out.append(f'{indent}{SEGMENTS[seg]}{tail}')
            continue

        m = RE_EXPORT.match(code)
        if m:
            indent, _zp, names = m.groups()
            names = names.strip()
            tail = ('  ' + comment) if comment.strip() else ''
            # one `global` per name is safest across vasm versions
            parts = [n.strip() for n in names.split(',') if n.strip()]
            out.append('\n'.join(f'{indent}global\t{p}' for p in parts) + tail)
            continue

        m = RE_IMPORT.match(code)
        if m:
            indent, zp, names = m.groups()
            names = names.strip()
            parts = [n.strip() for n in names.split(',') if n.strip()]
            tail = ('  ' + comment) if comment.strip() else ''
            if zp:
                # zeropage extern: declare each so `sta name` is 2-byte.
                out.append('\n'.join(f'{indent}zpage\t{p}' for p in parts) + tail)
            else:
                # code/data extern: vlink resolves by name; no decl needed.
                out.append(f'; (import: {names}){comment}')
            continue

        m = RE_MACRO.match(code)
        if m:
            indent, name, params = m.groups()
            macro_params = [p.strip() for p in params.split(',') if p.strip()]
            code = f'{indent}{name}\tmacro'

        if re.match(r'^\s*\.endmacro', code):
            macro_params = []
            code = re.sub(r'\.endmacro', 'endm', code)

        # --- conditionals -------------------------------------------------
        # vasm oldstyle reads a token in column 0 as a label, so a bare
        # `ifndef` there becomes a spurious label + "unknown mnemonic".
        # Force at least a tab of indent on every conditional directive.
        code = re.sub(r'^(\s*)\.ifndef\b',  r'\1\tifndef',  code)
        code = re.sub(r'^(\s*)\.ifdef\b',   r'\1\tifdef',   code)
        code = re.sub(r'^(\s*)\.elseif\b',  r'\1\telseif',  code)
        code = re.sub(r'^(\s*)\.else\b',    r'\1\telse',    code)
        code = re.sub(r'^(\s*)\.endif\b',   r'\1\tendif',   code)
        # `.if <expr>` -> `ifne <expr>` (ca65: nonzero is true)
        code = re.sub(r'^(\s*)\.if\b',      r'\1\tifne',    code)

        # --- include -----------------------------------------------------
        code = re.sub(r'^(\s*)\.include\b', r'\1include', code)

        # --- storage / data directives ------------------------------------
        code = re.sub(r'\.res\b',   'reserve', code)
        code = re.sub(r'\.byte\b',  'byte',    code)
        code = re.sub(r'\.word\b',  'word',    code)
        code = re.sub(r'\.dword\b', 'long',    code)
        code = re.sub(r'\.addr\b',  'word',    code)
        code = re.sub(r'\.fatal\b', 'fail',    code)

        # --- cheap locals -------------------------------------------------
        # ca65 @foo is scoped to the previous global label; vasm '.' locals
        # are too.  So @foo -> .foo, both at definition and reference.
        m = RE_CHEAP_DEF.match(code)
        if m:
            code = RE_CHEAP_DEF.sub(r'.\1:', code)
        code = RE_CHEAP_REF.sub(r'.\1', code)

        # --- anonymous labels ---------------------------------------------
        # ca65 ':' / :+ / :-  ->  vasm supports unnamed +/- labels.
        code = re.sub(r'^:(\s|$)', r'+\1', code)
        code = re.sub(r'\b(b\w\w|jmp)\s+:\+', r'\1 +', code)
        code = re.sub(r'\b(b\w\w|jmp)\s+:-',  r'\1 -', code)

        # --- macro parameters ---------------------------------------------
        # ca65 references a named macro arg by its name; vasm oldstyle uses
        # \1 \2 ... positionally.  Map each named param to its index.
        for idx, p in enumerate(macro_params, 1):
            code = re.sub(rf'(?<![\\\w]){re.escape(p)}\b', rf'\\{idx}', code)

        out.append(code + comment)

    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    drop = src.replace('\\', '/').endswith('const_zp.inc')
    with open(src, encoding='utf-8', newline='') as f:
        result = convert(f.read(), drop_kernal_regs=drop)
    with open(dst, 'w', encoding='utf-8', newline='') as f:
        f.write(result)

    # Report shims still on the cc65 ABI: each needs a hand rewrite.
    left = [
        f'{n}: {l.rstrip()}'
        for n, l in enumerate(result.split('\n'), 1)
        if re.search(r'\b(popa|popax)\b', l)
        and not l.lstrip().startswith(';')
    ]
    if left:
        print(f'{dst}: {len(left)} line(s) still on the cc65 ABI:', file=sys.stderr)
        for l in left:
            print('   ' + l, file=sys.stderr)
