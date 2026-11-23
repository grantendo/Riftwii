// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "riftwii/hbmsearch.hpp"
#include "riftwii/byteorder.hpp"

#include <algorithm>
#include <cstring>

namespace riftwii {
namespace {

std::int32_t simm(std::uint32_t insn) { return static_cast<std::int16_t>(insn & 0xFFFFu); }

std::uint32_t opcode(std::uint32_t insn) { return insn >> 26; }
std::uint32_t reg_d(std::uint32_t insn) { return (insn >> 21) & 31u; }
std::uint32_t reg_a(std::uint32_t insn) { return (insn >> 16) & 31u; }

// Whether `insn` writes register `reg`, for the common integer instructions:
// the D-form arithmetic and loads write rD; the logical immediates and the
// rotates write rA; among the opcode-31 forms, the logical and shift ones
// write rA, the stores write nothing, and the rest (arithmetic, indexed
// loads, mfspr) write rD.
bool writes_register(std::uint32_t insn, std::uint32_t reg) {
    const std::uint32_t op = opcode(insn);
    // The update forms (lwzu, lbzu, lhzu, lhau and the stores stwu, stbu,
    // sthu) also write their base, rA.
    const bool update = op == 33 || op == 35 || op == 37 || op == 39 || op == 41 || op == 43 || op == 45;
    if (update && reg_a(insn) == reg) return true;
    if (op == 46) return reg >= reg_d(insn);  // lmw: rD through r31
    const bool d_form = op == 7 || op == 8 || op == 12 || op == 13 || op == 14 || op == 15 ||
                        (op >= 32 && op <= 35) || (op >= 40 && op <= 43);  // mulli..addis, the loads
    if (d_form) return reg_d(insn) == reg;
    const bool a_form = op == 20 || op == 21 || op == 23 || (op >= 24 && op <= 29);  // rlwimi..andis.
    if (a_form) return reg_a(insn) == reg;
    if (op != 31) return false;
    const std::uint32_t xo = (insn >> 1) & 0x3FFu;
    switch (xo) {
        case 28: case 60: case 124: case 284: case 316: case 412: case 444: case 476:  // and..nand
        case 24: case 536: case 792: case 824: case 26: case 922: case 954:            // shifts, cntlzw, extsh/b
            return reg_a(insn) == reg;
        case 183: case 247: case 439:  // stwux, stbux, sthux: their base
            return reg_a(insn) == reg;
        case 55: case 119: case 311: case 375:  // lwzux, lbzux, lhzux, lhaux: rD and their base
            return reg_d(insn) == reg || reg_a(insn) == reg;
        case 151: case 215: case 407: case 662: case 918: case 150:  // the other stores
            return false;
        case 0: case 32: case 4:                          // cmp, cmpl, tw: compare only
        case 144: case 146: case 467:                     // mtcrf, mtmsr, mtspr: rS is read
        case 54: case 86: case 470: case 982: case 1014:  // dcbst, dcbf, dcbi, icbi, dcbz
            return false;
        default:
            return reg_d(insn) == reg;
    }
}

constexpr std::uint32_t kStwuR1Mask = 0xFFFF0000u;
constexpr std::uint32_t kStwuR1 = 0x94210000u;  // stwu r1,d(r1)
constexpr std::uint32_t kBlr = 0x4E800020u;
constexpr std::uint32_t kNop = 0x60000000u;
constexpr std::uint32_t kMtlrR0 = 0x7C0803A6u;

bool is_stwu_r1(std::uint32_t insn) { return (insn & kStwuR1Mask) == kStwuR1; }
// Where a framed function that starts at word `start` ends, before word
// `next` (the next `stwu r1`): after its last `blr`, or tail call (a `b` out
// of the function or a `bctr`), that follows a stack frame's undoing (`addi
// r1,r1,N` or `lwz r1,...` within the four words before, with no other
// return or tail call between), else after its first `blr`, else at `next`.
// Leaf code (no frame of its own) between it and the next framed function
// isn't counted, so its instructions can't lend the function marks it
// doesn't have.
std::size_t function_end(const std::uint8_t* bytes, std::size_t start, std::size_t next) {
    constexpr std::uint32_t kBlr = 0x4E800020u;
    constexpr std::uint32_t kBctr = 0x4E800420u;
    // A tail call: `bctr`, or `b` (no link) to somewhere outside start..next.
    const auto tail_call = [&](std::size_t k) {
        const std::uint32_t insn = be32(bytes + 4 * k);
        if (insn == kBctr) return true;
        if (opcode(insn) != 18 || (insn & 1u) != 0) return false;
        if ((insn & 2u) != 0) return true;  // absolute: never this function's own code
        std::int32_t offset = static_cast<std::int32_t>(insn & 0x03FFFFFCu);
        if (offset & 0x02000000) offset -= 0x04000000;
        const std::int64_t target = static_cast<std::int64_t>(k) + offset / 4;
        return target < static_cast<std::int64_t>(start) || target >= static_cast<std::int64_t>(next);
    };
    std::size_t last_framed = 0, first = 0;
    for (std::size_t k = start; k < next; ++k) {
        const bool ret = be32(bytes + 4 * k) == kBlr;
        if (!ret && !tail_call(k)) continue;
        if (ret && first == 0) first = k + 1;
        for (std::size_t back = 1; back <= 4 && back <= k - start; ++back) {
            const std::uint32_t insn = be32(bytes + 4 * (k - back));
            // Another function's return or tail call: its restore isn't this one's.
            if (insn == kBlr || tail_call(k - back)) break;
            const bool restore = (opcode(insn) == 14 && reg_d(insn) == 1 && reg_a(insn) == 1) ||  // addi r1,r1,N
                                 (opcode(insn) == 32 && reg_d(insn) == 1);                         // lwz r1,...
            if (restore) {
                last_framed = k + 1;
                break;
            }
        }
    }
    return last_framed != 0 ? last_framed : first != 0 ? first : next;
}
bool is_call(std::uint32_t insn) { return opcode(insn) == 18 && (insn & 3u) == 1u; }  // bl

// The word at `address` in any of `ranges`.
bool word_at(const std::vector<CodeRange>& ranges, std::uint32_t address, std::uint32_t& out) {
    for (const CodeRange& r : ranges) {
        if (r.bytes == nullptr || address < r.address || address - r.address + 4 > r.size) continue;
        out = be32(r.bytes + (address - r.address));
        return true;
    }
    return false;
}

// The function holding the word at `address`: its `stwu r1` going back, or
// the first word after the `blr` that ends the function before it (a leaf
// with no frame), past padding. 0 when neither comes within `limit` words.
std::uint32_t function_start(const std::vector<CodeRange>& text, std::uint32_t address, unsigned limit) {
    for (unsigned back = 0; back <= limit && back * 4 <= address; ++back) {
        std::uint32_t insn = 0;
        if (!word_at(text, address - back * 4, insn)) return 0;
        if (is_stwu_r1(insn)) return address - back * 4;
        if (back == 0 || insn != kBlr) continue;
        std::uint32_t first = address - back * 4 + 4;
        std::uint32_t next = 0;
        while (first < address && word_at(text, first, next) && (next == 0 || next == kNop)) first += 4;
        return first;
    }
    return 0;
}

// Calls `visit(address, insn)` for every word from `start` to its function's
// `blr` (at most `limit` words); stops early when `visit` returns false.
template <typename Visit>
void each_word(const std::vector<CodeRange>& text, std::uint32_t start, unsigned limit, Visit visit) {
    for (unsigned i = 0; i < limit; ++i) {
        std::uint32_t insn = 0;
        if (!word_at(text, start + i * 4, insn)) return;
        if (!visit(start + i * 4, insn)) return;
        if (insn == kBlr) return;
    }
}

// The KPAD status's fields the ring walker stores: the device type and the
// controller's error, as every SDK seen since 2007 lays them out.
constexpr std::int32_t kDevTypeOffset = 0x5C;
constexpr std::int32_t kWpadErrOffset = 0x5D;
// The 2006 KPAD (Excite Truck, Marvel Ultimate Alliance): a 136-byte
// status with the device type as a word at 0x5C, and a byte at 0x85 (the
// error, copied from the WPAD status's) its read stores.
constexpr std::int32_t kStatus2006Bytes = 136;
constexpr std::int32_t kWpadErr2006Offset = 0x85;
constexpr std::int32_t kMinStatusBytes = 0x80;
constexpr std::int32_t kMaxStatusBytes = 0x200;
constexpr unsigned kMinBackSteps = 3;
constexpr unsigned kErrorCopyWithin = 24;  // words from the read's start

// The function right after the one at `start` (up to `limit` words long,
// to its first `blr`), past any padding; 0 when there is none.
std::uint32_t next_function(const std::vector<CodeRange>& text, std::uint32_t start, unsigned limit) {
    std::uint32_t end = 0;
    each_word(text, start, limit, [&](std::uint32_t at, std::uint32_t w) {
        if (w == kBlr) end = at;
        return true;
    });
    if (end == 0) return 0;
    std::uint32_t next = end + 4, insn = 0;
    while (word_at(text, next, insn) && (insn == 0 || insn == kNop) && next < end + 64) next += 4;
    return word_at(text, next, insn) ? next : 0;
}

// Whether the wrapper at `at` keeps r3 (`mr rN,r3`) before its first call:
// it takes an argument, which HBMGetSelectBtnNum doesn't (Dr. Mario Online
// Rx has HBMSetAdjustFlag(BOOL) right after HBMDraw).
bool keeps_argument(const std::vector<CodeRange>& text, std::uint32_t at) {
    bool keeps = false;
    each_word(text, at, 16, [&](std::uint32_t, std::uint32_t w) {
        if (is_call(w)) return false;
        const bool mr_from_r3 = opcode(w) == 31 && ((w >> 1) & 0x3FFu) == 444u && reg_d(w) == 3 && ((w >> 11) & 31u) == 3u;
        keeps = keeps || mr_from_r3;
        return true;
    });
    return keeps;
}

// HBMGetSelectBtnNum, given where it should start: a short wrapper (a
// `stwu`, a call within 16 words, ended within 40, taking no argument;
// 2009 on), or a short leaf that answers -1 while the menu runs (`li
// r3,-1` within 12 words; the 2007 builds: Brawl, Mario Kart Wii, Cruis'n
// USA, and the C++ menu's inner one).
bool looks_like_select(const std::vector<CodeRange>& text, std::uint32_t at) {
    constexpr std::uint32_t kLiR3MinusOne = 0x3860FFFFu;
    std::uint32_t first = 0;
    if (at == 0 || !word_at(text, at, first)) return false;
    bool calls = false, ended = false, minus_one = false;
    unsigned words = 0;
    each_word(text, at, 40, [&](std::uint32_t where, std::uint32_t w) {
        ++words;
        calls = calls || (is_call(w) && where < at + 64);
        minus_one = minus_one || (w == kLiR3MinusOne && where < at + 48);
        if (w == kBlr) ended = true;
        return true;
    });
    if (!ended) return false;
    if (is_stwu_r1(first)) return calls && !keeps_argument(text, at);
    return minus_one && words <= 12;
}

// HBMCalc's answer, where it returns the button the menu ended on (the C++
// HOME Menu, 2008 on: Dr. Mario Online Rx): the leaf it calls last, right
// before its epilogue (`lwz` from the frame, `mtlr r0`, `addi r1`), whose
// r3 it passes back untouched. 0 when the last call isn't such a leaf.
std::uint32_t calc_answer(const std::vector<CodeRange>& text, std::uint32_t calc) {
    std::uint32_t last_call = 0, last_call_insn = 0;
    bool epilogue_only = false;
    each_word(text, calc, 64, [&](std::uint32_t at, std::uint32_t w) {
        if (is_call(w)) {
            last_call = at;
            last_call_insn = w;
            epilogue_only = true;
            return true;
        }
        const bool restores = opcode(w) == 32 && reg_a(w) == 1 && reg_d(w) != 3;  // lwz rN,d(r1)
        const bool frame_end = w == kMtlrR0 || (opcode(w) == 14 && reg_d(w) == 1 && reg_a(w) == 1);
        if (!(restores || frame_end || w == kBlr)) epilogue_only = false;
        return true;
    });
    if (last_call == 0 || !epilogue_only) return 0;
    std::int32_t offset = static_cast<std::int32_t>(last_call_insn & 0x03FFFFFCu);
    if (offset & 0x02000000) offset -= 0x04000000;
    const std::uint32_t leaf = last_call + static_cast<std::uint32_t>(offset);
    std::uint32_t first = 0;
    if (!word_at(text, leaf, first) || is_stwu_r1(first)) return 0;
    return looks_like_select(text, leaf) ? leaf : 0;
}

// The 2006 KPAD's read, where no later one is found: the one function that
// walks a ring of 136-byte statuses backwards (`mulli rD,rA,136` and at
// least three `addi rD,rA,-136`), copies the status's device type as a word
// (`stw` at 0x5C) and stores its error (`stb` at 0x85). Excite Truck
// (0x80047c0c) and Marvel Ultimate Alliance (0x8039e3d4). Three arguments,
// no error's address.
KpadRead find_kpad_read_2006(const std::vector<CodeRange>& text) {
    KpadRead found;
    unsigned matches = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        std::size_t i = 0;
        while (i < words) {
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            if (i >= words) break;
            const std::size_t start = i++;
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            bool multiplied = false, dev_type = false, error = false;
            unsigned back_steps = 0;
            const std::size_t end = function_end(r.bytes, start, i);
            for (std::size_t k = start; k < end; ++k) {
                const std::uint32_t insn = be32(r.bytes + 4 * k);
                const std::int32_t value = simm(insn);
                if (opcode(insn) == 7 && value == kStatus2006Bytes) multiplied = true;     // mulli
                if (opcode(insn) == 14 && value == -kStatus2006Bytes) ++back_steps;        // addi
                if (opcode(insn) == 36 && value == kDevTypeOffset) dev_type = true;        // stw
                if (opcode(insn) == 38 && value == kWpadErr2006Offset) error = true;       // stb
            }
            if (!multiplied || back_steps < kMinBackSteps || !dev_type || !error) continue;
            found.address = r.address + static_cast<std::uint32_t>(4 * start);
            found.status_bytes = static_cast<std::uint32_t>(kStatus2006Bytes);
            ++matches;
        }
    }
    return matches == 1 ? found : KpadRead{};
}

}  // namespace

