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

#ifndef TRAPDOOR_H
#define TRAPDOOR_H

#include "Trap.h"
#include "traps/TrapType.h"

class DoorEntity;

class TrapDoor : public Trap
{
public:
    //! \brief Creates a door of the given type.
    //! \param doorType One of the door trap types (wooden, ironbound, steel, barricade, secret or runed).
    TrapDoor(GameMap* gameMap, TrapType doorType);

    static const std::string ANIMATION_OPEN;
    static const std::string ANIMATION_CLOSE;

    //! \brief The type of this door.
    const TrapType getType() const override
    { return mDoorType; }

    //! \brief Health given to each tile of a newly built door, depending on the door type.
    double getDefaultTileHP() const override;

    bool isDoor() const override
    { return true; }

    //! \brief Doors return true to make sure every creature with vision on the door tile can see it.
    //! A secret door is only seen when a creature of its owner stands on it. A runed door also burns
    //! the enemies standing on it and then has to recharge.
    bool shoot(Tile* tile) override;

    //! \brief The runed door pays its mana itself when it fires (see shoot), not on every upkeep.
    double getManaToFire() const override
    { return 0.0; }

    void doUpkeep() override;

    double getHP(Tile* tile) const override;
    double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage, double magicalDamage,
        double elementDamage, Tile* tileTakingDamage, bool ko) override;

    bool displayTileMesh() const override
    { return true; }

    //! \brief The trap object covers the whole tile under
    //! but while it built, the ground tile still must be shown.
    bool shouldDisplayGroundTile() const override
    { return true; }

    void notifyDoorSlapped(DoorEntity* doorEntity, Tile* tile);

    TrapEntity* getTrapEntity(Tile* tile) override;

    double getCreatureSpeed(const Creature* creature, Tile* tile) const override;

    bool permitsVision(Tile* tile) override;

    //! \brief A secret door looks like a wall to every seat that has not seen a creature of the owner
    //! (or of an ally) pass through it. All other doors are always seen as doors.
    bool appearsAsWallForSeat(Tile* tile, Seat* seat) const override;

    //! Returns true if tiles North and South (or east and west) are suitable to have a door on the
    //! given tile
    static bool canDoorBeOnTile(GameMap* gameMap, Tile* tile);

    static bool buildTrapOnTile(GameMap* gameMap, Player* player, Tile* tile);

protected:
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:
    //! \brief Type of this door (wooden, ironbound, steel, barricade, secret or runed)
    TrapType mDoorType;
    //! \brief Wanted state for the door (changes when the player slaps the door)
    bool mIsLocked;
    //! \brief Current state of the door
    bool mIsLockedState;
    //! \brief Turns left before a runed door can fire again
    uint32_t mFireCooldownTurns;

    void changeDoorState(DoorEntity* doorEntity, Tile* tile, bool locked);
};

#endif // TRAPDOOR_H
