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

#ifndef ROOM_H
#define ROOM_H

#include "entities/Building.h"

#include <string>
#include <iosfwd>

class BuildingObject;
class GameMap;
class InputCommand;
class InputManager;
class ODPacket;
class Seat;

enum class RoomType;

class Room : public Building
{
public:
    // When room types are added to this enum they also need to be added to the switch statements in Room.cpp.

    // Constructors and operators
    Room(GameMap* gameMap);
    virtual ~Room()
    {}

    virtual GameEntityType getObjectType() const override;

    virtual bool isBridge() const
    { return false; }

    virtual void addToGameMap(GameMap* gameMap = nullptr) override;
    virtual void removeFromGameMap(GameMap* gameMap = nullptr) override;

    virtual void absorbRoom(Room* r);

    //! \brief By default, we consider that creatures using the room are working and
    //! should be forced to work in the new room (if possible). If not, this function
    //! should be overriden
    virtual void handleCreatureUsingAbsorbedRoom(Creature& creature);

    //! \brief Called on the room the tiles are leaving once they have been handed over, so
    //! that a room keeping something for the room as a whole, rather than per tile, can give
    //! the new one its share. Anything held in the tile data moves with the tile on its own.
    virtual void splitRoom(Room& newRoom, const std::vector<Tile*>& tiles)
    {}

    static std::string getRoomStreamFormat();

    virtual RoomType getType() const = 0;

    //! \brief What enemies can do to a room, read from RoomsClaimableByEnemies
    //! in the room configuration file.
    enum class ClaimMode
    {
        //! Rooms cannot be claimed, only destroyed (the historical behaviour).
        destructibleOnly = 0,
        //! Workers can dance room tiles away and fighters can still destroy them.
        claimableAndDestructible = 1,
        //! Only workers can take a room, all of it at once; fighters leave rooms alone.
        claimableOnly = 2
    };

    //! \brief The RoomsClaimableByEnemies value of the room configuration file.
    //! Anything unknown counts as destructibleOnly.
    static ClaimMode getClaimMode();

    //! \brief Rooms can be taken over by enemy workers when the RoomsClaimableByEnemies
    //! switch is set in the room configuration file. The whole room has one health
    //! pool (mClaimHealth, a fraction of tiles times RoomConvertSecondsPerTile) that
    //! every dance on any of its tiles lowers; when it is empty all the tiles change
    //! hands at once (changeOwner). The dungeon temple is
    //! never claimable: it can only be destroyed. A bridge is one room like any other
    //! (all its squares together make the pool); it only scales the dance by the research
    //! of its owner and decides on its own who may claim it.
    virtual bool isClaimable(Seat* seat) const override;
    virtual void claimForSeat(Seat* seat, Tile* tile, double danceRate) override;

    //! \brief Server only. True when the seat may not dance on the tile of this
    //! enemy room now: a defender that is no worker stands within
    //! RoomTakeoverGuardRadius tiles of the tile, or the seat cannot pay the price.
    //! Every kind of room can be taken over (the dungeon temple is excluded by
    //! isClaimable and the guards in claimForSeat and handTilesOverToSeat).
    //! Rooms of nobody are never blocked.
    bool isTakeoverBlocked(const Seat* seat, const Tile* tile) const;

    //! \brief The gold the taker pays when the room changes hands
    //! (RoomTakeoverCostPercent of the build cost of the tiles taken, 0 by default).
    int32_t getTakeoverPrice() const;

    //! \brief The health of the room against being taken over, 1.0 when full.
    inline double getClaimHealth() const
    { return mClaimHealth; }

    //! \brief True when an enemy has worn the room down and its own workers can
    //! still repair it.
    inline bool needsClaimRepair() const
    { return mClaimHealth < 1.0; }

    //! \brief A worker of the owner dancing on the room gives claim health back,
    //! faster than an enemy worker takes it away (RoomRepairFactor).
    void repairClaimHealth(double danceRate);

    //! \brief False in the claimableOnly mode, where a room changes hands by being
    //! danced away and fighters have nothing to do with it. The dungeon temple
    //! cannot be claimed, so it stays destructible in every mode: destroying it is
    //! how a player gets defeated.
    bool isDestructible() const;

