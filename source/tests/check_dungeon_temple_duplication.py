"""Compile production room splitting and temple placement without launching a game."""
from pathlib import Path
import argparse
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref', help='Read production code from a Git revision for a before/after check')
args = parser.parse_args()


def read_source(path):
    if args.source_ref:
        return subprocess.check_output(['git', 'show', f'{args.source_ref}:{path}'], cwd=repo, text=True)
    return (repo / path).read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


room = read_source('source/rooms/Room.cpp')
temple = read_source('source/rooms/RoomDungeonTemple.cpp')
header = read_source('source/rooms/RoomDungeonTemple.h')
building = read_source('source/entities/Building.cpp')
override = function(header, 'void checkForSplit()') if 'void checkForSplit()' in header else ''
probe = r'''
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#define OD_LOG_INF(x) ((void)0)
struct Room;
struct Tile {int x,y;Room* owner=nullptr;std::vector<Tile*> neighbors;
 int getX(){return x;}int getY(){return y;}
 const std::vector<Tile*>& getAllNeighbors(){return neighbors;}
 void setCoveringBuilding(Room* r){owner=r;}Room* getCoveringBuilding(){return owner;}};
struct TileData {double mHP=10;TileData* cloneTileData(){return new TileData(*this);}};
struct Creature {Tile* getPositionTile(){return nullptr;}};
struct BuildingObject {Tile* tile;};
struct GameMap {bool editor=false,server=true;int created=0;
 std::map<std::pair<int,int>,Tile> tiles;std::vector<Room*> rooms;
 bool isInEditorMode(){return editor;}bool isServerGameMap(){return server;}
 Tile* getTile(int x,int y){auto it=tiles.find({x,y});return it==tiles.end()?nullptr:&it->second;}
 std::string nextUniqueNameRoom(int){return "split";}};
struct Room {
 GameMap* map;bool temple=false;std::vector<Tile*> mCoveredTiles,mCoveredTilesDestroyed;
 std::map<Tile*,TileData*> mTileData;std::map<Tile*,BuildingObject*> mBuildingObjects;
 std::vector<Creature*> mCreaturesUsingRoom;
 Room(GameMap* m):map(m){}virtual ~Room(){for(auto& p:mTileData)delete p.second;removeAllBuildingObjects();}
 GameMap* getGameMap(){return map;}bool getIsOnServerMap(){return map->server;}
 int getType(){return temple?1:0;}int getSeat(){return 1;}
 void setIsOnMap(bool){}void setName(const std::string&){}void setSeat(int){}
 void createMesh(){}void splitRoom(Room&,const std::vector<Tile*>&){}
 void addToGameMap(GameMap* m){m->rooms.push_back(this);}
 void removeCreatureUsingRoom(Creature*){}void handleCreatureUsingAbsorbedRoom(Creature&){}
 static void reorderRoomTiles(std::vector<Tile*>& tiles){std::sort(tiles.begin(),tiles.end(),
 [](Tile* a,Tile* b){return a->x==b->x?a->y<b->y:a->x<b->x;});}
 virtual void checkForSplit();virtual void updateActiveSpots(GameMap* =nullptr){}
 Tile* getCentralTile();
 void removeAllBuildingObjects(){for(auto& p:mBuildingObjects)delete p.second;mBuildingObjects.clear();}
 void addBuildingObject(Tile* t,BuildingObject* b){mBuildingObjects[t]=b;}
};
struct PersistentObject:BuildingObject {
 PersistentObject(GameMap* m,Room&,const char*,Tile* t,double,bool){tile=t;++m->created;}
};
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;
 RoomDungeonTemple(GameMap* m):Room(m){temple=true;}
 void updateActiveSpots(GameMap* =nullptr) override;void updateTemplePosition();
 TEMPLE_OVERRIDE
};
struct DungeonHeartObject:PersistentObject {
 DungeonHeartObject(GameMap* m,RoomDungeonTemple& r,Tile* t):
  PersistentObject(m,r,"DungeonTempleObject",t,0.0,false){}
};
struct RoomManager {static Room* createRoom(GameMap* m,int type){
 return type?static_cast<Room*>(new RoomDungeonTemple(m)):new Room(m);}};
SPLIT
CENTRE
ACTIVE
POSITION
int main(){int checks=0,failures=0;
 auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::cout<<"FAIL "<<label<<'\n';}};
 // Cut a 5x5 floor down its centre, leaving two disconnected islands, then
 // cut again: exercise virtual dispatch through Room just as upkeep does.
 for(bool editor:{false,true})for(bool isTemple:{false,true})for(int axis:{0,1}){
  GameMap map;map.editor=editor;
  Room* room=RoomManager::createRoom(&map,isTemple?1:0);map.rooms.push_back(room);
  for(int x=0;x<5;++x)for(int y=0;y<5;++y)map.tiles.emplace(std::make_pair(x,y),Tile{x,y});
  for(auto& entry:map.tiles){Tile* t=&entry.second;t->owner=room;
   for(auto delta:{std::make_pair(1,0),{-1,0},{0,1},{0,-1}}){
    if(auto* n=map.getTile(t->x+delta.first,t->y+delta.second))t->neighbors.push_back(n);}
   room->mCoveredTiles.push_back(t);room->mTileData[t]=new TileData;}
  room->updateActiveSpots(&map);
  BuildingObject* original=isTemple?room->mBuildingObjects.begin()->second:nullptr;
  check(map.created==(isTemple?1:0),"initial object count");
  for(int cut:{2,1}){
   auto rooms=map.rooms;
   for(Room* r:rooms){auto tiles=r->mCoveredTiles;
    for(Tile* t:tiles)if((axis?t->y:t->x)==cut){
     r->mCoveredTiles.erase(std::find(r->mCoveredTiles.begin(),r->mCoveredTiles.end(),t));
     r->mCoveredTilesDestroyed.push_back(t);r->mTileData[t]->mHP=0;t->owner=nullptr;}
    r->updateActiveSpots(&map);r->checkForSplit();}
   check(map.rooms.size()==(isTemple?1u:2u),"temple remains one room; ordinary rooms split");
   int objects=0,covered=0;
   for(Room* r:map.rooms){objects+=int(r->mBuildingObjects.size());covered+=int(r->mCoveredTiles.size());
    for(Tile* t:r->mCoveredTiles)check(t->owner==r&&r->mTileData[t]->mHP==10,"live tile ownership and HP");}
   check(objects==(isTemple?1:0),"no second temple at an island centre");
   check(covered==(cut==2?20:15),"surviving floor retained");
   if(isTemple&&!editor){check(room->mBuildingObjects.begin()->second==original,"original core retained");
    check(room->getCentralTile()==map.getTile(2,2),"gameplay centre retained including damaged floor");
    check(map.created==1,"repeated updates never generate another core");}
  }
  // Complete destruction leaves no new room or replacement object.
  for(Room* r:map.rooms){for(Tile* t:r->mCoveredTiles){r->mCoveredTilesDestroyed.push_back(t);r->mTileData[t]->mHP=0;t->owner=nullptr;}
   r->mCoveredTiles.clear();r->updateActiveSpots(&map);r->checkForSplit();}
  check(map.rooms.size()==(isTemple?1u:2u),"no new room on complete floor destruction");
  if(isTemple)check(room->mBuildingObjects.size()==(editor?0u:1u),"existing editor/gameplay destruction lifecycle");
  for(Room* r:map.rooms)delete r;
 }
 // Clients never create a server-owned temple locally.
 GameMap client;client.server=false;RoomDungeonTemple clientRoom(&client);
 clientRoom.updateActiveSpots();check(client.created==0,"no client-side duplicate");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
probe = probe.replace('TEMPLE_OVERRIDE', override)
probe = probe.replace('SPLIT', function(room, 'void Room::checkForSplit()'))
probe = probe.replace('CENTRE', function(building, 'Tile* Building::getCentralTile()').replace('Building::', 'Room::'))
probe = probe.replace('ACTIVE', function(temple, 'void RoomDungeonTemple::updateActiveSpots('))
probe = probe.replace('POSITION', function(temple, 'void RoomDungeonTemple::updateTemplePosition()'))
with tempfile.TemporaryDirectory(prefix='odp-temple-duplication-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
