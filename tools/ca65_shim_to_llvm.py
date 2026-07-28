"""Rewrite the cc65 C-entry shims in a converted module for llvm-mos.

    python tools/ca65_shim_to_llvm.py src_ca65/audio/zsm.s src_llvm/audio/zsm.s

tools/ca65_to_llvm.py translates syntax; it deliberately leaves the
shims alone so they fail loudly rather than quietly passing the wrong
registers. This turns them over to the llvm-mos ABI.

WHERE THE ARGUMENTS ARE
-----------------------
cc65 fastcall puts the LAST argument in A/X and pops the rest off a
software stack, so a shim reads bottom-up. llvm-mos places all of them
up front: A, X, then __rc2, __rc3 ... one byte at a time in declaration
order, with one exception --

    a POINTER never occupies A/X. It takes the lowest free ALIGNED
    __rc pair, leaving A/X free for the NEXT scalar.

    void f(void *p, unsigned int n)  ->  p in __rc2/3, n in A/X

Measured against mos-cx16-clang -Os -fno-lto -S, not inferred. See
tools/llvm_abi_audit.py, which re-checks the result of this script.

WHY THE MOVE ORDER IS NOT OBVIOUS
---------------------------------
cx16/lib/imag-regs.ld aliases the imaginary registers onto the KERNAL
ones -- deliberately, so wrappers can set up KERNAL arguments without
stashing:

    __rc2/__rc3 = r0 = $02      __rc4/__rc5 = r1      __rc6/__rc7 = r2

So a shim's sources and destinations are frequently the SAME BYTES.
Writing r1 from __rc4 writes exactly where __rc4 lives. Emitting the
moves in argument order silently destroys arguments not yet read.

This resolves them as a parallel move: repeatedly emit a move whose
destination is nobody's unread source, and break a genuine cycle
through X16_T0. A/X are read last, off the hardware stack, because
every `lda` in between clobbers them.
"""
import io
import os
import re
import sys
import glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from llvm_abi_audit import parse_args, slot_map, load_protos     # noqa: E402

RE_LABEL = re.compile(r'^_?(x16_[a-z0-9_]+):\s*$')
RE_ST = re.compile(r'^\s+(sta|stx)\s+([A-Za-z_][A-Za-z0-9_+]*)\s*(?:;\s*(.*))?$')
RE_POP = re.compile(r'^\s+jsr\s+(popax|popa)\s*(?:;.*)?$')
RE_TAIL = re.compile(r'^\s+(jmp\s+\S+|rts)\s*(?:;.*)?$')
RE_ANYLABEL = re.compile(r'^\.?\w+:\s*$')
RE_RREG = re.compile(r'^r(\d+)(L|H)$')


def zp_addr(sym):
    """Byte address of a symbol that aliases the KERNAL register block."""
    m = RE_RREG.match(sym)
    if m:
        return 2 + 2 * int(m.group(1)) + (0 if m.group(2) == 'L' else 1)
    m = re.match(r'^__rc(\d+)$', sym)
    if m:
        return int(m.group(1))
    return None                 # X16_P*/X16_T* live at $26+; no aliasing


def parse_ca65_shim(lines, i):
    """Destinations of each argument, in DECLARATION order, or None."""
    segs, cur = [], []
    j = i + 1
    while j < len(lines):
        l = lines[j]
        if RE_POP.match(l):
            segs.append(cur)
            cur = []
        elif RE_ST.match(l):
            m = RE_ST.match(l)
            cur.append((m.group(1), m.group(2), (m.group(3) or '').strip()))
        elif RE_TAIL.match(l) or RE_ANYLABEL.match(l):
            break       # a tail jump, or a fall-through into the internal
                        # routine -- whose own label is not an x16_ one
        elif l.strip() and not l.strip().startswith(';'):
            return None, j      # not a mechanical shim
        j += 1
    segs.append(cur)
    if len(segs) < 2 or any(not s for s in segs):
        return None, j
    segs.reverse()              # cc65 reads bottom-up
    return segs, j


