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

#ifndef ROOMDUNGEONTEMPLE_H
#define ROOMDUNGEONTEMPLE_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

enum class TileVisual;

class RoomDungeonTemple: public Room
{
public:
    RoomDungeonTemple(GameMap* gameMap);

    virtual RoomType getType() const override
    { return mRoomType; }

    //! \brief Updates the temple position when in editor mode.
    void updateActiveSpots(GameMap* gameMap = nullptr) override;

    //! \brief The dungeon heart cannot be sold.
    bool canSeatSellBuilding(Seat* seat) const override
    { return false; }

    //! \brief The floor of the heart is never a combat target. Enemies attack the heart object, see canAttackHeart.
    bool isAttackable(Tile* tile, Seat* seat) const override
    { return false; }

    //! \brief Whether the seat can attack the heart on the given tile: only seats that are not allied with the
    //! owner, only on the tile of the heart object and only while the heart has health left.
    bool canAttackHeart(Tile* tile, Seat* seat) const;

    //! \brief The remaining health of the heart. Until the heart is hit, or when an old save has no
    //! health record of the heart, this is the remaining health of the floor tiles.
    double getHP(Tile* tile) const override;

    //! \brief Damage to the floor is ignored, the heart takes damage through takeHeartDamage.
    double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage,
        double magicalDamage, double elementDamage, Tile* tileTakingDamage, bool ko) override
    { return 0.0; }

    //! \brief Reduces the health of the heart by the damage that gets through its defenses.
    //! \return The damage that was dealt. 0 if the attacker may not attack the heart.
    double takeHeartDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage,
        double magicalDamage, double elementDamage, Tile* tileTakingDamage);

    //! \brief Outside the editor, the floor of a heart that still has health cannot be removed.
    bool removeCoveredTile(Tile* tile) override;

    //! \brief Releases the floor once the heart has no health left, then runs the room upkeep.
    void doUpkeep() override;

    //! \brief Saves the room and, outside the editor, the health of the heart.
    void exportToStream(std::ostream& os) const override;

    //! \brief Loads the room and the health of the heart. Old saves without a record of the heart health
    //! keep their remaining durability.
    bool importFromStream(std::istream& is) override;

    void checkForSplit() override
    {
        // Damaged floor must not create another dungeon core. Keep the original
        // room and persistent object until the whole temple is destroyed.
    }

    bool hasCarryEntitySpot(GameEntity* carriedEntity) override;
    Tile* askSpotForCarriedEntity(GameEntity* carriedEntity) override;
    void notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity) override;

    virtual void restoreInitialEntityState() override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;
    
protected:
    virtual void destroyMeshLocal(NodeType nt = NodeType::MTILES_NODE) override;

    void notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile) override
    {
        // This Room keeps its building object until it is destroyed (they will be released when
        // the room is destroyed)
    }

private:
    //! \brief The reference of the temple object
    BuildingObject* mTempleObject;

    //! One health pool for the heart, independent of individual floor tiles.
    double mHeartHP;

    //! \brief Updates the temple mesh position.
    void updateTemplePosition();
};

#endif // ROOMDUNGEONTEMPLE_H