KpadRead find_kpad_read(const std::vector<CodeRange>& text) {
    KpadRead found;
    unsigned matches = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        std::size_t i = 0;
        // One function at a time, from one `stwu r1` to the next.
        while (i < words) {
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            if (i >= words) break;
            const std::size_t start = i++;
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            // The sizes multiplied and stepped back by, and the two stores.
            std::int32_t sizes_multiplied[8] = {};
            unsigned multiplied = 0;
            unsigned back_steps[8] = {};
            bool dev_type = false, wpad_err = false;
            for (std::size_t k = start; k < i; ++k) {
                const std::uint32_t insn = be32(r.bytes + 4 * k);
                const std::int32_t value = simm(insn);
                if (opcode(insn) == 7 && value >= kMinStatusBytes && value <= kMaxStatusBytes && multiplied < 8) {
                    sizes_multiplied[multiplied++] = value;  // mulli
                } else if (opcode(insn) == 38) {           // stb
                    dev_type = dev_type || value == kDevTypeOffset;
                    wpad_err = wpad_err || value == kWpadErrOffset;
                }
            }
            if (!dev_type || !wpad_err || multiplied == 0) continue;
            for (std::size_t k = start; k < i; ++k) {
                const std::uint32_t insn = be32(r.bytes + 4 * k);
                if (opcode(insn) != 14) continue;  // addi rD,rA,-S
                for (unsigned m = 0; m < multiplied; ++m)
                    if (simm(insn) == -sizes_multiplied[m]) ++back_steps[m];
            }
            std::int32_t size = 0;
            for (unsigned m = 0; m < multiplied; ++m)
                if (back_steps[m] >= kMinBackSteps) size = sizes_multiplied[m];
            if (size == 0) continue;
            found.address = r.address + static_cast<std::uint32_t>(4 * start);
            found.status_bytes = static_cast<std::uint32_t>(size);
            ++matches;
        }
    }
    if (matches == 0) return find_kpad_read_2006(text);
    if (matches != 1) return KpadRead{};
    // The newer read keeps its fourth argument, the error's address, for
    // later: `mr rN,r6` among its first words, next to its copies of r3 to
    // r5 (Naruto, Kamen Rider, Ranger Cross). The 2007 read takes three
    // (Brawl, Mario Kart Wii), and r6 is whatever its caller left there.
    each_word(text, found.address, kErrorCopyWithin, [&](std::uint32_t, std::uint32_t w) {
        const bool or_form = opcode(w) == 31 && ((w >> 1) & 0x3FFu) == 444 && (w & 1u) == 0;  // or rA,rS,rB
        if (or_form && reg_d(w) == 6 && ((w >> 11) & 31u) == 6 && reg_a(w) != 6) found.takes_error = true;
        return !found.takes_error;
    });
    return found;
}

