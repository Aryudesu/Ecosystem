#include "Ecosystem/Simulation.h"

#include <DxLib.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ecosystem {
namespace {
constexpr float Pi = 3.14159265358979323846f;
}

Simulation::Simulation(mygame::Random& random, SimulationConfig config)
    : random_(random), config_(config) {
    Reset();
}

void Simulation::Reset() {
    animals_.fill({});
    grass_.fill({});
    frame_ = 0;

    for (int i = 0; i < config_.initialHerbivores; ++i) {
        TrySpawnAnimal(Species::Herbivore, RandomPosition());
    }
    for (int i = 0; i < config_.initialCarnivores; ++i) {
        TrySpawnAnimal(Species::Carnivore, RandomPosition());
    }
    for (int i = 0; i < config_.initialGrass; ++i) {
        TrySpawnGrass(RandomPosition());
    }
}

void Simulation::Update() {
    ++frame_;

    for (std::size_t i = 0; i < animals_.size(); ++i) {
        if (!animals_[i].active) continue;

        auto& animal = animals_[i];
        animal.energy -= animal.species == Species::Herbivore
            ? config_.herbivoreEnergyCost
            : config_.carnivoreEnergyCost;

        if (animal.energy <= 0.0f) {
            KillAnimal(animal);
            continue;
        }

        UpdateState(animal);
        if (animal.species == Species::Herbivore) {
            UpdateHerbivore(i);
        } else {
            UpdateCarnivore(i);
        }
    }

    TryRegrowGrass();
}

void Simulation::UpdateState(Animal& animal) {
    if (!animal.active || animal.state == LifeState::Breeding) return;

    const float hungryEnergy = animal.species == Species::Herbivore
        ? config_.herbivoreHungryEnergy
        : config_.carnivoreHungryEnergy;

    animal.state = animal.energy <= hungryEnergy
        ? LifeState::Hungry
        : LifeState::Normal;
}

void Simulation::UpdateHerbivore(std::size_t index) {
    auto& animal = animals_[index];

    // The original HSP herbivores prioritize escaping nearby carnivores.
    const int predatorIndex = FindNearestAnimal(animal, Species::Carnivore, config_.senseRadius);
    if (predatorIndex >= 0) {
        MoveAway(animal, animals_[static_cast<std::size_t>(predatorIndex)].position, config_.herbivoreFleeSpeed);
        ClampToWorld(animal);
        return;
    }

    if (animal.state == LifeState::Breeding) {
        TryBreed(index);
        ClampToWorld(animal);
        return;
    }

    if (animal.state == LifeState::Hungry) {
        const int grassIndex = FindNearestGrass(animal, config_.senseRadius);
        if (grassIndex >= 0) {
            auto& food = grass_[static_cast<std::size_t>(grassIndex)];
            if (DistanceSquared(animal.position, food.position) <= config_.interactionRadius * config_.interactionRadius) {
                food.active = false;
                animal.energy = std::min(config_.herbivoreMaxEnergy, animal.energy + config_.herbivoreFoodEnergy);
                ++animal.meals;
                if (animal.meals >= animal.breedTarget) {
                    animal.state = LifeState::Breeding;
                }
            } else {
                MoveToward(animal, food.position, config_.herbivoreFoodSpeed);
            }
        } else {
            Wander(animal, config_.herbivoreWanderSpeed);
        }
    } else {
        Wander(animal, config_.herbivoreWanderSpeed);
    }

    ClampToWorld(animal);
}

void Simulation::UpdateCarnivore(std::size_t index) {
    auto& animal = animals_[index];

    if (animal.state == LifeState::Breeding) {
        TryBreed(index);
        ClampToWorld(animal);
        return;
    }

    if (animal.state == LifeState::Hungry) {
        const int preyIndex = FindNearestAnimal(animal, Species::Herbivore, config_.senseRadius);
        if (preyIndex >= 0) {
            auto& prey = animals_[static_cast<std::size_t>(preyIndex)];
            if (DistanceSquared(animal.position, prey.position) <= config_.interactionRadius * config_.interactionRadius) {
                KillAnimal(prey);
                animal.energy = std::min(config_.carnivoreMaxEnergy, animal.energy + config_.carnivoreFoodEnergy);
                ++animal.meals;
                if (animal.meals >= animal.breedTarget) {
                    animal.state = LifeState::Breeding;
                }
            } else {
                MoveToward(animal, prey.position, config_.carnivoreChaseSpeed);
            }
        } else {
            Wander(animal, config_.carnivoreWanderSpeed);
        }
    } else {
        Wander(animal, config_.carnivoreWanderSpeed);
    }

    ClampToWorld(animal);
}

