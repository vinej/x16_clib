"""Convert src_ca65/util/double.s into an Oscar64 global __asm blob.

The double module is 3,128 lines of SELF-CONTAINED 6502: it makes no
KERNAL call and no cross-module call, all 376 of its labels are already
plain and globally unique, and it never uses (zp),y on its argument
block -- so it needs no zero page and nothing needs renaming. Oscar64
takes assembly at global scope with internal jsr targets, so the body
goes in whole rather than being rewritten.

What this script does NOT touch: the ca65 C entry shims (lines 91-236 of
the original). Those are cc65 ABI -- popax, sreg, and a phx/ply pointer
shuffle -- and are hand-written for Oscar64 in double.c instead. That is
also why the five 65C02 stack opcodes need no rule here: every one of
them is in a shim.

So the body needs exactly two downgrades, `stz` and `bra`, plus the
usual syntax mapping.

THE stz RULE, and why it is the conservative one. `stz addr` leaves the
flags alone; `lda #0 / sta addr` does not. An earlier Oscar64
wave was bitten by exactly this, where an original relied on stz not
touching N/Z. Rather than audit 45 sites by eye, each becomes a
sequence that is transparent by construction:

    php / pha / lda #0 / sta addr / pla / plp

That costs ~10 cycles a site. If any of them turns out to sit in an
inner loop that matters, audit THAT site and shorten it -- do not
shorten them wholesale.
"""
import io
import re
import sys

SRC = 'src_ca65/util/double.s'
DST = 'src_oscar64/x16/double_body.h'
DEFS = 'src_oscar64/x16/double_defs.h'

CORE_FROM = 237                 # after the ca65 C entry shims


EQUATES = []

# Branches the stz expansion pushes out of ±127. Each becomes the
# inverted branch over a jmp, which has unlimited reach.
#
# The stz rule below trades three bytes for nine, so a routine with
# several of them can grow past a branch's range. Oscar64 says so and
# refuses to assemble -- which is the good outcome: the llvm-mos tree hit
# THIS EXACT BRANCH, `bmi double_dto_over` in d_to_s32, and its
# assembler truncated it silently instead. That is why x16_d_to_s32()
# returned 0 there instead of clamping, and why tools/llvm_branch_check.py
# exists. Add to this list when the assembler names another one.
LONG_BRANCHES = {
    ('bmi', 'double_dto_over'),
}

INVERSE = {'bmi': 'bpl', 'bpl': 'bmi', 'beq': 'bne', 'bne': 'beq',
           'bcc': 'bcs', 'bcs': 'bcc', 'bvc': 'bvs', 'bvs': 'bvc'}

_long_seq = [0]


def hexlit(m):
    return '0x' + m.group(1)


def binlit(m):
    return '0x%02x' % int(m.group(1), 2)


