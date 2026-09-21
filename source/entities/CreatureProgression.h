#ifndef CREATUREPROGRESSION_H
#define CREATUREPROGRESSION_H

#include <algorithm>
#include <cstdint>

namespace CreatureProgression
{
// Spread the power anchors over all thirty levels without moving skill unlocks.
inline double powerMultiplier(uint32_t level)
{
    static const double anchors[] = {1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0, 4.0, 6.0};
    const double position = (std::max(1u, std::min(30u, level)) - 1) * 9.0 / 29.0;
    const uint32_t index = static_cast<uint32_t>(position);
    if(index == 9)
        return anchors[9];
    return anchors[index] + (anchors[index + 1] - anchors[index]) * (position - index);
}

inline double stat(double base, double perLevel, uint32_t level)
{
    // Retain explicitly configured growth above the shared power curve.
    return std::max(base + (std::max(1u, level) - 1) * perLevel,
        base * powerMultiplier(level));
}
}

#endif