std::uint32_t find_wpad_get_info(const std::vector<CodeRange>& text) {
    constexpr std::uint32_t kIndexChannel = 0x5460103Au;  // slwi r0,r3,2
    constexpr std::int32_t kStatusReport = 0x15;
    constexpr std::int32_t kBusy = -2;
    constexpr std::int32_t kCommandBytes = 48;
    std::uint32_t found = 0;
    unsigned matches = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        std::size_t i = 0;
        while (i < words) {
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            if (i >= words) break;
            const std::size_t start = i++;
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            bool index = false, report = false, busy = false, command = false;
            int info_reg = -1, callback_reg = -1;
            std::uint32_t stack_stores = 0;  // registers stored to the stack, one bit each
            const std::size_t end = function_end(r.bytes, start, i);
            for (std::size_t k = start; k < end; ++k) {
                const std::uint32_t insn = be32(r.bytes + 4 * k);
                const std::int32_t value = simm(insn);
                index = index || insn == kIndexChannel;
                const bool li = opcode(insn) == 14 && reg_a(insn) == 0;
                report = report || (li && value == kStatusReport);
                busy = busy || (li && value == kBusy);
                command = command || (opcode(insn) == 7 && value == kCommandBytes);  // mulli
                const bool mr = opcode(insn) == 31 && ((insn >> 1) & 0x3FFu) == 444 && (insn & 1u) == 0 &&
                                ((insn >> 21) & 31u) == ((insn >> 11) & 31u);
                if (mr && ((insn >> 21) & 31u) == 4) info_reg = static_cast<int>(reg_a(insn));
                if (mr && ((insn >> 21) & 31u) == 5) callback_reg = static_cast<int>(reg_a(insn));
                if (opcode(insn) == 36 && reg_a(insn) == 1) stack_stores |= 1u << reg_d(insn);  // stw rS,d(r1)
            }
            const bool keeps = info_reg >= 0 && callback_reg >= 0 && ((stack_stores >> info_reg) & 1u) &&
                               ((stack_stores >> callback_reg) & 1u);
            if (!(index && report && busy && command && keeps)) continue;
            found = r.address + static_cast<std::uint32_t>(4 * start);
            ++matches;
        }
    }
    return matches == 1 ? found : 0;
}

