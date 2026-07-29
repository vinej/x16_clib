"""Check the vbcc C-entry shims against the real argument ABI.

Run from the repo root:

    python tools/vbcc_abi_audit.py

WHY THIS EXISTS
---------------
vbcc passes arguments in the KERNAL's virtual registers -- r0/r1, r2/r3,
r4/r5, r6/r7 are the same bytes as KERNAL r0..r3 -- with an 8-bit
argument on the next EVEN register, and the surplus spilled to the soft
stack at (sp)+0,+1... That makes many wrappers a bare `jmp`, which is
the point.

It also makes a whole class of shim silently wrong. cc65 fastcall puts
the LAST argument in A/X, so a one-argument wrapper needed no
marshalling there either -- and tools/ca65_to_vbcc.py leaves such a shim
alone, because it contains no popa for the converter to notice. But:

    void f(unsigned int w)     cc65: w in A/X      vbcc: w in r0/r1

so a bare `jmp` into a routine that reads A/X gets whatever was left in
them. x16_ring_putw and x16_stack_pushw shipped exactly that way and
corrupted the buffer they wrote to.

A lone CHAR argument is fine: both ABIs put it in A. It is the 16-bit
(and wider) cases that differ.

WHAT IS CHECKED
---------------
A shim that is nothing but `jmp target`, whose target is a routine in
the same file that opens by reading A or X, while the ABI put that
argument somewhere else. Both halves matter: the KERNAL entries take
their arguments in r0..r3 and a bare jmp to one of those is correct
precisely because vbcc's registers ARE those bytes.
"""
import io
import os
import re
import sys
import glob

BYTES = {'char': 1, 'signed char': 1, 'unsigned char': 1,
         'int': 2, 'unsigned int': 2, 'short': 2,
         'long': 4, 'unsigned long': 4, 'float': 4}


def parse_args(arglist):
    a = ' '.join(arglist.split())
    if a in ('', 'void'):
        return []
    out = []
    for p in a.split(','):
        p = p.strip()
        reg = re.search(r'__reg\("([^"]+)"\)', p)
        p2 = re.sub(r'__reg\("[^"]+"\)', '', p).strip()
        if '*' in p2 or p2.endswith('[]') or p2.endswith('_t'):
            out.append((2, reg.group(1) if reg else None))
            continue
        p2 = re.sub(r'\b(const|volatile)\b', '', p2).strip()
        if len(p2.split()) > 1 and p2.split()[-1] not in BYTES:
            p2 = ' '.join(p2.split()[:-1])
        out.append((BYTES.get(p2.strip(), 2), reg.group(1) if reg else None))
    return out


def load_protos(incdir):
    protos = {}
    for h in glob.glob(os.path.join(incdir, 'x16', '*.h')):
        t = io.open(h, newline='').read().replace('\r\n', '\n')
        t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
        for m in re.finditer(r'\b(x16_\w+)\s*\(([^;{]*?)\)\s*;', t, re.S):
            protos.setdefault(m.group(1), m.group(2))
    return protos


def main():
    protos = load_protos('include_vbcc')
    problems = []
    for p in sorted(glob.glob(os.path.join('src_vbcc', '**', '*.s'),
                              recursive=True)):
        lines = io.open(p, newline='').read().replace('\r\n', '\n').split('\n')
        labels = {}
        for i, l in enumerate(lines):
            m = re.match(r'^(\w+):\s*$', l)
            if m:
                labels[m.group(1)] = i
        for i, l in enumerate(lines):
            m = re.match(r'^_(x16_\w+):\s*$', l)
            if not m or m.group(1) not in protos:
                continue
            name = m.group(1)
            # a pure pass-through: nothing between the label and the jmp
            body, target = [], None
            for l2 in lines[i + 1:]:
                s = l2.strip()
                if not s or s.startswith(';'):
                    continue
                mt = re.match(r'^jmp\s+(\S+)', s)
                if mt:
                    target = mt.group(1)
                    break
                if re.match(r'^\w+:', s):
                    break
                body.append(s)
            if body or target is None or target not in labels:
                continue
            args = parse_args(protos[name])
            if not args:
                continue
            # Does the callee open by reading A or X?
            reads_ax = False
            for l3 in lines[labels[target] + 1:labels[target] + 3]:
                s = l3.strip().split(';')[0].strip()
                if not s:
                    continue
                reads_ax = bool(re.match(r'(sta|stx|tax|tay|pha|and|ora|cmp|asl|lsr)\b', s))
                break
            if not reads_ax:
                continue
            # The first argument's register, from __reg() or vbcc's default.
            width, reg = args[0]
            if reg is None:
                reg = 'r0/r1' if width >= 2 else 'r0'
            if reg not in ('a', 'a/x'):
                problems.append(
                    (p, name, 'falls straight into %s, which reads A/X, but '
                     'the argument rides %s' % (target, reg)))

    for path, name, why in problems:
        print('%-26s %s' % (name, why))
        print('%-26s   %s' % ('', path))
    if problems:
        print('\n%d problem(s)' % len(problems))
        return 1
    print('vbcc ABI audit clean: no pass-through shim reads the wrong register')
    return 0


if __name__ == '__main__':
    sys.exit(main())
