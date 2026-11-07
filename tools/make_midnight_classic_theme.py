#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 RiftWii contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes Midnight Classic, a RiftWii menu theme: a flat, dark console look.
A plain charcoal backdrop, flat grey buttons with no gloss, and one colour
for the focus: a bold cyan ring a little outside whatever is focused.

The colour values follow RetroArch's Ozone menu, its dark theme
(menu/drivers/ozone.c, ozone_theme_dark; GPL-3.0-or-later): the #2D2D2D
backdrop and the #9F9FA1 second lines; the cursor's blue-cyan is brighter
here so it reads on a TV.

Drawn for a 480-line picture that a TV scales up, where a one-pixel line
shimmers or smears:

- No outlines. A button stands out from the backdrop by its fill alone, a
  clear step lighter.
- The focus ring is solid, RING pixels wide, GAP pixels outside the body,
  with every edge on a whole pixel so it stays crisp. The menu draws the
  focused picture over the plain one with a slow breathing fade, so the
  ring is bright enough to read at the dim end of it.
- Lines and strokes are 2 pixels or more; corners are rounded by 8, enough
  to stay smooth when scaled up.

Every picture is worked out here from arithmetic alone. Written: tile9
(and _over, _primary, _primary_over), pill (the same four), tile /
tile_over / tile_empty, cover_tile / cover_tile_over, round_button /
round_button_over, chip_off / chip_on, step_*, scroll_*, switch_on /
switch_off, row_focus, prompt_button, arrow_*, bar / bar_wide,
banner_stripes and theme.ini.

    python tools/make_midnight_classic_theme.py [out_dir [picture ...]]   (needs Pillow and numpy)

The default out_dir is themes/MidnightClassic. Naming pictures repaints only
those.
"""
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent


def hexrgb(value):
    return np.array([int(value[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.float64)


BACKDROP = hexrgb("#2D2D2D")
CARD = hexrgb("#444446")
FIELD = hexrgb("#333335")
LINE = hexrgb("#FFFFFF")
TEXT = hexrgb("#FFFFFF")
GLYPH = hexrgb("#E8E8EA")
CURSOR = hexrgb("#35C0EE")
PRIMARY = hexrgb("#1F4D5E")
CHIP_ON = hexrgb("#1F4D5E")
SELECTION = hexrgb("#38383A")
SWITCH_OFF = hexrgb("#5A5A5C")
KNOB_OFF = hexrgb("#C8C8CA")

# The focus ring: GAP pixels of nothing round the body, then RING pixels of
# the cursor's colour, as the console draws its cursor a little outside the
# focused thing. Whole pixels, so its edges land on pixel edges.
GAP = 3.0
RING = 4.0
RADIUS = 8.0

INI = """\
# A flat, dark console look for a 480-line picture: a charcoal backdrop,
# flat grey buttons with no gloss and no outlines, and a bold cyan ring for
# whatever is focused. Colours after RetroArch's Ozone dark theme
# (GPL-3.0-or-later). The pictures come from
# tools/make_midnight_classic_theme.py.
[theme]
name = Midnight Classic
author = grantendo

[shape]
corners = 0.5
gloss = no
bar = dip
home_menu = wii
shelf_grain = no

[backdrop]
stripes = no

[colors]
# Text: white, then Ozone's grey for second lines
ink = #FFFFFF
ink_soft = #D2D2D4
ink_dim = #A3A3A5
clock = #FFFFFF
# The focus: the cursor's cyan; switched-on values a turquoise
accent = #35C0EE
accent_ink = #2EE6C0
text_on_accent = #FFFFFF
warn = #F2545B

# Flat grey cards a clear step lighter than the backdrop. Their edges are
# the card's own colour (no outlines) and there is no shadow.
card = #444446
card_edge = #444446
card_edge_strong = #444446
shadow = #00000000
glow = #35C0EE33
glyph = #E8E8EA

# Option chips and switches
chip_on = #1F4D5E
chip_off = #333335
chip_off_edge = #333335
switch_off = #5A5A5C

# One charcoal everywhere: the bands and the bar match the backdrop
bar = #2D2D2D
backdrop = #2D2D2D
backdrop_stripe = #2D2D2D
banner_stripe = #FFFFFF0A
banner_tint = #2D2D2D59

# Lists and badges
divider = #3E3E40
scroll_track = #38383A
scroll_thumb = #6A6A6C
badge = #555557
shelf = #444446
shelf_edge = #262626

