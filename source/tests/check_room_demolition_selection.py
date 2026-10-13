"""Execute the production sale routing and room filtering without a game."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
game = (repo / 'source/modes/GameMode.cpp').read_text()
rooms = (repo / 'source/rooms/RoomManager.cpp').read_text()

def function(text, signature, start=0):
    begin = text.index(signature, start)
    end = text.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]

signature = 'void RoomManager::checkSellRoomTiles('
first = rooms.index(signature)
second = rooms.index(signature, first + 1)
preview = function(game, 'void GameMode::updateSelectedTiles()')
flags = preview[preview.index('    const bool building'):preview.index('    if(!mActionTargetValid')]
assert preview.count('colour, building || roomDemolition)') == 2
probe = r'''
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>
namespace Ogre {struct ColourValue {static const int Red=0,White=1;};}
enum class InputCommandState {infoOnly,building,validated};
enum class TileVisual {normal,portalRoom,portalWaveRoom,dungeonTempleRoom};
struct Seat{};struct Player {Seat seat;Seat* getSeat(){return &seat;}};
struct Tile {int x,y;bool room=true,trap=false;Seat* seat=nullptr;TileVisual visual=TileVisual::normal;
 bool getIsRoom(){return room;}bool getIsTrap(){return trap;}Seat* getSeat(){return seat;}
 TileVisual getTileVisual(){return visual;}unsigned getRefundPriceRoom(){return 10;}};
enum class SelectedAction {none,selectTile,buildRoom,buildTrap,destroyRoom,destroyTrap,sellBuilding};
struct Selection {SelectedAction action;SelectedAction getCurrentAction(){return action;}};
bool thick(Selection mPlayerSelection,std::vector<Tile*> mPreviewTiles){FLAGS return building||roomDemolition;}
struct Packet {std::vector<int> data;template<class T>Packet& operator<<(T value){data.push_back(int(value));return *this;}};
enum class ClientNotificationType {askSellRoomTiles};
struct ClientNotification {Packet mPacket;ClientNotification(ClientNotificationType){}};
struct ODClient {std::vector<Packet> sent;static ODClient& getSingleton(){static ODClient c;return c;}
 void queueClientNotification(ClientNotification* n){sent.push_back(n->mPacket);delete n;}};
struct GameMap {Player player;std::map<std::pair<int,int>,Tile> tiles;Player* getLocalPlayer(){return &player;}
 Tile* getTile(int x,int y){auto i=tiles.find({x,y});return i==tiles.end()?nullptr:&i->second;}
 std::vector<Tile*> rectangularRegion(int x,int y,int a,int b){std::vector<Tile*> result;for(int i=std::min(x,a);i<=std::max(x,a);++i)for(int j=std::min(y,b);j<=std::max(y,b);++j)if(auto* t=getTile(i,j))result.push_back(t);return result;}
 void tileToPacket(Packet& p,Tile* t){p<<t->x<<t->y;}};
struct InputManager {int mXPos=0,mYPos=0,mLStartDragX=0,mLStartDragY=0;InputCommandState mCommandState=InputCommandState::infoOnly;};
struct ModeManager {InputManager input;InputManager& getInputManager(){return input;}};
struct InputCommand {std::vector<Tile*> selected;std::string text;int colour=0;
 void unselectAllTiles(){selected.clear();}void selectTiles(const std::vector<Tile*> tiles){selected=tiles;}
 void displayText(int c,const std::string& t){colour=c;text=t;}};
struct RoomManager {static void checkSellRoomTiles(GameMap*,const InputManager&,InputCommand&);
 static void checkSellRoomTiles(GameMap*,const InputManager&,InputCommand&,const std::vector<Tile*>&);
 static std::string formatSellRoom(int price){return std::to_string(price);}};
struct TrapManager {static void checkSellTrapTiles(GameMap*,const InputManager&,InputCommand& c,const std::vector<Tile*>& tiles){c.selectTiles(tiles);c.text="trap";}};
struct GameMode:InputCommand {GameMap* mGameMap;ModeManager manager;ModeManager* mModeManager=&manager;
 void handlePlayerActionSell();};
METHODS
int main(){int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 GameMap map;GameMode game;game.mGameMap=&map;Seat enemy;auto& input=game.manager.input;auto& sent=ODClient::getSingleton().sent;
 for(int x=0;x<3;++x)for(int y=0;y<3;++y)map.tiles[{x,y}]={x,y,true,false,map.player.getSeat()};
 map.tiles[{1,1}].seat=&enemy;map.tiles[{2,1}].visual=TileVisual::portalRoom;map.tiles[{1,2}].room=false;
 for(int sx:{0,2})for(int sy:{0,2}){
  input.mLStartDragX=sx;input.mLStartDragY=sy;input.mXPos=2-sx;input.mYPos=2-sy;
  input.mCommandState=InputCommandState::building;sent.clear();game.handlePlayerActionSell();
  check(game.selected.size()==6);check(game.text=="60");check(sent.empty());
  input.mCommandState=InputCommandState::validated;game.handlePlayerActionSell();
  check(sent.size()==1);check(sent.back().data==std::vector<int>({6,0,0,0,1,0,2,1,0,2,0,2,2}));check(game.selected.empty());
 }
 input.mCommandState=InputCommandState::infoOnly;input.mXPos=0;input.mYPos=0;game.handlePlayerActionSell();
 check(game.selected.size()==1&&game.text=="10");
 map.tiles[{0,0}].room=false;input.mLStartDragX=0;input.mLStartDragY=0;input.mXPos=2;input.mYPos=2;
 input.mCommandState=InputCommandState::building;game.handlePlayerActionSell();check(game.selected.size()==5);
 input.mXPos=1;input.mYPos=1;game.handlePlayerActionSell();check(game.selected.size()==2); // Invalid endpoint still contains eligible rooms.
 map.tiles[{0,0}].trap=true;map.tiles[{2,2}].room=false;map.tiles[{2,2}].trap=true;
 input.mXPos=2;input.mYPos=2;game.handlePlayerActionSell();check(game.selected.size()==1&&game.text=="trap");
 for(auto action:{SelectedAction::none,SelectedAction::selectTile,SelectedAction::buildRoom,SelectedAction::buildTrap,SelectedAction::destroyRoom,SelectedAction::destroyTrap,SelectedAction::sellBuilding})
 for(bool room:{false,true}){Tile t{0,0,room};check(thick({action},{&t})==(action==SelectedAction::buildRoom||action==SelectedAction::buildTrap||action==SelectedAction::destroyRoom||(action==SelectedAction::sellBuilding&&room)));}
 // Heart tiles must never be offered or sent for demolition.
 map.tiles[{0,0}]={0,0,true,false,map.player.getSeat(),TileVisual::dungeonTempleRoom};
 input.mXPos=0;input.mYPos=0;input.mLStartDragX=0;input.mLStartDragY=0;
 for(auto state:{InputCommandState::infoOnly,InputCommandState::building,InputCommandState::validated}){
  input.mCommandState=state;sent.clear();game.handlePlayerActionSell();
  check(game.selected.empty());check(sent.empty());check(game.text=="Dungeon hearts cannot be sold.");}
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('FLAGS', flags).replace('METHODS', function(game, 'void GameMode::handlePlayerActionSell(') + '\n' +
    function(rooms, signature, first) + '\n' + function(rooms, signature, second))
with tempfile.TemporaryDirectory(prefix='odp-room-demolition-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