void Simulation::Wander(Animal& animal, float speed) {
    if (random_.Chance(0.025)) {
        animal.wanderDirection = RandomDirection();
    }
    animal.position.x += animal.wanderDirection.x * speed;
    animal.position.y += animal.wanderDirection.y * speed;
}

void Simulation::MoveToward(Animal& animal, const Vec2& target, float speed) {
    const float dx = target.x - animal.position.x;
    const float dy = target.y - animal.position.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) return;

    animal.position.x += dx / length * speed;
    animal.position.y += dy / length * speed;
}

void Simulation::MoveAway(Animal& animal, const Vec2& target, float speed) {
    const float dx = animal.position.x - target.x;
    const float dy = animal.position.y - target.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        animal.wanderDirection = RandomDirection();
        animal.position.x += animal.wanderDirection.x * speed;
        animal.position.y += animal.wanderDirection.y * speed;
        return;
    }

    animal.position.x += dx / length * speed;
    animal.position.y += dy / length * speed;
}

void Simulation::ClampToWorld(Animal& animal) {
    const float oldX = animal.position.x;
    const float oldY = animal.position.y;
    animal.position.x = std::clamp(animal.position.x, 0.0f, static_cast<float>(SimulationConfig::Width - 1));
    animal.position.y = std::clamp(animal.position.y, 0.0f, static_cast<float>(SimulationConfig::Height - 1));

    if (animal.position.x != oldX || animal.position.y != oldY) {
        animal.wanderDirection = RandomDirection();
    }
}

