#!/usr/bin/env python3
"""drawcalls.py FILE.c FUNCTION  print a decompiled BeOS view function's
drawing as a compact list: float constants decoded, packed rgb_color
arguments shown as r,g,b (gcc2 x86 passes them as little-endian uint32),
and Stroke/Fill/DrawString/MovePenTo/SetHighColor calls in order."""
import re, struct, sys

src = open(sys.argv[1]).read()
name = sys.argv[2]
m = re.search(r'^// [0-9a-f]+  ' + re.escape(name) + r'$', src, re.M)
if not m:
    sys.exit('not found')
end = re.search(r'^// [0-9a-f]+  ', src[m.end():], re.M)
body = src[m.end(): m.end() + end.start() if end else len(src)]

def fl(h):
    v = int(h, 16)
    f = struct.unpack('<f', struct.pack('<I', v & 0xffffffff))[0]
    return '%g' % f

def color(h):
    v = int(h, 16)
    return 'rgb(%d,%d,%d)' % (v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff)

keep = re.compile(r'Stroke|Fill|DrawString|MovePenTo|SetHighColor|SetLowColor|0xff[0-9a-f]{6}\)|'
                  r'local_\w+ = |param_\d\[|DrawBitmap|SetDrawingMode|SetFont|SetPenSize|for \(|while|if \(|\}')
for line in body.splitlines():
    s = line.strip()
    if not s or not keep.search(s) or 'vcall slot' in s:
        continue
    s = re.sub(r'0xff([0-9a-f]{6})\)', lambda mm: color('0xff' + mm.group(1)) + ')', s)
    s = re.sub(r'\b0x([34c][0-9a-f]{7})\b', lambda mm: fl(mm.group(0)), s)
    s = re.sub(r'\(\*pp_Var\d+\[0x4b\]\)\([^,]*,', 'SetHighColor(', s)
    s = re.sub(r'\(\*pp_Var\d+\[0x45\]\)\([^,]*,', 'SetDrawingMode(', s)
    s = re.sub(r'\(undefined4\)B_SOLID_HIGH,B_SOLID_HIGH._4_4_', 'SOLID', s)
    print(s[:140])
