#pragma once

#include <cstddef>

namespace ecosystem::graphics {

enum class SpriteIndex : std::size_t {
    Grass = 0,
    HerbivoreWalk1 = 1,
    HerbivoreWalk2 = 2,
    HerbivoreRun1 = 3,
    HerbivoreRun2 = 4,
    CarnivoreWalk1 = 5,
    CarnivoreWalk2 = 6,
    CarnivoreRun1 = 7,
    CarnivoreRun2 = 8,
    Seed = 9,
};

bool InitializeSprites();
void ShutdownSprites();
bool DrawSprite(SpriteIndex sprite, int centerX, int centerY, bool flipHorizontal = false);
bool IsSpriteReady();

} // namespace ecosystem::graphics
