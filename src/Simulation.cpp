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

    const float emergencyRadius = std::max(0.0f, config_.herbivoreEmergencyFleeRadius);
    const float awarenessRadius = std::max(emergencyRadius, std::max(0.0f, config_.senseRadius));
    const float emergencyLimit = emergencyRadius * emergencyRadius;
    const float awarenessLimit = awarenessRadius * awarenessRadius;

    int emergencyPredatorIndex = -1;
    int hungryPredatorIndex = -1;
    float nearestEmergencyDistance = std::numeric_limits<float>::max();
    float nearestHungryDistance = std::numeric_limits<float>::max();

    for (std::size_t i = 0; i < animals_.size(); ++i) {
        const auto& predator = animals_[i];
        if (!predator.active || predator.species != Species::Carnivore) continue;

        const float distance = DistanceSquared(animal.position, predator.position);
        if (distance <= emergencyLimit && distance < nearestEmergencyDistance) {
            nearestEmergencyDistance = distance;
            emergencyPredatorIndex = static_cast<int>(i);
        }

        const bool predatorHungry = predator.energy <= config_.carnivoreHungryEnergy;
        if (predatorHungry && distance <= awarenessLimit && distance < nearestHungryDistance) {
            nearestHungryDistance = distance;
            hungryPredatorIndex = static_cast<int>(i);
        }
    }

    // A carnivore that is extremely close is always treated as a threat. At
    // longer range, only a hungry carnivore interrupts feeding or breeding so a
    // harmless nearby carnivore cannot keep herbivores pinned away from food.
    const int predatorIndex = emergencyPredatorIndex >= 0
        ? emergencyPredatorIndex
        : hungryPredatorIndex;
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
        int herbivorePopulation = 0;
        for (const auto& candidate : animals_) {
            if (candidate.active && candidate.species == Species::Herbivore) {
                ++herbivorePopulation;
            }
        }

        const int baseOnlyThreshold = std::max(
            0,
            std::min(
                config_.carnivoreBaseOnlyPreySensePopulation,
                config_.carnivoreReducedPreySensePopulation));
        const int reducedSenseThreshold = std::max(
            baseOnlyThreshold,
            std::max(
                config_.carnivoreBaseOnlyPreySensePopulation,
                config_.carnivoreReducedPreySensePopulation));

        float preySenseRadius = config_.carnivorePreySenseRadius;
        if (herbivorePopulation < baseOnlyThreshold) {
            // When prey is very scarce, keep the normal local search radius even
            // for critically hungry carnivores so the last herbivores are not
            // globally swept up by desperation sensing.
            preySenseRadius = config_.carnivorePreySenseRadius;
        } else if (herbivorePopulation < reducedSenseThreshold) {
            // At low prey density, allow the first desperation boost but cap the
            // search before the critical 160-radius sweep.
            if (animal.energy <= config_.carnivoreDesperateEnergy) {
                preySenseRadius = config_.carnivoreDesperatePreySenseRadius;
            }
        } else if (animal.energy <= config_.carnivoreCriticalEnergy) {
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
    constexpr int EscapeDirectionCount = 16;
    constexpr float ScoreEpsilon = 0.0001f;

    const float maxX = static_cast<float>(SimulationConfig::Width - 1);
    const float maxY = static_cast<float>(SimulationConfig::Height - 1);

    bool hasPredator = false;
    for (const auto& candidate : animals_) {
        if (candidate.active && candidate.species == Species::Carnivore) {
            hasPredator = true;
            break;
        }
    }

    // This should not normally happen because UpdateHerbivore calls this only
    // after detecting a carnivore, but keep the old single-target escape as a
    // safe fallback.
    if (!hasPredator) {
        const float dx = animal.position.x - target.x;
        const float dy = animal.position.y - target.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0001f) return;

        const float nextX = std::clamp(animal.position.x + dx / length * speed, 0.0f, maxX);
        const float nextY = std::clamp(animal.position.y + dy / length * speed, 0.0f, maxY);
        const float deltaX = nextX - animal.position.x;
        const float deltaY = nextY - animal.position.y;
        UpdateFacing(animal, deltaX);
        const float movedDistance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        animal.moving = movedDistance > ScoreEpsilon;
        animal.animationDistance += movedDistance;
        animal.position.x = nextX;
        animal.position.y = nextY;
        return;
    }

    bool foundCandidate = false;
    float bestNearestPredatorDistance = -1.0f;
    float bestTotalPredatorDistance = -1.0f;
    float bestDeltaX = 0.0f;
    float bestDeltaY = 0.0f;

    // Evaluate several legal escape directions. The primary score maximizes
    // the distance to the nearest carnivore, so fleeing from one predator does
    // not send the herbivore directly toward another one. Total distance is a
    // tie-breaker for symmetric situations.
    for (int directionIndex = 0; directionIndex < EscapeDirectionCount; ++directionIndex) {
        const float angle = 2.0f * Pi * static_cast<float>(directionIndex)
            / static_cast<float>(EscapeDirectionCount);
        const float deltaX = std::cos(angle) * speed;
        const float deltaY = std::sin(angle) * speed;
        const float candidateX = animal.position.x + deltaX;
        const float candidateY = animal.position.y + deltaY;

        if (candidateX < 0.0f || candidateX > maxX ||
            candidateY < 0.0f || candidateY > maxY) {
            continue;
        }

        const Vec2 candidatePosition{ candidateX, candidateY };
        float nearestPredatorDistance = std::numeric_limits<float>::max();
        float totalPredatorDistance = 0.0f;

        for (const auto& predator : animals_) {
            if (!predator.active || predator.species != Species::Carnivore) continue;

            const float distance = DistanceSquared(candidatePosition, predator.position);
            nearestPredatorDistance = std::min(nearestPredatorDistance, distance);
            totalPredatorDistance += distance;
        }

        const bool betterNearest =
            nearestPredatorDistance > bestNearestPredatorDistance + ScoreEpsilon;
        const bool sameNearest =
            std::abs(nearestPredatorDistance - bestNearestPredatorDistance) <= ScoreEpsilon;
        const bool betterTotal = totalPredatorDistance > bestTotalPredatorDistance;

        if (!foundCandidate || betterNearest || (sameNearest && betterTotal)) {
            foundCandidate = true;
            bestNearestPredatorDistance = nearestPredatorDistance;
            bestTotalPredatorDistance = totalPredatorDistance;
            bestDeltaX = deltaX;
            bestDeltaY = deltaY;
        }
    }

    if (!foundCandidate) {
        return;
    }

    UpdateFacing(animal, bestDeltaX);
    const float movedDistance = std::sqrt(bestDeltaX * bestDeltaX + bestDeltaY * bestDeltaY);
    animal.moving = movedDistance > ScoreEpsilon;
    animal.animationDistance += movedDistance;
    animal.position.x += bestDeltaX;
    animal.position.y += bestDeltaY;
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

