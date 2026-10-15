#ifndef ROOMOBJECTSTEP_H
#define ROOMOBJECTSTEP_H

#include "gamemap/RoomObjectBounds.h"
#include "gamemap/RoomObjectPath.h"

namespace RoomObjectPath
{
// Only the completely measured low nest is traversable. Tall posts and
// unknown furniture retain infinite height and cannot enter this path.
inline float prepareLowStep(Obstacle& obstacle, const std::string& mesh,
    float scale, float groundZ)
{
    const float height = obstacle.maximumHeight - groundZ;
    if(height <= 0.0f || height > lowWalkingHeight * 1.02f || height > lowWalkingHeight * scale)
        return 0.0f;
    for(const LowWalkingBounds& body : lowWalkingBounds)
        if(mesh == body.name && !body.empty)
        {
            obstacle.bodyMinimum = Ogre::Vector2(body.minX - lowWalkingMargin, body.minY - lowWalkingMargin) * scale;
            obstacle.bodyMaximum = Ogre::Vector2(body.maxX + lowWalkingMargin, body.maxY + lowWalkingMargin) * scale;
            return height - (body.minZ - lowWalkingMargin) * scale;
        }
    return 0.0f;
}

inline float lowStepElevation(const Obstacle& obstacle, Ogre::Vector2 position,
    Ogre::Vector2 direction, float rise)
{
    if(rise <= 0.0f)
        return 0.0f;
    if(direction.squaredLength() < 0.000001f)
        direction = obstacle.initialHeading;
    direction.normalise();
    const Obstacle body = obstacle.forHeading(direction);
    if(body.contains(position))
        return rise;
    float distance = rise;
    // Lift before toes enter and lower only after the trailing foot leaves.
    // A parallel, physically clear lane does not trigger either intersection.
    for(float sign : {-1.0f, 1.0f})
    {
        const Ogre::Vector2 reach = direction * (sign * rise);
        if(!body.intersects(position, position + reach))
            continue;
        float low = 0.0f, high = 1.0f;
        for(int i = 0; i < 12; ++i)
        {
            const float middle = (low + high) * 0.5f;
            if(body.intersects(position, position + reach * middle))
                high = middle;
            else
                low = middle;
        }
        distance = std::min(distance, rise * low);
    }
    const float progress = 1.0f - distance / rise;
    return rise * progress * progress * (3.0f - 2.0f * progress);
}
}

#endif
