#!/usr/bin/env python3
"""make-hvif-icon.py — writes a Haiku vector icon (HVIF) as an .rdef resource.

Haiku has no host-side HVIF tool, so this encodes the flat icon format
directly (Haiku's src/libs/icon/flat_icon/FlatIconFormat.h): solid and
vertical-gradient styles, closed polygon paths, one path per shape. Copy it
into the app's tools/ and replace build() with the app's design (64x64
grid). The build() below is EasyPaint's icon (a grey tile, two green arcs,
three orange bars), from github.com/idumych/EasyPaint.

Check the result on Haiku: Tracker must show it (a generic icon means it
didn't parse); see references/porting.md section 5.

    python3 tools/make-icon.py icon.rdef [preview.svg]

then replace the vector_icon resource at the end of EasyPaint.rdef with the
contents of icon.rdef.
"""
import math
import struct
import sys

STYLE_SOLID = 1
STYLE_GRADIENT = 2
GRADIENT_LINEAR = 0
GRADIENT_FLAG_TRANSFORM = 1 << 1
PATH_FLAG_CLOSED = 1 << 1
PATH_FLAG_NO_CURVES = 1 << 3
SHAPE_PATH_SOURCE = 10


def coord(value):
    if value == int(value) and -32 <= value <= 95:
        return bytes([int(value) + 32])
    fixed = int(round((value + 128.0) * 102.0))
    return bytes([0x80 | (fixed >> 8), fixed & 0xff])


def float24(value):
    if value == 0:
        return b"\0\0\0"
    bits = struct.unpack("<I", struct.pack("<f", value))[0]
    sign = bits >> 31
    exponent = ((bits >> 23) & 0xff) - 127 + 32
    mantissa = (bits & 0x7fffff) >> 6
    packed = (sign << 23) | (exponent << 17) | mantissa
    return bytes([packed >> 16, (packed >> 8) & 0xff, packed & 0xff])


def solid(r, g, b, a=255):
    return {"kind": "solid", "color": (r, g, b, a)}


def vertical_gradient(top, bottom, y0, y1):
    """A linear gradient from y0 (colour top) to y1 (colour bottom)."""
    return {"kind": "gradient", "stops": [(0, top), (255, bottom)],
            "y0": y0, "y1": y1}


def encode_style(style):
    if style["kind"] == "solid":
        return bytes([STYLE_SOLID]) + bytes(style["color"])
    # Gradient space runs from -64 to 64 along x; map it onto y0..y1.
    half = (style["y1"] - style["y0"]) / 128.0
    matrix = [0.0, half, -half, 0.0, 32.0, (style["y0"] + style["y1"]) / 2]
    out = bytes([STYLE_GRADIENT, GRADIENT_LINEAR, GRADIENT_FLAG_TRANSFORM,
                 len(style["stops"])])
    for value in matrix:
        out += float24(value)
    for offset, color in style["stops"]:
        out += bytes([offset]) + bytes(color)
    return out


def encode_path(points):
    assert 2 <= len(points) <= 255
    out = bytes([PATH_FLAG_CLOSED | PATH_FLAG_NO_CURVES, len(points)])
    for x, y in points:
        out += coord(round(x, 2)) + coord(round(y, 2))
    return out


def rounded_rect(left, top, right, bottom, radius, steps=6):
    points = []
    corners = [(right - radius, top + radius, -90),
               (right - radius, bottom - radius, 0),
               (left + radius, bottom - radius, 90),
               (left + radius, top + radius, 180)]
    for cx, cy, start in corners:
        for i in range(steps + 1):
            angle = math.radians(start + 90.0 * i / steps)
            points.append((cx + radius * math.cos(angle),
                           cy + radius * math.sin(angle)))
    return points


def quarter_ring(cx, cy, inner, outer, steps=14):
    """From straight up (angle 90) round to the right (angle 0)."""
    points = []
    for i in range(steps + 1):
        angle = math.radians(90 - 90.0 * i / steps)
        points.append((cx + outer * math.cos(angle),
                       cy - outer * math.sin(angle)))
    for i in range(steps + 1):
        angle = math.radians(90.0 * i / steps)
        points.append((cx + inner * math.cos(angle),
                       cy - inner * math.sin(angle)))
    return points


def rect(left, top, right, bottom):
    return [(left, top), (right, top), (right, bottom), (left, bottom)]


