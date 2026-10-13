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

#ifndef TRAP_H
#define TRAP_H

#include "entities/Building.h"

#include <string>
#include <vector>
#include <iosfwd>

class BuildingObject;
class CraftedTrap;
class Creature;
class GameMap;
class InputCommand;
class InputManager;
class ODPacket;
class Player;
class Seat;
class Tile;
class TrapEntity;

enum class TrapType;


//! \brief A small class telling whether a trap tile is activated.
class TrapTileData : public TileData
{

    friend class ODServer;
    friend class GameMap;
public:
    TrapTileData() :
        TileData(),
        mClaimedValue(1.0),
        mIsActivated(false),
        mReloadTime(0),
        mCraftedTrap(nullptr),
        mNbShootsBeforeDeactivation(0),
        mTrapEntity(nullptr),
        mIsWorking(false),
        mRemoveTrap(false),
        mIsExhausted(false),
        mReloadWorker(nullptr),
        mReloadNextTurn(0)
    {}

    TrapTileData(const TrapTileData* trapTileData) :
        TileData(trapTileData),
        mIsActivated(trapTileData->mIsActivated),
        mReloadTime(trapTileData->mReloadTime),
        mCraftedTrap(trapTileData->mCraftedTrap),
        mNbShootsBeforeDeactivation(trapTileData->mNbShootsBeforeDeactivation),
        mTrapEntity(trapTileData->mTrapEntity),
        mIsWorking(trapTileData->mIsWorking),
        mRemoveTrap(trapTileData->mRemoveTrap),
        mIsExhausted(trapTileData->mIsExhausted),
        mReloadWorker(nullptr),
        mReloadNextTurn(trapTileData->mReloadNextTurn)
    {}

    virtual ~TrapTileData()
    {}

    virtual TrapTileData* cloneTileData() const override
    { return new TrapTileData(this); }

    inline void setTrapEntity(TrapEntity* trapEntity)
    { mTrapEntity = trapEntity; }

    inline TrapEntity* getTrapEntity() const
    { return mTrapEntity; }

    bool decreaseReloadTime()
    {
        if (mReloadTime > 1)
        {
            --mReloadTime;
            return true;
        }

        mReloadTime = 0;
        return false;
    }

    inline void setActivated(bool activated)
    { mIsActivated = activated; }

    bool decreaseShoot()
    {
        if(mNbShootsBeforeDeactivation < 0)
            return true;

        if(mNbShootsBeforeDeactivation > 1)
        {
            --mNbShootsBeforeDeactivation;
            return true;
        }

        return false;
    }

    inline bool isActivated() const
    { return mIsActivated; }

    inline uint32_t getReloadTime() const
    { return mReloadTime; }

    inline void setReloadTime(uint32_t reloadTime)
    { mReloadTime = reloadTime; }

    inline void setNbShootsBeforeDeactivation(int32_t nbShoot)
    { mNbShootsBeforeDeactivation = nbShoot; }

    inline int32_t getNbShootsBeforeDeactivation() const
    { return mNbShootsBeforeDeactivation; }

    inline void setCarriedCraftedTrap(CraftedTrap* craftedTrap)
    { mCraftedTrap = craftedTrap; }

    inline CraftedTrap* getCarriedCraftedTrap() const
    { return mCraftedTrap; }

    inline bool getIsWorking() const
    { return mIsWorking; }

    inline void setIsWorking(bool isWorking)
    { mIsWorking = isWorking; }

    inline bool getRemoveTrap() const
    { return mRemoveTrap; }

    inline void setRemoveTrap(bool removeTrap)
    { mRemoveTrap = removeTrap; }

    //! \brief True once the trap tile has used up its shots (saved). A tile that never was loaded is not exhausted.
    inline bool isExhausted() const
    { return mIsExhausted; }

    //! \brief Sets whether the trap tile has used up its shots
    inline void setExhausted(bool exhausted)
    { mIsExhausted = exhausted; }

    //! \brief The worker on its way to reload this tile (runtime only, not saved)
    inline Creature* getReloadWorker() const
    { return mReloadWorker; }

    //! \brief Sets the worker that reloads this tile (nullptr to free it)
    inline void setReloadWorker(Creature* worker)
    { mReloadWorker = worker; }

    //! \brief Turn number before which no worker reloads this tile (runtime only, not saved)
    inline int64_t getReloadNextTurn() const
    { return mReloadNextTurn; }

    //! \brief Sets the turn number before which no worker reloads this tile
    inline void setReloadNextTurn(int64_t turn)
    { mReloadNextTurn = turn; }

    void fireSeatsSawTriggering();
    void seatSawTriggering(Seat* seat);
    void seatsSawTriggering(const std::vector<Seat*>& seats);

    double mClaimedValue;

private:
    bool mIsActivated;
    uint32_t mReloadTime;
    CraftedTrap* mCraftedTrap;
    int32_t mNbShootsBeforeDeactivation;
    TrapEntity* mTrapEntity;
    bool mIsWorking;
    bool mRemoveTrap;
    bool mIsExhausted;
    Creature* mReloadWorker;
    int64_t mReloadNextTurn;
};

//! \brief What a trap effect notification tells the clients (see ServerNotificationType::trapEffect).
//! New values are only added at the end.
enum class TrapEffectKind : int32_t
{
    //! The trap went off just now
    fired = 0,
    //! A trigger trap set this (linked) trap off or marked it
    linked = 1
};

