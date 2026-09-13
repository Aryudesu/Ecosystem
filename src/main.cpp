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

        bool simulationAdvanced = false;
        if (!paused) {
            for (int i = 0; i < stepsPerFrame; ++i) simulation.Update();
            simulationAdvanced = true;
        } else if (stepOnce) {
            simulation.Update();
            simulationAdvanced = true;
        }
        stepOnce = false;

        // Death effects use rendered frames so they remain visible even at x16 simulation speed.
        if (simulationAdvanced) {
            simulation.UpdateVisualEffects();
        }

        ClearDrawScreen();
        DrawBox(0, 0, ecosystem::SimulationConfig::Width, ecosystem::SimulationConfig::Height,
            GetColor(20, 28, 24), TRUE);
        ecosystem::graphics::DrawSimulation(simulation);

        const auto population = simulation.GetPopulation();
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 190);
        DrawBox(4, 4, 308, 83, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawFormatString(12, 10, GetColor(255, 255, 255),
            "Carnivore: %d  Herbivore: %d  Grass: %d",
            population.carnivores, population.herbivores, population.grass);
        DrawFormatString(12, 31, GetColor(255, 255, 255),
            "Frame: %llu  Speed: x%d  %s",
            simulation.Frame(), stepsPerFrame, paused ? "PAUSED" : "RUNNING");
        DrawString(12, 52,
            "SPACE pause / N step / R reset / UP-DOWN speed / ESC exit",
            GetColor(210, 210, 210));

        ScreenFlip();
    }

    ecosystem::graphics::ShutdownSprites();
    DxLib_End();
    return 0;
}