WpadConnect2006 find_wpad_connect_2006(const std::vector<CodeRange>& text) {
    constexpr std::int32_t kConnectCallback = 2020;  // in the Oct 3 2006 WPAD's control block
    constexpr std::size_t kMaxWords = 40;
    WpadConnect2006 found;
    unsigned matches = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        std::size_t i = 0;
        while (i < words) {
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            if (i >= words) break;
            const std::size_t start = i++;
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            // The setter ends at its return: leaf code after it, before the
            // next framed function, isn't counted.
            const std::size_t end = function_end(r.bytes, start, i);
            if (end - start > kMaxWords) continue;
            std::uint32_t high[32] = {}, address[32] = {};
            std::uint32_t table = 0;
            std::uint32_t swapped = 0;  // the table whose block's callback was loaded and stored
            int block = -1;
            bool loads = false, stores = false;
            for (std::size_t k = start; k < end; ++k) {
                const std::uint32_t insn = be32(r.bytes + 4 * k);
                const std::uint32_t d = reg_d(insn), a = reg_a(insn);
                if (opcode(insn) == 15 && a == 0) high[d] = (insn & 0xFFFFu) << 16;  // lis
                if (opcode(insn) == 14 && a != 0 && high[a] != 0)                    // addi rD,rA,lo
                    address[d] = high[a] + static_cast<std::uint32_t>(simm(insn));
                const bool lwzx = opcode(insn) == 31 && ((insn >> 1) & 0x3FFu) == 23;
                if (lwzx && address[a] != 0) {
                    // Another table's block: its loads and stores are its own.
                    if (address[a] != table) loads = stores = false;
                    table = address[a];
                    block = static_cast<int>(d);
                }
                if (block >= 0 && static_cast<int>(a) == block && simm(insn) == kConnectCallback) {
                    loads = loads || opcode(insn) == 32;   // lwz
                    stores = stores || opcode(insn) == 36;  // stw
                }
                // The first table with both keeps them: a later lookup in
                // another table doesn't take its place.
                if (swapped == 0 && loads && stores) swapped = table;
            }
            if (swapped == 0) continue;
            found.table = swapped;
            found.offset = static_cast<std::uint32_t>(kConnectCallback);
            ++matches;
        }
    }
    return matches == 1 ? found : WpadConnect2006{};
}

namespace {

// Calls `visit(range, start, end)` (word indexes) for each framed function
// in `text`: from its `stwu r1` to where it ends (function_end: its last
// return or tail call after the frame is undone, else its first return,
// else the next `stwu r1`), if that is at most `max_words` long.
template <typename Visit>
void each_framed_function(const std::vector<CodeRange>& text, std::size_t max_words, Visit visit) {
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        std::size_t i = 0;
        while (i < words) {
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            if (i >= words) break;
            const std::size_t start = i++;
            while (i < words && !is_stwu_r1(be32(r.bytes + 4 * i))) ++i;
            // To its last return after the frame is undone (an early
            // error return doesn't end it), else its first.
            const std::size_t end = function_end(r.bytes, start, i);
            if (end - start <= max_words) visit(r, start, end);
        }
    }
}

// Whether the word at `k` (before word `end`) restores a register in an
// epilogue: a load off r1 (`lwz`/`lmw`) followed, through nothing but more
// such loads and `mtlr`, by the frame's undoing (`addi r1,r1,N`, N > 0) and
// then a return or tail call, within a few words. An ordinary reload
// through the stack isn't one.
bool epilogue_restore(const std::uint8_t* bytes, std::size_t k, std::size_t end) {
    constexpr std::size_t kWithin = 8;
    const auto restore_load = [](std::uint32_t w) { return (opcode(w) == 32 || opcode(w) == 46) && reg_a(w) == 1; };
    if (!restore_load(be32(bytes + 4 * k))) return false;
    for (std::size_t j = k + 1; j < end && j <= k + kWithin; ++j) {
        const std::uint32_t w = be32(bytes + 4 * j);
        if (opcode(w) == 14 && reg_d(w) == 1 && reg_a(w) == 1 && simm(w) > 0) {
            // The frame undone, then the function leaves (a return or a
            // tail call, maybe after reloading the return address off the
            // caller's frame and mtlr): only then is it an epilogue.
            for (std::size_t n = j + 1; n < end && n <= j + 4; ++n) {
                const std::uint32_t x = be32(bytes + 4 * n);
                if (x == kBlr || x == 0x4E800420u) return true;
                if (opcode(x) == 18 && (x & 1u) == 0) {
                    // A `b` is a tail call only if it leaves the code being
                    // read: absolute, or to before the restore or past `end`;
                    // a branch further into the function isn't an epilogue.
                    if ((x & 2u) != 0) return true;
                    std::int32_t offset = static_cast<std::int32_t>(x & 0x03FFFFFCu);
                    if (offset & 0x02000000) offset -= 0x04000000;
                    const std::int64_t target = static_cast<std::int64_t>(n) + offset / 4;
                    return target < static_cast<std::int64_t>(k) || target >= static_cast<std::int64_t>(end);
                }
                if ((x & 0xFC1FFFFFu) != 0x7C0803A6u && !restore_load(x)) return false;
            }
            return false;
        }
        const bool mtlr = (w & 0xFC1FFFFFu) == 0x7C0803A6u;
        if (!restore_load(w) && !mtlr) return false;
    }
    return false;
}

