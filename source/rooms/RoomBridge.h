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

#ifndef ROOMBRIDGE_H
#define ROOMBRIDGE_H

#include "rooms/Room.h"
#include "rooms/RoomManager.h"

class Tile;

enum class TileVisual;

class BridgeRoomFactory : public RoomFactory
{
protected:
    void checkBuildBridge(RoomType type, GameMap* gameMap, Seat* seat, const InputManager& inputManager,
        InputCommand& inputCommand, const std::vector<TileVisual>& allowedTilesVisual, bool isEditor) const;
    bool readBridgeFromPacket(std::vector<Tile*>& tiles, GameMap* gameMap, Seat* seat,
        const std::vector<TileVisual>& allowedTilesVisual, ODPacket& packet, bool isEditor) const;
};

//! \brief Per tile state of a bridge: how much dancing an enemy worker still has
//! to do on this tile before it changes hands.
class BridgeTileData : public TileData
{
public:
    BridgeTileData() :
        TileData(),
        mClaimedValue(0.0)
    {}

    BridgeTileData(const BridgeTileData* tileData) :
        TileData(tileData),
        mClaimedValue(tileData->mClaimedValue)
    {}

    virtual ~BridgeTileData()
    {}

    virtual BridgeTileData* cloneTileData() const override
    { return new BridgeTileData(this); }

    double mClaimedValue;
};

//! \brief this class is a convenience class that handle shared bridges behaviour. It
//! is designed to be extended
class RoomBridge: public Room
{
public:
    RoomBridge(GameMap* gameMap);

    virtual RoomType getType() const override = 0;

    virtual bool isBridge() const override
    { return true; }

    virtual bool displayTileMesh() const override
    { return true; }

    virtual bool colorCustomMesh() const override
    { return true; }

    virtual void setupRoom(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles) override;
    virtual void restoreInitialEntityState() override;

    //! \brief Bridges are claimable by enemies
    virtual bool isAttackable(Tile* tile, Seat* seat) const override
    { return false; }
    virtual bool isClaimable(Seat* seat) const override;
    virtual void claimForSeat(Seat* seat, Tile* tile, double danceRate) override;
    virtual double getCreatureSpeed(const Creature* creature, Tile* tile) const override;

    virtual bool removeCoveredTile(Tile* t) override;

protected:
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

    virtual BridgeTileData* createTileData(Tile* tile) override;

    virtual void updateFloodFillPathCreated(Seat* seat, const std::vector<Tile*>& tiles);
    virtual void updateFloodFillTileRemoved(Seat* seat, Tile* tile) = 0;

private:
    //! \brief Hands the given tile of this bridge over to a bridge owned by the
    //! claiming seat, merging it with an adjacent bridge of theirs when there is
    //! one and splitting this bridge when the loss cuts it in two.
    void claimTileForSeat(Seat* seat, Tile* tile);

    //! \brief The stream format keeps one claim value for the whole bridge, so old
    //! saves and level files load unchanged. It is read into this field and shared
    //! out over the tiles in restoreInitialEntityState(); the live value is per
    //! tile, in BridgeTileData.
    double mClaimedValue;
};

#endif // ROOMBRIDGE_H
