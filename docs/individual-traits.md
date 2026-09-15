# Individual traits

Each animal has one total trait capacity. The capacity is split across five competing allocations:

- speed
- energy efficiency
- mate sensing
- mate acceptance
- longevity

The allocations are normalized to the same total capacity, so increasing one trait necessarily leaves less budget for the others. Trait effects use diminishing returns.

Only part of the trait budget is expressed at birth. Expression gradually approaches the full allocation with age. Higher-capacity animals mature slightly more slowly and also pay a small energy burden, so capacity is not intended to be a pure free upgrade.

Current behavior effects:

- speed changes all movement speeds
- efficiency changes passive energy consumption
- high speed adds a small extra energy burden
- mate sensing changes breeding-partner search radius
- mate acceptance gives the contacted partner a chance to reject breeding, followed by a retry cooldown
- longevity moderately scales the species' existing randomized lifespan

## Inheritance

Initial animals receive a random capacity and random trait allocation.

Offspring inherit both parents instead of receiving a completely new random profile:

1. A parent blend ratio is chosen between 35% and 65%, so neither parent normally dominates completely.
2. Capacity is blended from both parents.
3. Each trait is inherited as a blend of the parents' allocation proportions rather than raw point values.
4. Capacity has a 25% chance of receiving a small mutation of up to +/-2 points.
5. Each trait allocation independently has a 20% chance of receiving a multiplicative mutation of up to +/-8%.
6. The five allocations are normalized again so their sum exactly matches the child's capacity.

Capacity remains clamped to the configured initial capacity range (90-110 by default). This keeps the first inheritance stage close to the already-tested ecosystem balance while still allowing selection to move the population distribution inside that range.

Siblings independently sample their blend and mutation, so offspring from the same parents are similar but not identical.
