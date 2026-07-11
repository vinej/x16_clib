#!/usr/bin/env python3
"""
KickC -> Oscar64 source converter for x16clib.

The KickC tree (src_kickc/x16) is already the right shape for Oscar64 --
C functions whose bodies are inline assembly reading parameters by name
-- so the Oscar64 port transforms it rather than re-porting from ca65.
This tool does the MECHANICAL half of one file:

    asm {              ->  __asm {          (multi-instruction one-liners
                                             are split, one per line)
    $9f25 / #$0f       ->  0x9f25 / #0x0f   (Oscar64 asm takes C-style
    #%11110011         ->  #0xf3             literals, not $ or %)
    /*NAME*/           ->  merged into the line's // comment
    .byte a, b, c      ->  byt a, b, c
    bra L              ->  jmp L            (Oscar64's inline assembler
                                             is NMOS-only: no 65C02)
    lda #m / trb addr  ->  lda addr, and #~m, sta addr
    lda #m / tsb addr  ->  lda addr, ora #m, sta addr

What it only FLAGS (loud TODO comments, never a guess), because each
needs its clobbers thought about at the site:

    stz addr           A is clobbered by the lda #0 replacement
    phx/plx/phy/ply    A is clobbered by the txa/tya route
    lda (p) / sta (p)  needs (p),y and a Y you can prove is free
    inc / dec  (bare)  accumulator mode does not exist on NMOS
    __mem/__address    KickC storage qualifiers; Oscar64 wants
                       __zeropage or nothing
    sta r (locals)     Oscar64 loses asm writes to C locals, even
                       volatile ones: results leave via `sta accu` in a
                       `return __asm {}` or via a module global

Usage:  python tools/kickc_to_oscar64.py in.c out.c
"""

import re
import sys

MNEM = ('adc and asl bcc bcs beq bit bmi bne bpl brk bvc bvs clc cld cli clv '
        'cmp cpx cpy dec dex dey eor inc inx iny jmp jsr lda ldx ldy lsr nop '
        'ora pha php pla plp rol ror rti rts sbc sec sed sei sta stx sty tax '
        'tay tsx txa txs tya bra stz phx plx phy ply trb tsb byt').split()

RE_HEX = re.compile(r'\$([0-9a-fA-F]+)')
RE_BIN = re.compile(r'#%([01]+)')


def split_oneliner(line):
    """`asm { lda c jsr $ffd2 }` -> a list of one-instruction lines."""
    m = re.match(r'^(\s*)(?:__)?asm(\s+volatile)?\s*\{\s*(.+?)\s*\}\s*$', line)
    if not m:
        return None
    indent, _, body = m.group(1), m.group(2), m.group(3)
    toks = body.split()
    lines = [indent + '__asm {']
    cur = []
    for t in toks:
        if t.lower() in MNEM and cur:
            lines.append(indent + '    ' + ' '.join(cur))
            cur = [t]
        else:
            cur.append(t)
    if cur:
        lines.append(indent + '    ' + ' '.join(cur))
    lines.append(indent + '}')
    return lines


def convert_line(code):
    """Token-level rewrites on one line of asm."""
    todos = []

    code = RE_BIN.sub(lambda m: '#0x%02x' % int(m.group(1), 2), code)
    code = RE_HEX.sub(lambda m: '0x' + m.group(1), code)
    code = re.sub(r'^(\s*)\.byte\b', r'\1byt', code)
    code = re.sub(r'^(\s*)bra\b', r'\1jmp', code)

    s = code.strip()
    if re.match(r'^stz\b', s):
        todos.append('stz: NMOS has none -- lda #0/sta if A is free')
    if re.match(r'^(phx|plx|phy|ply)\b', s):
        todos.append('phx/y: NMOS has none -- txa/tya route clobbers A')
    if re.match(r'^(trb|tsb)\b', s):
        todos.append('trb/tsb alone: pair with the preceding lda #mask')
    if re.match(r'^(lda|sta|ora|and|adc|sbc|cmp|bit)\s+\([^)]*\)\s*(//|$)', s):
        todos.append('(p) no-index: needs (p),y with Y proven free')
    if re.match(r'^(inc|dec)\s*(//|$)', s):
        todos.append('bare inc/dec: no accumulator mode on NMOS')
    return code, todos


def fold_comments(code):
    """/*NAME*/ chunks fold into the trailing // comment."""
    names = re.findall(r'/\*\s*(.*?)\s*\*/', code)
    if not names:
        return code
    code = re.sub(r'\s*/\*.*?\*/', '', code)
    joined = ' '.join(names)
    if '//' in code:
        return code + ' (' + joined + ')'
    return code.rstrip() + (' ' * max(1, 40 - len(code.rstrip()))) + '// ' + joined


def convert(text):
    out = []
    in_asm = False
    pending_mask = None      # (line_index, mask_value) of a `lda #imm`

    for raw in text.split('\n'):
        if not in_asm:
            split = split_oneliner(raw)
            if split:
                # run the body lines through the asm pipeline
                out.append(split[0])
                for l in split[1:-1]:
                    l, todos = convert_line(l)
                    l = fold_comments(l)
                    if todos:
                        l += '  // TODO[' + '; '.join(todos) + ']'
                    out.append(l)
                out.append(split[-1])
                continue
            line = raw
            if re.search(r'^\s*asm\s*\{', line):
                line = re.sub(r'\basm\s*\{', '__asm {', line)
                in_asm = True
                pending_mask = None
            out.append(line)
            continue

        # inside an asm block
        if re.match(r'^\s*\}\s*$', raw):
            in_asm = False
            out.append(raw)
            continue

        code, todos = convert_line(raw)

        # lda #imm followed by trb/tsb: rewrite the RMW idiom whole
        m = re.match(r'^(\s*)lda #(0x[0-9a-fA-F]+)\s*(//.*|/\*.*)?$', code.strip() and code or '')
        mm = re.match(r'^(\s*)lda #(0x[0-9a-fA-F]+)', code)
        if mm:
            pending_mask = (len(out), int(mm.group(2), 16), mm.group(1))
            out.append(fold_comments(code))
            continue
        tm = re.match(r'^(\s*)(trb|tsb)\s+(\S+)\s*(.*)$', code)
        if tm and pending_mask and pending_mask[0] == len(out) - 1:
            indent, op, addr, rest = tm.groups()
            _, mask, _ = pending_mask
            del out[-1]                     # drop the lda #mask
            out.append(indent + 'lda ' + addr + (('  ' + rest) if rest.strip() else ''))
            if op == 'trb':
                out.append(indent + 'and #0x%02x' % ((~mask) & 0xFF))
            else:
                out.append(indent + 'ora #0x%02x' % mask)
            out.append(indent + 'sta ' + addr)
            pending_mask = None
            continue
        pending_mask = None

        code = fold_comments(code)
        if todos:
            code += '  // TODO[' + '; '.join(todos) + ']'
        out.append(code)

    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src = open(sys.argv[1], encoding='utf-8').read()
    res = convert(src)
    open(sys.argv[2], 'w', encoding='utf-8', newline='\n').write(res)
    left = [f'{n}: {l.strip()[:70]}' for n, l in enumerate(res.split('\n'), 1)
            if 'TODO[' in l]
    if left:
        print(f'{sys.argv[2]}: {len(left)} site(s) need a human:', file=sys.stderr)
        for l in left:
            print('   ' + l, file=sys.stderr)
