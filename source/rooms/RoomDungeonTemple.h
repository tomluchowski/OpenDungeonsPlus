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

#include "rooms/HeartHealthTier.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
// The treasury tile data is a complete type here so the covariant createTileData() override
// can name it.
#include "rooms/RoomTreasury.h"

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
    //! \brief The tile the heart object stands on, or null when the heart has no object (ruin).
    Tile* getHeartTile() const;
    //! Health of an undamaged heart: HEART_MAX_HP, independent of the number of tiles of the
    //! room and of the floor tiles' own durability.
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

    //! \brief The outer tiles of a 5x5 heart form a treasury ring, 1000 gold each.
    //! A 3x3 heart has no ring and stores no gold.
    virtual int getTotalGoldStorage() const override;
    virtual int getTotalGoldStored() const override;
    virtual int depositGold(int gold, Tile* tile) override;
    virtual int withdrawGold(int gold) override;

    void checkForSplit() override
    {
        // Damaged floor must not create another dungeon core. Keep the original
        // room and persistent object until the whole temple is destroyed.
    }

    //! \brief The gold counted by the temple is the gold in every ring tile it has data for,
    //! which includes the tiles it has handed over, so it has to let go of theirs.
    void splitRoom(Room& newRoom, const std::vector<Tile*>& tiles) override;

    bool hasCarryEntitySpot(GameEntity* carriedEntity) override;
    Tile* askSpotForCarriedEntity(GameEntity* carriedEntity) override;
    void notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity) override;

    virtual void restoreInitialEntityState() override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;
    
protected:
    virtual void destroyMeshLocal(NodeType nt = NodeType::MTILES_NODE) override;

    //! \brief Ring tiles carry the treasury gold data, like the tiles of a treasury room.
    RoomTreasuryTileData* createTileData(Tile* tile) override;

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
    //! Health of an undamaged heart
    static const double HEART_MAX_HP;
    //! Health a living, damaged heart regains per second
    static const double HEART_HEAL_PER_SECOND;

    //! True once the critical-health warning was sent to the owner. Not saved: a reloaded
    //! game with an already critical heart warns once again at the next hit.
    bool mCriticalWarningSent;

    //! \brief The heart health tier the currently displayed mTempleObject was built for.
    HeartHealthTier mCurrentHeartTier;

    //! \brief Updates the temple mesh position.
    void updateTemplePosition();

    //! \brief The heart object tile, falling back to the room centre for tiles placed before
    //! the heart object existed.
    Tile* getRingCenterTile() const;
    //! \brief A covered tile outside the 3x3 core is part of the treasury ring.
    bool isTreasuryTile(Tile* tile) const;
    void updateTreasuryMeshesForTile(Tile* tile, RoomTreasuryTileData* roomTreasuryTileData);

    //! True when the gold of a ring tile changed and its mesh needs a refresh in doUpkeep().
    bool mGoldChanged;

    //! \brief Computes the heart's current health tier from its health fraction.
    HeartHealthTier computeHeartHealthTier() const;

    //! \brief Returns the mesh name to use for the given heart health tier.
    static const std::string& getMeshNameForHeartTier(HeartHealthTier tier);

    //! \brief Rebuilds the temple object if the heart's health tier changed since
    //! the last check.
    void checkHeartHealthTier();
};

#endif // ROOMDUNGEONTEMPLE_H
