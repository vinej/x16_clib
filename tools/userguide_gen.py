"""Generate the missing userguide sections from the headers.

The headers ARE the documentation: a banner explaining the module, a
doc comment over each declaration, and the declaration itself. This
lifts that material into the guide's own shape rather than paraphrasing
it -- paraphrase is how a second copy starts drifting from the first,
and 549 functions is far too many to keep in step by hand.

Emits to the scratchpad; the caller splices it into the guide.
"""
import io
import os
import re
import sys

INC = 'include_ca65/x16'

# The 36 modules the guide does not cover, in the order they should read:
# drawing, then text/util, storage, KERNAL wrappers, comms, audio/video,
# numeric, and the browser.
ORDER = [
    ('Drawing engines', ['bitmap2h', 'bitmap2l', 'bitmap4h', 'bitmap4l',
                         'bitmap8h', 'shapes', 'verafx_utils']),
    ('Text and utility', ['string', 'sort', 'bcd', 'bits', 'number',
                          'tscrunch']),
    ('Storage', ['fileio', 'iec', 'dir', 'ringbuffer', 'stack']),
    ('KERNAL wrappers', ['keyboard', 'mouse', 'clock', 'i2c', 'console',
                         'graph', 'fb']),
    ('Communications', ['spi', 'serial', 'zimodem']),
    ('Audio and video', ['audiorom', 'wavfile', 'zsm', 'vdc']),
    ('Numeric', ['int16', 'int32', 'double']),
    ('The file browser', ['filepick']),
]


def unwrap(block):
    """A /* ... */ banner or doc comment -> plain prose lines."""
    out = []
    for line in block.split('\n'):
        line = re.sub(r'^\s*/?\*+/?', '', line)
        line = re.sub(r'\*+/\s*$', '', line)
        line = re.sub(r'^\s?\*\*?', '', line)
        if set(line.strip()) <= {'=', '-'} and line.strip():
            continue                    # the ==== rules
        out.append(line.rstrip())
    while out and not out[0].strip():
        out.pop(0)
    while out and not out[-1].strip():
        out.pop()
    return out


def dedent(lines):
    ind = [len(l) - len(l.lstrip()) for l in lines if l.strip()]
    n = min(ind) if ind else 0
    return [l[n:] if len(l) >= n else l for l in lines]


def parse(path):
    """(title, intro-lines, [(kind, text)]) for one header."""
    src = io.open(path, newline='', errors='ignore').read().replace('\r\n', '\n')

    banner = re.match(r'\s*/\*.*?\*/', src, re.S)
    title, intro = '', []
    if banner:
        b = unwrap(banner.group(0))
        for i, l in enumerate(b):
            m = re.search(r'x16/\w+\.h\s*--\s*(.+)$', l)
            if m:
                title = m.group(1).strip()
                # shapes.h wraps its title onto a second line; taking
                # only the first left the entry ending on a stray slash.
                j = i + 1
                while (j < len(b) and b[j].strip()
                       and not re.match(r'\s*$', b[j])
                       and title.rstrip().endswith(("/", ",", "and"))):
                    title = title.rstrip() + " " + b[j].strip()
                    j += 1
                intro = dedent(b[j:])
                break
        else:
            intro = dedent(b)
    body = src[banner.end():] if banner else src

    items = []
    pos = 0
    # a doc comment (optional) followed by a declaration or a #define run
    pat = re.compile(
        r'(?:(/\*.*?\*/)\s*)?'
        # A function-pointer typedef must be tried BEFORE the plain
        # declaration: `typedef ... (*x16_sort_cmp_t)(...)` has a paren
        # before the name, so the declaration pattern cannot reach it and
        # the typedef would be skipped -- taking its prose with it, and
        # leaving the next real function looking undocumented.
        r'(typedef\s+[^;]*?\(\s*\*\s*x16_\w+\s*\)[^;]*?;'
        r'|(?:typedef\s+)?[A-Za-z_][\w \t\*]*?\b(x16_\w+)\s*\([^;]*?\)\s*;'
        r'|typedef\s+[^;]*?\bx16_\w+\s*;'
        r'|(?:#define\s+X16_\w+[^\n]*\n)+)', re.S)
    for m in pat.finditer(body):
        doc, decl = m.group(1), m.group(2).strip()
        pos = m.end()
        if decl.startswith('#define'):
            # the include guard is not documentation
            keep = [l for l in decl.strip().splitlines()
                    if not re.match(r'#define\s+X16_\w+_H\s*$', l.strip())]
            if not keep:
                continue
            items.append(('defines', doc, chr(10).join(keep)))
        elif decl.startswith('typedef') and '(' not in decl.split('x16_')[0]:
            items.append(('typedef', doc, decl))
        else:
            items.append(('fn', doc, decl))
    return title, intro, items