// `mask` (registers holding an argument, one bit each) after the word at
// `k` (before word `end`): a register it writes no longer holds it, unless
// the write is an `mr` from one that does. A register restored in an
// epilogue (epilogue_restore) is left as it was: the scan reads straight
// through an early return's epilogue into the code its branch around it
// reaches, where the register still holds the argument.
std::uint32_t track_argument(const std::uint8_t* bytes, std::size_t k, std::size_t end, std::uint32_t mask) {
    const std::uint32_t insn = be32(bytes + 4 * k);
    if (epilogue_restore(bytes, k, end)) return mask;
    const bool mr = opcode(insn) == 31 && ((insn >> 1) & 0x3FFu) == 444 && (insn & 1u) == 0 &&
                    reg_d(insn) == ((insn >> 11) & 31u);
    const bool copies = mr && ((mask >> reg_d(insn)) & 1u);
    for (std::uint32_t reg = 0; reg < 32; ++reg)
        if (((mask >> reg) & 1u) && writes_register(insn, reg)) mask &= ~(1u << reg);
    if (copies) mask |= 1u << reg_a(insn);
    return mask;
}

bool is_li(std::uint32_t insn, std::int32_t value) { return opcode(insn) == 14 && reg_a(insn) == 0 && simm(insn) == value; }

// `cmpwi rA,value` in cr0.
bool is_cmpwi(std::uint32_t insn, std::int32_t value) {
    return opcode(insn) == 11 && reg_d(insn) == 0 && simm(insn) == value;
}

// `slwi rX,rS,2` (`rlwinm rX,rS,2,0,29`) for an rS in `channel` (r3, the
// channel, and its copies, one bit each): the channel as an index into a
// table of words.
bool indexes_channel(std::uint32_t insn, std::uint32_t channel) {
    return (insn & ~(31u << 16) & ~(31u << 21)) == 0x5400103Au && ((channel >> reg_d(insn)) & 1u);
}

// WPAD's per-channel callback setters: the (table, offset) pair of each
// short framed function that takes a channel's control block from a table
// (`lwzx`) and swaps one word of it past its buffers, loading the old
// callback and storing its second argument (r4, or a copy) in its place.
std::vector<std::pair<std::uint32_t, std::int32_t>> wpad_setters(const std::vector<CodeRange>& text) {
    constexpr std::size_t kMaxSetterWords = 48;
    constexpr std::int32_t kBlockFields = 0x400;  // the callbacks sit past the control block's buffers
    std::vector<std::pair<std::uint32_t, std::int32_t>> setters;
    each_framed_function(text, kMaxSetterWords, [&](const CodeRange& r, std::size_t start, std::size_t end) {
        std::uint32_t high[32] = {}, address[32] = {};
        std::uint32_t table = 0, from_r4 = 1u << 4;  // r4 and its copies, one bit each
        const std::size_t first_of_function = setters.size();
        int block = -1;
        std::int32_t loaded = -1;
        for (std::size_t k = start; k < end; ++k) {
            const std::uint32_t insn = be32(r.bytes + 4 * k);
            const std::uint32_t d = reg_d(insn), a = reg_a(insn);
            const std::uint32_t r4_here = from_r4;  // as this instruction finds them
            from_r4 = track_argument(r.bytes, k, end, from_r4);
            if (opcode(insn) == 15 && a == 0) high[d] = (insn & 0xFFFFu) << 16;  // lis
            if (opcode(insn) == 14 && a != 0 && high[a] != 0)                    // addi rD,rA,lo
                address[d] = high[a] + static_cast<std::uint32_t>(simm(insn));
            const bool lwzx = opcode(insn) == 31 && ((insn >> 1) & 0x3FFu) == 23;
            if (lwzx && address[a] != 0) {
                table = address[a];
                block = static_cast<int>(d);
                loaded = -1;  // a load from another block isn't this one's
                continue;
            }
            if (block < 0 || static_cast<int>(a) != block || simm(insn) < kBlockFields) continue;
            if (opcode(insn) == 32) loaded = simm(insn);  // lwz: the old callback
            if (opcode(insn) != 36 || simm(insn) != loaded || !((r4_here >> d) & 1u)) continue;
            // stw: the new one, the second argument. Once per function, though
            // more than one of its paths swaps the same slot.
            const std::pair<std::uint32_t, std::int32_t> setter(table, loaded);
            if (std::find(setters.begin() + static_cast<std::ptrdiff_t>(first_of_function), setters.end(), setter) ==
                setters.end())
                setters.push_back(setter);
        }
    });
    return setters;
}

}  // namespace

