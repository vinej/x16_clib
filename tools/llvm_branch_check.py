"""Find -- and optionally fix -- relative branches llvm-mos put out of range.

    python tools/llvm_branch_check.py            # report
    python tools/llvm_branch_check.py --fix      # rewrite the offenders

THE DEFECT
----------
mos-as does not diagnose a conditional branch whose target is further
than the 6502's signed-byte displacement reaches. It truncates the
displacement and reports success. Minimal case, exit code 0, no
diagnostic:

        lda     $00
        cmp     #$7a
        bne     .Lfar           ; assembles to d0 8c -- 116 bytes BACKWARD
        <140 x nop>
    .Lfar:

That is how x16_zsm_init came to accept a file whose first magic byte
was wrong: the branch to the error tail jumped into another routine and
the machine ran off into the weeds. It surfaced only because the
corrupted PC happened to loop; a branch landing somewhere quieter would
just silently do the wrong thing.

ca65 rejects these outright, so the cc65 tree never had the problem.
The llvm tree does because the same source assembles LARGER there --
see tools/llvm_zp_narrow.py for why, and for the pass that wins most of
it back.

HOW THEY ARE FOUND
------------------
Assemble each module with -g, then read the disassembly back with the
DWARF line table threaded through it. That gives every instruction's
address AND the source line that produced it, which is enough to work
out where each branch was MEANT to go and compare it with the byte the
assembler actually wrote.

Resolving the target from the SOURCE rather than from the object is
what makes this trustworthy. An earlier version looked only at where
the branch landed and called it truncated if target+-256 was a known
symbol -- which flagged eleven innocent `bne 1f` branches, because GNU
as's numeric local labels never reach the symbol table and something
else happened to sit 256 bytes away. Named labels come from the symbol
table (on a copy with `.L` rewritten to `L_`, so they survive); `1f`
and `1b` are resolved by walking the source for the nearest `1:`.

HOW THEY ARE FIXED
------------------
Invert the branch over a jump, which has no range limit:

        bne     far                     bcc     .Lbrfix_1
                              ->        jmp     far
                                    .Lbrfix_1:

Neither the inverted branch nor the jump touches the flags, so anything
downstream that depends on them is unaffected. Fixing changes sizes, so
--fix rescans and repeats until the tree is clean.
"""
import io
import os
import re
import sys
import glob
import shutil
import subprocess
import tempfile

# Relative-branch opcodes. BBR/BBS are not emitted by this codebase.
BRANCH = {0x10: 'bpl', 0x30: 'bmi', 0x50: 'bvc', 0x70: 'bvs',
          0x90: 'bcc', 0xb0: 'bcs', 0xd0: 'bne', 0xf0: 'beq',
          0x80: 'bra'}

INVERSE = {'bpl': 'bmi', 'bmi': 'bpl', 'bvc': 'bvs', 'bvs': 'bvc',
           'bcc': 'bcs', 'bcs': 'bcc', 'bne': 'beq', 'beq': 'bne'}


def find_tool(root, name):
    for base in (os.path.join(root, 'llvm-mos', 'bin'), r'C:\llvm-mos\bin'):
        p = os.path.join(base, name)
        if os.path.exists(p):
            return p
    return None