/*! \class Trap Trap.h
 *  \brief Defines a trap
 */
class Trap : public Building
{
public:
    Trap(GameMap* gameMap);
    virtual ~Trap()
    {}

    virtual GameEntityType getObjectType() const override;

    virtual void addToGameMap(GameMap* gameMap = nullptr) override;
    virtual void removeFromGameMap(GameMap* gameMap = nullptr) override;

    virtual const TrapType getType() const = 0;

    //! Traps can be claimed by enemy seats
    virtual bool isClaimable(Seat* seat) const override;
    virtual void claimForSeat(Seat* seat, Tile* tile, double danceRate) override;

    virtual void doUpkeep() override;

    virtual bool shoot(Tile* tile)
    { return true; }

    //! \brief Sets off the trap on the given tile without an enemy standing on it (used by trigger traps).
    //! Returns true if the trap fired. Traps that are deactivated or reloading do not fire.
    bool forceTrigger(Tile* tile);

    //! \brief Mana taken from the owner each time the trap fires. The trap does not
    //! fire while the owner has less mana than that. 0 for traps that cost no mana.
    virtual double getManaToFire() const;

    //! \brief Mana per second the owner pays for each armed tile of this trap. 0 for traps that cost nothing to hold.
    double getManaUpkeepPerSecond() const;

    //! \brief Number of tiles of this trap that are armed.
    uint32_t getNbActivatedTiles() const;

    //! \brief Tells the human seats that see the tile about a trap effect (cosmetic only).
    //! \param kind What happened.
    //! \param tile The tile of the trap the effect belongs to; nothing is sent for nullptr.
    //! \param fraction Health left of a door (0 to 1), 1 for everything else.
    void fireTrapEffect(TrapEffectKind kind, Tile* tile, double fraction);

    virtual bool isDoor() const
    { return false; }

    //! \brief Health given to each tile of a newly built trap
    virtual double getDefaultTileHP() const
    { return DEFAULT_TILE_HP; }

    //! \brief Tells whether the trap is activated.
    bool isActivated(Tile* tile) const;

    //! \brief Tells whether a free worker of the owner may reload the tile now: the tile used up its shots,
    //! no crafted trap is on its way to it, nobody else reloads it and the pause after a refusal is over.
    //! Server only; doors and traps without shot limit are never reloaded this way.
    bool canBeReloadedByWorker(Tile* tile, const Creature* worker) const;

    //! \brief The worker that reserved the tile for reloading (nullptr if none)
    Creature* getReloadWorker(Tile* tile) const;
    //! \brief Reserves the tile for the worker (nullptr frees it)
    void setReloadWorker(Tile* tile, Creature* worker);

    //! \brief No worker reloads the tile before the given turn number
    void postponeReload(Tile* tile, int64_t untilTurn);

    //! \brief Gold a worker needs to reload one tile (per trap type from the configuration)
    int32_t getReloadPrice() const;

    //! \brief Sets the name, seat and associates the given tiles with the trap
    virtual void setupTrap(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles);

    virtual bool removeCoveredTile(Tile* t) override;
    virtual void updateActiveSpots(GameMap* gameMap = nullptr) override;

    virtual int32_t getNbNeededCraftedTrap() const;

    bool hasCarryEntitySpot(GameEntity* carriedEntity) override;
    Tile* askSpotForCarriedEntity(GameEntity* carriedEntity) override;
    void notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity) override;

    virtual bool isAttackable(Tile* tile, Seat* seat) const override;

    virtual bool shouldSetCoveringTileDirty(Seat* seat, Tile* tile) override;

    virtual void restoreInitialEntityState() override;

    virtual bool isTileVisibleForSeat(Tile* tile, Seat* seat) const override;

    static std::string getTrapStreamFormat();

    static bool sortForMapSave(Trap* t1, Trap* t2);

    static bool importTrapFromStream(Trap& trap, std::istream& is);

    //! \brief Triggered when the trap is activated
    void activate(Tile* tile);

    //! \brief Triggered when deactivated.
    virtual void deactivate(Tile* tile);

    
protected:
    static void fireTrapSound(Tile& tile, const std::string& soundFamily);

    virtual void exportHeadersToStream(std::ostream& os) const override;
    virtual void exportTileDataToStream(std::ostream& os, Tile* tile, TileData* tileData) const override;
    virtual bool importTileDataFromStream(std::istream& is, Tile* tile, TileData* tileData) override;

    virtual TrapTileData* createTileData(Tile* tile) override;

    //! \brief Checks the mana, shoots on the tile and updates reload, uses and visibility if the trap fired
    bool fireTile(Tile* tile, TrapTileData* trapTileData);

    virtual BuildingObject* notifyActiveSpotCreated(Tile* tile);
    virtual TrapEntity* getTrapEntity(Tile* tile) = 0;
    virtual void notifyActiveSpotRemoved(Tile* tile);

    uint32_t mNbShootsBeforeDeactivation;
    uint32_t mReloadTime;
    double mMinDamage;
    double mMaxDamage;

    //! True while forceTrigger() fires the trap: pressure traps then do not need an enemy on their tile
    bool mForcedTrigger;

    //! List of traps destroyed but with at least 1 player having vision. They will
    //! get removed when vision is gained by every player having seen it before destruction
    std::vector<BuildingObject*> mTrapEntitiesWaitingRemove;
};

#endif // TRAP_H
