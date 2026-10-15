"""Check production construction validators and hand dispatch without a game."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def function(path, signature):
    text = (repo / path).read_text()
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


probe = r'''
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace Ogre { struct ColourValue { static const int Red=0,White=1; }; }
namespace Helper { template<class T>std::string toString(T value){return std::to_string(value);} }
enum class InputCommandState { infoOnly,building,validated };
enum class SelectedAction { none,selectTile,buildRoom,destroyRoom,castSpell,buildTrap,destroyTrap,queryEntity,sellBuilding };
enum class RoomType { dormitory,treasury };
struct RoomTreasury {static constexpr RoomType mRoomType=RoomType::treasury;};
enum class TrapType { cannon,doorWooden };
struct Seat { int gold=0,treasuries=1;int getGold(){return gold;}int getNbRooms(RoomType){return treasuries;} };
struct Player { Seat seat;Seat* getSeat(){return &seat;} };
struct Tile { bool buildable=true;bool isBuildableUpon(Seat*){return buildable;} };
struct Packet { std::vector<int> values;template<class T>Packet& operator<<(T value){values.push_back(int(value));return *this;} };
struct ClientNotification { Packet mPacket; };
struct ODClient { int queued=0;Packet last;static ODClient& getSingleton(){static ODClient c;return c;}
 void queueClientNotification(ClientNotification* n){++queued;last=n->mPacket;delete n;} };
struct GameMap { Player player;Tile tile;bool inside=true,doorAllowed=true;int count=1;
 Player* getLocalPlayer(){return &player;}Tile* getTile(int,int){return inside?&tile:nullptr;}
 std::vector<Tile*> getBuildableTilesForPlayerInArea(int,int,int,int,Player*){return std::vector<Tile*>(count,&tile);}
 void tileToPacket(Packet& p,Tile*){p<<7<<9;}
};
struct InputManager { InputCommandState mCommandState=InputCommandState::infoOnly;int mXPos=7,mYPos=9,mLStartDragX=7,mLStartDragY=9; };
struct ModeManager { InputManager input;InputManager& getInputManager(){return input;} };
struct Selection { SelectedAction action=SelectedAction::none;TrapType trap=TrapType::cannon;
 RoomType room=RoomType::dormitory;
 SelectedAction getCurrentAction(){return action;}RoomType getNewRoomType(){return room;}
 TrapType getNewTrapType(){return trap;}int getNewSpellType(){return 0;} };
struct GameMode;using InputCommand=GameMode;
struct RoomFactory { std::string formatBuildRoom(RoomType,uint32_t)const{return "room";}
 void checkBuildRoomDefault(GameMap*,RoomType,const InputManager&,InputCommand&)const; };
struct TreasuryFactory:RoomFactory {void checkBuildRoom(GameMap*,const InputManager&,InputCommand&)const;};
struct TrapFactory { std::string formatBuildTrap(TrapType,uint32_t)const{return "trap";}
 void checkBuildTrapDefault(GameMap*,TrapType,const InputManager&,InputCommand&)const;
 void checkBuildTrap(GameMap*,const InputManager&,InputCommand&)const; };
struct TrapDoor { static bool canDoorBeOnTile(GameMap* map,Tile*){return map->doorAllowed;} };
struct RoomManager { static int costPerTile(RoomType){return 25;}
 static ClientNotification* createRoomClientNotification(RoomType){return new ClientNotification;}
 static void checkBuildRoom(GameMap* m,RoomType t,const InputManager& i,InputCommand& c){
  if(t==RoomType::treasury)TreasuryFactory{}.checkBuildRoom(m,i,c);else RoomFactory{}.checkBuildRoomDefault(m,t,i,c);}
 static void checkSellRoomTiles(GameMap*,const InputManager&,InputCommand&){} };
struct TrapManager { static int costPerTile(TrapType){return 50;}
 static ClientNotification* createTrapClientNotification(TrapType){return new ClientNotification;}
 static void checkBuildTrap(GameMap* m,TrapType t,const InputManager& i,InputCommand& c){
  if(t==TrapType::doorWooden)TrapFactory{}.checkBuildTrap(m,i,c);else TrapFactory{}.checkBuildTrapDefault(m,t,i,c);}
 static void checkSellTrapTiles(GameMap*,const InputManager&,InputCommand&){} };
struct SpellManager { static void checkSpellCast(GameMap*,int,const InputManager&,InputCommand&){} };
struct RenderManager { int swings=0;static RenderManager& getSingleton(){static RenderManager r;return r;}void rrPlayBuildAnimation(){++swings;} };
struct GameMode { GameMap* mGameMap;ModeManager* mModeManager;Selection mPlayerSelection;
 std::vector<Tile*> mPreviewTiles;bool mActionTargetValid=false;std::string mActionTargetText;
 void displayText(int colour,const std::string& text){mActionTargetValid=colour!=Ogre::ColourValue::Red;mActionTargetText=text;}
 void displayPointerText(int,const std::string&){}void displayTileBuildFailure(Tile*,Seat*){mActionTargetValid=false;}
 void selectTiles(const std::vector<Tile*>&){}void selectSquaredTiles(int,int,int,int){}
 void unselectAllTiles(){}void updateSelectedTiles(){}
 void handlePlayerActionNone(){}void handlePlayerActionSelectTile(){}void handlePlayerActionQuery(){}void handlePlayerActionSell(){}
 void checkInputCommand();
};
ROOM
TREASURY
TRAP
DOOR
DISPATCH
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<why<<'\n';}};
 auto& net=ODClient::getSingleton();auto& renderer=RenderManager::getSingleton();
 for(int kind=0;kind<4;++kind)for(bool inside:{false,true})for(int count:{0,1,9})for(int gold:{0,25,49,50,1000})
 for(auto state:{InputCommandState::infoOnly,InputCommandState::building,InputCommandState::validated}){
  GameMap map;map.inside=inside;map.count=count;map.tile.buildable=count>0;map.player.seat.gold=gold;
  ModeManager manager;manager.input.mCommandState=state;GameMode mode{&map,&manager};
  mode.mPlayerSelection.action=(kind==0||kind==3)?SelectedAction::buildRoom:SelectedAction::buildTrap;
  mode.mPlayerSelection.room=kind==3?RoomType::treasury:RoomType::dormitory;
  mode.mPlayerSelection.trap=kind==2?TrapType::doorWooden:TrapType::cannon;
  net.queued=renderer.swings=0;mode.mActionTargetValid=true;mode.checkInputCommand();
  bool expected=inside&&count>0&&state==InputCommandState::validated&&gold>=(kind==2?50:count*((kind==0||kind==3)?25:50));
  check(net.queued==int(expected),"validator retains request eligibility and prices");
  check(renderer.swings==net.queued,"each eligible request swings once; previews and rejected input never swing");
  if(expected){check(net.last.values.size()==size_t(kind==2?2:1+2*count),"original packet payload unchanged");}
 }
 GameMap map;map.player.seat.gold=1000;map.doorAllowed=false;ModeManager manager;
 manager.input.mCommandState=InputCommandState::validated;GameMode mode{&map,&manager};
 mode.mPlayerSelection.action=SelectedAction::buildTrap;mode.mPlayerSelection.trap=TrapType::doorWooden;
 net.queued=renderer.swings=0;mode.checkInputCommand();check(!renderer.swings&&!net.queued,"invalid door orientation does not swing");
 for(auto action:{SelectedAction::none,SelectedAction::selectTile,SelectedAction::destroyRoom,SelectedAction::destroyTrap,
  SelectedAction::castSpell,SelectedAction::queryEntity,SelectedAction::sellBuilding}){
  mode.mPlayerSelection.action=action;mode.checkInputCommand();check(!renderer.swings,"unrelated actions never swing the hammer");
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''
probe = probe.replace('ROOM', function('source/rooms/RoomManager.cpp', 'void RoomFactory::checkBuildRoomDefault('))
treasury = function('source/rooms/RoomTreasury.cpp', 'void checkBuildRoom(')
probe = probe.replace('TREASURY', treasury.replace('void checkBuildRoom(', 'void TreasuryFactory::checkBuildRoom(').replace(' override', ''))
probe = probe.replace('TRAP', function('source/traps/TrapManager.cpp', 'void TrapFactory::checkBuildTrapDefault('))
door = function('source/traps/TrapDoor.cpp', 'virtual void checkBuildTrap(')
probe = probe.replace('DOOR', door.replace('virtual void checkBuildTrap(', 'void TrapFactory::checkBuildTrap(').replace(' override', ''))
probe = probe.replace('DISPATCH', function('source/modes/GameMode.cpp', 'void GameMode::checkInputCommand('))
with tempfile.TemporaryDirectory(prefix='odp-build-strike-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
