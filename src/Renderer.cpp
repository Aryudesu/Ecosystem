#include "Ecosystem/Renderer.h"

#include "Ecosystem/Simulation.h"
#include "Ecosystem/SpriteAsset.h"

#include <DxLib.h>

namespace ecosystem::graphics {

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
            SpriteIndex::Grass,
            static_cast<int>(grass.position.x),
            static_cast<int>(grass.position.y));
    }

    for (const auto& animal : simulation.Animals()) {
        if (!animal.active) continue;

        const int x = static_cast<int>(animal.position.x);
        const int y = static_cast<int>(animal.position.y);
        const auto sprite = animal.species == Species::Herbivore
            ? SpriteIndex::Herbivore
            : SpriteIndex::Carnivore;

        DrawSprite(sprite, x, y);

        if (animal.state == LifeState::Hungry) {
            DrawCircle(x, y, 13, hungryColor, FALSE, 1);
        } else if (animal.state == LifeState::Breeding) {
            DrawCircle(x, y, 13, breedingColor, FALSE, 2);
        }
    }
}

} // namespace ecosystem::graphics
