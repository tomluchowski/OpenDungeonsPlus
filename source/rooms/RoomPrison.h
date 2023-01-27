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

#ifndef ROOMPRISON_H
#define ROOMPRISON_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

class InputCommand;
class InputManager;

enum class FencingDirection
{
    NS,
    EW,
    SN,
    WE,
    SE,
    SW,
    NE,
    NW,
    nbDirections
};


class RoomPrison: public Room
{

public:
    RoomPrison(GameMap* gameMap);

    virtual RoomType getType() const override
    { return mRoomType; }

    void absorbRoom(Room *r) override;

    virtual void updateActiveSpots(GameMap* gameMap = nullptr) override;
    void splitTilesIntoClasses();
    
    void doUpkeep() override;

    bool hasOpenCreatureSpot(Creature* creature) override;
    bool addCreatureUsingRoom(Creature* creature) override;
    void removeCreatureUsingRoom(Creature* creature) override;

    uint32_t countPrisoners();

    bool hasCarryEntitySpot(GameEntity* carriedEntity) override;
    Tile* askSpotForCarriedEntity(GameEntity* carriedEntity) override;
    void notifyCarryingStateChanged(Creature* carrier, GameEntity* carriedEntity) override;

    bool isInContainment(Creature& creature) override;

    bool useRoom(Creature& creature, bool forced) override;

    void creatureDropped(Creature& creature) override;

    void restoreInitialEntityState() override;

    double getCreatureSpeed(const Creature* creature, Tile* tile) const override;

    std::vector<Tile*>& getUnusedTiles(){ return mUnusedTiles; }

    std::vector<Tile*>& getFenceTiles(){ return mFenceTiles; }

    Tile* getActualPrisonTile(int index);
    
    static const RoomType mRoomType;

    virtual void setupRoom(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles) override;    
    
protected:
    virtual BuildingObject* notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile) override;
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:

    Tile* getGateTile();
    
    void deleteFenceMeshes();
    void putFenceMeshes();
    BuildingObject* createFencingMesh(FencingDirection hd, Tile*  tt);
    void addFencingObject(Tile* targetTile, BuildingObject* obj, GameMap* gameMap = nullptr);
    
    std::vector<Tile*> mUnusedTiles;
    std::vector<Tile*> mFenceTiles;
    std::vector<Tile*> mActualPrisonTiles;
    std::vector<Creature*> mPendingPrisoners;

    std::map<Tile*, BuildingObject*> mFencingObjects;
    
    //! \brief Used at map loading to save creatures in prison
    std::vector<std::string> mPrisonersLoad;
};

#endif // ROOMPRISON_H
