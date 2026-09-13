#include "Ecosystem/Renderer.h"
#include "Ecosystem/Simulation.h"
#include "Ecosystem/SpriteAsset.h"

#include <DxLib.h>
#include <mygame/input/InputManager.h>
#include <mygame/random/Random.h>

#include <algorithm>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetOutApplicationLogValidFlag(FALSE);
    SetGraphMode(ecosystem::SimulationConfig::Width, ecosystem::SimulationConfig::Height, 32);
    ChangeWindowMode(TRUE);
    SetMainWindowText("Ecosystem - HSP port");

    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);
    SetWaitVSyncFlag(TRUE);

    ecosystem::graphics::InitializeSprites();

    mygame::Random random;
    auto& input = mygame::InputManager::GetInstance();
    ecosystem::Simulation simulation(random);

    bool paused = false;
    bool stepOnce = false;
    int stepsPerFrame = 1;

    while (ProcessMessage() == 0) {
        if (!input.Update()) break;
        if (input.IsPressed(KEY_INPUT_ESCAPE)) break;

        if (input.IsPressed(KEY_INPUT_SPACE)) paused = !paused;
        if (input.IsPressed(KEY_INPUT_N)) stepOnce = true;
        if (input.IsPressed(KEY_INPUT_R)) simulation.Reset();
        if (input.IsPressed(KEY_INPUT_UP)) stepsPerFrame = std::min(16, stepsPerFrame * 2);
        if (input.IsPressed(KEY_INPUT_DOWN)) stepsPerFrame = std::max(1, stepsPerFrame / 2);

        const bool simulationAdvanced = !paused || stepOnce;

        // Age existing death effects once per rendered frame. Do this before the
        // simulation update so a corpse created this frame gets its full lifetime.
        if (simulationAdvanced) {
            simulation.UpdateVisualEffects();
        }

        if (!paused) {
            for (int i = 0; i < stepsPerFrame; ++i) simulation.Update();
        } else if (stepOnce) {
            simulation.Update();
        }
        stepOnce = false;

        ClearDrawScreen();
        DrawBox(0, 0, ecosystem::SimulationConfig::Width, ecosystem::SimulationConfig::Height,
            GetColor(20, 28, 24), TRUE);
        ecosystem::graphics::DrawSimulation(simulation);

        const auto population = simulation.GetPopulation();
        const auto& statistics = simulation.GetStatistics();
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
        DrawBox(4, 4, 620, 136, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawFormatString(12, 10, GetColor(255, 255, 255),
            "Carnivore: %d  Herbivore: %d  Grass: %d",
            population.carnivores, population.herbivores, population.grass);
        DrawFormatString(12, 31, GetColor(255, 255, 255),
            "Frame: %llu  Speed: x%d  %s",
            simulation.Frame(), stepsPerFrame, paused ? "PAUSED" : "RUNNING");
        DrawFormatString(12, 52, GetColor(225, 225, 225),
            "Births  Herbivore: %llu  Carnivore: %llu",
            statistics.herbivore.births, statistics.carnivore.births);
        DrawFormatString(12, 73, GetColor(225, 225, 225),
            "Herbivore deaths  Age: %llu  Starve: %llu  Predation: %llu",
            statistics.herbivore.oldAgeDeaths,
            statistics.herbivore.starvationDeaths,
            statistics.herbivore.predationDeaths);
        DrawFormatString(12, 94, GetColor(225, 225, 225),
            "Carnivore deaths  Age: %llu  Starve: %llu  Predation: %llu",
            statistics.carnivore.oldAgeDeaths,
            statistics.carnivore.starvationDeaths,
            statistics.carnivore.predationDeaths);
        DrawString(12, 115,
            "SPACE pause / N step / R reset / UP-DOWN speed / ESC exit",
            GetColor(210, 210, 210));

        ScreenFlip();
    }

    ecosystem::graphics::ShutdownSprites();
    DxLib_End();
    return 0;
}