def sigline(decl):
    """One-line signature, __fastcall__ dropped, whitespace squeezed."""
    s = re.sub(r'\s+', ' ', decl.replace('__fastcall__ ', '')).strip()
    return s.rstrip(';')


def emit(mod):
    path = os.path.join(INC, mod + '.h')
    title, intro, items = parse(path)
    out = []
    out.append('## `x16/%s.h` — %s' % (mod, title))
    out.append('')
    out.extend(intro)
    out.append('')

    # A doc comment often covers a RUN of declarations -- sort.h's four
    # typed entries share one. Collect the run under one heading block so
    # the prose lands once, below all of them, instead of orphaning the
    # rest.
    merged = []
    for kind, doc, decl in items:
        if (kind == 'fn' and doc is None and merged
                and merged[-1][0] == 'fn'):
            merged[-1][2].append(decl)
        elif kind == 'fn':
            merged.append(['fn', doc, [decl]])
        else:
            merged.append([kind, doc, decl])
    items = [(k, d, v) for k, d, v in merged]

    for kind, doc, decl in items:
        text = dedent(unwrap(doc)) if doc else []
        if kind == 'defines':
            if text:
                out.extend(text)
                out.append('')
            out.append('```c')
            out.extend(l.rstrip() for l in decl.strip().split('\n'))
            out.append('```')
            out.append('')
        elif kind == 'typedef':
            if text:
                out.extend(text)
                out.append('')
            out.append('```c')
            out.append(sigline(decl) + ';')
            out.append('```')
            out.append('')
        else:
            for d in decl:
                out.append('### `%s`' % sigline(d))
            out.append('')
            if text:
                out.extend(text)
                out.append('')
    return '\n'.join(out).rstrip() + '\n\n---\n'


chunks = []
toc = []
for group, mods in ORDER:
    for mod in mods:
        p = os.path.join(INC, mod + '.h')
        if not os.path.exists(p):
            sys.stderr.write('missing header: %s\n' % p)
            continue
        chunks.append(emit(mod))
        title, _, _ = parse(p)
        toc.append((mod, title))

io.open('_ug_sections.md', 'w', encoding='utf-8', newline='').write(
    '\n'.join(chunks))
def anchor(heading):
    """GitHub's slug rule, which is not the obvious one.

    Punctuation is DROPPED, not replaced -- so "I/O" is "io", not "i-o".
    Underscores SURVIVE, so `VERA_2` is "vera_2". And each space becomes
    one hyphen without collapsing runs, which is why "` — `" yields two.
    Getting this wrong left eight entries in the guide pointing nowhere.
    """
    s = re.sub(r'[^a-z0-9 _-]', '', heading.lower())
    return s.strip().replace(' ', '-')


# Numbered, continuing the guide's existing 1..26 list rather than
# starting a second one in bullets.
io.open('_ug_toc.md', 'w', encoding='utf-8', newline='').write(
    '\n'.join('%d. [`x16/%s.h` — %s](#%s)'
              % (i, m, t, anchor('`x16/%s.h` — %s' % (m, t)))
              for i, (m, t) in enumerate(toc, 27)) + '\n')

print('%d modules, %d KB of sections'
      % (len(chunks), os.path.getsize('_ug_sections.md') // 1024))
