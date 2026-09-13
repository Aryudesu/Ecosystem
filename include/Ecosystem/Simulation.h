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

struct Animal {
    bool active = false;
    Species species = Species::Herbivore;
    LifeState state = LifeState::Inactive;
    Vec2 position{};
    Vec2 wanderDirection{1.0f, 0.0f};
    bool facingLeft = false;
    float energy = 0.0f;
    int meals = 0;
    int breedTarget = 0;
};

struct Grass {
    bool active = false;
    GrassState state = GrassState::Mature;
    Vec2 position{};
    int growthFrames = 0;
    int growthTarget = 0;
};

struct SimulationConfig {
    static constexpr int Width = 640;
    static constexpr int Height = 480;
    static constexpr std::size_t MaxObjects = 300;

    int initialCarnivores = 10;
    int initialHerbivores = 50;
    int initialGrass = 100;

    float senseRadius = 64.0f;
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
    float herbivoreEnergyCost = 0.025f;
    float carnivoreEnergyCost = 0.04f;
    float herbivoreFoodEnergy = 42.0f;
    float carnivoreFoodEnergy = 72.0f;

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

class Simulation {
public:
    using AnimalArray = std::array<Animal, SimulationConfig::MaxObjects>;
    using GrassArray = std::array<Grass, SimulationConfig::MaxObjects>;

    explicit Simulation(mygame::Random& random, SimulationConfig config = {});

    void Reset();
    void Update();
    void Draw() const;

    [[nodiscard]] Population GetPopulation() const;
    [[nodiscard]] unsigned long long Frame() const { return frame_; }
    [[nodiscard]] const AnimalArray& Animals() const { return animals_; }
    [[nodiscard]] const GrassArray& GrassItems() const { return grass_; }

private:
    void UpdateHerbivore(std::size_t index);
    void UpdateCarnivore(std::size_t index);
    void UpdateState(Animal& animal);
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
    void KillAnimal(Animal& animal);
    void TryBreed(std::size_t index);
    void TryRegrowGrass();

    [[nodiscard]] float DistanceSquared(const Vec2& a, const Vec2& b) const;
    [[nodiscard]] Vec2 RandomPosition();
    [[nodiscard]] Vec2 RandomDirection();

    mygame::Random& random_;
    SimulationConfig config_;
    AnimalArray animals_{};
    GrassArray grass_{};
    unsigned long long frame_ = 0;
};

} // namespace ecosystem
