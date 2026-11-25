// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// Slot ownership (runtime/rtslot.h), the stand-in's one place for "who
// holds each Wii Remote slot, and what the game has been told". Each test
// is a past review finding on #587's pull requests (KakarottoCake/Riftwii
// #609, #610, #616 and their fork copies), written as the situation and
// the outcome it needs, so the rules hold whatever the hooks around them
// become (notes: the restructure's findings catalogue).
#include <cstdint>
#include <cstring>
#include <iostream>

extern "C" {
#include "rtslot.h"
}

static int g_failures = 0;
#define EXPECT_TRUE(cond) do { if (!(cond)) { std::cerr << "FAILED: " #cond " at line " << __LINE__ << std::endl; g_failures++; } } while (0)
#define EXPECT_EQ(a, b) do { const auto expect_a_ = (a); const auto expect_b_ = (b); if (expect_a_ != expect_b_) { std::cerr << "FAILED: " #a " == " #b " (" << expect_a_ << " != " << expect_b_ << ") at line " << __LINE__ << std::endl; g_failures++; } } while (0)

namespace {

constexpr std::uint32_t kGrace = 300;  // ticks (1 a millisecond here)
constexpr std::uint32_t kNone = RTSLOT_NO_SLOT;

rtslot fresh() {
    rtslot s;
    std::memset(&s, 0, sizeof(s));
    s.grace = kGrace;
    return s;
}

void plug(rtslot& s, std::uint32_t slot) { s.present |= 1u << slot; }
void unplug(rtslot& s, std::uint32_t slot) { s.present &= ~(1u << slot); }

// Tells every change due now (each delivered), as the hook's pass does;
// returns how many "connected" (joins) and "gone" (leaves) were told.
struct Told {
    int joins = 0, leaves = 0, extensions = 0;
};
Told tell_all(rtslot& s, std::uint32_t home = kNone, int remote_shapes = 1) {
    Told t;
    std::uint32_t skip = 0, slot = 0;
    std::int32_t reason = 0;
    while (rtslot_next_tell(&s, home, remote_shapes, skip, &slot, &reason)) {
        (reason == 0 ? t.joins : t.leaves) += 1;
        if (rtslot_told(&s, slot, reason, 1) == RTSLOT_AFTER_EXTENSION) ++t.extensions;
        skip |= 1u << slot;
    }
    return t;
}

// Who holds a slot: a real controller first, then the HOME Menu's stand-in
// on its own slot, then the permanent stand-in where a pad is plugged in.
void TestOwnerOrder() {
    rtslot s = fresh();
    EXPECT_EQ(rtslot_owner(&s, 0, kNone), RTSLOT_NOBODY);
    plug(s, 0);
    EXPECT_EQ(rtslot_owner(&s, 0, kNone), RTSLOT_STANDIN);
    EXPECT_EQ(rtslot_owner(&s, 0, 0), RTSLOT_HOME);
    rtslot_probed(&s, 0, 0);  // a real Wii Remote answered there
    EXPECT_EQ(rtslot_owner(&s, 0, 0), RTSLOT_REAL);
    EXPECT_EQ(rtslot_owner(&s, 0, kNone), RTSLOT_REAL);
    EXPECT_EQ(rtslot_owner(&s, 9, kNone), RTSLOT_NOBODY);  // past the four slots
}

// #609 (CodeRabbit): a real Wii Remote's samples are kept when the HOME
// Menu's stand-in shares its slot: the HOME stand-in yields to it.
void TestRealBeatsHome() {
    rtslot s = fresh();
    rtslot_probed(&s, 1, 0);
    EXPECT_EQ(rtslot_home_unless_real(&s, 1), kNone);
    EXPECT_EQ(rtslot_home_unless_real(&s, 2), 2u);
    EXPECT_EQ(rtslot_home_unless_real(&s, kNone), kNone);
}

// #610 (CodeRabbit): when the HOME Menu ends on a slot a real Wii Remote
// now holds, the game isn't told "gone" for it.
void TestHomeEndsOnRealSlot() {
    rtslot s = fresh();
    plug(s, 0);
    EXPECT_EQ(tell_all(s).joins, 1);
    rtslot_probed(&s, 0, 0);  // a real Wii Remote took slot 0
    const Told t = tell_all(s);
    EXPECT_EQ(t.leaves, 0);
    EXPECT_EQ(t.joins, 0);
}

// #616 (CodeRabbit): while the HOME Menu's stand-in holds a slot in another
// shape (a Classic Controller, in a Wii Remote form), the permanent
// stand-in's "connected" there waits until it ends; then it's told.
void TestHomeHoldsDifferentShape() {
    rtslot s = fresh();
    rtslot_home_took(&s, 0);  // nothing of the stand-in's there: Classic-shaped HOME
    EXPECT_EQ(rtslot_home_keeps_shape(&s, 0), 0);
    plug(s, 0);               // a GameCube controller plugged in under the menu
    EXPECT_EQ(tell_all(s, 0).joins, 0);
    EXPECT_EQ(tell_all(s, kNone).joins, 1);  // the menu ended
    // The Classic form has no other shape to hold for: told at once.
    rtslot c = fresh();
    rtslot_home_took(&c, 0);
    plug(c, 0);
    EXPECT_EQ(tell_all(c, 0, 0).joins, 1);
}

// #12 (fork, CodeRabbit): the HOME stand-in keeps the shape it took the
// slot in, though the permanent stand-in's pad unplugs under it.
void TestHomeKeepsShapeAfterUnplug() {
    rtslot s = fresh();
    plug(s, 2);
    EXPECT_EQ(tell_all(s).joins, 1);
    rtslot_home_took(&s, 2);
    EXPECT_EQ(rtslot_home_keeps_shape(&s, 2), 1);
    unplug(s, 2);
    EXPECT_EQ(rtslot_home_keeps_shape(&s, 2), 1);
    EXPECT_EQ(tell_all(s, 2).leaves, 0);  // never "gone" while the menu has it
    EXPECT_EQ(tell_all(s, kNone).leaves, 1);
}

// #10 (fork, CodeRabbit): a "connected" the game couldn't take (no callback
// yet) isn't marked told; it's told at the next pass. The same for "gone".
void TestUndeliveredStaysToTell() {
    rtslot s = fresh();
    plug(s, 0);
    std::uint32_t slot = 0;
    std::int32_t reason = 1;
    EXPECT_TRUE(rtslot_next_tell(&s, kNone, 1, 0, &slot, &reason));
    EXPECT_EQ(reason, 0);
    EXPECT_EQ(rtslot_told(&s, slot, reason, 0), RTSLOT_AFTER_NOTHING);
    EXPECT_EQ(tell_all(s).joins, 1);  // tried again
    unplug(s, 0);
    EXPECT_TRUE(rtslot_next_tell(&s, kNone, 1, 0, &slot, &reason));
    EXPECT_EQ(reason, -1);
    rtslot_told(&s, slot, reason, 0);
    EXPECT_EQ(tell_all(s).leaves, 1);
}

// A delivered "connected" on a slot that is still the stand-in's tells its
// attachment too; a "gone" whose slot a real controller took meanwhile
// (wpsx, ws2: CodeRabbit CLI) is followed by "connected", so the game hears
// last that a controller is there.
void TestAfterTelling() {
    rtslot s = fresh();
    plug(s, 3);
    EXPECT_EQ(tell_all(s).extensions, 1);
    unplug(s, 3);
    std::uint32_t slot = 0;
    std::int32_t reason = 0;
    EXPECT_TRUE(rtslot_next_tell(&s, kNone, 1, 0, &slot, &reason));
    EXPECT_EQ(reason, -1);
    rtslot_probed(&s, 3, 0);  // a real Wii Remote arrives while "gone" is told
    EXPECT_EQ(rtslot_told(&s, slot, reason, 1), RTSLOT_AFTER_CONNECTED);
}

// #610 (CodeRabbit): an older probe answer applied late doesn't undo a
// newer one (the caller takes the count first, and applies only if it
// hasn't moved).
void TestProbeCountGuard() {
    rtslot s = fresh();
    const std::uint32_t before = rtslot_probe_count(&s, 0);
    rtslot_probed(&s, 0, 0);  // a newer answer: a real Wii Remote
    EXPECT_TRUE(rtslot_probe_count(&s, 0) != before);
    rtslot_probed(&s, 0, -2);  // busy: no answer, no count
    EXPECT_EQ(rtslot_probe_count(&s, 0), before + 1u);
}

// #609 (CodeRabbit): a pause in the game's KPAD reads isn't a controller
// gone; a steady stream of empty reads for the grace period is (pr2b:
// and the count starts over when a controller comes back).
void TestEmptyReads() {
    rtslot s = fresh();
    rtslot_kpad_read(&s, 1, 2, RTSLOT_NO_ERROR, 1000);
    EXPECT_EQ(rtslot_owner(&s, 1, kNone), RTSLOT_REAL);
    for (std::uint32_t i = 0; i + 1 < RTSLOT_EMPTY_READS; ++i) rtslot_kpad_read(&s, 1, 0, RTSLOT_NO_ERROR, 1001 + i);
    rtslot_kpad_read(&s, 1, 0, RTSLOT_NO_ERROR, 1001 + 3 * kGrace);  // after a long pause
    EXPECT_EQ(rtslot_owner(&s, 1, kNone), RTSLOT_REAL);
    std::uint32_t t = 5000;
    for (std::uint32_t i = 0; i <= RTSLOT_EMPTY_READS; ++i) {
        t += kGrace / 10;
        rtslot_kpad_read(&s, 1, 0, RTSLOT_NO_ERROR, t);
    }
    EXPECT_EQ(rtslot_owner(&s, 1, kNone), RTSLOT_NOBODY);
    rtslot_kpad_read(&s, 1, 0, -1, t + 1);  // "no controller" with an error output: gone at once
    rtslot_kpad_read(&s, 1, 1, RTSLOT_NO_ERROR, t + 2);
    EXPECT_EQ(rtslot_owner(&s, 1, kNone), RTSLOT_REAL);
    EXPECT_EQ(s.empty_since[1], 0u);  // a controller there: no empty count carried over
}

// ws15 (CodeRabbit CLI): a new "connected" clears `heard`, so only a read
// after it says the game heard it; ws2: a channel due a retell keeps its
// told state.
void TestHeardAndRetell() {
    rtslot s = fresh();
    s.retell_ticks = 100;
    plug(s, 0);
    tell_all(s);
    rtslot_heard(&s, 0);
    EXPECT_TRUE((s.heard & 1u) != 0);
    unplug(s, 0);
    tell_all(s);
    plug(s, 0);
    tell_all(s);
    EXPECT_EQ(s.heard & 1u, 0u);  // a new "connected": not heard yet
    rtslot_retell(&s, 1000);
    rtslot_retell(&s, 1200);  // unheard for retell_ticks: due again
    EXPECT_TRUE((s.retell & 1u) != 0);
    EXPECT_TRUE((s.told & 1u) != 0);  // still counts as told meanwhile
    EXPECT_EQ(tell_all(s).joins, 1);
}

// ws5 (CodeRabbit CLI): once quiet, quiet until the next Wii controller
// read, even when the time base comes round to just after the old read.
void TestQuietLatch() {
    rtslot s = fresh();
    rtslot_wii_read(&s, 1000);
    EXPECT_EQ(rtslot_quiet_due(&s, 1000 + 2999, 1), 0);
    EXPECT_EQ(rtslot_quiet_due(&s, 1000 + 3000, 1), 1);
    EXPECT_EQ(rtslot_quiet_due(&s, 1010, 1), 1);  // a wrapped clock
    rtslot_wii_read(&s, 9000);
    EXPECT_EQ(rtslot_quiet_due(&s, 9100, 1), 0);
}

}  // namespace

int main() {
    TestOwnerOrder();
    TestRealBeatsHome();
    TestHomeEndsOnRealSlot();
    TestHomeHoldsDifferentShape();
    TestHomeKeepsShapeAfterUnplug();
    TestUndeliveredStaysToTell();
    TestAfterTelling();
    TestProbeCountGuard();
    TestEmptyReads();
    TestHeardAndRetell();
    TestQuietLatch();
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)" << std::endl;
        return 1;
    }
    std::cout << "rtslot tests passed" << std::endl;
    return 0;
}