WpadConnect2006 find_wpad_connect_later(const std::vector<CodeRange>& text) {
    constexpr std::uint32_t kBctrl = 0x4E800421u;
    constexpr unsigned kCallWindow = 8;  // words from the slot's load to its call
    const std::vector<std::pair<std::uint32_t, std::int32_t>> setters = wpad_setters(text);
    // The slot offsets WPAD calls with "no controller" (the call's load
    // doesn't say which table's block it is: that is the setters').
    std::vector<std::int32_t> told_gone;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        for (std::size_t i = 0; i < words; ++i) {
            const std::uint32_t insn = be32(r.bytes + 4 * i);
            if (opcode(insn) != 32 || reg_d(insn) != 12) continue;  // lwz r12,O(rB)
            const auto setter = std::find_if(setters.begin(), setters.end(),
                                             [&](const auto& s) { return s.second == simm(insn); });
            if (setter == setters.end()) continue;
            bool gone = false;
            for (std::size_t k = i + 1; k < words && k <= i + kCallWindow; ++k) {
                const std::uint32_t next = be32(r.bytes + 4 * k);
                if (next == kBctrl) {
                    if (gone && std::find(told_gone.begin(), told_gone.end(), setter->second) == told_gone.end())
                        told_gone.push_back(setter->second);
                    break;
                }
                // A jump or return, or r12 loaded again: not this slot's call
                // (the `beq` past it when the slot is empty is fine).
                if (opcode(next) == 18 || next == kBlr ||
                    ((opcode(next) == 32 || opcode(next) == 14) && reg_d(next) == 12))
                    break;
                // r4, the callback's argument, as the call will see it: -1
                // ("no controller") only if its last write was `li r4,-1`.
                if (writes_register(next, 4)) gone = is_li(next, -1);
            }
        }
    }
    if (told_gone.size() != 1) return WpadConnect2006{};
    // One table with a setter for that offset; two: not guessed.
    std::uint32_t table = 0;
    for (const auto& setter : setters) {
        if (setter.second != told_gone[0]) continue;
        if (table != 0 && table != setter.first) return WpadConnect2006{};
        table = setter.first;
    }
    WpadConnect2006 found;
    found.table = table;
    found.offset = static_cast<std::uint32_t>(told_gone[0]);
    return found;
}

WpadConnect2006 find_wpad_extension(const std::vector<CodeRange>& text, const WpadConnect2006& connect) {
    if (connect.table == 0 || connect.offset < 4) return WpadConnect2006{};
    const std::pair<std::uint32_t, std::int32_t> slot(connect.table, static_cast<std::int32_t>(connect.offset) - 4);
    const std::vector<std::pair<std::uint32_t, std::int32_t>> setters = wpad_setters(text);
    if (std::count(setters.begin(), setters.end(), slot) != 1) return WpadConnect2006{};
    WpadConnect2006 found;
    found.table = slot.first;
    found.offset = static_cast<std::uint32_t>(slot.second);
    return found;
}

std::uint32_t find_wpad_read(const std::vector<CodeRange>& text) {
    constexpr std::size_t kMaxWords = 192;
    constexpr std::int32_t kCoreStatusBytes = 42;  // sizeof(WPADStatus)
    constexpr std::int32_t kErrorAt = 41;          // WPADStatus.err
    std::uint32_t found = 0;
    unsigned matches = 0;
    each_framed_function(text, kMaxWords, [&](const CodeRange& r, std::size_t start, std::size_t end) {
        bool index = false, copy = false, corrupt = false, busy = false, error = false;
        std::uint32_t from_r3 = 1u << 3, from_r4 = 1u << 4;
        for (std::size_t k = start; k < end; ++k) {
            const std::uint32_t insn = be32(r.bytes + 4 * k);
            // The checks see the registers as this instruction finds them.
            index = index || indexes_channel(insn, from_r3);
            copy = copy || (is_li(insn, kCoreStatusBytes) && reg_d(insn) == 5);
            corrupt = corrupt || is_cmpwi(insn, -7);
            busy = busy || is_cmpwi(insn, -2);
            error = error || (opcode(insn) == 38 && simm(insn) == kErrorAt && ((from_r4 >> reg_a(insn)) & 1u));
            from_r3 = track_argument(r.bytes, k, end, from_r3);
            from_r4 = track_argument(r.bytes, k, end, from_r4);
        }
        if (!(index && copy && corrupt && busy && error)) return;
        found = r.address + static_cast<std::uint32_t>(4 * start);
        ++matches;
    });
    return matches == 1 ? found : 0;
}

std::uint32_t find_wpad_set_format(const std::vector<CodeRange>& text) {
    constexpr std::size_t kMaxWords = 96;
    constexpr std::int32_t kBlockFields = 0x400;
    std::uint32_t found = 0;
    unsigned matches = 0;
    each_framed_function(text, kMaxWords, [&](const CodeRange& r, std::size_t start, std::size_t end) {
        bool none = false, busy = false, compares = false, stores = false;
        // r4 (the format) and its copies, one bit each; the checks count
        // its copies only, as this setter keeps the format in a copy.
        std::uint32_t from_r4 = 1u << 4;
        for (std::size_t k = start; k < end; ++k) {
            const std::uint32_t insn = be32(r.bytes + 4 * k);
            const std::uint32_t copies = from_r4 & ~(1u << 4);  // as this instruction finds them
            from_r4 = track_argument(r.bytes, k, end, from_r4);
            // "No controller": returned as such, or the probe's result kept
            // and compared with it (The Complete Saga's 2007 WPAD).
            none = none || is_li(insn, -1) || is_cmpwi(insn, -1);
            busy = busy || is_li(insn, -2);
            const bool cmplw = opcode(insn) == 31 && ((insn >> 1) & 0x3FFu) == 32 && ((insn >> 21) & 1u) == 0;
            compares = compares || (cmplw && (((copies >> reg_a(insn)) & 1u) || ((copies >> ((insn >> 11) & 31u)) & 1u)));
            stores = stores || (opcode(insn) == 36 && simm(insn) >= kBlockFields && ((copies >> reg_d(insn)) & 1u));
        }
        if (!(none && busy && compares && stores)) return;
        found = r.address + static_cast<std::uint32_t>(4 * start);
        ++matches;
    });
    return matches == 1 ? found : 0;
}

