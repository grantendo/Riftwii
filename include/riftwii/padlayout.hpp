// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A game's GameCube controller layout (#587's light patches): which Wii
// button each GameCube button presses when GameCube controllers stand in
// for Wii controllers, read from sd:/riftwii/layouts/<game ID>.txt (or
// <its first four characters>.txt, for every region and for mods that
// change the last two). A file
// changes only the buttons it names; the rest keep the setting's built-in
// layout. One section per shape:
//
//     # Kamen Rider: Dragon Knight
//     [classic]
//     z = zl
//     l = zr, l          # two buttons at once
//
//     [nunchuk]
//     x = 1
//     r = shake          # the Wii Remote shaken while held
//
// Left of `=`: a GameCube button (a, b, x, y, z, start, l, r, up, down,
// left, right). Right: the Wii buttons it presses, by comma: for
// [classic] a, b, x, y, l, r, zl, zr, plus, minus, home, up, down, left,
// right; for [nunchuk] and [sideways] the Wii Remote's own a, b, 1, 2,
// plus, minus, home, up, down, left, right (as the Wii Remote is held
// upright, so in [sideways] the screen's left is the Wii Remote's up),
// [nunchuk] also c and z (the Nunchuk's); `shake` (the Wii Remote shapes);
// `none`. Names aren't case-sensitive; `#` starts a comment.

#include <cstdint>
#include <string>
#include <vector>

namespace riftwii {

enum class GcShape { Classic, Nunchuk, Sideways };

// One word per GameCube button, at the button's bit in a PADStatus button
// word (left 0, right 1, down 2, up 3, z 4, r 5, l 6, a 8, b 9, x 10, y 11,
// start 12): the Wii buttons it presses (KPAD's Classic Controller bits, or
// the Wii Remote's and Nunchuk's), with kGcLayoutShake for a shake.
constexpr unsigned kGcLayoutButtons = 13;
constexpr std::uint32_t kGcLayoutShake = 0x80000000u;

struct GcLayout {
    bool present = false;  // a section for the shape was read: the table replaces the built-in mapping
    std::uint32_t buttons[kGcLayoutButtons] = {};
};

// The setting's built-in layout for `shape`, as the stand-in maps it
// without a file (runtime/rtgcwii.c, runtime/rthome.c).
GcLayout default_gc_layout(GcShape shape);

// Reads a layout file's section for `shape` over the built-in layout.
// `present` stays false when the file has no section for the shape. Each
// line it can't use (an unknown button, a Wii button the shape doesn't
// have, a line outside a section) goes into `notes` and is skipped.
GcLayout parse_gc_layout(const std::string& text, GcShape shape, std::vector<std::string>& notes);

}  // namespace riftwii
