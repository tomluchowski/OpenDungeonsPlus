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

#include "entities/Creature.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "game/HeartHealthRing.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/ODClient.h"
#include "render/RoomAmbience.h"

#include <cmath>

namespace
{

//! The mesh of the dungeon heart
static const std::string HEART_MESH = "DungeonTempleObject";
//! The tile visual of a temple
static const std::string TEMPLE_VISUAL = "templeRoom";
//! Seconds before a creature can ring the temple bell or flash again
const double CREATURE_EVENT_COOLDOWN = 4.0;
//! The beat of a heart is this much faster for every unit of health it lacks (a heart at no health beats 1 + this
//! as fast as a calm one) ...
const double HEART_RATE_HURT_FACTOR = 1.6;
//! ... and this much faster again while it is attacked
const double HEART_RATE_ATTACK_FACTOR = 0.3;
//! Below this heart health fraction the heart shows its wounds
const double HEART_HURT_FRACTION = 0.5;
//! Below this part of the heart health every beat is also heard (the flare starts at the same limit)
const double HEART_LOW_FRACTION = 0.35;
//! Seconds between two beats of a hurt heart: this much for a heart with no health left ...
const double HEART_BEAT_PAUSE_MIN = 0.7;
//! ... plus this much for every unit of health it has
const double HEART_BEAT_PAUSE_PER_HEALTH = 0.8;
//! Added to a coordinate before it is rounded down to the tile it lies on
const double TILE_ROUNDING = 0.5;

//! \brief The tile coordinate that a position coordinate lies on
int32_t toTileCoordinate(double value)
{
    return static_cast<int32_t>(std::floor(value + TILE_ROUNDING));
}

} // namespace

RoomAmbienceExtras::RoomAmbienceExtras() :
    mNextHeartBeat(0.0),
    mGeneration(0),
    mInitialized(false)
{
}

void RoomAmbienceExtras::reset()
{
    mCreatures.clear();
    mNextHeartBeat = 0.0;
    mInitialized = false;
}

bool RoomAmbienceExtras::isTempleTile(GameMap* gameMap, const Ogre::Vector3& position) const
{
    Tile* tile = gameMap->getTile(toTileCoordinate(position.x), toTileCoordinate(position.y));
    if(tile == nullptr)
        return false;

    return Tile::tileVisualToString(tile->getTileVisual()) == TEMPLE_VISUAL;
}

void RoomAmbienceExtras::scan(RoomAmbience& ambience, GameMap* gameMap, double clock)
{
    if(gameMap == nullptr)
        return;

    ++mGeneration;
    scanCreatures(ambience, gameMap, clock);
    scanHearts(ambience, gameMap, clock);
    mInitialized = true;
}

void RoomAmbienceExtras::scanCreatures(RoomAmbience& ambience, GameMap* gameMap, double clock)
{
    for(Creature* creature : gameMap->getCreatures())
    {
        if(!creature->getIsOnMap())
            continue;

        uint32_t health = creature->getOverlayHealthValue();
        bool prisoner = (creature->getSeatPrison() != nullptr);
        const Seat* seat = creature->getSeat();
        std::map<std::string, CreatureSnapshot>::iterator it = mCreatures.find(creature->getName());
        if(it == mCreatures.end())
        {
            // A creature seen for the first time only teaches its state
            CreatureSnapshot snapshot;
            snapshot.mHealth = health;
            snapshot.mSeat = seat;
            snapshot.mPrisoner = prisoner;
            snapshot.mGeneration = mGeneration;
            mCreatures.insert(std::make_pair(creature->getName(), snapshot));
            continue;
        }

        CreatureSnapshot& snapshot = it->second;
        snapshot.mGeneration = mGeneration;
        if(mInitialized && ((clock - snapshot.mLastEvent) >= CREATURE_EVENT_COOLDOWN))
        {
            const Ogre::Vector3& position = creature->getPosition();
            if((health < snapshot.mHealth) && isTempleTile(gameMap, position))
            {
                // Getting better in the temple: a bell rings and a ring of light spreads
                snapshot.mLastEvent = clock;
                ambience.triggerEvent("TempleHealed", position, false, TEMPLE_VISUAL);
            }
            else if(snapshot.mPrisoner && !prisoner && (seat != snapshot.mSeat))
            {
                // A prisoner now serves another keeper: it was converted
                snapshot.mLastEvent = clock;
                ambience.triggerEvent("ConvertedFlash", position, false);
            }
        }

        snapshot.mHealth = health;
        snapshot.mSeat = seat;
        snapshot.mPrisoner = prisoner;
    }

    for(std::map<std::string, CreatureSnapshot>::iterator it = mCreatures.begin(); it != mCreatures.end();)
    {
        if(it->second.mGeneration == mGeneration)
            ++it;
        else
            mCreatures.erase(it++);
    }
}

void RoomAmbienceExtras::scanHearts(RoomAmbience& ambience, GameMap* gameMap, double clock)
{
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
        double fraction = static_cast<double>(badge.mFraction);
        double factor = 1.0 + HEART_RATE_HURT_FACTOR * (1.0 - fraction);
        if(badge.mGlow)
            factor += HEART_RATE_ATTACK_FACTOR;
        ambience.addHeartRate(position, factor);

        // A hurt heart pulses red with every beat, a nearly dead one is also heard
        if(mInitialized && (fraction < HEART_HURT_FRACTION) && (clock >= mNextHeartBeat))
        {
            mNextHeartBeat = clock + HEART_BEAT_PAUSE_MIN + HEART_BEAT_PAUSE_PER_HEALTH * fraction;
            ambience.triggerEvent("HeartHurt", position, false);
            if(fraction < HEART_LOW_FRACTION)
                ambience.triggerEvent("HeartLowBeat", position, false);
        }
    }
}
