"""Exercise the production room-sale effect dispatch without launching a game."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/rooms/RoomManager.cpp').read_text()
start = source.index('void RoomManager::sellRoomTiles(')
end = source.index('\nstd::string RoomManager::formatSellRoom', start)
method = source[start:end]

probe = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
#define OD_ASSERT_TRUE(value) assert(value)
#define OD_LOG_ERR(value) ((void)0)
struct ODPacket {
 std::vector<int> data;size_t cursor=0;
 template<class T>ODPacket& operator<<(T v){data.push_back(int(v));return *this;}
 template<class T>ODPacket& operator>>(T& v){assert(cursor<data.size());v=T(data[cursor++]);return *this;}
 explicit operator bool()const{return true;}
};
struct Player;
struct Tile;
struct GameMap;
struct Seat {
 int id;Player* player=nullptr;std::set<int> visible;
 int getId(){return id;}Player* getPlayer(){return player;}
 bool hasVisionOnTile(Tile* tile);
 void updateTileStateForSeat(Tile*,bool){}
};
struct Player {Seat* seat;bool human=true;Seat* getSeat(){return seat;}bool getIsHuman(){return human;}};
struct Room {
 Seat* owner;bool sellable=true;int updates=0,splits=0;
 bool canSeatSellBuilding(Seat* s){return owner==s&&sellable;}
 bool removeCoveredTile(Tile* tile);
 int getType(){return 1;}
 void updateActiveSpots(GameMap*){++updates;}void checkForSplit(){++splits;}
};
struct Tile {
 int id;Room* room=nullptr;bool removable=true;
 Room* getCoveringRoom(){return room;}
 void changeNotifiedForSeat(Seat*){}
 void exportToPacketForUpdate(ODPacket& p,Seat*){p<<0;}
};
bool Room::removeCoveredTile(Tile* t){if(!t->removable)return false;t->room=nullptr;return true;}
bool Seat::hasVisionOnTile(Tile* t){return visible.count(t->id)!=0;}
enum class ServerNotificationType {refreshTiles,roomConstructionEffect};
struct ServerNotification {
 ServerNotificationType type;Player* recipient;ODPacket mPacket;
 ServerNotification(ServerNotificationType t,Player* p):type(t),recipient(p){}
};
struct ODServer {
 std::vector<ServerNotification> sent;
 static ODServer& getSingleton(){static ODServer server;return server;}
 void sendAsyncMsg(const ServerNotification& n){sent.push_back(n);}
};
struct GameMap {
 bool editor=false;int gold=0;std::map<int,Tile> tiles;std::vector<Seat*> seats;
 Tile* tileFromPacket(ODPacket& p){int id;p>>id;auto i=tiles.find(id);return i==tiles.end()?nullptr:&i->second;}
 void tileToPacket(ODPacket& p,Tile* t){p<<t->id;}
 void addGoldToSeat(int amount,int){gold+=amount;}
 const std::vector<Seat*>& getSeats(){return seats;}
 int getNodeType(){return 7;}bool isInEditorMode(){return editor;}
};
struct RoomManager {static int costPerTile(int){return 100;}static void sellRoomTiles(GameMap*,Player*,ODPacket&);};
METHOD
int main(){
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::cout<<"FAIL "<<label<<'\n';}};
 // Partial vision, failed/foreign/unsellable/empty/duplicate/invalid requests.
 for(bool editor:{false,true})for(int count:{1,6}) {
  Seat owner{1},observer{2},ai{3},absent{4};
  Player p{&owner},q{&observer},bot{&ai,false};owner.player=&p;observer.player=&q;ai.player=&bot;
  GameMap map;map.editor=editor;map.seats={&owner,&observer,&ai,&absent};
  Room room{&owner},foreign{&observer},portal{&owner,false};
  std::vector<int> accepted,seen;
  ODPacket request;request<<count+6;
  for(int i=0;i<count;++i){map.tiles.emplace(i,Tile{i,&room});request<<i;accepted.push_back(i);
   owner.visible.insert(i);if(i%2==0){observer.visible.insert(i);seen.push_back(i);}ai.visible.insert(i);absent.visible.insert(i);}
  map.tiles.emplace(20,Tile{20,&room,false});map.tiles.emplace(21,Tile{21,&foreign});
  map.tiles.emplace(22,Tile{22,&portal});map.tiles.emplace(23,Tile{23});
  request<<20<<21<<22<<23<<0<<99;
  auto& messages=ODServer::getSingleton().sent;messages.clear();
  RoomManager::sellRoomTiles(&map,&p,request);
  check(map.gold==50*count,"refund unchanged");
  check(room.updates==1&&room.splits==1,"room lifecycle unchanged");
  check(foreign.updates==0&&portal.updates==0,"excluded rooms untouched");
  check(messages.size()==(editor?2:4),"one refresh/effect pair per visible human");
  for(Player* recipient:{&p,&q}) {
   const auto& expected=recipient==&p?accepted:seen;
   int refreshIndex=-1,effectIndex=-1,refreshCount=0,effectCount=0;
   for(size_t i=0;i<messages.size();++i){const auto& n=messages[i];if(n.recipient!=recipient)continue;
    if(n.type==ServerNotificationType::refreshTiles){refreshIndex=int(i);++refreshCount;
     std::vector<int> packet{int(expected.size()),7};for(int id:expected){packet.push_back(id);packet.push_back(0);}
     check(n.mPacket.data==packet,"refresh remains exact");
    }else{effectIndex=int(i);++effectCount;std::vector<int> packet{int(expected.size())};
     packet.insert(packet.end(),expected.begin(),expected.end());check(n.mPacket.data==packet,"effect contains only visible successfully removed tiles");}
   }
   check(refreshCount==1,"one refresh per recipient");
   check(effectCount==(editor?0:1),"gameplay-only effect per recipient");
   check(editor||effectIndex>refreshIndex,"effect follows final tile refresh");
  }
  messages.clear();ODPacket repeat;repeat<<count;for(int id:accepted)repeat<<id;
  RoomManager::sellRoomTiles(&map,&p,repeat);
  check(messages.empty(),"already removed tiles never replay effect");
  check(map.gold==50*count,"repeat request never refunds twice");
 }
 // Successful demolition with no observers must not reveal coordinates.
 Seat owner{1};Player player{&owner};owner.player=&player;Room room{&owner};GameMap map;
 map.seats={&owner};map.tiles.emplace(0,Tile{0,&room});ODPacket request;request<<1<<0;
 ODServer::getSingleton().sent.clear();RoomManager::sellRoomTiles(&map,&player,request);
 check(ODServer::getSingleton().sent.empty(),"hidden demolition sends no notification");
 check(map.gold==50,"visibility does not alter refund");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='odp-room-demolition-effect-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
