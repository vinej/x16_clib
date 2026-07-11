#!/usr/bin/env python3
"""
ca65 -> KickC inline-asm converter for x16clib.

KickC has no linker and no archive format, so the KickC port is a SOURCE
distribution: each module becomes src_kickc/x16/<name>.c, whose public
functions are C signatures with `asm {}` bodies carrying the same
hand-written 6502 as src_ca65. This tool translates the MECHANICAL parts
of one ca65 module into lines that paste into those bodies. It does NOT
write the C wrappers: parameter plumbing depends on what KickC does with
each signature, and a plausible-looking automatic translation would be
silently wrong (see the ABI-mutation-testing lesson).

What it translates:

    ; comment            -> // comment      KickC's asm parser only knows //
    @label               -> <parent>_label  cheap locals are scoped to the
                                            preceding real label; KickC asm
                                            blocks have no such concept
    VERA_CTRL            -> $9f25 /*VERA_CTRL*/
                                            #define does NOT substitute
                                            inside asm{} blocks, so every
                                            symbolic constant is inlined
                                            from src_ca65/core/const_*.inc
    #<CONST / #>CONST    -> #$xx            computed here, since KickAss
                                            cannot take < of an expression
                                            we already flattened

What it only FLAGS (loud TODO comments, never a guess):

    .byte/.word/.res     data: becomes a C array, or stays in a kickasm
                         {{ }} initializer if it must sit with the code
    X16_P*/X16_T*        the cc65 zero-page block: in KickC these become
                         the C parameters/locals of the wrapper function
    popa/popax/ptr1/...  cc65 ABI shim lines: the wrapper replaces them
    vera_addrsel etc.    macros.inc macros: expand by hand (or extend
                         MACRO_EXPANSIONS below)
    :+ / :- / :          anonymous labels: two uses in the whole library,
                         rename by hand
    'x' char literals    ca65 -t cx16 maps them through the PETSCII
                         charmap; verify the byte you actually want

Usage:  python tools/ca65_to_kickc.py src_ca65/util/collide.s > collide.draft
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONST_FILES = [
    ROOT / 'src_ca65' / 'core' / p
    for p in ('const_zp.inc', 'const_vera.inc', 'const_kernal.inc', 'const_rom.inc')
]

RE_ASSIGN = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+?)\s*(?:;.*)?$')
RE_LABEL = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')
RE_CHEAP_DEF = re.compile(r'^@([A-Za-z0-9_]+):')
RE_CHEAP_REF = re.compile(r'@([A-Za-z0-9_]+)')
RE_IDENT = re.compile(r'\b[A-Za-z_][A-Za-z0-9_]*\b')

# Lines whose only sane translation is a human. Each pattern gets a TODO tag.
FLAGS = [
    (re.compile(r'\b(popa|popax|popptr1|pusha|pushax|ptr1|ptr2|ptr3|sreg|tmp1)\b'),
     'cc65 ABI: the C wrapper replaces this'),
    (re.compile(r'\bX16_(P[0-7]|T[0-7]|PTR[0-3]|TPTR[0-3])\b'),
     'zp block: becomes a C parameter/local of the wrapper'),
    (re.compile(r'^\s*\.(byte|word|res|addr|dword)\b'),
     'data: becomes a C array (or kickasm {{ }} if it must sit with code)'),
    (re.compile(r'^\s*(vera_addrsel|vera_dcsel|vera_addr|vera_addr_decr|vpoke_const|'
                r'set_rambank|set_rombank|jsrfar|rom_call_fast)\b'),
     'macro: expand by hand from core/macros.inc'),
    (re.compile(r'(^|\s):([+-]|\s|$)'),
     'anonymous label: rename by hand'),
    (re.compile(r"'"),
     "char literal: ca65 -t cx16 PETSCII charmap -- check the byte"),
]


def eval_ca65(expr, consts):
    """Evaluate a ca65 constant expression against known constants."""
    e = expr
    e = re.sub(r'\$([0-9A-Fa-f]+)', r'0x\1', e)
    e = re.sub(r'%([01]+)', r'0b\1', e)

    def sub_name(m):
        name = m.group(0)
        if name in consts:
            return str(consts[name])
        raise KeyError(name)
    e = re.sub(r'\b(?!0[xb])[A-Za-z_][A-Za-z0-9_]*\b', sub_name, e)
    return eval(e, {'__builtins__': {}}, {})  # arithmetic only


def load_consts():
    consts = {}
    for path in CONST_FILES:
        for raw in path.read_text(encoding='utf-8').split('\n'):
            m = RE_ASSIGN.match(raw.strip())
            if not m:
                continue
            name, expr = m.groups()
            if name.endswith('_INC') and expr.strip() == '1':
                continue        # the include guard, not a constant
            try:
                consts[name] = eval_ca65(expr, consts)
            except (KeyError, SyntaxError):
                pass            # forward reference or non-numeric: skip
    return consts


def split_code_comment(line):
    in_str = False
    for i, ch in enumerate(line):
        if ch == '"':
            in_str = not in_str
        elif ch == ';' and not in_str:
            return line[:i], line[i:]
    return line, ''


def convert(text, consts):
    out = []
    parent = 'anon'

    for raw in text.split('\n'):
        code, comment = split_code_comment(raw)
        comment = ('// ' + comment[1:].strip()) if comment.strip() else ''

        stripped = code.strip()

        # Directives that vanish in a single-program world.
        if re.match(r'^\.(include|import|importzp|export|exportzp|segment|ifndef|endif|define)\b',
                    stripped) or re.match(r'^[A-Za-z_][A-Za-z0-9_]*\s*=', stripped):
            out.append(f'// [dropped] {raw.rstrip()}')
            continue

        # Cheap locals -> parent-scoped labels.
        m = RE_CHEAP_DEF.match(stripped)
        if m:
            code = code.replace('@' + m.group(1), f'{parent}_{m.group(1)}', 1)
        else:
            lm = RE_LABEL.match(stripped)
            if lm:
                parent = re.sub(r'^_?x16_', '', lm.group(1))
        code = RE_CHEAP_REF.sub(lambda m: f'{parent}_{m.group(1)}', code)

        # ca65 writes accumulator mode as `asl a`; KickC's asm parser would
        # read that `a` as a C variable. KickAss wants the bare mnemonic.
        code = re.sub(r'\b(asl|lsr|rol|ror|inc|dec)\s+[aA]\b(?!\w)', r'\1', code)

        # #<NAME / #>NAME with a known constant: compute the byte here.
        def lohi(m):
            op, name = m.groups()
            if name in consts:
                v = consts[name]
                return '#$%02x /*%s %s*/' % (
                    (v & 0xFF) if op == '<' else ((v >> 8) & 0xFF), op, name)
            return m.group(0)
        code = re.sub(r'#([<>])([A-Za-z_][A-Za-z0-9_]*)\b', lohi, code)

        # Remaining known constants -> hex literals, name kept as a comment.
        def sub_const(m):
            name = m.group(0)
            if name in consts:
                v = consts[name]
                return ('$%04x' % v if v > 0xFF else '$%02x' % v) + f' /*{name}*/'
            return name
        code = RE_IDENT.sub(sub_const, code)

        # Anything left that needs a human gets a loud tag.
        todos = [tag for rx, tag in FLAGS if rx.search(code)]
        todo = (' // TODO[' + '; '.join(todos) + ']') if todos else ''

        line = (code.rstrip() + ('  ' + comment if comment else '') + todo).rstrip()
        out.append(line)

    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    consts = load_consts()
    src = Path(sys.argv[1])
    result = convert(src.read_text(encoding='utf-8'), consts)
    # stdout so the output is pasted, not blindly saved: every module still
    # needs its C wrappers written by hand around these lines.
    sys.stdout.write(result)