def emit(arg_slots, segs):
    """The llvm-mos body: move each argument from its slot to its home."""
    moves = []                  # (dst_sym, src, comment)
    for slots, seg in zip(arg_slots, segs):
        # Within one argument, `sta` takes the low byte and `stx` the high.
        for op, dst, cmt in seg:
            idx = 0 if op == 'sta' else 1
            if idx >= len(slots):
                return None
            moves.append((dst, slots[idx], cmt))

    out = []
    # The aliasing cuts both ways: a leading pointer's __rc pair IS r0,
    # so the "move" is already done. Dropping those is not just an
    # optimisation -- a self-move looks like an unbreakable cycle.
    moves = [m for m in moves
             if zp_addr(m[0]) is None or zp_addr(m[0]) != zp_addr(m[1])]
    if not moves:
        return []
    mem = [m for m in moves if m[1].startswith('__rc')]
    reg = [m for m in moves if not m[1].startswith('__rc')]

    # A and X have to survive every `lda` below, so park them first.
    if mem and reg:
        out.append('        pha                             ; A and X carry arguments that')
        if any(s == 'X' for _, s, _ in reg):
            out.append('        phx                             ; the loads below clobber')
        else:
            out.append('                                        ; the loads below clobber')

    pending = list(mem)
    while pending:
        live = {zp_addr(s) for _, s, _ in pending}
        live.discard(None)
        pick = next((m for m in pending if zp_addr(m[0]) not in live), None)
        if pick is None:
            # A true cycle: two arguments swapping bytes. Break it by
            # copying one source out to the library's own scratch, which
            # is at $26+ and aliases nothing.
            dst, src, cmt = pending[0]
            out.append('        lda     %s' % src)
            out.append('        sta     X16_T0                  ; break a move cycle')
            pending[0] = (dst, 'X16_T0', cmt)
            continue
        dst, src, cmt = pick
        out.append('        lda     %s' % src)
        out.append('        sta     %s%s' % (dst, ('                ; ' + cmt) if cmt else ''))
        pending.remove(pick)

    if mem and reg:
        if any(s == 'X' for _, s, _ in reg):
            out.append('        plx')
        out.append('        pla')
    for dst, src, cmt in reg:
        op = 'sta' if src == 'A' else 'stx'
        out.append('        %s     %s%s' % (op, dst, ('                ; ' + cmt) if cmt else ''))
    return out


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    ca65_path, llvm_path = sys.argv[1], sys.argv[2]
    protos = load_protos('include_ca65')

    ca = io.open(ca65_path, newline='').read().replace('\r\n', '\n').split('\n')
    ca_shims = {}
    for i, l in enumerate(ca):
        m = RE_LABEL.match(l)
        if m:
            segs, _ = parse_ca65_shim(ca, i)
            if segs:
                ca_shims[m.group(1)] = segs

    src = io.open(llvm_path, newline='').read()
    crlf = '\r\n' in src
    lines = src.replace('\r\n', '\n').split('\n')

    out, i, done, skipped = [], 0, [], []
    while i < len(lines):
        m = RE_LABEL.match(lines[i])
        name = m.group(1) if m else None
        if not name or name not in ca_shims or name not in protos:
            out.append(lines[i])
            i += 1
            continue
        _, end = parse_ca65_shim(lines, i)
        segs = ca_shims[name]
        slots = slot_map(parse_args(protos[name]))
        body = emit(slots, segs) if len(slots) == len(segs) else None
        if body is None:
            out.append(lines[i])
            i += 1
            skipped.append(name)
            continue
        out.append(lines[i])
        out.extend(body)
        done.append(name)
        i = end

    text = '\n'.join(out)
    io.open(llvm_path, 'w', newline='').write(
        text.replace('\n', '\r\n') if crlf else text)
    # Anything still calling popa/popax is still on the cc65 ABI. Name it:
    # a shim left half-converted is the failure mode this whole exercise
    # exists to prevent.
    left = []
    for n, l in enumerate(text.split('\n')):
        if RE_POP.match(l):
            for k in range(n, -1, -1):
                mm = RE_LABEL.match(text.split('\n')[k])
                if mm:
                    if mm.group(1) not in left:
                        left.append(mm.group(1))
                    break

    print('%s: rewrote %d shim(s)' % (os.path.basename(llvm_path), len(done)))
    if skipped:
        print('    argument count did not match the header: %s'
              % ', '.join(skipped))
    if left:
        print('    STILL ON THE cc65 ABI, convert by hand: %s' % ', '.join(left))
    return 1 if left or skipped else 0


if __name__ == '__main__':
    sys.exit(main())
