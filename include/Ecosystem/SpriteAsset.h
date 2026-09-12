#pragma once

#include <cstddef>

namespace ecosystem::graphics {

enum class SpriteIndex : std::size_t {
    Grass = 0,
    Herbivore = 1,
    Carnivore = 2,
};

bool InitializeSprites();
void ShutdownSprites();
bool DrawSprite(SpriteIndex sprite, int centerX, int centerY);
bool IsSpriteReady();

} // namespace ecosystem::graphics