# Player pointers
pointer1 = #3B8FD6
pointer2 = #E05252
pointer3 = #48B858
pointer4 = #E5AC24
"""


class Canvas:
    """A float RGBA picture; shapes are composited over what is there, each
    with its coverage from a signed distance (soft over one pixel)."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.rgb = np.zeros((h, w, 3))
        self.a = np.zeros((h, w))
        ys, xs = np.mgrid[0:h, 0:w]
        self.x = xs + 0.5
        self.y = ys + 0.5

    def over(self, coverage, rgb, alpha=1.0):
        a = np.clip(coverage, 0.0, 1.0) * alpha
        rgb = np.broadcast_to(rgb, (self.h, self.w, 3)) if np.ndim(rgb) == 1 else rgb
        out_a = a + self.a * (1 - a)
        safe = np.where(out_a > 0, out_a, 1.0)
        self.rgb = (rgb * a[..., None] + self.rgb * (self.a * (1 - a))[..., None]) / safe[..., None]
        self.a = out_a

    def image(self):
        # The Wii filters a texture between neighbouring pixels, colour and
        # alpha alike, so a clear pixel's colour bleeds into the edge next to
        # it: black there drew a dark fringe round every shape. Clear pixels
        # take the colour of the nearest drawn one instead (a few pixels out;
        # they stay clear).
        rgb, known = self.rgb.copy(), self.a > 0
        for _ in range(4):
            grown = known.copy()
            total, count = np.zeros_like(rgb), np.zeros(known.shape)
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                shifted_known = np.roll(known, (dy, dx), axis=(0, 1))
                total += np.roll(rgb, (dy, dx), axis=(0, 1)) * shifted_known[..., None]
                count += shifted_known
            fill_in = ~known & (count > 0)
            rgb[fill_in] = total[fill_in] / count[fill_in][..., None]
            grown |= fill_in
            known = grown
        data = np.dstack([np.clip(np.round(rgb), 0, 255), np.clip(np.round(self.a * 255), 0, 255)])
        return Image.fromarray(data.astype(np.uint8), "RGBA")


def rounded_box(c, x0, y0, w, h, r):
    """Signed distance to a rounded rectangle: positive inside."""
    r = min(r, w / 2, h / 2)
    qx = np.abs(c.x - (x0 + w / 2)) - (w / 2 - r)
    qy = np.abs(c.y - (y0 + h / 2)) - (h / 2 - r)
    outside = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0))
    inside = np.minimum(np.maximum(qx, qy), 0)
    return r - (outside + inside)


def circle(c, cx, cy, r):
    return r - np.hypot(c.x - cx, c.y - cy)


def segment(c, ax, ay, bx, by, half_width):
    """Signed distance to a thick line with round ends: positive inside."""
    px, py = c.x - ax, c.y - ay
    dx, dy = bx - ax, by - ay
    t = np.clip((px * dx + py * dy) / (dx * dx + dy * dy), 0, 1)
    return half_width - np.hypot(px - t * dx, py - t * dy)


def triangle(c, points):
    """Signed distance (close enough for a pixel's softening) to a convex
    polygon: the nearest of its edges' planes, positive inside whichever way
    round the points are given."""
    mx = sum(p[0] for p in points) / len(points)
    my = sum(p[1] for p in points) / len(points)
    d = None
    for i, (ax, ay) in enumerate(points):
        bx, by = points[(i + 1) % len(points)]
        ex, ey = bx - ax, by - ay
        length = np.hypot(ex, ey)
        # The middle of the polygon is inside: that sets the edge's sign.
        sign = 1.0 if ((mx - ax) * ey - (my - ay) * ex) > 0 else -1.0
        side = sign * ((c.x - ax) * ey - (c.y - ay) * ex) / length
        d = side if d is None else np.minimum(d, side)
    return d


def fill(body):
    """A shape's coverage from its signed distance."""
    return np.clip(body + 0.5, 0, 1)


def focus_ring(c, body):
    """The ring GAP pixels outside a body's signed distance, RING wide."""
    outward = -body
    coverage = np.clip(GAP + RING - outward + 0.5, 0, 1) * np.clip(outward - GAP + 0.5, 0, 1)
    c.over(coverage, CURSOR)


def flat_button(cw, ch, x0, y0, w, h, r=RADIUS, *, over=False, colour=CARD):
    """A flat button, one colour; focused, the cursor's ring outside it."""
    c = Canvas(cw, ch)
    body = rounded_box(c, x0, y0, w, h, r)
    c.over(fill(body), colour)
    if over:
        focus_ring(c, body)
    return c