std::uint32_t find_wpad_probe(const std::vector<CodeRange>& text) {
    constexpr std::uint32_t kNoDevice = 253;     // WPAD's "not known yet"
    constexpr std::uint32_t kIndexChannel = 0x5460103Au;  // slwi r0,r3,2
    std::uint32_t found = 0;
    unsigned matches = 0;
    std::vector<std::uint32_t> checked;  // a function with several compares is looked at once
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        for (std::size_t k = 0; k * 4 + 4 <= r.size; ++k) {
            const std::uint32_t insn = be32(r.bytes + 4 * k);
            if (opcode(insn) != 10 || (insn & 0xFFFFu) != kNoDevice) continue;  // cmplwi rX,253
            const std::uint32_t start = function_start(text, r.address + static_cast<std::uint32_t>(4 * k), 64);
            if (start == 0 || std::find(checked.begin(), checked.end(), start) != checked.end()) continue;
            checked.push_back(start);
            bool index = false, lwzx = false, store_type = false, minus_one = false, minus_two = false;
            std::int32_t type_offsets[8] = {};
            unsigned type_loads = 0;
            bool repeated_type = false;
            each_word(text, start, 80, [&](std::uint32_t, std::uint32_t w) {
                index = index || w == kIndexChannel;
                lwzx = lwzx || (opcode(w) == 31 && ((w >> 1) & 0x3FFu) == 23);
                if (opcode(w) == 34 && reg_d(w) == 0) {  // lbz r0,D(rA): the device type
                    for (unsigned t = 0; t < type_loads; ++t) repeated_type = repeated_type || type_offsets[t] == simm(w);
                    if (type_loads < 8) type_offsets[type_loads++] = simm(w);
                }
                store_type = store_type || (opcode(w) == 36 && reg_d(w) == 0 && (w & 0xFFFFu) == 0);  // stw r0,0(rA)
                minus_one = minus_one || (opcode(w) == 14 && reg_a(w) == 0 && (w & 0xFFFFu) == 0xFFFFu);
                minus_two = minus_two || (opcode(w) == 14 && reg_a(w) == 0 && (w & 0xFFFFu) == 0xFFFEu);
                return true;
            });
            if (!index || !lwzx || !store_type || !minus_one || !minus_two || !repeated_type) continue;
            found = start;
            ++matches;
        }
    }
    return matches == 1 ? found : 0;
}

KpadUnified find_kpad_unified(const std::vector<CodeRange>& text) {
    constexpr std::uint32_t kCmplwiR5By16 = 0x28050010u;
    KpadUnified found;
    unsigned matches = 0;
    std::vector<std::uint32_t> checked;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        for (std::size_t k = 0; k * 4 + 4 <= r.size; ++k) {
            if (be32(r.bytes + 4 * k) != kCmplwiR5By16) continue;
            const std::uint32_t start = function_start(text, r.address + static_cast<std::uint32_t>(4 * k), 24);
            if (start == 0 || std::find(checked.begin(), checked.end(), start) != checked.end()) continue;
            checked.push_back(start);
            bool channel_block = false, limit = false;
            std::int32_t multiples[8] = {}, copies[8] = {};
            unsigned n_multiples = 0, n_copies = 0;
            each_word(text, start, 80, [&](std::uint32_t, std::uint32_t w) {
                if (opcode(w) == 7) {  // mulli
                    if (reg_a(w) == 3 && simm(w) >= 256) channel_block = true;
                    else if (n_multiples < 8) multiples[n_multiples++] = simm(w);
                }
                if (opcode(w) == 14 && reg_a(w) == 0) {  // li
                    if (simm(w) == 16) limit = true;
                    if (reg_d(w) == 5 && n_copies < 8) copies[n_copies++] = simm(w);
                }
                return true;
            });
            std::uint32_t size = 0;
            for (unsigned m = 0; m < n_multiples; ++m)
                for (unsigned c = 0; c < n_copies; ++c)
                    if (multiples[m] == copies[c] && multiples[m] >= 40 && multiples[m] <= 96)
                        size = static_cast<std::uint32_t>(multiples[m]);
            if (!channel_block || !limit || size == 0) continue;
            found.address = start;
            found.status_bytes = size;
            ++matches;
        }
    }
    return matches == 1 ? found : KpadUnified{};
}

KpadConnect find_kpad_connect(const std::vector<CodeRange>& text, std::uint32_t kpad_read) {
    // The KPAD read's channel stride: its first `mulli rD,r3,K` with K at
    // least 256 (the per-channel block, not a status).
    std::uint32_t stride = 0;
    for (unsigned i = 0; i < 40 && stride == 0 && kpad_read != 0; ++i) {
        std::uint32_t w = 0;
        if (!word_at(text, kpad_read + 4 * i, w)) break;
        if (opcode(w) == 7 && reg_a(w) == 3 && simm(w) >= 256) stride = static_cast<std::uint32_t>(simm(w));
    }
    if (stride == 0) return {};
    KpadConnect found;
    unsigned matches = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        for (std::size_t k = 0; k * 4 + 24 <= r.size; ++k) {
            const auto at = [&](std::size_t n) { return be32(r.bytes + 4 * (k + n)); };
            const std::uint32_t m = at(0), hi = at(1), lo = at(2), add = at(3), load = at(4), store = at(5);
            if (opcode(m) != 7 || reg_d(m) != 0 || simm(m) != static_cast<std::int32_t>(stride)) continue;
            const std::uint32_t base = reg_d(hi);
            const bool lis = opcode(hi) == 15 && reg_a(hi) == 0;
            const bool addi = opcode(lo) == 14 && reg_d(lo) == base && reg_a(lo) == base;
            // add rB,rB,r0
            const bool sum = opcode(add) == 31 && ((add >> 1) & 0x1FFu) == 266 && reg_d(add) == base &&
                             reg_a(add) == base && ((add >> 11) & 31u) == 0;
            const bool swap = opcode(load) == 32 && opcode(store) == 36 && reg_a(load) == base &&
                              reg_a(store) == base && (load & 0xFFFFu) == (store & 0xFFFFu) && simm(load) > 0;
            if (!lis || !addi || !sum || !swap) continue;
            found.address = function_start(text, r.address + static_cast<std::uint32_t>(4 * k), 24);
            found.block = (static_cast<std::uint32_t>(hi & 0xFFFFu) << 16) + static_cast<std::uint32_t>(simm(lo));
            found.stride = stride;
            found.offset = static_cast<std::uint32_t>(simm(load));
            ++matches;
        }
    }
    return matches == 1 && found.address != 0 ? found : KpadConnect{};
}

