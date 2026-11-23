// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// A game's GameCube controller layout file (riftwii/padlayout.hpp, #587's
// light patches): the built-in layouts match what the stand-in maps
// without a file, a section changes only the buttons it names, and lines
// it can't use are reported and skipped.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "riftwii/padlayout.hpp"

extern "C" {
#include "rtgcwii.h"
#include "rthome.h"
}

static int g_failures = 0;
#define EXPECT_TRUE(cond) do { if (!(cond)) { std::cerr << "FAILED: " #cond " at line " << __LINE__ << std::endl; g_failures++; } } while (0)
#define EXPECT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "FAILED: " #a " == " #b " (" << (a) << " != " << (b) << ") at line " << __LINE__ << std::endl; g_failures++; } } while (0)

namespace {

// What a layout presses for a PADStatus button word (shakes left out).
std::uint32_t pressed(const riftwii::GcLayout& layout, std::uint32_t gc) {
    std::uint32_t out = 0;
    for (unsigned bit = 0; bit < riftwii::kGcLayoutButtons; ++bit)
        if ((gc >> bit) & 1u) out |= layout.buttons[bit] & ~riftwii::kGcLayoutShake;
    return out;
}

// Every GameCube button combination: the built-in layouts press what the
// stand-in's own mappings press.
void TestDefaultsMatchTheStandIn() {
    const riftwii::GcLayout classic = riftwii::default_gc_layout(riftwii::GcShape::Classic);
    const riftwii::GcLayout nunchuk = riftwii::default_gc_layout(riftwii::GcShape::Nunchuk);
    const riftwii::GcLayout sideways = riftwii::default_gc_layout(riftwii::GcShape::Sideways);
    unsigned differ = 0;
    for (std::uint32_t gc = 0; gc < (1u << riftwii::kGcLayoutButtons); ++gc) {
        if (gc & 0x80u) continue;  // bit 7 is no button
        std::uint8_t pad[12] = {};
        pad[0] = static_cast<std::uint8_t>(gc >> 8);
        pad[1] = static_cast<std::uint8_t>(gc);
        if (pressed(classic, gc) != rthome_map_classic(gc, RTHOME_CL_PLUS)) ++differ;
        if (pressed(nunchuk, gc) != rtgcwii_map_nunchuk(pad)) ++differ;
        if (pressed(sideways, gc) != rtgcwii_map_sideways(pad)) ++differ;
    }
    EXPECT_EQ(differ, 0u);
    // The shakes: X with a Nunchuk, R held sideways.
    EXPECT_EQ(nunchuk.buttons[10], riftwii::kGcLayoutShake);
    EXPECT_EQ(sideways.buttons[5], riftwii::kGcLayoutShake);
}

void TestSectionOverDefaults() {
    std::vector<std::string> notes;
    const std::string text =
        "# Kamen Rider: Dragon Knight\n"
        "[Classic]\n"
        "Z = ZL\n"
        "l = zr, L   # two at once\n"
        "\n"
        "[nunchuk]\n"
        "x = 1\n"
        "R = shake\n";
    const riftwii::GcLayout classic = riftwii::parse_gc_layout(text, riftwii::GcShape::Classic, notes);
    EXPECT_TRUE(classic.present);
    EXPECT_EQ(classic.buttons[4], static_cast<std::uint32_t>(RTHOME_CL_ZL));
    EXPECT_EQ(classic.buttons[6], static_cast<std::uint32_t>(RTHOME_CL_ZR | RTHOME_CL_L));
    EXPECT_EQ(classic.buttons[8], static_cast<std::uint32_t>(RTHOME_CL_A));  // not named: the default
    const riftwii::GcLayout nunchuk = riftwii::parse_gc_layout(text, riftwii::GcShape::Nunchuk, notes);
    EXPECT_TRUE(nunchuk.present);
    EXPECT_EQ(nunchuk.buttons[10], 0x0200u);  // x: 1, no longer the shake
    EXPECT_EQ(nunchuk.buttons[5], riftwii::kGcLayoutShake);
    // No section for it: the built-in layout, and nothing to say.
    const riftwii::GcLayout sideways = riftwii::parse_gc_layout(text, riftwii::GcShape::Sideways, notes);
    EXPECT_TRUE(!sideways.present);
    EXPECT_EQ(notes.size(), 0u);
}

void TestLinesItCantUse() {
    std::vector<std::string> notes;
    const std::string text =
        "a = 2\n"           // outside a section
        "[nunchuk]\n"
        "q = a\n"           // no such GameCube button
        "a = zl\n"          // a Classic button in the Nunchuk shape
        "b = none\n"
        "[pointing]\n";     // not a shape a file can set
    const riftwii::GcLayout nunchuk = riftwii::parse_gc_layout(text, riftwii::GcShape::Nunchuk, notes);
    EXPECT_EQ(notes.size(), 4u);
    EXPECT_EQ(nunchuk.buttons[8], 0x0800u);  // a keeps its default (the bad line skipped)
    EXPECT_EQ(nunchuk.buttons[9], 0u);       // b presses nothing
    // `shake` is a Wii Remote's: not in the Classic shape.
    notes.clear();
    (void)riftwii::parse_gc_layout("[classic]\nx = shake\n", riftwii::GcShape::Classic, notes);
    EXPECT_EQ(notes.size(), 1u);
}

}  // namespace

int main() {
    TestDefaultsMatchTheStandIn();
    TestSectionOverDefaults();
    TestLinesItCantUse();
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)" << std::endl;
        return 1;
    }
    std::cout << "padlayout tests passed" << std::endl;
    return 0;
}
