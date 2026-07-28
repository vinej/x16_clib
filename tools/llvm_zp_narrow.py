"""Force zero-page addressing for the library's zero-page symbols.

    python tools/llvm_zp_narrow.py src_llvm/audio/zsm.s ...
    python tools/llvm_zp_narrow.py            # the whole src_llvm tree

ca65 is told a symbol lives in page zero with .importzp, and picks the
two-byte form. llvm-mos has no equivalent: an external symbol's address
is a relocation, the assembler cannot know it is under $100, and it
emits the three-byte absolute form. Nothing shrinks it later -- ld.lld
does not re-lay-out the section, as the relocation records show: every
`sta X16_P0` carries R_MOS_ADDR16 and stays three bytes in the final
image.

`mos8(sym)` is llvm-mos's way of saying "this operand is one byte":

        lda     mos8(X16_P0)    ; a5 xx
        lda     X16_P0          ; ad xx xx

Applying it to the block in core/x16zp.s, and to the __rc imaginary
registers, is worth a byte and a cycle per access -- the payoff the
build script's -mreserve-zp=16 comment promises but that the absolute
form was quietly throwing away.

It also matters for CORRECTNESS. The fat encoding inflates every
routine, and mos-as does not diagnose a conditional branch that ends up
further than a signed byte reaches -- it truncates the displacement and
reports success (see tools/llvm_branch_check.py). Shrinking the code
pulls branches back into range.

Left alone deliberately:
  - (X16_PTR0),y and friends -- indirect,y is already a two-byte form
  - #<X16_P0 / #>X16_P0       -- immediates, no addressing mode involved
  - anything indexed by Y     -- there is no lda zp,y on this CPU, so
                                 narrowing it would not assemble
  - r0L, VERA_*, ...          -- plain constants; the assembler already
                                 picks the short form for those
"""
import io
import os
import re
import sys
import glob

# Symbols that genuinely live in page zero and arrive as relocations.
ZPSYM = r'(?:X16_(?:[PT][0-7]|[PT]PTR[0-3])|__rc\d+)'

# op  operand[+n][,x]  ; comment
LINE = re.compile(
    r'^(\s+)(ad[cd]|and|asl|bit|cmp|cp[xy]|dec|eor|inc|jmp|lda|ld[xy]|lsr|ora'
    r'|ro[lr]|sbc|sta|st[xyz]|trb|tsb)'
    r'(\s+)(' + ZPSYM + r'(?:\+\d+)?)((?:,\s*[xX])?)(\s*)(;.*)?$')


def narrow(text):
    out = []
    for l in text.split('\n'):
        m = LINE.match(l)
        if not m:
            out.append(l)
            continue
        ws, op, sp, operand, index, tail, cmt = m.groups()
        new = '%s%s%smos8(%s)%s' % (ws, op, sp, operand, index)
        # Keep the comment column where it was, so diffs stay readable.
        if cmt:
            pad = max(1, len(l) - len(l.rstrip()) + (len(l) - len(cmt)) - len(new))
            new = new + ' ' * pad + cmt
        out.append(new)
    return '\n'.join(out)


def main():
    paths = sys.argv[1:] or sorted(
        glob.glob(os.path.join('src_llvm', '**', '*.s'), recursive=True))
    changed = 0
    for p in paths:
        src = io.open(p, newline='').read()
        crlf = '\r\n' in src
        body = src.replace('\r\n', '\n')
        new = narrow(body)
        if new != body:
            io.open(p, 'w', newline='').write(
                new.replace('\n', '\r\n') if crlf else new)
            n = len(re.findall(r'mos8\(', new)) - len(re.findall(r'mos8\(', body))
            print('%-34s %d access(es) narrowed' % (p, n))
            changed += 1
    print('%d file(s) changed' % changed)
    return 0


if __name__ == '__main__':
    sys.exit(main())