std::uint32_t find_small_data_base(const std::vector<CodeRange>& sections, std::uint32_t entry) {
    std::uint32_t high = 0;
    bool have_high = false;
    for (unsigned i = 0; i < 256; ++i) {
        std::uint32_t insn = 0;
        if (!word_at(sections, entry + i * 4, insn)) return 0;
        if (opcode(insn) == 15 && reg_d(insn) == 13 && reg_a(insn) == 0) {  // lis r13,hi
            high = (insn & 0xFFFFu) << 16;
            have_high = true;
        } else if (have_high && opcode(insn) == 24 && reg_d(insn) == 13 && reg_a(insn) == 13) {  // ori r13,r13,lo
            return high | (insn & 0xFFFFu);
        } else if (have_high && opcode(insn) == 14 && reg_d(insn) == 13 && reg_a(insn) == 13) {  // addi r13,r13,lo
            return high + static_cast<std::uint32_t>(simm(insn));
        }
    }
    return 0;
}

HbmFunctions find_hbm_functions(const std::vector<CodeRange>& text, const std::vector<CodeRange>& sections,
                                std::uint32_t small_data_base) {
    static const char kBanner[] = "<< RVL_SDK - HBM";
    const std::size_t banner_length = sizeof(kBanner) - 1;
    // 1. The banner, in any section.
    std::uint32_t banner = 0;
    unsigned banners = 0;
    for (const CodeRange& r : sections) {
        if (r.bytes == nullptr || r.size < banner_length) continue;
        for (std::size_t k = 0; k + banner_length <= r.size; ++k) {
            if (std::memcmp(r.bytes + k, kBanner, banner_length) != 0) continue;
            banner = r.address + static_cast<std::uint32_t>(k);
            ++banners;
        }
    }
    if (banners != 1) return HbmFunctions{};
    // 2. The word pointing at it (__HBMVersion), outside the code.
    std::uint32_t pointer = 0;
    unsigned pointers = 0;
    for (const CodeRange& r : sections) {
        if (r.bytes == nullptr) continue;
        bool is_text = false;
        for (const CodeRange& t : text) is_text = is_text || t.address == r.address;
        if (is_text) continue;
        for (std::size_t k = 0; k * 4 + 4 <= r.size; ++k) {
            if (be32(r.bytes + 4 * k) != banner) continue;
            pointer = r.address + static_cast<std::uint32_t>(4 * k);
            ++pointers;
        }
    }
    if (pointers != 1) return HbmFunctions{};
    // 3. The function that loads that word: `lwz rD,d(r13)` with the
    // small-data base, or `lis rX,ha` and `lwz rD,lo(rX)` a few words on.
    const std::uint32_t high_adjusted = ((pointer + 0x8000u) >> 16) & 0xFFFFu;
    const std::uint32_t low = pointer & 0xFFFFu;
    std::uint32_t init = 0;
    unsigned loaders = 0;
    for (const CodeRange& r : text) {
        if (r.bytes == nullptr) continue;
        const std::size_t words = r.size / 4;
        for (std::size_t k = 0; k < words; ++k) {
            const std::uint32_t insn = be32(r.bytes + 4 * k);
            if (opcode(insn) != 32) continue;  // lwz
            bool loads_pointer = false;
            if (reg_a(insn) == 13 && small_data_base != 0) {
                loads_pointer = small_data_base + static_cast<std::uint32_t>(simm(insn)) == pointer;
            } else if (reg_a(insn) != 0 && (insn & 0xFFFFu) == low) {
                for (std::size_t back = 1; back <= 6 && back <= k && !loads_pointer; ++back) {
                    const std::uint32_t prior = be32(r.bytes + 4 * (k - back));
                    loads_pointer = opcode(prior) == 15 && reg_a(prior) == 0 && reg_d(prior) == reg_a(insn) &&
                                    (prior & 0xFFFFu) == high_adjusted;
                }
            }
            if (!loads_pointer) continue;
            const std::uint32_t start = function_start(text, r.address + static_cast<std::uint32_t>(4 * k), 32);
            if (start == 0) continue;
            bool calls = false;
            each_word(text, start, 48, [&](std::uint32_t, std::uint32_t w) {
                calls = calls || is_call(w);
                return true;
            });
            if (!calls || start == init) continue;
            init = start;
            ++loaders;
        }
    }
    if (loaders != 1) return HbmFunctions{};
    // 4. HBMCalc: the next function, past HBMInit's `blr` and any padding.
    std::uint32_t end = 0;
    each_word(text, init, 64, [&](std::uint32_t at, std::uint32_t w) {
        if (w == kBlr) end = at;
        return true;
    });
    if (end == 0) return HbmFunctions{};
    std::uint32_t calc = end + 4;
    std::uint32_t insn = 0;
    while (word_at(text, calc, insn) && (insn == 0 || insn == kNop) && calc < end + 64) calc += 4;
    if (!word_at(text, calc, insn) || !is_stwu_r1(insn)) return HbmFunctions{};
    bool calls = false;
    each_word(text, calc, 32, [&](std::uint32_t, std::uint32_t w) {
        calls = calls || is_call(w);
        return true;
    });
    if (!calls) return HbmFunctions{};
    // The button the menu ends on: what HBMCalc returns (2008 on), else
    // HBMGetSelectBtnNum, the function after HBMDraw (the one after HBMCalc).
    std::uint32_t select = calc_answer(text, calc);
    const std::uint32_t draw = next_function(text, calc, 64);
    std::uint32_t draw_first = 0;
    if (select == 0 && draw != 0 && word_at(text, draw, draw_first) && is_stwu_r1(draw_first)) {
        const std::uint32_t candidate = next_function(text, draw, 256);
        if (looks_like_select(text, candidate)) select = candidate;
    }
    return HbmFunctions{init, calc, select};
}

}  // namespace riftwii
