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

#ifndef ROOMTEMPLE_H
#define ROOMTEMPLE_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

#include <map>
#include <string>
#include <vector>

class Creature;
class Tile;
enum class TileVisual;

//! \brief A room where creatures pray. Each praying creature adds mana to its keeper and
//! feels better afterwards. The inner tiles of the room (tiles with the room on all eight
//! sides) form the pool: a creature dropped there is sacrificed.
class RoomTemple: public Room
{
public:
    RoomTemple(GameMap* gameMap);

    RoomType getType() const override
    { return mRoomType; }

    //! \brief One praying creature per outer tile, the pool is not used for praying
    bool hasOpenCreatureSpot(Creature* c) override;
    void removeCreatureUsingRoom(Creature* c) override;
    bool useRoom(Creature& creature, bool forced) override;

    //! \brief Praying relieves bad mood, so even unhappy creatures keep praying
    bool shouldNotUseIfBadMood(Creature& creature, bool forced) override
    { return false; }

    void creatureDropped(Creature& creature) override;
    void absorbRoom(Room* r) override;
    void doUpkeep() override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;

private:
    //! \brief Returns the tile where the given creature prays, picking a free one if needed
    Tile* getPrayerSpotForCreature(Creature& creature);

    //! \brief True if the tile is part of the pool (needs the room on all eight sides)
    bool isPoolTile(const Tile& tile) const;

    //! \brief The room tiles that are not part of the pool
    std::vector<Tile*> getOuterTiles() const;

    //! \brief Sacrifices the creature: it is removed and put in the queue of the last
    //! sacrifices. The first recipe that matches the end of the queue gives its result
    void sacrificeCreature(Creature& creature);

    //! \brief Gives the result of a recipe to the owner of the room
    void giveSacrificeResult(const std::string& result, uint32_t averageLevel, Tile& tile);

    std::map<Creature*, Tile*> mPrayerSpots;

    //! \brief Names of the creatures dropped in the pool. They are sacrificed during the next upkeep
    std::vector<std::string> mCreaturesToSacrifice;

    //! \brief The last sacrificed creatures (at most three, oldest first): definition name and level.
    //! This is not saved: after loading a game the queue is empty.
    std::vector<std::pair<std::string, uint32_t> > mSacrificed;

    //! \brief Prayer mana below one point, kept for the next prayer turn. Not saved.
    double mPrayerManaPending;
};

#endif // ROOMTEMPLE_H