int Simulation::FindNearestAnimal(const Animal& from, Species species, float maxDistance, bool breedingPartner) const {
    const float limit = maxDistance * maxDistance;
    float bestDistance = limit;
    int bestIndex = -1;

    for (std::size_t i = 0; i < animals_.size(); ++i) {
        const auto& candidate = animals_[i];
        if (!candidate.active || &candidate == &from || candidate.species != species) continue;
        if (breedingPartner && candidate.state == LifeState::Hungry) continue;

        const float distance = DistanceSquared(from.position, candidate.position);
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

int Simulation::FindNearestGrass(const Animal& from, float maxDistance) const {
    const float limit = maxDistance * maxDistance;
    float bestDistance = limit;
    int bestIndex = -1;

    for (std::size_t i = 0; i < grass_.size(); ++i) {
        if (!grass_[i].active) continue;
        const float distance = DistanceSquared(from.position, grass_[i].position);
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

bool Simulation::TrySpawnAnimal(Species species, const Vec2& position) {
    for (auto& animal : animals_) {
        if (animal.active) continue;

        animal = {};
        animal.active = true;
        animal.species = species;
        animal.state = LifeState::Normal;
        animal.position = position;
        animal.wanderDirection = RandomDirection();
        animal.energy = species == Species::Herbivore
            ? config_.herbivoreMaxEnergy
            : config_.carnivoreMaxEnergy;
        animal.breedTarget = species == Species::Herbivore
            ? random_.Int(2, 4)
            : random_.Int(4, 6);
        return true;
    }
    return false;
}

bool Simulation::TrySpawnGrass(const Vec2& position) {
    for (auto& grass : grass_) {
        if (grass.active) continue;
        grass.active = true;
        grass.position.x = std::clamp(position.x, 0.0f, static_cast<float>(SimulationConfig::Width - 1));
        grass.position.y = std::clamp(position.y, 0.0f, static_cast<float>(SimulationConfig::Height - 1));
        return true;
    }
    return false;
}

void Simulation::SpawnGrassAround(const Vec2& position, int count) {
    for (int i = 0; i < count; ++i) {
        Vec2 p{
            position.x + static_cast<float>(random_.Int(-24, 24)),
            position.y + static_cast<float>(random_.Int(-24, 24)),
        };
        if (!TrySpawnGrass(p)) return;
    }
}

void Simulation::KillAnimal(Animal& animal) {
    if (!animal.active) return;
    const Vec2 deathPosition = animal.position;
    animal = {};
    SpawnGrassAround(deathPosition, config_.grassFromDeath);
}

void Simulation::TryBreed(std::size_t index) {
    auto& animal = animals_[index];
    const int partnerIndex = FindNearestAnimal(animal, animal.species, config_.senseRadius, true);
    if (partnerIndex < 0) {
        Wander(animal, animal.species == Species::Herbivore
            ? config_.herbivoreWanderSpeed
            : config_.carnivoreWanderSpeed);
        return;
    }

    auto& partner = animals_[static_cast<std::size_t>(partnerIndex)];
    if (DistanceSquared(animal.position, partner.position) > config_.interactionRadius * config_.interactionRadius) {
        MoveToward(animal, partner.position, animal.species == Species::Herbivore
            ? config_.herbivoreFoodSpeed
            : config_.carnivoreChaseSpeed);
        return;
    }

    Vec2 childPosition{
        (animal.position.x + partner.position.x) * 0.5f,
        (animal.position.y + partner.position.y) * 0.5f,
    };

    if (TrySpawnAnimal(animal.species, childPosition)) {
        animal.meals = 0;
        animal.breedTarget = animal.species == Species::Herbivore
            ? random_.Int(2, 4)
            : random_.Int(4, 6);
        animal.state = LifeState::Normal;
    }
}

void Simulation::TryRegrowGrass() {
    if (config_.grassRegrowFrames <= 0 || frame_ % static_cast<unsigned long long>(config_.grassRegrowFrames) != 0) return;
    if (GetPopulation().grass >= config_.initialGrass) return;
    TrySpawnGrass(RandomPosition());
}

float Simulation::DistanceSquared(const Vec2& a, const Vec2& b) const {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vec2 Simulation::RandomPosition() {
    return {
        static_cast<float>(random_.Int(0, SimulationConfig::Width - 1)),
        static_cast<float>(random_.Int(0, SimulationConfig::Height - 1)),
    };
}

Vec2 Simulation::RandomDirection() {
    const float angle = static_cast<float>(random_.Real(0.0, 2.0 * Pi));
    return {std::cos(angle), std::sin(angle)};
}

Population Simulation::GetPopulation() const {
    Population result{};
    for (const auto& animal : animals_) {
        if (!animal.active) continue;
        if (animal.species == Species::Herbivore) ++result.herbivores;
        else ++result.carnivores;
    }
    for (const auto& grass : grass_) {
        if (grass.active) ++result.grass;
    }
    return result;
}

void Simulation::Draw() const {
    const unsigned int grassColor = GetColor(70, 185, 70);
    const unsigned int herbivoreColor = GetColor(60, 125, 235);
    const unsigned int carnivoreColor = GetColor(220, 70, 70);
    const unsigned int hungryColor = GetColor(255, 175, 45);
    const unsigned int breedingColor = GetColor(220, 90, 220);

    for (const auto& grass : grass_) {
        if (!grass.active) continue;
        DrawCircle(static_cast<int>(grass.position.x), static_cast<int>(grass.position.y), 3, grassColor, TRUE);
    }

    for (const auto& animal : animals_) {
        if (!animal.active) continue;
        const int x = static_cast<int>(animal.position.x);
        const int y = static_cast<int>(animal.position.y);
        const unsigned int bodyColor = animal.species == Species::Herbivore ? herbivoreColor : carnivoreColor;
        const int radius = animal.species == Species::Herbivore ? 6 : 8;
        DrawCircle(x, y, radius, bodyColor, TRUE);

        if (animal.state == LifeState::Hungry) {
            DrawCircle(x, y, radius + 2, hungryColor, FALSE, 1);
        } else if (animal.state == LifeState::Breeding) {
            DrawCircle(x, y, radius + 2, breedingColor, FALSE, 2);
        }
    }
}

} // namespace ecosystem
