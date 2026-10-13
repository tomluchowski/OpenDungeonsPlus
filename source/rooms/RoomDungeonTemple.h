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

    bool canSeatSellBuilding(Seat* seat) const override
    { return false; }
    bool isAttackable(Tile* tile, Seat* seat) const override
    { return false; }
    bool canAttackHeart(Tile* tile, Seat* seat) const;
    double getHP(Tile* tile) const override;
    //! Health of an undamaged heart: HEART_HP_PER_TILE for every tile of the room
    //! (90000 for the usual 3 by 3 heart), independent of the floor tiles' own durability.
    double getHeartMaxHP() const;
    //! \brief Remaining heart health divided by getHeartMaxHP(), from 0 to 1.
    double getHeartHealthFraction() const;
    double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage,
        double magicalDamage, double elementDamage, Tile* tileTakingDamage, bool ko) override
    { return 0.0; }
    double takeHeartDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage,
        double magicalDamage, double elementDamage, Tile* tileTakingDamage);
    bool removeCoveredTile(Tile* tile) override;
    void doUpkeep() override;
    void exportToStream(std::ostream& os) const override;
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
    //! Health of the heart per room tile
    static const double HEART_HP_PER_TILE;

    //! True once the critical-health warning was sent to the owner. Not saved: a reloaded
    //! game with an already critical heart warns once again at the next hit.
    bool mCriticalWarningSent;

    //! \brief Updates the temple mesh position.
    void updateTemplePosition();
};

#endif // ROOMDUNGEONTEMPLE_H