def convert(line):
    """One ca65 body line -> Oscar64 asm."""
    # strip a trailing comment, keeping it as a C comment
    code, comment = line, ''
    i = line.find(';')
    if i >= 0:
        code, comment = line[:i], line[i + 1:].rstrip()

    code = code.rstrip()
    if not code.strip():
        return ('    /*' + comment + ' */') if comment.strip() else ''

    # $1234 -> 0x1234, %1010 -> 0x0a
    code = re.sub(r'\$([0-9A-Fa-f]+)', hexlit, code)
    code = re.sub(r'%([01]+)', binlit, code)

    s = code.strip()

    # `NAME = value` equates are ca65, not Oscar64 assembler. They are
    # lifted out into double_defs.h as #defines, which the preprocessor
    # substitutes before the assembler ever sees them.
    m = re.match(r'^([A-Za-z_]\w*)\s*=\s*(.+)$', s)
    if m:
        EQUATES.append((m.group(1), m.group(2).strip(), comment.strip()))
        return ''

    # data directives
    m = re.match(r'\.byte\s+(.*)$', s)
    if m:
        return '    byt ' + m.group(1)
    m = re.match(r'\.word\s+(.*)$', s)
    if m:
        return '    wor ' + m.group(1)
    m = re.match(r'\.res\s+(\d+)$', s)
    if m:
        n = int(m.group(1))
        return '    byt ' + ', '.join(['0'] * n)

    # A label keeps its colon and its column. The data table puts a
    # directive on the SAME line as its label, so anything after the
    # colon is re-converted rather than passed through.
    m = re.match(r'^([a-zA-Z_][a-zA-Z0-9_]*:)\s*(.*)$', s)
    if m:
        label, rest = m.group(1), m.group(2).strip()
        if not rest:
            return label + ('        /*' + comment + ' */'
                            if comment.strip() else '')
        return label + '\n' + convert('        ' + rest
                                      + (';' + comment if comment else ''))

    # bra -> jmp
    m = re.match(r'^bra\s+(\S+)$', s)
    if m:
        s = 'jmp ' + m.group(1)

    # a branch the expansion put out of reach: invert it over a jmp
    m = re.match(r'^(b[a-z]{2})\s+(\S+)$', s)
    if m and (m.group(1), m.group(2)) in LONG_BRANCHES:
        op, tgt = m.group(1), m.group(2)
        _long_seq[0] += 1
        skip = 'x16__d_far%d' % _long_seq[0]
        return ('    %s %s\n    jmp %s\n%s:'
                % (INVERSE[op], skip, tgt, skip)
                + ('        /*' + comment + ' */' if comment.strip() else ''))

    # stz -> a flag- and A-transparent sequence (see the module note)
    m = re.match(r'^stz\s+(\S.*)$', s)
    if m:
        tgt = m.group(1)
        return ('    php\n    pha\n    lda #0\n    sta %s\n    pla\n    plp'
                % tgt) + (('        /*' + comment + ' */')
                          if comment.strip() else '')

    return '    ' + s + ('        /*' + comment + ' */'
                         if comment.strip() else '')


def main():
    lines = io.open(SRC, newline='', errors='ignore').read() \
              .replace('\r\n', '\n').split('\n')
    body = lines[CORE_FROM - 1:]

    out = []
    dropped = 0
    for l in body:
        s = l.strip()
        # ca65 bookkeeping the blob does not want
        if re.match(r'^\.(segment|export|import|importzp|include)\b', s):
            dropped += 1
            continue
        out.append(convert(l))

    text = '\n'.join(out)

    # sanity: nothing 65C02 may survive
    for op in ('stz', 'bra', 'phx', 'plx', 'phy', 'ply'):
        left = re.findall(r'(?m)^\s+%s\b' % op, text)
        if left:
            sys.stderr.write('STILL PRESENT: %s x%d\n' % (op, len(left)))
            return 1

    defs = ['/* GENERATED by tools/oscar64_double_gen.py -- do not edit.',
            '   ca65 `NAME = value` equates, lifted out of the body: the',
            '   Oscar64 assembler has no equate directive, so these are',
            '   substituted by the preprocessor instead. */']
    for name, value, comment in EQUATES:
        line = '#define %-12s %s' % (name, value)
        if comment:
            line += '   /* %s */' % comment
        defs.append(line)
    io.open(DEFS, 'w', newline='').write('\n'.join(defs) + '\n')

    io.open(DST, 'w', newline='').write(
        '/* GENERATED by tools/oscar64_double_gen.py from '
        'src_ca65/util/double.s -- do not edit.\n'
        '   Included once, inside the global __asm block in double.c. */\n'
        + text + '\n')

    print('%s: %d lines, %d ca65 directives dropped' %
          (DST, text.count('\n'), dropped))
    print('labels: %d, equates lifted: %d'
          % (len(re.findall(r'(?m)^[a-zA-Z_]\w*:', text)), len(EQUATES)))
    return 0


sys.exit(main())
