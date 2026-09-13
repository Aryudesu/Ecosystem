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
    deathEffects_.fill({});
    statistics_ = {};
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
        animal.moving = false;
        animal.motion = AnimalMotion::Walk;

        if (AdvanceAge(animal)) {
            continue;
        }

        animal.energy -= animal.species == Species::Herbivore
            ? config_.herbivoreEnergyCost
            : config_.carnivoreEnergyCost;

        if (animal.energy <= 0.0f) {
            KillAnimal(animal, DeathCause::Starvation);
            continue;
        }

        UpdateState(animal);
        if (animal.species == Species::Herbivore) {
            UpdateHerbivore(i);
        } else {
            UpdateCarnivore(i);
        }
    }

    UpdateGrass();
    TryRegrowGrass();
}

void Simulation::UpdateVisualEffects() {
    for (auto& death : deathEffects_) {
        if (!death.active) continue;

        --death.remainingFrames;
        if (death.remainingFrames <= 0) {
            death = {};
        }
    }
}

void Simulation::UpdateState(Animal& animal) {
    if (!animal.active) return;

    const float hungryEnergy = animal.species == Species::Herbivore
        ? config_.herbivoreHungryEnergy
        : config_.carnivoreHungryEnergy;

    // Hunger takes priority over breeding so an animal that has already met
    // its breeding target can temporarily return to feeding instead of starving
    // while searching for a partner.
    if (animal.energy <= hungryEnergy) {
        animal.state = LifeState::Hungry;
        return;
    }

    animal.state = animal.meals >= animal.breedTarget
        ? LifeState::Breeding
        : LifeState::Normal;
}

bool Simulation::AdvanceAge(Animal& animal) {
    if (!animal.active || config_.framesPerAge <= 0 || animal.lifespan <= 0) {
        return false;
    }

    ++animal.ageFrames;
    if (animal.ageFrames < config_.framesPerAge) {
        return false;
    }

    animal.ageFrames = 0;
    ++animal.age;
    if (animal.age < animal.lifespan) {
        return false;
    }

    KillAnimal(animal, DeathCause::OldAge);
    return true;
}

void Simulation::UpdateGrass() {
    for (auto& grass : grass_) {
        if (!grass.active || grass.state != GrassState::Seed) continue;

        ++grass.growthFrames;
        if (grass.growthFrames >= grass.growthTarget) {
            grass.state = GrassState::Mature;
            grass.growthFrames = 0;
            grass.growthTarget = 0;
        }
    }
}

