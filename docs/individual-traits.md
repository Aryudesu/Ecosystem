# Individual traits (initial stage)

This stage introduces per-animal variation without inheritance yet.

Each animal receives one total trait capacity at birth. The capacity is split across five competing allocations:

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

Offspring currently receive a fresh random trait profile. Parent-to-child inheritance and mutation are intentionally deferred to the next stage so individual variation can be balanced first.
