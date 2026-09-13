#include "Ecosystem/Renderer.h"
#include "Ecosystem/Simulation.h"
#include "Ecosystem/SpriteAsset.h"

#include <DxLib.h>
#include <mygame/input/InputManager.h>
#include <mygame/random/Random.h>

#include <algorithm>

namespace {

void DrawSectionTitle(int x, int y, const char* title) {
    DrawString(x, y, title, GetColor(245, 245, 245));
    DrawLine(x, y + 17, x + 284, y + 17, GetColor(85, 90, 105));
}

void DrawInfoPanel(
    const ecosystem::Simulation& simulation,
    const ecosystem::Population& population,
    bool paused,
    int stepsPerFrame) {
    const auto& statistics = simulation.GetStatistics();

    const int panelX = ecosystem::SimulationConfig::Width;
    const int panelWidth = ecosystem::SimulationConfig::InfoPanelWidth;
    const int panelHeight = ecosystem::SimulationConfig::WindowHeight;
    const int x = panelX + 12;

    DrawBox(
        panelX,
        0,
        panelX + panelWidth,
        panelHeight,
        GetColor(18, 19, 24),
        TRUE);
    DrawLine(panelX, 0, panelX, panelHeight, GetColor(115, 120, 135));

    DrawSectionTitle(x, 12, "Population");
    DrawFormatString(x, 36, GetColor(255, 210, 210), "Carnivore : %d", population.carnivores);
    DrawFormatString(x, 55, GetColor(210, 225, 255), "Herbivore : %d", population.herbivores);
    DrawFormatString(x, 74, GetColor(190, 235, 190), "Grass     : %d", population.grass);

    DrawSectionTitle(x, 103, "Simulation");
    DrawFormatString(x, 127, GetColor(230, 230, 230), "Frame : %llu", simulation.Frame());
    DrawFormatString(x, 146, GetColor(230, 230, 230), "Speed : x%d", stepsPerFrame);
    DrawFormatString(x, 165, GetColor(230, 230, 230), "State : %s", paused ? "PAUSED" : "RUNNING");

    DrawSectionTitle(x, 194, "Statistics");
    DrawFormatString(
        x,
        218,
        GetColor(230, 230, 230),
        "Births  H:%llu  C:%llu",
        statistics.herbivore.births,
        statistics.carnivore.births);

    DrawString(x, 242, "Herbivore deaths", GetColor(210, 225, 255));
    DrawFormatString(
        x,
        261,
        GetColor(225, 225, 225),
        "Age:%llu  Starve:%llu  Pred:%llu",
        statistics.herbivore.oldAgeDeaths,
        statistics.herbivore.starvationDeaths,
        statistics.herbivore.predationDeaths);

    DrawString(x, 285, "Carnivore deaths", GetColor(255, 210, 210));
    DrawFormatString(
        x,
        304,
        GetColor(225, 225, 225),
        "Age:%llu  Starve:%llu  Pred:%llu",
        statistics.carnivore.oldAgeDeaths,
        statistics.carnivore.starvationDeaths,
        statistics.carnivore.predationDeaths);

    DrawSectionTitle(x, 333, "Population Graph");
    DrawBox(x, 357, x + 284, 423, GetColor(8, 9, 12), TRUE);
    DrawBox(x, 357, x + 284, 423, GetColor(70, 75, 90), FALSE);
    DrawString(x + 48, 381, "reserved for population history", GetColor(145, 150, 165));

    DrawString(x, 438, "SPACE pause  N step  R reset", GetColor(195, 195, 205));
    DrawString(x, 457, "UP/DOWN speed  ESC exit", GetColor(195, 195, 205));
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetOutApplicationLogValidFlag(FALSE);
    SetGraphMode(
        ecosystem::SimulationConfig::WindowWidth,
        ecosystem::SimulationConfig::WindowHeight,
        32);
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

        // The simulation world remains 640x480. Only the application window is wider.
        DrawBox(
            0,
            0,
            ecosystem::SimulationConfig::Width,
            ecosystem::SimulationConfig::Height,
            GetColor(20, 28, 24),
            TRUE);
        ecosystem::graphics::DrawSimulation(simulation);

        const auto population = simulation.GetPopulation();
        DrawInfoPanel(simulation, population, paused, stepsPerFrame);

        ScreenFlip();
    }

    ecosystem::graphics::ShutdownSprites();
    DxLib_End();
    return 0;
}