void Simulation::UpdateHerbivore(std::size_t index) {
    auto& animal = animals_[index];

    const int predatorIndex = FindNearestAnimal(animal, Species::Carnivore, config_.senseRadius);
    if (predatorIndex >= 0) {
        animal.motion = AnimalMotion::Run;
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
                food = {};
                animal.energy = std::min(config_.herbivoreMaxEnergy, animal.energy + config_.herbivoreFoodEnergy);
                ++animal.meals;
                if (animal.meals >= animal.breedTarget) {
                    animal.state = LifeState::Breeding;
                }
            } else {
                animal.motion = AnimalMotion::Run;
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
        float preySenseRadius = config_.carnivorePreySenseRadius;
        if (animal.energy <= config_.carnivoreCriticalEnergy) {
            preySenseRadius = config_.carnivoreCriticalPreySenseRadius;
        } else if (animal.energy <= config_.carnivoreDesperateEnergy) {
            preySenseRadius = config_.carnivoreDesperatePreySenseRadius;
        }

        const int preyIndex = FindNearestAnimal(animal, Species::Herbivore, preySenseRadius);
        if (preyIndex >= 0) {
            auto& prey = animals_[static_cast<std::size_t>(preyIndex)];
            if (DistanceSquared(animal.position, prey.position) <= config_.interactionRadius * config_.interactionRadius) {
                KillAnimal(prey, DeathCause::Predation);
                animal.energy = std::min(config_.carnivoreMaxEnergy, animal.energy + config_.carnivoreFoodEnergy);
                ++animal.meals;
                if (animal.meals >= animal.breedTarget) {
                    animal.state = LifeState::Breeding;
                }
            } else {
                animal.motion = AnimalMotion::Run;
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

    const float deltaX = animal.wanderDirection.x * speed;
    const float deltaY = animal.wanderDirection.y * speed;
    UpdateFacing(animal, deltaX);
    animal.moving = true;
    animal.animationDistance += std::abs(speed);
    animal.position.x += deltaX;
    animal.position.y += deltaY;
}

void Simulation::MoveToward(Animal& animal, const Vec2& target, float speed) {
    const float dx = target.x - animal.position.x;
    const float dy = target.y - animal.position.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) return;

    const float deltaX = dx / length * speed;
    const float deltaY = dy / length * speed;
    UpdateFacing(animal, deltaX);
    animal.moving = true;
    animal.animationDistance += std::abs(speed);
    animal.position.x += deltaX;
    animal.position.y += deltaY;
}

void Simulation::MoveAway(Animal& animal, const Vec2& target, float speed) {
    const float dx = animal.position.x - target.x;
    const float dy = animal.position.y - target.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        animal.wanderDirection = RandomDirection();
        const float deltaX = animal.wanderDirection.x * speed;
        const float deltaY = animal.wanderDirection.y * speed;
        UpdateFacing(animal, deltaX);
        animal.moving = true;
        animal.animationDistance += std::abs(speed);
        animal.position.x += deltaX;
        animal.position.y += deltaY;
        return;
    }

    float deltaX = dx / length * speed;
    float deltaY = dy / length * speed;

    const float maxX = static_cast<float>(SimulationConfig::Width - 1);
    const float maxY = static_cast<float>(SimulationConfig::Height - 1);
    const float nextX = animal.position.x + deltaX;
    const float nextY = animal.position.y + deltaY;
    const bool directMoveInside =
        nextX >= 0.0f && nextX <= maxX &&
        nextY >= 0.0f && nextY <= maxY;

    // If the straight escape direction points outside the world, do not keep
    // pushing against the boundary. Slide along a legal axis instead and pick
    // the step that leaves the animal farthest from the predator.
    if (!directMoveInside) {
        const Vec2 candidates[] = {
            { speed, 0.0f },
            { -speed, 0.0f },
            { 0.0f, speed },
            { 0.0f, -speed },
        };

        bool foundCandidate = false;
        float bestDistance = -1.0f;
        float bestDeltaX = 0.0f;
        float bestDeltaY = 0.0f;

        for (const auto& candidate : candidates) {
            const float candidateX = animal.position.x + candidate.x;
            const float candidateY = animal.position.y + candidate.y;
            if (candidateX < 0.0f || candidateX > maxX ||
                candidateY < 0.0f || candidateY > maxY) {
                continue;
            }

            const Vec2 candidatePosition{ candidateX, candidateY };
            const float distance = DistanceSquared(candidatePosition, target);
            if (!foundCandidate || distance > bestDistance) {
                foundCandidate = true;
                bestDistance = distance;
                bestDeltaX = candidate.x;
                bestDeltaY = candidate.y;
            }
        }

        if (foundCandidate) {
            deltaX = bestDeltaX;
            deltaY = bestDeltaY;
        } else {
            // Extremely small worlds or unusually large speeds may leave no
            // full-speed candidate. Fall back to the largest legal partial step.
            const float clampedX = std::clamp(nextX, 0.0f, maxX);
            const float clampedY = std::clamp(nextY, 0.0f, maxY);
            deltaX = clampedX - animal.position.x;
            deltaY = clampedY - animal.position.y;
        }
    }

    UpdateFacing(animal, deltaX);
    const float movedDistance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    animal.moving = movedDistance > 0.0001f;
    animal.animationDistance += movedDistance;
    animal.position.x += deltaX;
    animal.position.y += deltaY;
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

void Simulation::UpdateFacing(Animal& animal, float deltaX) {
    constexpr float Epsilon = 0.0001f;
    if (deltaX < -Epsilon) {
        animal.facingLeft = true;
    } else if (deltaX > Epsilon) {
        animal.facingLeft = false;
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
        if (!grass_[i].active || grass_[i].state != GrassState::Mature) continue;
        const float distance = DistanceSquared(from.position, grass_[i].position);
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

bool Simulation::TrySpawnAnimal(Species species, const Vec2& position) {
    const std::size_t speciesLimit = species == Species::Herbivore
        ? SimulationConfig::MaxHerbivores
        : SimulationConfig::MaxCarnivores;

    std::size_t speciesCount = 0;
    for (const auto& animal : animals_) {
        if (animal.active && animal.species == species) {
            ++speciesCount;
        }
    }
    if (speciesCount >= speciesLimit) {
        return false;
    }

    for (auto& animal : animals_) {
        if (animal.active) continue;

        animal = {};
        animal.active = true;
        animal.species = species;
        animal.state = LifeState::Normal;
        animal.position = position;
        animal.wanderDirection = RandomDirection();
        animal.facingLeft = animal.wanderDirection.x < 0.0f;
        animal.moving = false;
        animal.motion = AnimalMotion::Walk;
        animal.animationDistance = 0.0f;
        animal.energy = species == Species::Herbivore
            ? config_.herbivoreMaxEnergy
            : config_.carnivoreMaxEnergy;
        animal.breedTarget = species == Species::Herbivore
            ? random_.Int(2, 4)
            : random_.Int(5, 7);
        animal.age = 0;
        animal.ageFrames = 0;

        const int configuredMinLifespan = species == Species::Herbivore
            ? config_.herbivoreLifespanMin
            : config_.carnivoreLifespanMin;
        const int configuredMaxLifespan = species == Species::Herbivore
            ? config_.herbivoreLifespanMax
            : config_.carnivoreLifespanMax;
        const int minLifespan = std::max(1, std::min(configuredMinLifespan, configuredMaxLifespan));
        const int maxLifespan = std::max(minLifespan, std::max(configuredMinLifespan, configuredMaxLifespan));
        animal.lifespan = random_.Int(minLifespan, maxLifespan);
        return true;
    }
    return false;
}

bool Simulation::TrySpawnGrass(const Vec2& position, GrassState state, int growthTarget) {
    for (auto& grass : grass_) {
        if (grass.active) continue;
        grass = {};
        grass.active = true;
        grass.state = state;
        grass.position.x = std::clamp(position.x, 0.0f, static_cast<float>(SimulationConfig::Width - 1));
        grass.position.y = std::clamp(position.y, 0.0f, static_cast<float>(SimulationConfig::Height - 1));
        grass.growthTarget = state == GrassState::Seed ? std::max(1, growthTarget) : 0;
        return true;
    }
    return false;
}

void Simulation::SpawnGrassAround(const Vec2& position, int count) {
    const int minGrow = std::min(config_.grassSeedGrowMinFrames, config_.grassSeedGrowMaxFrames);
    const int maxGrow = std::max(config_.grassSeedGrowMinFrames, config_.grassSeedGrowMaxFrames);

    for (int i = 0; i < count; ++i) {
        Vec2 p{
            position.x + static_cast<float>(random_.Int(-24, 24)),
            position.y + static_cast<float>(random_.Int(-24, 24)),
        };
        if (!TrySpawnGrass(p, GrassState::Seed, random_.Int(minGrow, maxGrow))) return;
    }
}

void Simulation::SpawnDeathEffect(Species species, const Vec2& position) {
    if (config_.deathDisplayFrames <= 0) return;

    DeathEffect* slot = nullptr;
    for (auto& death : deathEffects_) {
        if (!death.active) {
            slot = &death;
            break;
        }
    }

    if (slot == nullptr) {
        slot = &*std::min_element(
            deathEffects_.begin(),
            deathEffects_.end(),
            [](const DeathEffect& lhs, const DeathEffect& rhs) {
                return lhs.remainingFrames < rhs.remainingFrames;
            });
    }

    slot->active = true;
    slot->species = species;
    slot->position = position;
    slot->remainingFrames = config_.deathDisplayFrames;
}

void Simulation::KillAnimal(Animal& animal, DeathCause cause) {
    if (!animal.active) return;

    const Species species = animal.species;
    const Vec2 deathPosition = animal.position;
    auto& speciesStats = species == Species::Herbivore
        ? statistics_.herbivore
        : statistics_.carnivore;

    switch (cause) {
    case DeathCause::OldAge:
        ++speciesStats.oldAgeDeaths;
        break;
    case DeathCause::Starvation:
        ++speciesStats.starvationDeaths;
        break;
    case DeathCause::Predation:
        ++speciesStats.predationDeaths;
        break;
    }

    SpawnDeathEffect(species, deathPosition);
    animal = {};
    SpawnGrassAround(deathPosition, config_.grassFromDeath);
}

void Simulation::TryBreed(std::size_t index) {
    auto& animal = animals_[index];
    const float breedSenseRadius = animal.species == Species::Herbivore
        ? config_.herbivoreBreedSenseRadius
        : config_.carnivoreBreedSenseRadius;
    const int partnerIndex = FindNearestAnimal(animal, animal.species, breedSenseRadius, true);
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

    const Vec2 childPosition{
        (animal.position.x + partner.position.x) * 0.5f,
        (animal.position.y + partner.position.y) * 0.5f,
    };

    const int configuredMin = animal.species == Species::Herbivore
        ? config_.herbivoreOffspringMin
        : config_.carnivoreOffspringMin;
    const int configuredMax = animal.species == Species::Herbivore
        ? config_.herbivoreOffspringMax
        : config_.carnivoreOffspringMax;
    const int minOffspring = std::max(0, std::min(configuredMin, configuredMax));
    const int maxOffspring = std::max(minOffspring, std::max(configuredMin, configuredMax));
    const int offspringTarget = random_.Int(minOffspring, maxOffspring);

    int spawnedOffspring = 0;
    for (int i = 0; i < offspringTarget; ++i) {
        if (!TrySpawnAnimal(animal.species, childPosition)) break;
        ++spawnedOffspring;
    }

    if (spawnedOffspring > 0) {
        auto& speciesStats = animal.species == Species::Herbivore
            ? statistics_.herbivore
            : statistics_.carnivore;
        speciesStats.births += static_cast<unsigned long long>(spawnedOffspring);

        animal.meals = 0;
        animal.breedTarget = animal.species == Species::Herbivore
            ? random_.Int(2, 4)
            : random_.Int(5, 7);
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
    const unsigned int seedColor = GetColor(155, 105, 55);
    const unsigned int herbivoreColor = GetColor(60, 125, 235);
    const unsigned int carnivoreColor = GetColor(220, 70, 70);
    const unsigned int deathColor = GetColor(125, 125, 125);
    const unsigned int hungryColor = GetColor(255, 175, 45);
    const unsigned int breedingColor = GetColor(220, 90, 220);

    for (const auto& grass : grass_) {
        if (!grass.active) continue;
        const bool seed = grass.state == GrassState::Seed;
        DrawCircle(
            static_cast<int>(grass.position.x),
            static_cast<int>(grass.position.y),
            seed ? 2 : 3,
            seed ? seedColor : grassColor,
            TRUE);
    }

    for (const auto& death : deathEffects_) {
        if (!death.active) continue;
        const int x = static_cast<int>(death.position.x);
        const int y = static_cast<int>(death.position.y);
        const int halfWidth = death.species == Species::Herbivore ? 7 : 9;
        DrawBox(x - halfWidth, y - 3, x + halfWidth, y + 3, deathColor, TRUE);
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
