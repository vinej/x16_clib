#!/usr/bin/env python3
"""
ca65 -> llvm-mos assembly converter for x16clib.

This translates the MECHANICAL parts of a src_ca65 module into llvm-mos
syntax. It deliberately does NOT translate the `_x16_*` C entry points:
those depend on the calling convention, which is completely different, and
a plausible-looking automatic translation would be silently wrong.

Instead it leaves cc65's runtime symbols (popa, popax, ptr1, ptr2, sreg)
exactly as they are. They resolve to nothing under llvm-mos, so any shim
that has not been rewritten by hand fails at LINK time with an undefined
symbol. That is the point: the failure is loud and it is early.

What it does translate:

    .segment "CODE"     -> .section .text,"ax",@progbits
    .segment "RODATA"   -> .section .rodata,"a",@progbits
    .segment "DATA"     -> .section .data,"aw",@progbits
    .segment "BSS"      -> .section .bss,"aw",@nobits
    .segment "ZEROPAGE" -> .section .zp.bss,"aw",@nobits
    .export / .exportzp -> .globl
    .import / .importzp -> dropped (ELF resolves externs by name)
    .res N              -> .zero N
    .fatal              -> .error
    .endmacro           -> .endm
    ^(expr)             -> ((expr) >> 16)      -- no bank-byte operator
    @label              -> .L<parent>_<label>  -- cheap locals are scoped
                                                  to the preceding real
                                                  label; LLVM has no such
                                                  concept, so uniquify
    :+ / :- / :         -> 1f / 1b / 1:        -- anonymous labels
    macro parameters    -> \\param
    _x16_foo            -> x16_foo             -- llvm-mos does not
                                                  decorate C symbols

Usage:  python tools/ca65_to_llvm.py src_ca65/video/screen.s src_llvm/video/screen.s

The output path is an argument rather than stdout on purpose: Windows
Python rewrites '\\n' to '\\r\\n' on a text stream, and a converted file full
of CRLFs silently defeats every later exact-match edit.
"""

import re
import sys

SEGMENTS = {
    'CODE':     '.section .text,"ax",@progbits',
    'RODATA':   '.section .rodata,"a",@progbits',
    'DATA':     '.section .data,"aw",@progbits',
    'BSS':      '.section .bss,"aw",@nobits',
    'ZEROPAGE': '.section .zp.bss,"aw",@nobits',
}

# A real (non-cheap) label at the start of a line opens a new cheap-local scope.
RE_LABEL = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')
RE_CHEAP_DEF = re.compile(r'^@([A-Za-z0-9_]+):')
RE_CHEAP_REF = re.compile(r'@([A-Za-z0-9_]+)')
RE_SEGMENT = re.compile(r'^(\s*)\.segment\s+"([A-Z]+)"')
RE_EXPORT = re.compile(r'^(\s*)\.export(zp)?\s+(.*)$')
RE_IMPORT = re.compile(r'^(\s*)\.import(zp)?\s+(.*)$')
RE_RES = re.compile(r'\.res\s+')
RE_BANK = re.compile(r'\^\(')
RE_MACRO = re.compile(r'^(\s*)\.macro\s+(\w+)\s*(.*)$')


def split_code_comment(line):
    """Split off a trailing `;` comment, ignoring `;` inside a string."""
    in_str = False
    for i, ch in enumerate(line):
        if ch == '"':
            in_str = not in_str
        elif ch == ';' and not in_str:
            return line[:i], line[i:]
    return line, ''


def convert(text):
    out = []
    parent = 'anon'          # the label cheap locals hang off
    macro_params = []        # parameters of the macro currently open

    for raw in text.split('\n'):
        code, comment = split_code_comment(raw)

        # --- things that consume the whole line -------------------------
        m = RE_SEGMENT.match(code)
        if m:
            indent, seg = m.groups()
            if seg not in SEGMENTS:
                raise SystemExit(f'unknown segment: {seg}')
            out.append(f'{indent}{SEGMENTS[seg]}' + (('  ' + comment) if comment.strip() else ''))
            continue

        m = RE_IMPORT.match(code)
        if m:
            # Externs need no declaration in ELF. Keep the comment as a note.
            names = m.group(3).strip()
            out.append(f'; (import dropped: {names}){comment}')
            continue

        m = RE_EXPORT.match(code)
        if m:
            indent, _, names = m.groups()
            names = names.strip()
            names = re.sub(r'\b_x16_', 'x16_', names)
            out.append(f'{indent}.globl  {names}' + (('  ' + comment) if comment.strip() else ''))
            continue

        m = RE_MACRO.match(code)
        if m:
            indent, name, params = m.groups()
            macro_params = [p.strip() for p in params.split(',') if p.strip()]
            code = f'{indent}.macro  {name} {", ".join(macro_params)}'.rstrip()

        if re.match(r'^\s*\.endmacro', code):
            macro_params = []
            code = re.sub(r'\.endmacro', '.endm', code)

        # --- token-level rewrites ---------------------------------------
        code = RE_RES.sub('.zero  ', code)
        code = re.sub(r'\.fatal\b', '.error', code)

        # ^(expr)  ->  ((expr) >> 16)
        while True:
            m = RE_BANK.search(code)
            if not m:
                break
            start = m.start()
            depth = 0
            for i in range(m.end() - 1, len(code)):
                if code[i] == '(':
                    depth += 1
                elif code[i] == ')':
                    depth -= 1
                    if depth == 0:
                        inner = code[m.end():i]
                        code = code[:start] + f'(({inner}) >> 16)' + code[i + 1:]
                        break
            else:
                raise SystemExit(f'unbalanced ^( in: {raw}')

        # Strip cc65's leading underscore BEFORE the parent label is
        # recorded, so cheap locals become .Lx16_foo_bar and not
        # .L_x16_foo_bar.
        code = re.sub(r'\b_x16_', 'x16_', code)

        # Cheap locals. Definition first: it does NOT change the parent.
        m = RE_CHEAP_DEF.match(code)
        if m:
            code = RE_CHEAP_DEF.sub(rf'.L{parent}_\1:', code)
        else:
            m = RE_LABEL.match(code)
            if m:
                parent = m.group(1)
        code = RE_CHEAP_REF.sub(rf'.L{parent}_\1', code)

        # Anonymous labels. Only a handful, and never nested in this library.
        code = re.sub(r'^:(\s|$)', r'1:\1', code)
        code = re.sub(r'\b(b\w\w|jmp)\s+:\+', r'\1 1f', code)
        code = re.sub(r'\b(b\w\w|jmp)\s+:-', r'\1 1b', code)

        # Macro parameters are referenced as \param.
        for p in macro_params:
            code = re.sub(rf'(?<![\\\w]){re.escape(p)}\b', rf'\\{p}', code)

        # C symbols lose cc65's leading underscore.
        code = re.sub(r'\b_x16_', 'x16_', code)

        out.append(code + comment)

    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, encoding='utf-8', newline='') as f:
        result = convert(f.read())
    with open(dst, 'w', encoding='utf-8', newline='') as f:
        f.write(result)

    # Report what still needs a human: cc65's runtime has no llvm-mos
    # equivalent, so each of these is a shim awaiting a hand rewrite.
    left = [
        f'{n}: {l.rstrip()}'
        for n, l in enumerate(result.split('\n'), 1)
        if re.search(r'\b(popa|popax|ptr1|ptr2|sreg)\b', l)
        and not l.lstrip().startswith(';')
    ]
    if left:
        print(f'{dst}: {len(left)} line(s) still on the cc65 ABI:', file=sys.stderr)
        for l in left:
            print('   ' + l, file=sys.stderr)
