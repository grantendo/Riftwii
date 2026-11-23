// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "riftwii/padlayout.hpp"

#include <cctype>

namespace riftwii {
namespace {

struct Name {
    const char* name;
    std::uint32_t value;
};

// The GameCube buttons, by their bit in a PADStatus button word.
constexpr Name kGcButtons[] = {{"left", 0},  {"right", 1}, {"down", 2}, {"up", 3},     {"z", 4},
                               {"r", 5},     {"l", 6},     {"a", 8},    {"b", 9},      {"x", 10},
                               {"y", 11},    {"start", 12}};

// KPAD's Classic Controller buttons (runtime/rthome.h's RTHOME_CL_*).
constexpr Name kClassic[] = {{"a", 0x0010},    {"b", 0x0040},     {"x", 0x0008},    {"y", 0x0020},
                             {"l", 0x2000},    {"r", 0x0200},     {"zl", 0x0080},   {"zr", 0x0004},
                             {"plus", 0x0400}, {"minus", 0x1000}, {"home", 0x0800}, {"up", 0x0001},
                             {"down", 0x4000}, {"left", 0x0002},  {"right", 0x8000}};

// The Wii Remote's buttons held upright (runtime/rtgcwii.c's RTGCWII_WR_*).
constexpr Name kRemote[] = {{"a", 0x0800},    {"b", 0x0400},     {"1", 0x0200},    {"2", 0x0100},
                            {"plus", 0x0010}, {"minus", 0x1000}, {"home", 0x8000}, {"up", 0x0008},
                            {"down", 0x0004}, {"left", 0x0001},  {"right", 0x0002}};

// The Nunchuk's (RTGCWII_NC_*).
constexpr Name kNunchuk[] = {{"c", 0x4000}, {"z", 0x2000}};

template <std::size_t N>
bool look_up(const Name (&names)[N], const std::string& word, std::uint32_t& out) {
    for (const Name& n : names) {
        if (word == n.name) {
            out = n.value;
            return true;
        }
    }
    return false;
}

std::string trimmed_lower(const std::string& s) {
    std::size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    std::string out;
    for (std::size_t i = start; i < end; ++i) out += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return out;
}

// One Wii button (or `shake`, `none`) as `shape` has it.
bool wii_button(const std::string& word, GcShape shape, std::uint32_t& out) {
    if (word == "none") {
        out = 0;
        return true;
    }
    if (shape == GcShape::Classic) return look_up(kClassic, word, out);
    if (word == "shake") {
        out = kGcLayoutShake;
        return true;
    }
    if (shape == GcShape::Nunchuk && look_up(kNunchuk, word, out)) return true;
    return look_up(kRemote, word, out);
}

const char* section_name(GcShape shape) {
    return shape == GcShape::Classic ? "classic" : shape == GcShape::Nunchuk ? "nunchuk" : "sideways";
}

}  // namespace

GcLayout default_gc_layout(GcShape shape) {
    GcLayout layout;
    std::uint32_t* b = layout.buttons;
    switch (shape) {
    case GcShape::Classic:
        b[8] = 0x0010;   // a: a
        b[9] = 0x0040;   // b: b
        b[10] = 0x0008;  // x: x
        b[11] = 0x0020;  // y: y
        b[12] = 0x0400;  // start: plus
        b[4] = 0x0004;   // z: zr
        b[6] = 0x2000;   // l: l
        b[5] = 0x0200;   // r: r
        b[3] = 0x0001;   // up
        b[2] = 0x4000;   // down
        b[0] = 0x0002;   // left
        b[1] = 0x8000;   // right
        break;
    case GcShape::Nunchuk:
        b[8] = 0x0800;   // a: A
        b[9] = 0x0400;   // b: B
        b[10] = kGcLayoutShake;  // x: shake
        b[11] = 0x0100;  // y: 2
        b[4] = 0x1000;   // z: minus
        b[12] = 0x0010;  // start: plus
        b[6] = 0x2000;   // l: the Nunchuk's Z
        b[5] = 0x4000;   // r: the Nunchuk's C
        b[0] = 0x0001;   // the D-pad as itself
        b[1] = 0x0002;
        b[3] = 0x0008;
        b[2] = 0x0004;
        break;
    case GcShape::Sideways:
        b[8] = 0x0100;   // a: 2
        b[9] = 0x0200;   // b: 1
        b[10] = 0x0800;  // x: A
        b[11] = 0x0400;  // y: B
        b[4] = 0x1000;   // z: minus
        b[12] = 0x0010;  // start: plus
        b[5] = kGcLayoutShake;  // r: shake
        b[0] = 0x0008;   // the D-pad turned: left is the Wii Remote's up
        b[1] = 0x0004;   // right: down
        b[3] = 0x0002;   // up: right
        b[2] = 0x0001;   // down: left
        break;
    }
    return layout;
}

GcLayout parse_gc_layout(const std::string& text, GcShape shape, std::vector<std::string>& notes) {
    GcLayout layout = default_gc_layout(shape);
    std::string section;
    std::size_t line_start = 0;
    unsigned number = 0;
    while (line_start <= text.size()) {
        std::size_t line_end = text.find('\n', line_start);
        if (line_end == std::string::npos) line_end = text.size();
        std::string line = text.substr(line_start, line_end - line_start);
        line_start = line_end + 1;
        ++number;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        line = trimmed_lower(line);
        if (line.empty()) continue;
        const std::string where = "line " + std::to_string(number) + ": ";
        if (line.front() == '[') {
            section = line.back() == ']' ? trimmed_lower(line.substr(1, line.size() - 2)) : "";
            if (section != "classic" && section != "nunchuk" && section != "sideways")
                notes.push_back(where + "not a shape this file can set (classic, nunchuk or sideways)");
            if (section == section_name(shape)) layout.present = true;
            continue;
        }
        if (section.empty()) {
            notes.push_back(where + "a button outside a [shape] section");
            continue;
        }
        if (section != section_name(shape)) continue;  // another shape's: not this launch's
        const std::size_t equals = line.find('=');
        std::uint32_t bit = 0;
        if (equals == std::string::npos || !look_up(kGcButtons, trimmed_lower(line.substr(0, equals)), bit)) {
            notes.push_back(where + "not `<GameCube button> = <Wii buttons>`");
            continue;
        }
        std::uint32_t pressed = 0;
        bool usable = true;
        std::string rest = line.substr(equals + 1);
        std::size_t at = 0;
        while (usable && at <= rest.size()) {
            std::size_t comma = rest.find(',', at);
            if (comma == std::string::npos) comma = rest.size();
            const std::string word = trimmed_lower(rest.substr(at, comma - at));
            std::uint32_t value = 0;
            if (!wii_button(word, shape, value)) {
                notes.push_back(where + "\"" + word + "\" isn't a button of the " + section_name(shape) + " shape");
                usable = false;
            }
            pressed |= value;
            at = comma + 1;
        }
        if (usable) layout.buttons[bit] = pressed;
    }
    return layout;
}

}  // namespace riftwii
