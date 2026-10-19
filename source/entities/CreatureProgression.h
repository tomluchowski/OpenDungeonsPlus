#ifndef CREATUREPROGRESSION_H
#define CREATUREPROGRESSION_H

#include <algorithm>
#include <cstdint>

namespace CreatureProgression
{
//! Highest creature level; the power curve is spread over the levels 1 to this value.
const uint32_t MAX_POWER_LEVEL = 30;

//! Returns the factor by which a creature's base power grows at the given level: 1 at level 1 and
//! 6 at the highest level. The levels are clamped to 1..MAX_POWER_LEVEL.
inline double powerMultiplier(uint32_t level)
{
    // Power factors at evenly spaced levels; the levels in between are interpolated linearly.
    static const double anchors[] = {1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0, 4.0, 6.0};
    const uint32_t lastAnchor = static_cast<uint32_t>(sizeof(anchors) / sizeof(anchors[0])) - 1;
    const double position = (std::max(1u, std::min(MAX_POWER_LEVEL, level)) - 1) * static_cast<double>(lastAnchor) /
        (MAX_POWER_LEVEL - 1);
    const uint32_t index = static_cast<uint32_t>(position);
    if(index == lastAnchor)
        return anchors[lastAnchor];
    return anchors[index] + (anchors[index + 1] - anchors[index]) * (position - index);
}

//! Returns a stat at the given level: the larger of the configured linear growth
//! (base + perLevel for every level above 1) and the base scaled by the shared power curve.
inline double stat(double base, double perLevel, uint32_t level)
{
    // Retain explicitly configured growth above the shared power curve.
    return std::max(base + (std::max(1u, level) - 1) * perLevel,
        base * powerMultiplier(level));
}

//! Share of the cooldown that is saved per level above 1 (0.75 percent).
const double COOLDOWN_REDUCTION_PER_LEVEL = 0.0075;

//! Returns the cooldown of an attack at the given level: the base cooldown times
//! 1 - 0.0075 x (level - 1), not rounded to whole turns, but never below 1 turn.
//! A base cooldown of 0 stays 0.
//! \param baseTurns The cooldown at level 1 in turns.
//! \param level The creature level.
inline double cooldownAtLevel(uint32_t baseTurns, uint32_t level)
{
    if(baseTurns == 0)
        return 0.0;

    const double factor = 1.0 - COOLDOWN_REDUCTION_PER_LEVEL * (std::max(1u, level) - 1);
    return std::max(1.0, baseTurns * factor);
}
}

#endif
