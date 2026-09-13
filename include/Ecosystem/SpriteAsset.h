#pragma once

#include <cstddef>

namespace ecosystem::graphics {

enum class SpriteIndex : std::size_t {
    Grass = 0,
    Herbivore = 1,
    Carnivore = 2,
    Seed = 3,
};

bool InitializeSprites();
void ShutdownSprites();
bool DrawSprite(SpriteIndex sprite, int centerX, int centerY, bool flipHorizontal = false);
bool IsSpriteReady();

} // namespace ecosystem::graphics
