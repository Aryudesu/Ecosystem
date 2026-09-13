#pragma once

#include <mygame/collision/Collision2D.h>
#include <mygame/random/Random.h>

#include <array>
#include <cstddef>

namespace ecosystem {

using Vec2 = mygame::Vec2;

enum class LifeState {
    Inactive = 0,
    Normal = 1,
    Hungry = 2,
    Breeding = 3,
};

enum class Species {
    Herbivore,
    Carnivore,
};

enum class GrassState {
    Mature,
    Seed,
};

enum class AnimalMotion {
    Walk,
    Run,
};

enum class DeathCause {
    OldAge,
    Starvation,
    Predation,
};

struct Animal {
    bool active = false;
    Species species = Species::Herbivore;
    LifeState state = LifeState::Inactive;
    Vec2 position{};
    Vec2 wanderDirection{1.0f, 0.0f};
    bool facingLeft = false;
    bool moving = false;
    AnimalMotion motion = AnimalMotion::Walk;
    float animationDistance = 0.0f;
    float energy = 0.0f;
    int meals = 0;
    int breedTarget = 0;
    int age = 0;
    int ageFrames = 0;
    int lifespan = 0;
};

struct Grass {
    bool active = false;
    GrassState state = GrassState::Mature;
    Vec2 position{};
    int growthFrames = 0;
    int growthTarget = 0;
};

struct DeathEffect {
    bool active = false;
    Species species = Species::Herbivore;
    Vec2 position{};
    int remainingFrames = 0;
};

struct SimulationConfig {
    static constexpr int Width = 640;
    static constexpr int Height = 480;
    static constexpr int InfoPanelWidth = 320;
    static constexpr int WindowWidth = Width + InfoPanelWidth;
    static constexpr int WindowHeight = Height;

    // Keep independent species capacities so one species cannot consume every
    // animal slot and prevent the other species from reproducing.
    static constexpr std::size_t MaxHerbivores = 300;
    static constexpr std::size_t MaxCarnivores = 100;
    static constexpr std::size_t MaxAnimals = MaxHerbivores + MaxCarnivores;
    static constexpr std::size_t MaxGrass = 300;
    static constexpr std::size_t MaxDeathEffects = 300;

    int initialCarnivores = 10;
    int initialHerbivores = 50;
    int initialGrass = 100;

    float senseRadius = 64.0f;
    float herbivoreBreedSenseRadius = 64.0f;
    float carnivoreBreedSenseRadius = 800.0f;
    float interactionRadius = 16.0f;

    float herbivoreWanderSpeed = 0.65f;
    float herbivoreFoodSpeed = 1.5f;
    float herbivoreFleeSpeed = 1.2f;
    float carnivoreWanderSpeed = 0.75f;
    float carnivoreChaseSpeed = 2.4f;

    float herbivoreMaxEnergy = 100.0f;
    float carnivoreMaxEnergy = 130.0f;
    float herbivoreHungryEnergy = 45.0f;
    float carnivoreHungryEnergy = 60.0f;
    float herbivoreEnergyCost = 0.10f;
    float carnivoreEnergyCost = 0.20f;
    float herbivoreFoodEnergy = 42.0f;
    float carnivoreFoodEnergy = 72.0f;

    // Litter size per successful breeding event.
    int herbivoreOffspringMin = 1;
    int herbivoreOffspringMax = 2;
    int carnivoreOffspringMin = 1;
    int carnivoreOffspringMax = 1;

    // The original HSP source had a lifespan of 8-12, but aging was disabled.
    // One age unit advances every framesPerAge simulation updates.
    int framesPerAge = 600;
    int herbivoreLifespanMin = 8;
    int herbivoreLifespanMax = 12;
    int carnivoreLifespanMin = 12;
    int carnivoreLifespanMax = 16;

    // Death effects are counted in rendered frames, not simulation steps.
    int deathDisplayFrames = 30;

    int grassFromDeath = 5;
    int grassSeedGrowMinFrames = 100;
    int grassSeedGrowMaxFrames = 199;
    int grassRegrowFrames = 24;
};

struct Population {
    int carnivores = 0;
    int herbivores = 0;
    int grass = 0;
};

struct SpeciesStatistics {
    unsigned long long births = 0;
    unsigned long long oldAgeDeaths = 0;
    unsigned long long starvationDeaths = 0;
    unsigned long long predationDeaths = 0;
};

struct SimulationStatistics {
    SpeciesStatistics herbivore{};
    SpeciesStatistics carnivore{};
};

class Simulation {
public:
    using AnimalArray = std::array<Animal, SimulationConfig::MaxAnimals>;
    using GrassArray = std::array<Grass, SimulationConfig::MaxGrass>;
    using DeathEffectArray = std::array<DeathEffect, SimulationConfig::MaxDeathEffects>;

    explicit Simulation(mygame::Random& random, SimulationConfig config = {});

    void Reset();
    void Update();
    void UpdateVisualEffects();
    void Draw() const;

    [[nodiscard]] Population GetPopulation() const;
    [[nodiscard]] const SimulationStatistics& GetStatistics() const { return statistics_; }
    [[nodiscard]] unsigned long long Frame() const { return frame_; }
    [[nodiscard]] float SenseRadius() const { return config_.senseRadius; }
    [[nodiscard]] const AnimalArray& Animals() const { return animals_; }
    [[nodiscard]] const GrassArray& GrassItems() const { return grass_; }
    [[nodiscard]] const DeathEffectArray& DeathEffects() const { return deathEffects_; }

private:
    void UpdateHerbivore(std::size_t index);
    void UpdateCarnivore(std::size_t index);
    void UpdateState(Animal& animal);
    bool AdvanceAge(Animal& animal);
    void UpdateGrass();
    void Wander(Animal& animal, float speed);
    void MoveToward(Animal& animal, const Vec2& target, float speed);
    void MoveAway(Animal& animal, const Vec2& target, float speed);
    void ClampToWorld(Animal& animal);
    void UpdateFacing(Animal& animal, float deltaX);

    int FindNearestAnimal(const Animal& from, Species species, float maxDistance, bool breedingPartner = false) const;
    int FindNearestGrass(const Animal& from, float maxDistance) const;

    bool TrySpawnAnimal(Species species, const Vec2& position);
    bool TrySpawnGrass(const Vec2& position, GrassState state = GrassState::Mature, int growthTarget = 0);
    void SpawnGrassAround(const Vec2& position, int count);
    void SpawnDeathEffect(Species species, const Vec2& position);
    void KillAnimal(Animal& animal, DeathCause cause);
    void TryBreed(std::size_t index);
    void TryRegrowGrass();

    [[nodiscard]] float DistanceSquared(const Vec2& a, const Vec2& b) const;
    [[nodiscard]] Vec2 RandomPosition();
    [[nodiscard]] Vec2 RandomDirection();

    mygame::Random& random_;
    SimulationConfig config_;
    AnimalArray animals_{};
    GrassArray grass_{};
    DeathEffectArray deathEffects_{};
    SimulationStatistics statistics_{};
    unsigned long long frame_ = 0;
};

} // namespace ecosystem
