#ifndef FENCEDROOM_H
#define FENCEDROOM_H

#include "rooms/Room.h"

#include <vector>

class GameMap;
class Tile;







class FencedRoom: public Room
{

public:
    
    std::vector<Tile*>& getUnusedTiles(){ return mUnusedTiles; }
    std::vector<Tile*>& getFenceTiles(){ return mFenceTiles; }
    std::vector<Tile*>& getActualRoomTiles(){return mActualTiles; };

    Tile* getActualRoomTile(int index);

    
    double getCreatureSpeed(const Creature* creature, Tile* tile) const;

    virtual void creatureDropped(Creature& creature) override;
    Tile* getGateTile();
    
protected:
    FencedRoom(GameMap*);
    void setupRoom(const std::string& name, Seat* seat, const std::vector<Tile*>& tiles);    
    void splitTilesIntoClasses();    
    std::vector<Tile*> mUnusedTiles;
    std::vector<Tile*> mFenceTiles;
    std::vector<Tile*> mActualTiles;

    
private:



    
};





#endif // FENCEDROOM_H
