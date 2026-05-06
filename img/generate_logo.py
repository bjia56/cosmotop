#!/usr/bin/env python3
# Generates logo.svg from the banner data in cosmotop.cpp.
# Each character cell is CW x CH pixels. Full-block █ chars fill the entire
# cell; box-drawing chars (╗ ╚ ╝ ╔ ║ ═) are rendered as centered bars:
#   ║ = vertical bar centered horizontally in the cell
#   ═ = horizontal bar centered vertically in the cell
#   corners = intersection of the two bars extending toward the open quadrant

import os

CW = 10  # cell width in pixels
CH = 20  # cell height in pixels
T  = CW // 3  # edge-strip thickness (~6px)

cosmo_rows = [
    ("██████╗ ██████╗ ██████╗ ██╗     ██╗ ██████╗ ", "#ced7e0"),
    ("██╔═══╝ ██╔═██║ ██╔═══╝ ████╗ ████║ ██╔═██║ ", "#9ccddc"),
    ("██║     ██║ ██║ ██████╗ ██╔═██╔═██║ ██║ ██║ ", "#5591a9"),
    ("██║     ██║ ██║ ╚═══██║ ██║ ╚═╝ ██║ ██║ ██║ ", "#054569"),
    ("██████╗ ██████║ ██████║ ██║     ██║ ██████║ ",  "#062c43"),
    ("╚═════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ", "#000000"),
]

top_rows = [
    ("██████╗ ██████╗ ██████╗", "#A9A9A9"),
    ("╚═██╔═╝ ██╔═██║ ██╔═██║", "#989898"),
    ("  ██║   ██║ ██║ ██████║", "#888888"),
    ("  ██║   ██║ ██║ ██╔═══╝", "#777777"),
    ("  ██║   ██████║ ██║",     "#666666"),
    ("  ╚═╝   ╚═════╝ ╚═╝",    "#000000"),
]


def cell_rects(char, cx, cy, color):
    x, y = cx * CW, cy * CH
    # Centered bar offsets
    bx = (CW - T) // 2  # horizontal center offset (for vertical bars)
    by = (CH - T) // 2  # vertical center offset (for horizontal bars)
    if char == '█':
        return [f'<rect x="{x}" y="{y}" width="{CW}" height="{CH}" fill="{color}"/>']
    if char == '╗':  # horizontal bar from left to center, vertical bar from center down
        return [
            f'<rect x="{x}" y="{y+by}" width="{bx+T}" height="{T}" fill="{color}"/>',
            f'<rect x="{x+bx}" y="{y+by+T}" width="{T}" height="{CH-by-T}" fill="{color}"/>',
        ]
    if char == '╚':  # vertical bar from top to center, horizontal bar from center to right
        return [
            f'<rect x="{x+bx}" y="{y}" width="{T}" height="{by+T}" fill="{color}"/>',
            f'<rect x="{x+bx+T}" y="{y+by}" width="{CW-bx-T}" height="{T}" fill="{color}"/>',
        ]
    if char == '╝':  # vertical bar from top to center, horizontal bar from left to center
        return [
            f'<rect x="{x+bx}" y="{y}" width="{T}" height="{by+T}" fill="{color}"/>',
            f'<rect x="{x}" y="{y+by}" width="{bx}" height="{T}" fill="{color}"/>',
        ]
    if char == '╔':  # horizontal bar from center to right, vertical bar from center down
        return [
            f'<rect x="{x+bx+T}" y="{y+by}" width="{CW-bx-T}" height="{T}" fill="{color}"/>',
            f'<rect x="{x+bx}" y="{y+by}" width="{T}" height="{CH-by}" fill="{color}"/>',
        ]
    if char == '║':  # centered vertical bar
        return [f'<rect x="{x+bx}" y="{y}" width="{T}" height="{CH}" fill="{color}"/>']
    if char == '═':  # centered horizontal bar
        return [f'<rect x="{x}" y="{y+by}" width="{CW}" height="{T}" fill="{color}"/>']
    return []  # space or unknown


def render_rows(rows, col_offset=0):
    rects = []
    for row_idx, (text, color) in enumerate(rows):
        for col_idx, char in enumerate(text):
            rects.extend(cell_rects(char, col_offset + col_idx, row_idx, color))
    return rects


cosmo_len = len(cosmo_rows[0][0])
top_len   = len(top_rows[0][0])
width     = (cosmo_len + top_len) * CW
height    = 6 * CH

rects = render_rows(cosmo_rows, col_offset=0) + render_rows(top_rows, col_offset=cosmo_len)

lines = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    f'<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg">',
    *[f'  {r}' for r in rects],
    '</svg>',
]

out = os.path.join(os.path.dirname(__file__), 'logo.svg')
with open(out, 'w') as f:
    f.write('\n'.join(lines) + '\n')
print(f"Written {len(rects)} elements to {out}")
