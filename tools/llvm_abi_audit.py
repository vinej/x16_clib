"""Check the llvm-mos C-entry shims against the real argument ABI.

Run from the repo root:

    python tools/llvm_abi_audit.py

It reads the prototypes from include_ca65/x16/*.h (the two trees share
one API) and checks every x16_* entry point in src_llvm/ against them.
Exits non-zero if anything looks wrong.

WHY THIS EXISTS
---------------
The converter (tools/ca65_to_llvm.py) turns the cc65 shims into llvm-mos
ones, and the cc65 originals encode the argument order in a form that is
easy to misread. cc65 fastcall puts the LAST argument in A/X and pops
the rest off a software stack; llvm-mos places every argument up front.
The naive translation -- "fill A, X, __rc2, __rc3 ... in declaration
order" -- is right for scalars and WRONG whenever a pointer is involved:

    a POINTER never occupies A/X. It takes the lowest free ALIGNED
    __rc pair, which leaves A/X free for the NEXT scalar.

    void f(void *p, unsigned int n)  ->  p in __rc2/3, n in A/X

Measured, not assumed: mos-cx16-clang -Os -fno-lto -S over probe calls
covering (ptr,int), (char,ptr,int,char), (ptr,int,ptr), (ptr,ptr,char),
(ptr,ptr,int), (int,int,ptr,int) and (ptr,long).

Getting this backwards swaps two arguments. The shim still assembles,
links and runs -- it just does the wrong thing, and when the swapped
argument is a byte count that means a 65535-iteration loop. Three bugs
of exactly this shape shipped before this check existed:
x16_spi_read_bytes, x16_spi_write_bytes and x16_con_put_image.

THE SECOND TRAP
---------------
A shim that is nothing but `jmp internal` needs no work under cc65,
because fastcall already left the single argument in A/X. Under
llvm-mos a lone POINTER argument is in __rc2/3 instead, so the bare jmp
hands the routine whatever happened to be in A/X. Nothing warns.

That is only a bug when the callee reads A/X. KERNAL entries mostly take
their pointer in r0 -- and __rc2/3 IS r0 on this target (see
cx16/lib/imag-regs.ld) -- so for those the bare jmp is exactly right.
The check below distinguishes the two by looking at what the tail target
actually does with A/X.
"""
import io
import os
import re
import sys
import glob

BYTES = {'char': 1, 'signed char': 1, 'unsigned char': 1,
         'int': 2, 'unsigned int': 2, 'short': 2,
         'long': 4, 'unsigned long': 4, 'float': 4}

NAMES = ['A', 'X'] + ['__rc%d' % n for n in range(2, 32)]


def parse_args(arglist):
    a = ' '.join(arglist.split())
    if a in ('', 'void'):
        return []
    out = []
    for p in a.split(','):
        p = p.strip()
        # Pointers, arrays, function-pointer typedefs and struct handles
        # are all two bytes that must land in an aligned pair.
        if '*' in p or p.endswith('[]') or p.endswith('_t') or p.startswith('x16_'):
            out.append(('ptr', 2))
            continue
        p = re.sub(r'\b(const|volatile)\b', '', p).strip()
        if len(p.split()) > 1 and p.split()[-1] not in BYTES:
            p = ' '.join(p.split()[:-1])
        out.append(('int', BYTES.get(p.strip(), 2)))
    return out


def slot_map(args):
    """Per-argument register slots, in declaration order."""
    free = [True] * 32
    out = []
    for kind, n in args:
        if kind == 'ptr':
            i = 2
            while not (free[i] and free[i + 1] and i % 2 == 0):
                i += 1
            free[i] = free[i + 1] = False
            out.append((NAMES[i], NAMES[i + 1]))
        else:
            s = []
            for _ in range(n):
                i = 0
                while not free[i]:
                    i += 1
                free[i] = False
                s.append(NAMES[i])
            out.append(tuple(s))
    return out


def load_protos(incdir):
    protos = {}
    for h in glob.glob(os.path.join(incdir, 'x16', '*.h')):
        t = io.open(h, newline='').read().replace('\r\n', '\n')
        t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
        for m in re.finditer(r'\b(x16_\w+)\s*\(([^;{]*?)\)\s*;', t, re.S):
            protos.setdefault(m.group(1), m.group(2))
    return protos


TAIL = re.compile(r'^\s+(jmp\s+(\S+)|rts)\s*(;.*)?$')


