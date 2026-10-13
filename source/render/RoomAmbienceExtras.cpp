/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "render/RoomAmbienceExtras.h"

#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "game/HeartHealthRing.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/ODClient.h"
#include "render/RoomAmbience.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace
{

//! The mesh of the dungeon heart
static const std::string HEART_MESH = "DungeonTempleObject";
//! The beat of a heart is this much faster for every unit of health it lacks (a heart at no health beats 1 + this
//! as fast as a calm one) ...
const double HEART_RATE_HURT_FACTOR = 1.6;
//! ... and this much faster again while it is attacked
const double HEART_RATE_ATTACK_FACTOR = 0.3;
//! Added to a coordinate before it is rounded down to the tile it lies on
const double TILE_ROUNDING = 0.5;

//! \brief The tile coordinate that a position coordinate lies on
int32_t toTileCoordinate(double value)
{
    return static_cast<int32_t>(std::floor(value + TILE_ROUNDING));
}

} // namespace

void RoomAmbienceExtras::scan(RoomAmbience& ambience, GameMap* gameMap)
{
    if(gameMap == nullptr)
        return;

    const Player* localPlayer = gameMap->getLocalPlayer();
    const Seat* localSeat = (localPlayer != nullptr) ? localPlayer->getSeat() : nullptr;
    ambience.clearHeartRates();
    if((localSeat == nullptr) || (ODClient::getSingletonPtr() == nullptr))
        return;

    for(RenderedMovableEntity* entity : gameMap->getRenderedMovableEntities())
    {
        if(entity->getMeshName() != HEART_MESH)
            continue;

        // Only the health of the own heart is known to the client, a foreign heart beats calmly
        const Ogre::Vector3& position = entity->getPosition();
        Tile* tile = gameMap->getTile(toTileCoordinate(position.x), toTileCoordinate(position.y));
        if((tile == nullptr) || (tile->getSeat() != localSeat))
            continue;

        // The beat gets faster the more the heart is hurt, and a bit faster again while it is attacked
        HeartHealthRing::BadgeState& badge = ODClient::getSingleton().getHeartBadge();
        double factor = 1.0 + HEART_RATE_HURT_FACTOR * (1.0 - static_cast<double>(badge.mFraction));
        if(badge.mGlow)
            factor += HEART_RATE_ATTACK_FACTOR;
        ambience.addHeartRate(position, factor);
    }
}