def chevron(c, cx, cy, direction, size, colour, half_width=1.6):
    """An open arrow (two strokes over 3 pixels wide) pointing `direction`."""
    s = size
    if direction in ("left", "right"):
        k = -1 if direction == "left" else 1
        tip = (cx + k * s * 0.45, cy)
        a = (cx - k * s * 0.35, cy - s * 0.8)
        b = (cx - k * s * 0.35, cy + s * 0.8)
    else:
        k = -1 if direction == "up" else 1
        tip = (cx, cy + k * s * 0.45)
        a = (cx - s * 0.8, cy - k * s * 0.35)
        b = (cx + s * 0.8, cy - k * s * 0.35)
    stroke = np.maximum(segment(c, *a, *tip, half_width), segment(c, *tip, *b, half_width))
    c.over(fill(stroke), colour)


def round_control(direction, over):
    """44x44: a list's scroll arrow or an option's step arrow, a flat disc in
    the value boxes' darker grey (the step arrows sit on a card of CARD's
    colour), with an open arrow; focused, the arrow turns cyan and the ring
    goes round the disc (14.5 + 3 + 4 fits the 22 to the canvas's edge)."""
    c = Canvas(44, 44)
    body = circle(c, 22, 22, 14.5)
    c.over(fill(body), FIELD)
    if over:
        focus_ring(c, body)
    chevron(c, 22, 22, direction, 7.0, CURSOR if over else GLYPH)
    return c.image()


def page_arrow(left, over):
    """48x48: Home's page buttons, a flat light triangle; focused, cyan."""
    c = Canvas(48, 48)
    s = -1 if left else 1
    tip, back = 24 + 11 * s, 24 - 9 * s
    shape = triangle(c, [(back, 9), (tip, 24), (back, 39)])
    c.over(fill(shape), CURSOR if over else GLYPH)
    return c.image()


def chip(on):
    """212x36: an option's value box, a field darker than its row, or a
    deep teal when it is changed from the default. No outline."""
    c = Canvas(212, 36)
    c.over(fill(rounded_box(c, 2, 3, 208, 30, RADIUS)), CHIP_ON if on else FIELD)
    return c.image()


def switch(on):
    """68x40: an On/Off switch, a flat track with a round knob: cyan with a
    white knob on the right when on, grey with a pale grey knob when off."""
    c = Canvas(68, 40)
    c.over(fill(rounded_box(c, 3, 5, 62, 30, 15)), CURSOR if on else SWITCH_OFF)
    knob_x = 50.0 if on else 18.0
    c.over(fill(circle(c, knob_x, 20, 12)), TEXT if on else KNOB_OFF)
    return c.image()


def row_focus():
    """548x44: the focused row of a list without cards, a lighter fill with
    the ring as its own edge (the picture has no room outside the row)."""
    c = Canvas(548, 44)
    outer = rounded_box(c, 1, 1, 546, 42, RADIUS)
    c.over(fill(outer), SELECTION)
    inner = rounded_box(c, 1 + RING, 1 + RING, 546 - 2 * RING, 42 - 2 * RING, RADIUS - RING)
    c.over(fill(outer) * (1 - fill(inner)), CURSOR)
    return c.image()


def prompt_button():
    """24x24: the round badge under a button's name in the prompts, a flat
    light disc as the console's button hints."""
    c = Canvas(24, 24)
    c.over(fill(circle(c, 12, 12, 11)), GLYPH)
    return c.image()


def tile_empty():
    """140x100: an empty place, a flat slot only a little lighter than the
    backdrop."""
    c = Canvas(140, 100)
    c.over(fill(rounded_box(c, 8, 8, 124, 84, RADIUS)), FIELD)
    return c.image()


def cover_tile(over):
    """96x128: behind a cover (80x112 at 8, 8: the cover is drawn on top),
    a flat slot; focused, the ring round the cover (3 + 4 fits the 8)."""
    c = Canvas(96, 128)
    body = rounded_box(c, 8, 8, 80, 112, 3)
    c.over(fill(body), CARD)
    if over:
        focus_ring(c, body)
    return c.image()


def round_button(over):
    """80x80: Home's two round buttons, a flat grey disc; focused, the ring
    round it (31 + 3 + 4 fits the 40). The icon is drawn on top."""
    c = Canvas(80, 80)
    body = circle(c, 40, 40, 31)
    c.over(fill(body), CARD)
    if over:
        focus_ring(c, body)
    return c.image()