    virtual bool isAttackable(Tile* tile, Seat* seat) const override;
    virtual double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage, double magicalDamage, double elementDamage,
        Tile* tileTakingDamage, bool ko) override;

    static bool compareTile(Tile* tile1, Tile* tile2);

    //! \brief Adds a creature using the room. If the creature is allowed, true is returned
    virtual bool addCreatureUsingRoom(Creature* c);
    virtual void removeCreatureUsingRoom(Creature* c);
    virtual Creature* getCreatureUsingRoom(unsigned index);
    virtual bool hasOpenCreatureSpot(Creature* c) { return false; }

    //! \brief Called by the creature during its upkeep when using the room when it is ready
    //! to do something (no cooldown or no other action).
    //! Returns true if the action queue should continue to be proceeded and false otherwise
    virtual bool useRoom(Creature& creature, bool forced)
    { return false; }

    //! \brief Returns true if the creature should stop using the room if hungry/sleepy
    virtual bool shouldStopUseIfHungrySleepy(Creature& creature, bool forced)
    { return true; }

    //! \brief Returns true if the creature should not use room if in bad mood
    virtual bool shouldNotUseIfBadMood(Creature& creature, bool forced)
    { return true; }

    //! \brief Updates the active spot lists.
    virtual void updateActiveSpots(GameMap* gameMap = nullptr) override;

    inline unsigned int getNumActiveSpots() const
    { return mNumActiveSpots; }

    //! \brief Sets the name, seat and associates the given tiles with the room
    virtual void setupRoom(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles);

    //! \brief Checks on the neighboor tiles of the room if there are other rooms of the same type/same seat.
    //! if so, it aborbs them
    void checkForRoomAbsorbtion();

    //! \brief The counterpart of checkForRoomAbsorbtion: checks whether the covered tiles
    //! still hold together and, if they do not, moves every group but the biggest into a room
    //! of its own. Losing tiles in the middle, by selling or by destruction, leaves what is
    //! left looking like one room while being two, and everything done to a room is then done
    //! to both: claiming one half of a bridge used to claim the other half across the lava.
    void checkForSplit() override;

    //! \brief returns true if the room can be repaired and there are destroyed tiles. false otherwise.
    bool canBeRepaired() const;

    virtual int getCostRepair();

    //! \brief Repairs the destroyed tiles of the room
    virtual void repairRoom();

    virtual void restoreInitialEntityState() override;

    static bool sortForMapSave(Room* r1, Room* r2);

    //! \brief Returns the total gold that can be stored in the room
    virtual int getTotalGoldStorage() const
    { return 0; }

    //! \brief Returns the total gold that is stored in the room
    virtual int getTotalGoldStored() const
    { return 0; }

    //! \brief Deposits the given gold in the room. Returns the gold that could
    //! be deposited (may be less than the given amount if not enough space)
    virtual int depositGold(int gold, Tile *tile)
    { return 0; }

    //! \brief Tries to withdraw gold from the room. Returns the amount that could
    //! be taken (may be less than gold if not enough gold)
    virtual int withdrawGold(int gold)
    { return 0; }

    virtual void creatureDropped(Creature& creature) override;

    virtual bool isInContainment(Creature& creature)
    { return false; }

    static bool importRoomFromStream(Room& room, std::istream& is);

protected:
    static void fireRoomSound(Tile& tile, const std::string& soundFamily);

    //! \brief Hands the given tiles of this room over to a room of the same type
    //! owned by the claiming seat, merging it with an adjacent room of theirs
    //! when there is one and splitting this room when the loss cuts it in two.
    //! Room-level state gets shared out through splitRoom(), so e.g. a treasury
    //! tile takes its share of the stored gold with it. Counts as a captured room
    //! for the claimer when this room is left without a tile. Returns the room the
    //! tiles ended up in, or nullptr if no room could be created.
    Room* handTilesOverToSeat(Seat* seat, const std::vector<Tile*>& tiles);

    //! \brief Called when the claim health of the room is used up. By default every
    //! tile goes to a new room of the claiming seat, merged with the rooms of the
    //! same type it already has next to it. A room that has to stay the same object
    //! (the portal) overrides this.
    virtual void changeOwner(Seat* seat);

    //! \brief Tells the seat losing the room and the one getting it. The old seat is
    //! nullptr when the room was nobody's.
    void notifyOwnerChanged(Seat* oldSeat, Seat* newSeat);

    //! \brief 1.0 when full. Saved with the first tile of the room only while it is worn down (a room
    //! that was never touched writes nothing extra, and a save without it loads full); portals and
    //! bridges also keep the value their files always had.
    double mClaimHealth;

    /*! \brief Exports the headers needed to recreate the Room. It allows to extend Room as much as wanted.
     * The content of the Room will be exported by exportToPacket.
     */
    virtual void exportHeadersToStream(std::ostream& os) const override;
    void exportTileDataToStream(std::ostream& os, Tile* tile, TileData* tileData) const override;
    bool importTileDataFromStream(std::istream& is, Tile* tile, TileData* tileData) override;

    enum ActiveSpotPlace
    {
        activeSpotCenter,
        activeSpotTop,
        activeSpotBottom,
        activeSpotLeft,
        activeSpotRight
    };
    std::vector<Creature*> mCreaturesUsingRoom;

    //! \brief Lists the active spots in the middle of 3x3 squares.
    std::vector<Tile*> mCentralActiveSpotTiles;
    std::vector<Tile*> mLeftWallsActiveSpotTiles;
    std::vector<Tile*> mRightWallsActiveSpotTiles;
    std::vector<Tile*> mTopWallsActiveSpotTiles;
    std::vector<Tile*> mBottomWallsActiveSpotTiles;

    //! \brief The number of active spots.
    unsigned int mNumActiveSpots;

    virtual BuildingObject* notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile);
    virtual void notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile);

    //! \brief This function will be called when reordering room is needed (for example if another room has been absorbed)
    static void reorderRoomTiles(std::vector<Tile*>& tiles);
private :
    void activeSpotCheckChange(ActiveSpotPlace place, const std::vector<Tile*>& originalSpotTiles,
                               const std::vector<Tile*>& newSpotTiles, GameMap* gameMap = nullptr);

};

#endif // ROOM_H
