// SPDX-FileCopyrightText: 2026 RiftWii contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

#include "riftwii/padlayout.hpp"

// A game's GameCube controller layout on the SD card
// (sd:/riftwii/layouts/<game ID>.txt, else <its first four characters>.txt;
// riftwii/padlayout.hpp: #587's light patches), read for a launch with
// GameCube controllers as Wii controllers.
namespace riftwii::wii {

// The layout for `shape` from the game's file, `present` false when there
// is no file or it has no section for the shape. Says in the log what it
// used and which lines it skipped.
GcLayout LoadPadLayout(const std::string& game_id, GcShape shape);

}  // namespace riftwii::wii
