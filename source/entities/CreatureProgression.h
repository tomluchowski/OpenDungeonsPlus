#ifndef CREATUREPROGRESSION_H
#define CREATUREPROGRESSION_H

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace CreatureProgression
{
//! Combat growth coefficients and exponents for health, damage and accuracy.
static const double HP_GROWTH = 0.173;
static const double HP_EXPONENT = 1.37;
static const double DAMAGE_GROWTH = 0.278;
static const double DAMAGE_EXPONENT = 1.02;
static const double ATTACK_RATING_GROWTH = 0.847;
static const double ATTACK_RATING_EXPONENT = 1.13;
//! Highest creature level; the power curve is spread over the levels 1 to this value.
const uint32_t MAX_POWER_LEVEL = 30;

//! \brief Returns the creature health multiplier, clamped to levels 1 through 30.
inline double hpMultiplier(uint32_t level)
{
    return 1.0 + HP_GROWTH * std::pow(std::max(1u, std::min(MAX_POWER_LEVEL, level)) - 1, HP_EXPONENT);
}

//! \brief Returns the creature damage and defense multiplier, clamped to levels 1 through 30.
inline double damageMultiplier(uint32_t level)
{
    return 1.0 + DAMAGE_GROWTH * std::pow(std::max(1u, std::min(MAX_POWER_LEVEL, level)) - 1, DAMAGE_EXPONENT);
}

//! \brief Returns the attack rating multiplier, clamped to levels 1 through 30.
inline double attackRatingMultiplier(uint32_t level)
{
    return 1.0 + ATTACK_RATING_GROWTH * std::pow(std::max(1u, std::min(MAX_POWER_LEVEL, level)) - 1, ATTACK_RATING_EXPONENT);
}

//! \brief Returns the defense rating multiplier, clamped to levels 1 through 30.
inline double defenseRatingMultiplier(uint32_t level)
{
    return std::max(1u, std::min(MAX_POWER_LEVEL, level));
}

//! \brief Returns the larger of linear growth and the supplied combat curve at the clamped level.
//! \param multiplier The health, damage or rating multiplier to apply to the base stat.
inline double combatStat(double base, double perLevel, uint32_t level, double (*multiplier)(uint32_t))
{
    level = std::max(1u, std::min(MAX_POWER_LEVEL, level));
    return std::max(base + (level - 1) * perLevel, base * multiplier(level));
}

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
//! \return The cooldown in turns, possibly a fraction of a turn.
inline double cooldownAtLevel(uint32_t baseTurns, uint32_t level)
{
    if(baseTurns == 0)
        return 0.0;

    const double factor = 1.0 - COOLDOWN_REDUCTION_PER_LEVEL * (std::max(1u, level) - 1);
    return std::max(1.0, baseTurns * factor);
}
}

#endif
