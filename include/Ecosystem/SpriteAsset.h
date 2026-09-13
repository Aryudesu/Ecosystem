#pragma once

#include <cstddef>

namespace ecosystem::graphics {

enum class SpriteIndex : std::size_t {
    Grass = 0,
    Seed = 1,
    HerbivoreDead = 2,
    CarnivoreDead = 3,

    HerbivoreWalk1 = 4,
    HerbivoreWalk2 = 5,
    HerbivoreRun1 = 6,
    HerbivoreRun2 = 7,

    CarnivoreWalk1 = 8,
    CarnivoreWalk2 = 9,
    CarnivoreRun1 = 10,
    CarnivoreRun2 = 11,
};

bool InitializeSprites();
void ShutdownSprites();
bool DrawSprite(SpriteIndex sprite, int centerX, int centerY, bool flipHorizontal = false);
bool IsSpriteReady();

} // namespace ecosystem::graphics
