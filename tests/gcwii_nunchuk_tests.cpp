// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// GameCube controllers as a Wii Remote + Nunchuk (runtime/rtgcwii.h with
// RT_PAD_FORM_NUNCHUK, #587): #587's default Nunchuk layout, the analog
// triggers as the Nunchuk's buttons, the pointer following the C-stick and
// hiding after a while, and X shaking the Wii Remote (the axes flipping
// every few samples). This test compiles rtgcwii.c and rthome.c itself,
// with the form's define, as the pad builds that carry it do.
#include <cstdint>
#include <cstring>
#include <iostream>

extern "C" {
#include "rtgcwii.h"
#include "rthome.h"
}

static int g_failures = 0;
#define EXPECT_TRUE(cond) do { if (!(cond)) { std::cerr << "FAILED: " #cond " at line " << __LINE__ << std::endl; g_failures++; } } while (0)
#define EXPECT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "FAILED: " #a " == " #b " (" << (a) << " != " << (b) << ") at line " << __LINE__ << std::endl; g_failures++; } } while (0)

namespace {

constexpr std::uint32_t kGcLeft = 0x0001, kGcRight = 0x0002, kGcDown = 0x0004, kGcUp = 0x0008;
constexpr std::uint32_t kGcZ = 0x0010, kGcR = 0x0020, kGcL = 0x0040;
constexpr std::uint32_t kGcA = 0x0100, kGcB = 0x0200, kGcX = 0x0400, kGcY = 0x0800, kGcStart = 0x1000;
constexpr std::uint32_t kWrLeft = 0x0001, kWrRight = 0x0002, kWrDown = 0x0004, kWrUp = 0x0008, kWrPlus = 0x0010;
constexpr std::uint32_t kWr2 = 0x0100, kWrB = 0x0400, kWrA = 0x0800, kWrMinus = 0x1000;
constexpr std::uint32_t kNcZ = 0x2000, kNcC = 0x4000;
constexpr std::uint32_t kGrace = 300;     // ticks (1 a millisecond here)
constexpr std::uint32_t kShowFor = 2000;  // the pointer's time shown

struct Pads {
    std::uint8_t s[48];
    Pads() {
        std::memset(s, 0, sizeof(s));
        for (int i = 0; i < 4; ++i) s[i * 12 + 10] = 0xFF;
    }
    std::uint8_t* plug(int port, std::uint32_t buttons = 0) {
        std::uint8_t* p = s + port * 12;
        p[0] = static_cast<std::uint8_t>(buttons >> 8);
        p[1] = static_cast<std::uint8_t>(buttons);
        p[10] = 0;
        return p;
    }
};

rtgcwii fresh() {
    rtgcwii w;
    std::memset(&w, 0, sizeof(w));
    w.grace = kGrace;
    w.pointer_show = kShowFor;
    return w;
}

// The buttons, one by one, and the triggers past a third of the way.
void TestMapping() {
    std::uint8_t pad[12] = {};
    const struct {
        std::uint32_t gc, wii;
    } cases[] = {{kGcA, kWrA},       {kGcB, kWrB},         {kGcY, kWr2},       {kGcZ, kWrMinus},
                 {kGcStart, kWrPlus}, {kGcL, kNcZ},         {kGcR, kNcC},       {kGcLeft, kWrLeft},
                 {kGcRight, kWrRight}, {kGcUp, kWrUp},      {kGcDown, kWrDown}, {kGcX, 0u}};
    for (const auto& c : cases) {
        pad[0] = static_cast<std::uint8_t>(c.gc >> 8);
        pad[1] = static_cast<std::uint8_t>(c.gc);
        EXPECT_EQ(rtgcwii_map_nunchuk(pad), c.wii);
    }
    pad[0] = pad[1] = 0;
    pad[6] = 84;  // L just short of a third
    EXPECT_EQ(rtgcwii_map_nunchuk(pad), 0u);
    pad[6] = 85;
    EXPECT_EQ(rtgcwii_map_nunchuk(pad), kNcZ);
    pad[6] = 0;
    pad[7] = 200;
    EXPECT_EQ(rtgcwii_map_nunchuk(pad), kNcC);
}

// The words for the game: newly pressed once, then held, then let go.
void TestWords() {
    rtgcwii w = fresh();
    Pads p;
    p.plug(0);
    rtgcwii_pads(&w, p.s, 1);
    rtgcwii_fs f;
    rtgcwii_nunchuk(&w, 0, 1, &f);
    EXPECT_EQ(f.hold, 0u);
    p.plug(0, kGcA | kGcL);
    rtgcwii_pads(&w, p.s, 17);
    rtgcwii_nunchuk(&w, 0, 17, &f);
    EXPECT_EQ(f.hold, kWrA | kNcZ);
    EXPECT_EQ(f.trig, kWrA | kNcZ);
    rtgcwii_nunchuk(&w, 0, 18, &f);  // a second read of the same sample
    EXPECT_EQ(f.trig, kWrA | kNcZ);
    p.plug(0, kGcL);
    rtgcwii_pads(&w, p.s, 33);
    rtgcwii_nunchuk(&w, 0, 33, &f);
    EXPECT_EQ(f.hold, kNcZ);
    EXPECT_EQ(f.trig, 0u);
    EXPECT_EQ(f.release, kWrA);
    // A channel with no stand-in: a still controller, nothing held.
    rtgcwii_nunchuk(&w, 1, 33, &f);
    EXPECT_EQ(f.hold, 0u);
    EXPECT_EQ(f.acc[1], -RTHOME_AXIS_FULL);
    EXPECT_EQ(f.acc_value, RTHOME_AXIS_FULL);
    EXPECT_EQ(f.pointer_shown, 0);
}

// The stick as the Nunchuk's, the controllers still.
void TestStickAndRest() {
    rtgcwii w = fresh();
    Pads p;
    std::uint8_t* s = p.plug(0);
    s[2] = 72;  // full right
    s[3] = static_cast<std::uint8_t>(-72);
    rtgcwii_pads(&w, p.s, 1);
    rtgcwii_fs f;
    rtgcwii_nunchuk(&w, 0, 1, &f);
    EXPECT_EQ(f.stick[0], RTHOME_AXIS_FULL);
    EXPECT_EQ(f.stick[1], -RTHOME_AXIS_FULL);
    EXPECT_EQ(f.acc[0], 0);
    EXPECT_EQ(f.acc[1], -RTHOME_AXIS_FULL);
    EXPECT_EQ(f.fs_acc[1], -RTHOME_AXIS_FULL);
    EXPECT_EQ(f.fs_acc_value, RTHOME_AXIS_FULL);
    EXPECT_EQ(f.acc_speed, 0);
}

// The pointer moves with the C-stick, stops at the edge, and hides a
// while after the C-stick lets go.
void TestPointer() {
    rtgcwii w = fresh();
    Pads p;
    std::uint8_t* s = p.plug(0);
    s[4] = 59;  // C-stick full right
    rtgcwii_pads(&w, p.s, 100);
    rtgcwii_fs f;
    rtgcwii_nunchuk(&w, 0, 100, &f);
    EXPECT_EQ(f.pointer[0], RTGCWII_POINTER_STEP);
    EXPECT_EQ(f.pointer[1], 0);
    EXPECT_EQ(f.pointer_shown, 1);
    for (int i = 0; i < 40; ++i) rtgcwii_pads(&w, p.s, 101 + i);
    rtgcwii_nunchuk(&w, 0, 140, &f);
    EXPECT_EQ(f.pointer[0], RTGCWII_POINTER_EDGE);
    // Up on the C-stick is up on the screen: y goes negative.
    s[4] = 0;
    s[5] = 59;
    rtgcwii_pads(&w, p.s, 200);
    rtgcwii_nunchuk(&w, 0, 200, &f);
    EXPECT_EQ(f.pointer[1], -RTGCWII_POINTER_STEP);
    // Let go: it stays where it was, shown until kShowFor has passed.
    s[5] = 0;
    rtgcwii_pads(&w, p.s, 300);
    rtgcwii_nunchuk(&w, 0, 300 + kShowFor - 101, &f);
    EXPECT_EQ(f.pointer_shown, 1);
    EXPECT_EQ(f.pointer[0], RTGCWII_POINTER_EDGE);
    rtgcwii_nunchuk(&w, 0, 200 + kShowFor, &f);
    EXPECT_EQ(f.pointer_shown, 0);
    // Hidden for good: a sample after its time clears the mark, so the
    // time base wrapping round (about every 70 s) doesn't show it again.
    rtgcwii_pads(&w, p.s, 200 + kShowFor);
    rtgcwii_nunchuk(&w, 0, 200 + kShowFor + 1, &f);
    EXPECT_EQ(f.pointer_shown, 0);
    rtgcwii_pads(&w, p.s, 200 + 4u * kShowFor);
    rtgcwii_nunchuk(&w, 0, 0xFFFFFFFFu, &f);  // past the wrap
    EXPECT_EQ(f.pointer_shown, 0);
    rtgcwii_nunchuk(&w, 0, 250, &f);  // an hour of ticks later, wrapped round
    EXPECT_EQ(f.pointer_shown, 0);
    EXPECT_EQ(f.pointer[0], RTGCWII_POINTER_EDGE);  // still where it was left
}

// X held shakes the Wii Remote: the axes swing past 3 g and flip every
// RTGCWII_SHAKE_FLIP samples, a flip a jump in speed; let go, it's still.
void TestShake() {
    rtgcwii w = fresh();
    Pads p;
    p.plug(0, kGcX);
    rtgcwii_fs f;
    int flips = 0;
    std::int32_t last = 0;
    for (std::uint32_t i = 0; i < 4 * RTGCWII_SHAKE_FLIP; ++i) {
        rtgcwii_pads(&w, p.s, 1 + i * 16);
        rtgcwii_nunchuk(&w, 0, 1 + i * 16, &f);
        EXPECT_EQ(f.acc_value, RTGCWII_SHAKE_VALUE);
        EXPECT_TRUE(f.acc[0] == RTGCWII_SHAKE_G || f.acc[0] == -RTGCWII_SHAKE_G);
        EXPECT_EQ(f.acc[1], f.acc[0]);
        EXPECT_EQ(f.acc[2], -f.acc[0]);
        if (i > 0 && f.acc[0] != last) {
            ++flips;
            EXPECT_EQ(f.acc_speed, RTGCWII_SHAKE_VALUE);
        }
        last = f.acc[0];
        EXPECT_EQ(f.fs_acc_value, RTHOME_AXIS_FULL);  // the Nunchuk stays still
    }
    EXPECT_EQ(flips, 4);  // at samples 4, 8, 12 and 16
    EXPECT_EQ(f.hold, 0u);  // X itself presses nothing
    p.plug(0);
    rtgcwii_pads(&w, p.s, 1000);
    rtgcwii_nunchuk(&w, 0, 1000, &f);
    EXPECT_EQ(f.acc_value, RTHOME_AXIS_FULL);
    EXPECT_EQ(f.acc[1], -RTHOME_AXIS_FULL);
}

// The game turning the camera on shows the pointer with no C-stick, and
// off hides it again (unless the C-stick has just moved it).
void TestCamera() {
    rtgcwii w = fresh();
    Pads p;
    p.plug(0);
    rtgcwii_pads(&w, p.s, 10);
    rtgcwii_fs f;
    rtgcwii_nunchuk(&w, 0, 10, &f);
    EXPECT_EQ(f.pointer_shown, 0);
    rtgcwii_camera(&w, 0, 1);  // KPADEnableDPD(0)
    rtgcwii_nunchuk(&w, 0, 5000, &f);
    EXPECT_EQ(f.pointer_shown, 1);
    EXPECT_EQ(f.pointer[0], 0);  // the middle of the screen, until the C-stick moves it
    rtgcwii_nunchuk(&w, 1, 5000, &f);
    EXPECT_EQ(f.pointer_shown, 0);  // another channel's camera is its own
    rtgcwii_camera(&w, 0, 0);
    rtgcwii_nunchuk(&w, 0, 5001, &f);
    EXPECT_EQ(f.pointer_shown, 0);
    rtgcwii_camera(&w, 9, 3);  // no such channel: nothing
    EXPECT_EQ(w.camera, 0u);
}

}  // namespace

int main() {
    TestMapping();
    TestWords();
    TestStickAndRest();
    TestPointer();
    TestShake();
    TestCamera();
    if (g_failures == 0) std::cout << "gcwii_nunchuk: all passed" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