int Simulation::CountGrassNear(const Vec2& position, float radius) const {
    const float safeRadius = std::max(0.0f, radius);
    const float limit = safeRadius * safeRadius;
    int count = 0;

    for (const auto& grass : grass_) {
        if (!grass.active) continue;
        if (DistanceSquared(position, grass.position) <= limit) {
            ++count;
        }
    }
    return count;
}

bool Simulation::TrySpawnAnimal(Species species, const Vec2& position, float initialEnergy) {
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
        const float maxEnergy = species == Species::Herbivore
            ? config_.herbivoreMaxEnergy
            : config_.carnivoreMaxEnergy;
        animal.energy = initialEnergy >= 0.0f
            ? std::clamp(initialEnergy, 0.0f, maxEnergy)
            : maxEnergy;

        if (species == Species::Herbivore) {
            animal.breedTarget = random_.Int(2, 4);
        } else {
            const int minBreedMeals = std::max(
                1,
                std::min(config_.carnivoreBreedMealsMin, config_.carnivoreBreedMealsMax));
            const int maxBreedMeals = std::max(
                minBreedMeals,
                std::max(config_.carnivoreBreedMealsMin, config_.carnivoreBreedMealsMax));
            animal.breedTarget = random_.Int(minBreedMeals, maxBreedMeals);
        }
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
    const int localLimit = std::max(1, config_.grassLocalDensityLimit);

    for (int i = 0; i < count; ++i) {
        Vec2 p{
            position.x + static_cast<float>(random_.Int(-24, 24)),
            position.y + static_cast<float>(random_.Int(-24, 24)),
        };
        p.x = std::clamp(p.x, 0.0f, static_cast<float>(SimulationConfig::Width - 1));
        p.y = std::clamp(p.y, 0.0f, static_cast<float>(SimulationConfig::Height - 1));

        if (CountGrassNear(p, config_.grassLocalDensityRadius) >= localLimit) {
            continue;
        }
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

    int configuredMin = config_.carnivoreOffspringMin;
    int configuredMax = config_.carnivoreOffspringMax;
    if (animal.species == Species::Herbivore) {
        const int herbivorePopulation = GetPopulation().herbivores;
        const int lowDensityThreshold = std::min(
            config_.herbivoreLowDensityThreshold,
            config_.herbivoreHighDensityThreshold);
        const int highDensityThreshold = std::max(
            config_.herbivoreLowDensityThreshold,
            config_.herbivoreHighDensityThreshold);

        if (herbivorePopulation < lowDensityThreshold) {
            configuredMin = config_.herbivoreLowDensityOffspringMin;
            configuredMax = config_.herbivoreLowDensityOffspringMax;
        } else if (herbivorePopulation < highDensityThreshold) {
            configuredMin = config_.herbivoreMidDensityOffspringMin;
            configuredMax = config_.herbivoreMidDensityOffspringMax;
        } else {
            configuredMin = config_.herbivoreHighDensityOffspringMin;
            configuredMax = config_.herbivoreHighDensityOffspringMax;
        }
    }

    const int minOffspring = std::max(0, std::min(configuredMin, configuredMax));
    const int maxOffspring = std::max(minOffspring, std::max(configuredMin, configuredMax));
    const int offspringTarget = random_.Int(minOffspring, maxOffspring);
    const float offspringInitialEnergy = animal.species == Species::Carnivore
        ? config_.carnivoreBirthEnergy
        : -1.0f;

    int spawnedOffspring = 0;
    for (int i = 0; i < offspringTarget; ++i) {
        if (!TrySpawnAnimal(animal.species, childPosition, offspringInitialEnergy)) break;
        ++spawnedOffspring;
    }

    if (spawnedOffspring > 0) {
        auto& speciesStats = animal.species == Species::Herbivore
            ? statistics_.herbivore
            : statistics_.carnivore;
        speciesStats.births += static_cast<unsigned long long>(spawnedOffspring);

        if (animal.species == Species::Carnivore) {
            const float breedingCost = std::max(0.0f, config_.carnivoreBreedingEnergyCost)
                * static_cast<float>(spawnedOffspring);
            animal.energy = std::max(0.0f, animal.energy - breedingCost);
        }
    }

    const bool herbivoreAttemptBlockedByCapacity =
        animal.species == Species::Herbivore && offspringTarget > 0 && spawnedOffspring == 0;
    if (spawnedOffspring > 0 || herbivoreAttemptBlockedByCapacity) {
        // A herbivore that reached a mate has spent this breeding opportunity
        // even when the population cap prevents offspring from being created.
        // This avoids accumulating a large queue of ready-to-breed animals that
        // instantly refill every slot opened by predation.
        animal.meals = 0;
        if (animal.species == Species::Herbivore) {
            animal.breedTarget = random_.Int(2, 4);
        } else {
            const int minBreedMeals = std::max(
                1,
                std::min(config_.carnivoreBreedMealsMin, config_.carnivoreBreedMealsMax));
            const int maxBreedMeals = std::max(
                minBreedMeals,
                std::max(config_.carnivoreBreedMealsMin, config_.carnivoreBreedMealsMax));
            animal.breedTarget = random_.Int(minBreedMeals, maxBreedMeals);
        }
        animal.state = LifeState::Normal;
    }
}

void Simulation::TryRegrowGrass() {
    if (config_.grassRegrowFrames <= 0 ||
        frame_ % static_cast<unsigned long long>(config_.grassRegrowFrames) != 0) {
        return;
    }
    if (GetPopulation().grass >= static_cast<int>(SimulationConfig::MaxGrass)) {
        return;
    }

    const int localLimit = std::max(1, config_.grassLocalDensityLimit);
    const int attempts = std::max(1, config_.grassRegrowAttempts);
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const Vec2 position = RandomPosition();
        if (CountGrassNear(position, config_.grassLocalDensityRadius) >= localLimit) {
            continue;
        }
        if (TrySpawnGrass(position)) {
            return;
        }
    }
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
