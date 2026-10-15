"""Compile the production tile validator and treasury hover against small stubs."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
tile_source = (repo / 'source/entities/Tile.cpp').read_text()
method = tile_source[tile_source.index('bool Tile::isBuildableUpon('):
                     tile_source.index('\nvoid Tile::setCoveringBuilding(')]
treasury = (repo / 'source/rooms/RoomTreasury.cpp').read_text()
start = treasury.index('        Player* player =', treasury.index('void checkBuildRoom('))
end = treasury.index('        std::vector<Tile*> buildableTiles', start)
hover = treasury[start:end]
code = r'''
#include "gamemap/RoomObjectBounds.h"
#include <iostream>
#include <vector>
struct Seat { int getNbRooms(int){return 1;} int getGold(){return 100;} };
struct Player { Seat seat; Seat* getSeat(){return &seat;} };
struct Position {float x,y;};
struct Object {std::string mesh;Position position;const std::string& getMeshName(){return mesh;}Position getPosition(){return position;}};
struct GameMap;
struct Tile {
 GameMap* map;int x=0,y=0;bool full=false,building=false,claimed=true;
 bool isFullTile()const{return full;}bool getIsBuilding()const{return building;}
 bool isClaimedForSeat(Seat*)const{return claimed;}GameMap* getGameMap()const{return map;}
 int getX()const{return x;}int getY()const{return y;}bool isBuildableUpon(Seat*)const;
};
struct GameMap {bool editor=false;Player player;Tile* hovered=nullptr;std::vector<Object*> objects;
 bool isInEditorMode(){return editor;}const std::vector<Object*>& getRenderedMovableEntities(){return objects;}
 Player* getLocalPlayer(){return &player;}Tile* getTile(int,int){return hovered;}
};
namespace Ogre {struct ColourValue {static const int Red=1,White=2;};}
namespace Helper {std::string toString(int n){return std::to_string(n);}}
struct RoomTreasury {static const int mRoomType=1;};
struct RoomManager {static int costPerTile(int){return 25;}};
enum class InputCommandState {infoOnly,building,validated};
struct InputManager {InputCommandState mCommandState=InputCommandState::infoOnly;int mXPos=0,mYPos=0;};
struct InputCommand {int selected=0,failures=0;void unselectAllTiles(){selected=0;}
 void displayTileBuildFailure(Tile*,Seat*){++failures;}
 void displayText(int,const std::string&){}void displayPointerText(int,const std::string&){}
 void selectSquaredTiles(int,int,int,int){selected=1;}
};
std::string formatBuildRoom(int,int){return "build";}
METHOD
void hover(GameMap* gameMap,const InputManager& inputManager,InputCommand& inputCommand){HOVER}
int main(){int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 GameMap map;Tile tile{&map};Seat seat;Object heart{"DungeonTempleObject",{58,102}};
 map.objects={&heart};map.hovered=&tile;
 for(int x=54;x<=62;++x)for(int y=98;y<=106;++y){
  tile.x=x;tile.y=y;bool expected=std::abs(x-58)>2||std::abs(y-102)>2;
  check(tile.isBuildableUpon(&seat)==expected);
  InputCommand command;hover(&map,InputManager{},command);
  check(command.selected==(expected?1:0));check(command.failures==(expected?0:1));
 }
 tile.x=56;tile.y=102;map.editor=true;check(tile.isBuildableUpon(&seat));map.editor=false;
 map.objects.clear();check(tile.isBuildableUpon(&seat));
 Object portal{"PortalObject",{58,102}};map.objects={&portal};check(tile.isBuildableUpon(&seat));
 tile.building=true;check(!tile.isBuildableUpon(&seat));tile.building=false;
 tile.full=true;check(!tile.isBuildableUpon(&seat));tile.full=false;
 tile.claimed=false;check(!tile.isBuildableUpon(&seat));
 map.hovered=nullptr;InputCommand outside;hover(&map,InputManager{},outside);check(outside.selected==0&&outside.failures==1);
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHOD', method).replace('HOVER', hover)
with tempfile.TemporaryDirectory(prefix='odp-heart-build-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14',
                    f'/I{repo / "source"}', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
