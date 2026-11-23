// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "padlayoutstore.hpp"

#include <vector>

#include "log.hpp"
#include "textfile.hpp"

namespace riftwii::wii {
namespace {

constexpr std::size_t kPadLayoutCap = 16 * 1024;  // a few dozen lines: far more than any layout needs

const char* shape_name(GcShape shape) {
    return shape == GcShape::Classic ? "Classic Controller" : shape == GcShape::Nunchuk ? "Wii Remote + Nunchuk"
                                                                                        : "sideways Wii Remote";
}

}  // namespace

GcLayout LoadPadLayout(const std::string& game_id, GcShape shape) {
    GcLayout layout;
    if (game_id.size() != 6) return layout;
    // The full game ID first, then its first four characters: a mod that
    // changes the last two (its own maker code or a region swap) keeps the
    // game's layout, and a file can cover every region of a game at once.
    std::string path = "sd:/riftwii/layouts/" + game_id + ".txt";
    std::string text;
    if (!ReadReplacedFile(path, text, kPadLayoutCap)) {
        path = "sd:/riftwii/layouts/" + game_id.substr(0, 4) + ".txt";
        if (!ReadReplacedFile(path, text, kPadLayoutCap)) return layout;
    }
    std::vector<std::string> notes;
    layout = parse_gc_layout(text, shape, notes);
    for (const std::string& note : notes) logf("GameCube controller layout: %s, %s (skipped)\n", path.c_str(), note.c_str());
    if (layout.present)
        logf("GameCube controller layout: %s gives the %s shape its buttons\n", path.c_str(), shape_name(shape));
    else
        logf("GameCube controller layout: %s has no section for the %s shape; the built-in layout stays\n",
             path.c_str(), shape_name(shape));
    return layout;
}

}  // namespace riftwii::wii