def build():
    """PowerMix: three translucent columns on a blue perspective floor."""
    styles = [
        solid(10, 24, 70),                                    # 0 floor outline
        vertical_gradient((40, 70, 190, 255),
                          (16, 34, 120, 255), 38, 58),        # 1 floor
        solid(110, 150, 255, 160),                            # 2 floor lines
        solid(90, 10, 10),                                    # 3 red outline
        solid(235, 40, 40, 215),                              # 4 red front
        solid(160, 20, 20, 215),                              # 5 red side
        solid(255, 130, 120, 230),                            # 6 red top
        solid(10, 70, 10),                                    # 7 green outline
        solid(50, 210, 60, 215),                              # 8 green front
        solid(25, 140, 35, 215),                              # 9 green side
        solid(150, 250, 150, 230),                            # 10 green top
        solid(10, 20, 100),                                   # 11 blue outline
        solid(60, 90, 255, 215),                              # 12 blue front
        solid(35, 55, 180, 215),                              # 13 blue side
        solid(150, 170, 255, 230),                            # 14 blue top
    ]
    shapes = []
    floor = [(14, 38), (50, 38), (61, 57), (3, 57)]
    shapes.append((0, [(13, 37), (51, 37), (63, 58), (1, 58)]))
    shapes.append((1, floor))
    for t in (0.33, 0.66):
        y = 38 + 19 * t
        left = 14 - 11 * t
        right = 50 + 11 * t
        shapes.append((2, [(left, y - 0.5), (right, y - 0.5), (right, y + 0.5), (left, y + 0.5)]))
    for x in (26, 38):
        shapes.append((2, [(x - 0.5, 38), (x + 0.5, 38), (x + 0.5 + (x - 32) * 0.6, 57),
                           (x - 0.5 + (x - 32) * 0.6, 57)]))

    def column(x, top, bottom, width, outline, front, side, cap):
        d = 4    # depth to the upper right
        shapes.append((outline, [(x - 1, top - 1), (x + d - 1, top - d - 1),
                                 (x + width + d + 1, top - d - 1), (x + width + d + 1, bottom - d + 1),
                                 (x + width + 1, bottom + 1), (x - 1, bottom + 1)]))
        shapes.append((front, [(x, top), (x + width, top), (x + width, bottom), (x, bottom)]))
        shapes.append((side, [(x + width, top), (x + width + d, top - d),
                              (x + width + d, bottom - d), (x + width, bottom)]))
        shapes.append((cap, [(x, top), (x + d, top - d), (x + width + d, top - d),
                             (x + width, top)]))

    column(27, 10, 44, 9, 7, 8, 9, 10)     # green, at the back
    column(10, 22, 50, 9, 3, 4, 5, 6)      # red, front left
    column(43, 26, 52, 9, 11, 12, 13, 14)  # blue, front right
    return styles, shapes


def encode(styles, shapes):
    out = b"ncif"
    out += bytes([len(styles)])
    for style in styles:
        out += encode_style(style)
    out += bytes([len(shapes)])
    for _, points in shapes:
        out += encode_path(points)
    out += bytes([len(shapes)])
    for index, (style, _) in enumerate(shapes):
        out += bytes([SHAPE_PATH_SOURCE, style, 1, index, 0])
    return out


def svg(styles, shapes):
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" '
             'width="256" height="256">', '<defs>']
    for index, style in enumerate(styles):
        if style["kind"] == "gradient":
            (o0, c0), (o1, c1) = style["stops"]
            lines.append(
                f'<linearGradient id="g{index}" gradientUnits="userSpaceOnUse"'
                f' x1="0" y1="{style["y0"]}" x2="0" y2="{style["y1"]}">'
                f'<stop offset="0" stop-color="rgb{c0[:3]}"/>'
                f'<stop offset="1" stop-color="rgb{c1[:3]}"/>'
                '</linearGradient>')
    lines.append('</defs>')
    for style_index, points in shapes:
        style = styles[style_index]
        if style["kind"] == "gradient":
            fill = f'url(#g{style_index})'
            opacity = 1
        else:
            fill = f'rgb{style["color"][:3]}'
            opacity = style["color"][3] / 255
        path = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
        lines.append(f'<polygon points="{path}" fill="{fill}" '
                     f'fill-opacity="{opacity:.2f}"/>')
    lines.append('</svg>')
    return "\n".join(lines)


def main():
    styles, shapes = build()
    data = encode(styles, shapes)
    hex_data = data.hex().upper()
    rows = [hex_data[i:i + 64] for i in range(0, len(hex_data), 64)]
    with open(sys.argv[1], "w") as rdef:
        rdef.write("// Generated by tools/make-icon.py; don't edit.\n")
        rdef.write("resource vector_icon {\n")
        for row in rows:
            rdef.write(f'\t$"{row}"\n')
        rdef.write("};\n")
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w") as preview:
            preview.write(svg(styles, shapes))
    print(f"{len(data)} bytes of HVIF")


if __name__ == "__main__":
    main()
