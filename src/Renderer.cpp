#include "Ecosystem/Renderer.h"

#include "Ecosystem/Simulation.h"
#include "Ecosystem/SpriteAsset.h"

#include <DxLib.h>

namespace ecosystem::graphics {
namespace {

SpriteIndex AnimalSprite(const Animal& animal, bool secondFrame) {
    if (animal.species == Species::Herbivore) {
        if (animal.motion == AnimalMotion::Run) {
            return secondFrame ? SpriteIndex::HerbivoreRun2 : SpriteIndex::HerbivoreRun1;
        }
        return secondFrame ? SpriteIndex::HerbivoreWalk2 : SpriteIndex::HerbivoreWalk1;
    }

    if (animal.motion == AnimalMotion::Run) {
        return secondFrame ? SpriteIndex::CarnivoreRun2 : SpriteIndex::CarnivoreRun1;
    }
    return secondFrame ? SpriteIndex::CarnivoreWalk2 : SpriteIndex::CarnivoreWalk1;
}

bool IsSecondAnimationFrame(const Animal& animal) {
    if (!animal.moving) return false;

    const float stride = animal.motion == AnimalMotion::Run ? 4.0f : 6.0f;
    return static_cast<int>(animal.animationDistance / stride) % 2 != 0;
}

} // namespace

void DrawSimulation(const Simulation& simulation) {
    if (!IsSpriteReady()) {
        simulation.Draw();
        return;
    }

    const unsigned int hungryColor = GetColor(255, 175, 45);
    const unsigned int breedingColor = GetColor(220, 90, 220);

    for (const auto& grass : simulation.GrassItems()) {
        if (!grass.active) continue;
        DrawSprite(
            grass.state == GrassState::Seed ? SpriteIndex::Seed : SpriteIndex::Grass,
            static_cast<int>(grass.position.x),
            static_cast<int>(grass.position.y));
    }

    for (const auto& animal : simulation.Animals()) {
        if (!animal.active) continue;

        const int x = static_cast<int>(animal.position.x);
        const int y = static_cast<int>(animal.position.y);
        DrawSprite(
            AnimalSprite(animal, IsSecondAnimationFrame(animal)),
            x,
            y,
            animal.facingLeft);

        if (animal.state == LifeState::Hungry) {
            DrawCircle(x, y, 13, hungryColor, FALSE, 1);
        } else if (animal.state == LifeState::Breeding) {
            DrawCircle(x, y, 13, breedingColor, FALSE, 2);
        }
    }
}

} // namespace ecosystem::graphics