def scan(clang, objdump, nm, inc, tmp):
    """Every out-of-range branch, as (src, line, mnemonic, target, distance)."""
    findings = []
    for src in sorted(glob.glob(os.path.join('src_llvm', '**', '*.s'),
                                recursive=True)):
        text = io.open(src, newline='').read().replace('.L', 'L_')
        cp = os.path.join(tmp, os.path.basename(src))
        io.open(cp, 'w', newline='').write(text)
        obj = cp[:-2] + '.o'
        r = subprocess.run([clang, '-c', '-g', '-I', inc, '-o', obj, cp],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print('%s: did not assemble\n%s' % (src, r.stderr.strip()[:400]))
            findings.append((src, None, None, None, 0))
            continue

        symaddr = {}
        out = subprocess.run([nm, '-n', obj], capture_output=True, text=True)
        for l in out.stdout.split('\n'):
            m = re.match(r'^([0-9a-fA-F]+)\s+\S\s+(\S+)', l)
            if m:
                symaddr.setdefault(m.group(2), int(m.group(1), 16))

        dis = subprocess.run([objdump, '-d', '-l', '--triple=mos', obj],
                             capture_output=True, text=True).stdout
        srclines = io.open(src, newline='').read().replace('\r\n', '\n').split('\n')

        # First pass: where each source line starts in the object.
        line_addr = {}
        branches = []
        srcline = None
        for l in dis.split('\n'):
            ml = re.match(r'^; .*\.s:(\d+)', l)
            if ml:
                srcline = int(ml.group(1))
                continue
            m = re.match(r'^\s+([0-9a-f]+):\s+([0-9a-f]{2})(?: ([0-9a-f]{2}))?\s+\t(\w+)', l)
            if not m or srcline is None:
                continue
            addr, op, mnem = int(m.group(1), 16), int(m.group(2), 16), m.group(4)
            line_addr.setdefault(srcline, addr)
            if op in BRANCH and BRANCH[op] == mnem:
                branches.append((addr, mnem, srcline))

        for addr, mnem, srcline in branches:
            if not (1 <= srcline <= len(srclines)):
                continue
            m = re.match(r'^\s*\S+\s+(\S+)\s*(;.*)?$', srclines[srcline - 1])
            if not m:
                continue
            tgt = m.group(1)
            # `1f` / `1b`: the nearest numeric label after / before this line.
            mn = re.match(r'^(\d+)([fb])$', tgt)
            if mn:
                want, direction = mn.group(1) + ':', mn.group(2)
                rng = (range(srcline, len(srclines)) if direction == 'f'
                       else range(srcline - 2, -1, -1))
                taddr = None
                for k in rng:
                    if srclines[k].strip().startswith(want):
                        taddr = line_addr.get(k + 1)
                        break
            else:
                taddr = symaddr.get(tgt if not tgt.startswith('.L')
                                    else 'L_' + tgt[2:])
            if taddr is None:
                continue
            dist = taddr - (addr + 2)
            if dist < -128 or dist > 127:
                findings.append((src, srcline, mnem, tgt, dist))
    return findings


def apply_fixes(findings):
    """Invert each offending branch over a jmp. Returns files touched."""
    by_file = {}
    for src, line, mnem, target, _ in findings:
        if line and mnem in INVERSE:
            by_file.setdefault(src, []).append((line, mnem, target))
    for src, items in by_file.items():
        raw = io.open(src, newline='').read()
        crlf = '\r\n' in raw
        lines = raw.replace('\r\n', '\n').split('\n')
        # Bottom-up, so earlier line numbers stay valid.
        for n, (line, mnem, target) in enumerate(
                sorted(items, key=lambda t: -t[0])):
            i = line - 1
            m = re.match(r'^(\s+)%s(\s+)(\S+)\s*(;.*)?$' % mnem, lines[i])
            if not m or m.group(3) != target:
                print('  %s:%d no longer looks like `%s %s`, skipped'
                      % (src, line, mnem, target))
                continue
            ws, sp, cmt = m.group(1), m.group(2), m.group(4)
            lbl = '.Lbrfix_%d_%d' % (line, n)
            out = ['%s%s%s%s' % (ws, INVERSE[mnem], sp, lbl),
                   '%sjmp%s%s%s' % (ws, sp, target,
                                    ('     ' + cmt) if cmt else
                                    '     ; inverted over a jmp: too far'
                                    ' for a relative branch'),
                   '%s:' % lbl]
            lines[i:i + 1] = out
        text = '\n'.join(lines)
        io.open(src, 'w', newline='').write(
            text.replace('\n', '\r\n') if crlf else text)
    return sorted(by_file)


def main():
    do_fix = '--fix' in sys.argv
    root = os.getcwd()
    clang = find_tool(root, 'mos-cx16-clang.bat')
    objdump = find_tool(root, 'llvm-objdump.exe')
    nm = find_tool(root, 'llvm-nm.exe')
    if not (clang and objdump and nm):
        print('llvm-mos not found; set LLVM_MOS_HOME or unpack it to llvm-mos/')
        return 2

    tmp = tempfile.mkdtemp(prefix='x16branch')
    inc = os.path.join(tmp, 'inc')
    shutil.copytree(os.path.join(root, 'src_llvm', 'core'), inc)
    for f in glob.glob(os.path.join(inc, '*.inc')):
        t = io.open(f, newline='').read()
        io.open(f, 'w', newline='').write(t.replace('.L', 'L_'))

    try:
        for round_no in range(1, 7):
            findings = scan(clang, objdump, nm, inc, tmp)
            for src, line, mnem, target, dist in findings:
                if mnem:
                    print('%s:%s  %s %s is %+d bytes away, past the %s limit'
                          % (src, line, mnem, target, dist,
                             'forward +127' if dist > 0 else 'backward -128'))
            if not findings:
                print('branch check clean: every relative branch reaches its label')
                return 0
            if not do_fix:
                print('\n%d out-of-range branch(es). mos-as does not diagnose '
                      'these -- rerun with --fix to invert them over a jmp.'
                      % len(findings))
                return 1
            print('-- round %d: fixing %d --' % (round_no, len(findings)))
            touched = apply_fixes(findings)
            if not touched:
                print('nothing could be fixed automatically')
                return 1
        print('still not clean after 6 rounds')
        return 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
