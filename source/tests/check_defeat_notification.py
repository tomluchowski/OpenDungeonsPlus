"""Exercise the production Player::notifyNoMoreDungeonTemple defeat notification without a game."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/game/Player.cpp').read_text()
header = (repo / 'source/game/Player.h').read_text()
client = (repo / 'source/network/ODClient.cpp').read_text()
notification_header = (repo / 'source/network/ServerNotification.h').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


probe = r'''
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_INF(x)
namespace Helper {template<typename T> std::string toString(T v){return std::to_string(v);}}
enum class ServerNotificationType {chatServer, playerDefeated, levelStatistics};
enum class EventShortNoticeType {majorGameEvent};
enum class RoomType {dungeonTemple};
enum class SoundRelativeKeeperStatements {Lost, Defeat, AllyDefeated};
struct ODPacket {std::vector<std::string> texts;std::vector<int32_t> ints;
 ODPacket& operator<<(const char* t){texts.push_back(t);return *this;}
 ODPacket& operator<<(EventShortNoticeType){return *this;}
 ODPacket& operator<<(int32_t v){ints.push_back(v);return *this;}
 ODPacket& operator<<(uint32_t){return *this;}ODPacket& operator<<(bool){return *this;}};
struct ODApplication {static double turnsPerSecond;};double ODApplication::turnsPerSecond=4.0;
struct SeatStatistics {uint32_t mKeepersDefeated=0,mCreaturesKilled=0,mHeroesDestroyed=0,mRoomsCaptured=0,mItemsMade=0,mCreaturesConverted=0;};
struct Player;
struct ServerNotification {ServerNotificationType type;Player* player;ODPacket mPacket;
 ServerNotification(ServerNotificationType t,Player* p):type(t),player(p){}};
struct ODServer {std::vector<ServerNotification*> queue;
 static ODServer& getSingleton(){static ODServer server;return server;}
 void queueServerNotification(ServerNotification* n){queue.push_back(n);}};
struct Seat;
struct Room {Seat* seat;Seat* getSeat(){return seat;}};
struct GameMapMock;
struct Player {
 Player(Seat* s,bool h):mSeat(s),mIsHuman(h){}
 Seat* mSeat;bool mIsHuman;bool mHasLost=false;GameMapMock* mGameMap=nullptr;
 int32_t mConquerorSeatId=-1,mDefeatHeartTileX=-1,mDefeatHeartTileY=-1;
 Seat* getSeat(){return mSeat;}bool getIsHuman()const{return mIsHuman;}
 RECORD
 void notifyNoMoreDungeonTemple();
};
struct Seat {int id,team;Player* player=nullptr;SeatStatistics stats;
 bool isRogueSeat()const{return id==0;}const SeatStatistics& getStatistics()const{return stats;}
 int getId()const{return id;}Player* getPlayer(){return player;}bool isAlliedSeat(Seat* s){return s&&team==s->team;}};
struct GameMapMock {int64_t turn=0;int64_t getTurnNumber()const{return turn;}std::vector<Seat*> seats;std::vector<Room*> temples;int sounds=0;
 std::vector<Room*> getRoomsByType(RoomType){return temples;}
 std::vector<Seat*>& getSeats(){return seats;}
 void fireRelativeSound(std::vector<Seat*>&,SoundRelativeKeeperStatements){++sounds;}};
METHOD
int main(){int checks=0,failures=0;
 ODServer& server=ODServer::getSingleton();
 std::function<void(bool,const char*)> check=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}};
 std::function<int(Player*,ServerNotificationType)> count=[&](Player* p,ServerNotificationType t){int n=0;for(ServerNotification* s:server.queue)if(s->player==p&&s->type==t)++n;return n;};
 {// 1v1, human loses, whole team lost: conqueror data recorded before the loss is sent
  server.queue.clear();GameMapMock map;Seat a{1,1},b{2,2};Player pa(&a,true),pb(&b,false);a.player=&pa;b.player=&pb;
  pa.mGameMap=&map;map.seats={&a,&b};
  pa.recordHeartDestroyed(2,17,23);
  pa.notifyNoMoreDungeonTemple();pa.notifyNoMoreDungeonTemple();
  check(count(&pa,ServerNotificationType::playerDefeated)==1,"defeat notification sent exactly once");
  check(count(&pb,ServerNotificationType::playerDefeated)==0,"no defeat notification for the bot enemy");
  int found=0;for(ServerNotification* n:server.queue)if(n->type==ServerNotificationType::playerDefeated){
   ++found;check(n->mPacket.ints.size()==3&&n->mPacket.ints[0]==2&&n->mPacket.ints[1]==17&&n->mPacket.ints[2]==23,"payload is conqueror seat then heart x then heart y");}
  check(found==1,"one defeat packet in total");
  check(count(&pa,ServerNotificationType::chatServer)==1,"existing chat message unchanged");}
 {// unknown conqueror and heart position
  server.queue.clear();GameMapMock map;Seat a{1,1};Player pa(&a,true);a.player=&pa;pa.mGameMap=&map;map.seats={&a};
  pa.notifyNoMoreDungeonTemple();
  check(server.queue.size()==3&&server.queue[1]->type==ServerNotificationType::playerDefeated,"defeat notification follows the chat text");
  if(server.queue.size()==3){ODPacket& p=server.queue[1]->mPacket;
   check(p.ints.size()==3&&p.ints[0]==-1&&p.ints[1]==-1&&p.ints[2]==-1,"unknown values are sent as -1");}}
 {// bot loses: nothing
  server.queue.clear();GameMapMock map;Seat a{1,1};Player pa(&a,false);a.player=&pa;pa.mGameMap=&map;map.seats={&a};
  pa.notifyNoMoreDungeonTemple();
  check(count(&pa,ServerNotificationType::playerDefeated)==0,"no defeat notification for a non-human player");}
 {// team game: player loses while an ally still has a heart; the ally gets no defeat notification
  server.queue.clear();GameMapMock map;Seat a{1,1},ally{2,1},e{3,2};Player pa(&a,true),pally(&ally,true),pe(&e,true);
  a.player=&pa;ally.player=&pally;e.player=&pe;pa.mGameMap=&map;map.seats={&a,&ally,&e};
  Room allyHeart{&ally};map.temples={&allyHeart};pa.recordHeartDestroyed(3,5,6);
  pa.notifyNoMoreDungeonTemple();
  check(count(&pa,ServerNotificationType::playerDefeated)==1,"defeated player is notified when only he lost");
  check(count(&pally,ServerNotificationType::playerDefeated)==0,"ally is not sent the defeat sequence");
  check(count(&pe,ServerNotificationType::playerDefeated)==0,"enemy is not sent the defeat sequence");
  check(count(&pally,ServerNotificationType::chatServer)==1,"ally still gets the ally-lost text");}
 {// team lost: every human ally gets the chat text but only the defeated player the sequence
  server.queue.clear();GameMapMock map;Seat a{1,1},ally{2,1};Player pa(&a,true),pally(&ally,true);
  a.player=&pa;ally.player=&pally;pa.mGameMap=&map;map.seats={&a,&ally};
  pa.notifyNoMoreDungeonTemple();
  check(count(&pa,ServerNotificationType::playerDefeated)==1&&count(&pally,ServerNotificationType::playerDefeated)==0,"team loss notifies only the defeated player");}
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
record = function(header, 'inline void recordHeartDestroyed(')
method = function(source, 'void Player::notifyNoMoreDungeonTemple(')
probe = probe.replace('RECORD', record).replace('METHOD', method)

# Static wiring checks on the production sources (client handler and enum position).
handler = client[client.index('case ServerNotificationType::playerDefeated:'):]
handler = handler[:handler.index('break;')]
assert 'packetReceived >> conquerorSeatId >> heartTileX >> heartTileY' in handler
assert 'startDefeatSequence(conquerorSeatId, heartTileX, heartTileY)' in handler
assert 'ModeManager::GAME' in handler
enum_body = notification_header[notification_header.index('enum class ServerNotificationType'):]
enum_body = enum_body[:enum_body.index('};')]
assert 'playerDefeated,' in enum_body and 'levelStatistics,' in enum_body and enum_body.rstrip().endswith('heartHealth')
print('WIRING OK: enum value is not moved (only levelStatistics was appended after it), client handler reads 3 int32 and guards on GAME mode')

with tempfile.TemporaryDirectory(prefix='odp-defeat-notification-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