def bar(width):
    """width x 124: Home's bottom bar, the backdrop's charcoal from row 11
    down with a 2-pixel light line along its top (rows 11 and 12), held 28
    pixels in from each side. No dip is drawn: the clock sits under it."""
    c = Canvas(width, 124)
    c.over((c.y >= 11.0).astype(float), BACKDROP)
    inset = 28.0
    line = ((c.y >= 11.0) & (c.y < 13.0)).astype(float)
    ends = np.clip(c.x - inset + 0.5, 0, 1) * np.clip(width - inset - c.x + 0.5, 0, 1)
    c.over(line * ends, LINE, 0.8)
    return c.image()


def banner_stripes():
    """640x192: clear, so the game page's band shows the game's colour plain."""
    return Image.new("RGBA", (640, 192), (0, 0, 0, 0))


def main():
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / "themes" / "MidnightClassic"
    out.mkdir(parents=True, exist_ok=True)
    (out / "theme.ini").write_bytes(INI.replace("\n", "\r\n").encode("utf-8"))

    # The 64x64 tiles are nine-sliced with 24-pixel corners kept: the body
    # is 48x48 at 8, so the ring's 3 + 4 fits the margin. The menu draws
    # the _over picture on top of the plain one.
    tile9 = dict(cw=64, ch=64, x0=8, y0=8, w=48, h=48)
    # The wide buttons: 238x46 at 7, 7 in 252x60 (the ring needs 7).
    pill = dict(cw=252, ch=60, x0=7, y0=7, w=238, h=46)
    # The names view's tiles: 124x84 at 8 in 140x100.
    tile = dict(cw=140, ch=100, x0=8, y0=8, w=124, h=84)
    pictures = {
        "tile9": lambda: flat_button(**tile9).image(),
        "tile9_over": lambda: flat_button(**tile9, over=True).image(),
        "tile9_primary": lambda: flat_button(**tile9, colour=PRIMARY).image(),
        "tile9_primary_over": lambda: flat_button(**tile9, over=True, colour=PRIMARY).image(),
        "pill": lambda: flat_button(**pill).image(),
        "pill_over": lambda: flat_button(**pill, over=True).image(),
        "pill_primary": lambda: flat_button(**pill, colour=PRIMARY).image(),
        "pill_primary_over": lambda: flat_button(**pill, over=True, colour=PRIMARY).image(),
        "tile": lambda: flat_button(**tile).image(),
        "tile_over": lambda: flat_button(**tile, over=True).image(),
        "tile_empty": tile_empty,
        "cover_tile": lambda: cover_tile(False),
        "cover_tile_over": lambda: cover_tile(True),
        "round_button": lambda: round_button(False),
        "round_button_over": lambda: round_button(True),
        "chip_off": lambda: chip(False),
        "chip_on": lambda: chip(True),
        "step_back": lambda: round_control("left", False),
        "step_back_over": lambda: round_control("left", True),
        "step_forward": lambda: round_control("right", False),
        "step_forward_over": lambda: round_control("right", True),
        "scroll_up": lambda: round_control("up", False),
        "scroll_up_over": lambda: round_control("up", True),
        "scroll_down": lambda: round_control("down", False),
        "scroll_down_over": lambda: round_control("down", True),
        "switch_off": lambda: switch(False),
        "switch_on": lambda: switch(True),
        "row_focus": row_focus,
        "prompt_button": prompt_button,
        "arrow_left": lambda: page_arrow(True, False),
        "arrow_left_over": lambda: page_arrow(True, True),
        "arrow_right": lambda: page_arrow(False, False),
        "arrow_right_over": lambda: page_arrow(False, True),
        "bar": lambda: bar(640),
        "bar_wide": lambda: bar(856),
        "banner_stripes": banner_stripes,
    }
    only = set(sys.argv[2:])
    written = []
    for name, paint in pictures.items():
        if only and name not in only:
            continue
        path = out / (name + ".png")
        paint().save(path, optimize=True)
        written.append(str(path))
    print(f"{len(written)} pictures and theme.ini written to {out}")
    # Smaller files, the same picture on a TV (tools/shrink_theme_pictures.py).
    if written:
        shrinker = str(pathlib.Path(__file__).with_name("shrink_theme_pictures.py"))
        subprocess.run([sys.executable, shrinker, "--"] + written, check=True, stdout=subprocess.DEVNULL)


if __name__ == "__main__":
    main()