def shims(path):
    """Yield (name, body_lines, tail_target) for each x16_* entry."""
    lines = io.open(path, newline='').read().replace('\r\n', '\n').split('\n')
    labels = {}
    for i, l in enumerate(lines):
        m = re.match(r'^(\w+):\s*$', l)
        if m:
            labels[m.group(1)] = i
    for i, l in enumerate(lines):
        m = re.match(r'^(x16_\w+):\s*$', l)
        if not m:
            continue
        body, target = [], None
        for l2 in lines[i + 1:]:
            t = TAIL.match(l2)
            if t:
                target = t.group(2)
                break
            # A label means we fell through into the internal routine.
            if re.match(r'^\w+:\s*$', l2):
                target = l2.strip()[:-1]
                break
            body.append(l2)
        yield m.group(1), body, target, labels, lines


def reads_ax(target, labels, lines):
    """Does `target` consume A/X, or is it a KERNAL entry taking r0?"""
    if target is None or target not in labels:
        return False        # external / KERNAL: assume the r0 convention
    for l in lines[labels[target] + 1:labels[target] + 4]:
        s = l.strip()
        if not s or s.startswith(';'):
            continue
        return bool(re.match(r'(sta|stx|tay|tax|pha|phx)\b', s))
    return False


# Where a shim stages its FIRST argument. If a leading pointer is the
# first argument the ABI has already put it in __rc2/3 (= r0), so any of
# these being written from the incoming A means two arguments got swapped.
FIRST_DEST = re.compile(r'\s+sta\s+(r0L|X16_P0|X16_TPTR0)\b')


def incoming_a_stored_first(body):
    """Name the destination if the A the caller passed lands there.

    Tracks just enough to tell "A still holds an argument" from "A was
    reloaded from an __rc slot", which is the difference between the
    converter's swap and a correct staging sequence.
    """
    live = True             # A holds an incoming argument
    stack = []
    for l in body:
        s = l.strip()
        if s.startswith(';') or not s:
            continue
        op = s.split()[0]
        if op == 'pha':
            stack.append(live)
        elif op == 'pla':
            live = stack.pop() if stack else False
        elif op in ('lda', 'txa', 'tya'):
            live = False
        elif op == 'sta':
            m = FIRST_DEST.match(l)
            if m and live:
                return m.group(1)
    return None


def main():
    protos = load_protos('include_ca65')
    problems = []
    for p in sorted(glob.glob(os.path.join('src_llvm', '**', '*.s'),
                              recursive=True)):
        for name, body, target, labels, lines in shims(p):
            if name not in protos:
                continue
            args = parse_args(protos[name])
            smap = slot_map(args)
            used = set()
            for s in smap:
                used |= {int(r[4:]) for r in s if r.startswith('__rc')}
            code = '\n'.join(body)
            seen = set(int(x) for x in re.findall(r'__rc(\d+)', code))

            # (1) Reading a slot no argument occupies. Harmless for a
            # function with no arguments, where __rc2/3 is the RETURN
            # slot for a pointer or the high half of a long.
            if args and (seen - used):
                problems.append((p, name, 'reads %s; the ABI assigns %s'
                                 % (sorted(seen - used), sorted(used) or 'nothing')))

            # (2) A LEADING POINTER is already in __rc2/3, which IS r0.
            # A shim that stores A/X into the first staging destination
            # is treating A/X as that pointer -- the converter's mistake.
            # Reading __rc2/3 out first makes it deliberate instead, so
            # only an unguarded store counts.
            if args and args[0][0] == 'ptr':
                hit = incoming_a_stored_first(body)
                if hit:
                    problems.append(
                        (p, name, 'stores the incoming A into %s, but the '
                         'leading pointer is in __rc2/3 (= r0) -- A/X hold '
                         'the NEXT argument' % hit))

            # (3) A pure pass-through whose callee wants A/X, when the
            # ABI put that argument in an __rc pair instead.
            stripped = [l for l in body if l.strip()
                        and not l.strip().startswith(';')]
            if not stripped and args and reads_ax(target, labels, lines):
                last = smap[-1]
                if last[0].startswith('__rc'):
                    problems.append(
                        (p, name, 'falls straight into %s, which reads A/X, '
                         'but the last argument is in %s'
                         % (target, '/'.join(last))))

    for path, name, why in problems:
        print('%-26s %s' % (name, why))
        print('%-26s   %s' % ('', path))
    if problems:
        print('\n%d problem(s)' % len(problems))
        return 1
    print('llvm ABI audit clean: every shim reads the slots the ABI assigns')
    return 0


if __name__ == '__main__':
    sys.exit(main())
