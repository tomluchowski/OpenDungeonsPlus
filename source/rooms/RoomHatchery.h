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

#ifndef ROOMHATCHERY_H
#define ROOMHATCHERY_H

#include "entities/ChickenEntity.h"
#include "rooms/HatcheryCycle.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"

class Creature;
enum class TileVisual;

class RoomHatchery: public Room
{
public:
    RoomHatchery(GameMap* gameMap);

    ~RoomHatchery()
    {}

    RoomType getType() const override
    { return mRoomType; }

    void doUpkeep() override;
    bool hasOpenCreatureSpot(Creature* c) override;
    bool shouldStopUseIfHungrySleepy(Creature& creature, bool forced) override
    { return false; }
    bool shouldNotUseIfBadMood(Creature& creature, bool forced) override
    { return false; }

    bool useRoom(Creature& creature, bool forced) override;
    void handleCreatureUsingAbsorbedRoom(Creature& creature) override;

    void creatureDropped(Creature& creature) override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;
    
protected:
    BuildingObject* notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile) override;
    void notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile) override;
private:
    //! Settings of the life cycle from the config, laying times scaled by the research.
    HatcheryCycleSettings getCycleSettings() const;
    //! Creates a hatchery animal at the given position.
    ChickenEntity* spawnAnimal(ChickenKind kind, const Ogre::Vector3& position, const HatcheryCycleSettings& settings);
    //! Lets a hen or a rooster come out of a coop. Returns false if no coop has a free place.
    bool spawnFromCoop(ChickenKind kind, const HatcheryCycleSettings& settings);

    //! Turns the hatchery has been empty (no hen, chick or egg)
    uint32_t mCoopHenWait;
    //! Turns the hatchery has been without rooster
    uint32_t mCoopRoosterWait;
};

#endif // ROOMHATCHERY_H
