// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
// The controller finders' shared code reading (src/patching/codescan.hpp):
// RegTrack's rules, each one a past review finding on #616's finders.
#include <cstdint>
#include <iostream>
#include <vector>

#include "../src/patching/codescan.hpp"  // the finders own it: not a public header

using riftwii::codescan::RegTrack;

static int g_failures = 0;
#define EXPECT_TRUE(cond) do { if (!(cond)) { std::cerr << "FAILED: " #cond " at line " << __LINE__ << std::endl; g_failures++; } } while (0)
#define EXPECT_EQ(a, b) do { const auto expect_a_ = (a); const auto expect_b_ = (b); if (expect_a_ != expect_b_) { std::cerr << "FAILED: " #a " == " #b " (" << expect_a_ << " != " << expect_b_ << ") at line " << __LINE__ << std::endl; g_failures++; } } while (0)

namespace {

std::vector<std::uint8_t> code(const std::vector<std::uint32_t>& words) {
    std::vector<std::uint8_t> out;
    for (std::uint32_t w : words) {
        out.push_back(static_cast<std::uint8_t>(w >> 24));
        out.push_back(static_cast<std::uint8_t>(w >> 16));
        out.push_back(static_cast<std::uint8_t>(w >> 8));
        out.push_back(static_cast<std::uint8_t>(w));
    }
    return out;
}

// Runs the tracker over every word; returns it after the last.
RegTrack run(const std::vector<std::uint8_t>& bytes, unsigned what, unsigned follow) {
    RegTrack t(bytes.data(), 0, bytes.size() / 4, what);
    if (follow < 32) t.follow(follow);
    for (std::size_t k = 0; k < bytes.size() / 4; ++k) t.step(k);
    return t;
}

constexpr std::uint32_t kStwu = 0x9421FFE0u;    // stwu r1,-32(r1)
constexpr std::uint32_t kMrR31R4 = 0x7C9F2378u;  // mr r31,r4
constexpr std::uint32_t kMrR5R4 = 0x7C852378u;   // mr r5,r4
constexpr std::uint32_t kBl = 0x48000101u;       // bl +0x100
constexpr std::uint32_t kLiR3 = 0x38600000u;     // li r3,0
constexpr std::uint32_t kLisR6 = 0x3CC08050u;    // lis r6,0x8050
constexpr std::uint32_t kAddiR6 = 0x38C61234u;   // addi r6,r6,0x1234
constexpr std::uint32_t kLisR30 = 0x3FC08050u;   // lis r30,0x8050
constexpr std::uint32_t kAddiR30 = 0x3BDE1234u;  // addi r30,r30,0x1234
constexpr std::uint32_t kAddiR11R1 = 0x39610020u;  // addi r11,r1,32 (a save helper's)
constexpr std::uint32_t kStw = 0x90A10008u;      // stw r5,8(r1)

// A copy (`mr`) holds the argument; a call takes it from the volatile
// registers (r5) but not the kept ones (r31).
void TestCopiesAndCalls() {
    const auto bytes = code({kStwu, kMrR31R4, kMrR5R4, kStw, kBl, kStw});
    RegTrack t = run(bytes, RegTrack::kArguments, 4);
    EXPECT_EQ(t.copies(4), 1u << 31);
}

// `bl _savegpr` after `addi r11,r1,N` keeps the argument registers: the
// compiler's register save helpers touch nothing else (every game's
// WPADSetDataFormat broke when it counted as a call).
void TestSaveHelperKeeps() {
    const auto bytes = code({kStwu, kMrR5R4, kAddiR11R1, kBl, kStw});
    RegTrack t = run(bytes, RegTrack::kArguments, 4);
    EXPECT_TRUE((t.copies(4) >> 5) & 1u);
}

// A call that ends an error path (only `li r3` before the epilogue's
// return) clobbers nothing for the code its branch around it reaches.
void TestErrorPathCall() {
    const auto bytes = code({kStwu, kMrR5R4, kBl, kLiR3, 0x4E800020u});
    RegTrack t = run(bytes, RegTrack::kArguments, 4);
    EXPECT_TRUE((t.copies(4) >> 5) & 1u);
}

// lis + addi build a table's address; a call forgets it in a volatile
// register, not in a kept one.
void TestAddresses() {
    const auto bytes = code({kStwu, kLisR6, kAddiR6, kLisR30, kAddiR30});
    RegTrack t = run(bytes, RegTrack::kAddresses, 32);
    EXPECT_EQ(t.address(6), 0x80501234u);
    EXPECT_EQ(t.address(30), 0x80501234u);
    const auto called = code({kStwu, kLisR6, kAddiR6, kLisR30, kAddiR30, kBl, kStw});
    RegTrack u = run(called, RegTrack::kAddresses, 32);
    EXPECT_EQ(u.address(6), 0u);
    EXPECT_EQ(u.address(30), 0x80501234u);
}

// An early return's epilogue restore (`lwz r31,...(r1)` then the frame
// undone and `blr`) doesn't end what r31 held for the code after it.
void TestEpilogueRestoreKeeps() {
    const auto bytes = code({kStwu, kMrR31R4, 0x83E1001Cu /* lwz r31,28(r1) */, 0x38210020u /* addi r1,r1,32 */,
                             0x4E800020u});
    RegTrack t = run(bytes, RegTrack::kArguments, 4);
    EXPECT_TRUE((t.copies(4) >> 31) & 1u);
}

// CodeRabbit CLI (restructure rs3): a framed function that ends with a
// plain `b` after its frame is undone ends there; a later leaf's `blr`
// doesn't borrow that restore and stretch the function over the leaf.
void TestFunctionEndAtBranch() {
    const auto bytes = code({kStwu, 0x38210020u /* addi r1,r1,32 */, 0x48000008u /* b +8 */, 0x60000000u,
                             0x3860FFFEu /* li r3,-2 */, 0x4E800020u});
    EXPECT_EQ(riftwii::codescan::function_end(bytes.data(), 0, bytes.size() / 4), 3u);
}

// A call clobbers the volatile register holding a table's block.
void TestClobbers() {
    const auto bytes = code({kStwu, kMrR5R4, kBl, kStw, kAddiR11R1, kBl, kStw});
    RegTrack t(bytes.data(), 0, bytes.size() / 4, RegTrack::kArguments);
    EXPECT_TRUE(t.clobbers(2, 5));    // an ordinary call: r5 gone
    EXPECT_TRUE(!t.clobbers(2, 31));  // a kept register
    EXPECT_TRUE(!t.clobbers(5, 5));   // a save helper's call
    EXPECT_TRUE(t.clobbers(1, 5));    // mr r5,r4 writes it
}

}  // namespace

int main() {
    TestCopiesAndCalls();
    TestSaveHelperKeeps();
    TestErrorPathCall();
    TestAddresses();
    TestEpilogueRestoreKeeps();
    TestFunctionEndAtBranch();
    TestClobbers();
    if (g_failures != 0) {
        std::cerr << g_failures << " failure(s)" << std::endl;
        return 1;
    }
    std::cout << "codescan tests passed" << std::endl;
    return 0;
}
