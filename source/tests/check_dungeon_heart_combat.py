"""Exercise production heart protection, damage, death and save methods without a game."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/rooms/RoomDungeonTemple.cpp').read_text()
header = (repo / 'source/rooms/RoomDungeonTemple.h').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


probe = r'''
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
struct Tile{int getX()const{return 3;}int getY()const{return 4;}};
struct Player {bool human=true;bool lost=false;int conqueror=-1,heartX=-1,heartY=-1,recorded=0;bool getIsHuman()const{return human;}bool getHasLost()const{return lost;}
 void recordHeartDestroyed(int c,int x,int y){conqueror=c;heartX=x;heartY=y;++recorded;}};
struct SeatStatistics {unsigned mKeepersDefeated=0;};
struct Seat {int team;Player* player=nullptr;int id=0;SeatStatistics stats;SeatStatistics& getStatistics(){return stats;}Player* getPlayer(){return player;}int getId()const{return id;}bool isAlliedSeat(Seat* s){return s&&team==s->team;}};
enum class ServerNotificationType {chatServer};
enum class EventShortNoticeType {majorGameEvent};
struct ODPacket {std::vector<std::string> texts;int majorEvents=0;
 ODPacket& operator<<(const std::string& t){texts.push_back(t);return *this;}
 ODPacket& operator<<(EventShortNoticeType){++majorEvents;return *this;}};
struct ServerNotification {ServerNotificationType type;Player* player;ODPacket mPacket;
 ServerNotification(ServerNotificationType t,Player* p):type(t),player(p){}};
struct ODServer {std::vector<ServerNotification*> queue;
 static ODServer& getSingleton(){static ODServer server;return server;}
 void queueServerNotification(ServerNotification* n){queue.push_back(n);}};
enum class GameEntityType {creature, other};
struct CreatureDefinition {bool worker=false;bool isWorker()const{return worker;}};
struct GameEntity {Seat* seat;int deaths=0;GameEntity(Seat* s=nullptr):seat(s){}
 virtual GameEntityType getObjectType()const{return GameEntityType::other;}
 Seat* getSeat(){return seat;}void setSeat(Seat* s){seat=s;}void fireEntityDead(){++deaths;}
 virtual bool isAttackable(Tile*,Seat*)const{return false;}virtual double getHP(Tile*)const{return 0;}
 virtual double takeDamage(GameEntity*,double,double,double,double,Tile*,bool){return 0;}};
struct Creature:GameEntity {CreatureDefinition definition;Creature(Seat* s,bool worker):GameEntity(s){definition.worker=worker;}
 GameEntityType getObjectType()const override{return GameEntityType::creature;}
 const CreatureDefinition* getDefinition()const{return &definition;}};
struct GameMap {bool editor=false;int fights=0;
 bool isInEditorMode(){return editor;}void playerIsFighting(Player*,Tile*){++fights;}};
struct BuildingObject:GameEntity {Tile* tile;BuildingObject(Tile* t):tile(t){}Tile* getPositionTile(){return tile;}bool notifyRemoveAsked(){return true;}};
struct Building {double floorHP=250;double getHP(Tile*)const{return floorHP;}};
struct Room:Building {
 GameMap* map;Seat* seat;int dead=0,removed=0,upkeep=0,objectsRemoved=0;std::vector<Tile*> mCoveredTiles;
 void removeAllBuildingObjects(){++objectsRemoved;}
 static Tile* nineTiles(){static Tile tiles[9];return tiles;}
 Room(GameMap* m,Seat* s):map(m),seat(s){for(int i=0;i<9;++i)mCoveredTiles.push_back(nineTiles()+i);}virtual ~Room()=default;
 unsigned numCoveredTiles()const{return static_cast<unsigned>(mCoveredTiles.size());}
 GameMap* getGameMap()const{return map;}Seat* getSeat()const{return seat;}
 double getPhysicalDefense(){return 1;}double getMagicalDefense(){return 2;}double getElementDefense(){return 3;}
 void fireEntityDead(){++dead;}
 virtual bool canSeatSellBuilding(Seat*)const{return true;}
 virtual bool isAttackable(Tile*,Seat*)const{return true;}
 virtual double takeDamage(GameEntity*,double,double,double,double,Tile*,bool){return 99;}
 virtual bool removeCoveredTile(Tile* tile){++removed;
  mCoveredTiles.erase(std::remove(mCoveredTiles.begin(),mCoveredTiles.end(),tile),mCoveredTiles.end());return true;}
 virtual void doUpkeep(){++upkeep;}
 virtual void exportToStream(std::ostream& os)const{os<<floorHP<<'\n';}
 virtual bool importFromStream(std::istream& is){return bool(is>>floorHP);}
};
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;double mHeartHP=-1;bool mCriticalWarningSent=false;
 RoomDungeonTemple(GameMap* m,Seat* s):Room(m,s){}
 INLINE_METHODS
 static const double HEART_HP_PER_TILE;double getHeartMaxHP()const;
 bool canAttackHeart(Tile*,Seat*)const;double getHP(Tile*)const;
 double takeHeartDamage(GameEntity*,double,double,double,double,Tile*);
 bool removeCoveredTile(Tile*)override;void doUpkeep()override;
 void exportToStream(std::ostream&)const override;bool importFromStream(std::istream&)override;
};
struct PersistentObject:BuildingObject {
 PersistentObject(GameMap*,Room&,const char*,Tile* t,double,bool):BuildingObject(t){}
};
METHODS
HEART_OBJECT;
int main(){int checks=0,failures=0;
 auto check=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}};
 Player ownerPlayer;Seat owner{1,&ownerPlayer},ally{1},enemy{2,nullptr,5};Tile centre,floor;BuildingObject object{&centre};
 GameMap map;RoomDungeonTemple heart(&map,&owner);DungeonHeartObject core(&map,heart,&centre);
 heart.mTempleObject=&core;heart.mCoveredTiles={&centre,&floor};
 check(core.getSeat()==&owner,"heart entity carries room ownership");
 check(RoomDungeonTemple::HEART_HP_PER_TILE==10000&&RoomDungeonTemple(&map,&owner).getHeartMaxHP()==90000,"10000 per tile, 90000 for a 3 by 3 heart");
 check(heart.getHP(nullptr)==20000&&heart.floorHP==250,"an undamaged heart has 10000 per room tile, independent of the floor durability");
 for(Seat* seat:{&owner,&ally,&enemy,static_cast<Seat*>(nullptr)}){
  GameEntity attacker{seat};
  check(!heart.canSeatSellBuilding(seat),"no gameplay demolition permission");
  check(!heart.isAttackable(&floor,seat)&&!heart.isAttackable(&centre,seat),"room floor is not a combat target");
  check(heart.takeDamage(&attacker,999,999,999,999,&floor,false)==0,"direct floor damage rejected");
  check(!heart.canAttackHeart(&floor,seat),"outer floor is not the heart");
  check(heart.canAttackHeart(&centre,seat)==(seat==&enemy),"only enemies can attack the heart");
  if(seat!=&enemy)check(heart.takeHeartDamage(&attacker,999,999,999,999,&centre)==0,"friendly and unknown damage rejected");
 }
 check(!heart.removeCoveredTile(&floor)&&heart.removed==0,"server refuses floor removal");
 GameEntity attacker{&enemy};
 check(heart.takeHeartDamage(nullptr,99,0,0,0,&centre)==0,"unattributed damage rejected");
 check(heart.takeHeartDamage(&attacker,99,0,0,0,&floor)==0,"enemy cannot damage the floor");
 check(core.takeDamage(&attacker,4,5,6,7,&centre,false)==16,"actual heart entity receives damage");
 check(heart.getHP(nullptr)==19984&&heart.floorHP==250,"heart damage never reduces floor health");
 check(heart.dead==0&&map.fights==1,"nonlethal hit reports combat without death");
 heart.doUpkeep();check(heart.removed==0&&heart.upkeep==1,"living heart retains all floor tiles");
 std::stringstream save;heart.exportToStream(save);save<<"[/Room]\n";
 RoomDungeonTemple loaded(&map,&owner);check(loaded.importFromStream(save)&&loaded.getHP(nullptr)==19984,"damaged heart round trip");
 std::string next;save>>next;check(next=="[/Room]","save parser preserves room boundary");
 std::stringstream legacy("80\n[/Room]\n");RoomDungeonTemple old(&map,&owner);
 check(old.importFromStream(legacy)&&old.getHP(nullptr)==90000,"a heart without a health record starts undamaged");
 legacy>>next;check(next=="[/Room]","legacy boundary not consumed");
 check(save.str().find("HeartHealth 19984")!=std::string::npos,"the health is saved as HeartHealth on the heart's own scale");
 // Saves from before the heart had its own health: HeartHP was measured against the floor durability (250 here)
 {std::stringstream full("250\nHeartHP 250\n[/Room]\n");RoomDungeonTemple h(&map,&owner);
  check(h.importFromStream(full)&&h.getHP(nullptr)==90000,"an old save with a full heart (HeartHP equal to the floor durability) loads full");}
 {std::stringstream half("250\nHeartHP 125\n[/Room]\n");RoomDungeonTemple h(&map,&owner);
  check(h.importFromStream(half)&&h.getHP(nullptr)==45000,"an old half damaged heart keeps its share of the health");}
 {std::stringstream ruin("250\nHeartHP 0\n[/Room]\n");RoomDungeonTemple h(&map,&owner);
  check(h.importFromStream(ruin)&&h.getHP(nullptr)==0,"an old destroyed heart stays destroyed");}
 {std::stringstream big("250\nHeartHealth 999999\n[/Room]\n");RoomDungeonTemple h(&map,&owner);
  check(h.importFromStream(big)&&h.getHP(nullptr)==90000,"a saved health above the maximum is limited to it");}
 for(const char* text:{"250\nHeartHP -1\n", "250\nHeartHP nan\n", "250\nHeartHP nope\n", "250\nHeartHealth -1\n", "250\nHeartSomething 5\n"}){
  std::stringstream bad(text);RoomDungeonTemple invalid(&map,&owner);check(!invalid.importFromStream(bad),"invalid health rejected");}
 {Creature worker(&enemy,true);Creature fighter(&enemy,false);
  check(heart.takeHeartDamage(&worker,999,999,999,999,&centre)==0&&heart.getHP(nullptr)==19984,"an enemy worker cannot damage the heart");
  check(heart.takeHeartDamage(&fighter,4,0,0,0,&centre)==4&&heart.getHP(nullptr)==19980,"an enemy fighter damages the heart");}
 check(core.takeDamage(&attacker,99999,0,0,0,&centre,false)==19980,"lethal damage clamped");
 check(core.deaths==1&&core.getHP(nullptr)==0,"object death listeners notified");
 check(heart.dead==1&&!heart.canAttackHeart(&centre,&enemy),"death fires once and disables targeting");
 check(ownerPlayer.recorded==1&&ownerPlayer.conqueror==5&&ownerPlayer.heartX==3&&ownerPlayer.heartY==4,"owner records conqueror seat and heart tile on death");
 check(heart.takeHeartDamage(&attacker,99999,0,0,0,&centre)==0&&heart.dead==1,"dead heart cannot be hit twice");
 check(ownerPlayer.recorded==1,"conqueror recorded only once");
 check(enemy.stats.mKeepersDefeated==1&&owner.stats.mKeepersDefeated==0,"final blow counts one defeated keeper for the attacker seat only, once");
 heart.doUpkeep();check(heart.removed==0&&heart.mCoveredTiles.size()==2&&heart.objectsRemoved==1&&heart.mTempleObject==nullptr,"heart death releases only the heart object and keeps the floor");
 // Critical-health warning (threshold 11 % of 90000 = 9900)
 ODServer& server=ODServer::getSingleton();GameEntity hitter{&enemy};
 const char* warning="Your dungeon heart is in critical condition!";
 {server.queue.clear();BuildingObject obj{&centre};RoomDungeonTemple h(&map,&owner);h.mTempleObject=&obj;
  h.takeHeartDamage(&hitter,80000,0,0,0,&centre);
  h.takeHeartDamage(&hitter,99.9,0,0,0,&centre);
  check(server.queue.empty(),"no warning above 11 percent");
  h.takeHeartDamage(&hitter,0.2,0,0,0,&centre);
  check(server.queue.size()==1,"exactly one warning at the threshold");
  if(server.queue.size()==1){ServerNotification* n=server.queue[0];
   check(n->type==ServerNotificationType::chatServer&&n->player==&ownerPlayer,"warning goes to the owner only");
   check(n->mPacket.texts.size()==1&&n->mPacket.texts[0]==warning&&n->mPacket.majorEvents==1,"warning text and event type");}
  h.takeHeartDamage(&hitter,1,0,0,0,&centre);h.takeHeartDamage(&hitter,1,0,0,0,&centre);
  check(server.queue.size()==1,"no second warning on further hits");
  h.takeHeartDamage(&hitter,99999,0,0,0,&centre);
  check(h.dead==1&&server.queue.size()==1,"killing hit sends no extra warning");}
 {server.queue.clear();BuildingObject obj{&centre};RoomDungeonTemple h(&map,&owner);h.mTempleObject=&obj;h.mHeartHP=20;
  h.takeHeartDamage(&hitter,1,0,0,0,&centre);
  check(server.queue.size()==1,"first hit while already below the threshold warns once");
  h.takeHeartDamage(&hitter,1,0,0,0,&centre);
  check(server.queue.size()==1,"no repeat after a low first hit");}
 {server.queue.clear();BuildingObject obj{&centre};RoomDungeonTemple h(&map,&owner);h.mTempleObject=&obj;h.mHeartHP=20;
  h.takeHeartDamage(&hitter,999,0,0,0,&centre);
  check(h.dead==1&&server.queue.empty(),"killing hit from below the threshold sends no warning");}
 {server.queue.clear();BuildingObject obj{&centre};RoomDungeonTemple h(&map,&owner);h.mTempleObject=&obj;
  h.takeHeartDamage(&hitter,99999,0,0,0,&centre);
  check(h.dead==1&&server.queue.empty(),"one-blow kill from full health sends no warning");}
 {Player bot;bot.human=false;Player loser;loser.lost=true;Seat botSeat{1,&bot},lostSeat{1,&loser},emptySeat{1};
  for(Seat* seat:{&botSeat,&lostSeat,&emptySeat}){
   server.queue.clear();BuildingObject obj{&centre};RoomDungeonTemple h(&map,seat);h.mTempleObject=&obj;h.mHeartHP=20;
   h.takeHeartDamage(&hitter,1,0,0,0,&centre);
   check(server.queue.empty(),"no warning for a non-human, lost or absent owner");}}
 {server.queue.clear();map.editor=true;BuildingObject obj{&centre};RoomDungeonTemple h(&map,&owner);h.mTempleObject=&obj;h.mHeartHP=20;
  h.takeHeartDamage(&hitter,1,0,0,0,&centre);map.editor=false;
  check(server.queue.empty(),"no warning in editor mode");}
 {Player p;Seat s{1,&p};BuildingObject noTile{nullptr};RoomDungeonTemple h(&map,&s);h.mTempleObject=&noTile;
  h.takeHeartDamage(&hitter,99999,0,0,0,nullptr);check(p.recorded==1&&p.conqueror==5&&p.heartX==-1&&p.heartY==-1,"unknown centre tile is recorded as -1/-1");}
 map.editor=true;RoomDungeonTemple edit(&map,&owner);edit.mCoveredTiles={&floor};
 check(edit.removeCoveredTile(&floor),"editor editing preserved");
 std::stringstream level;edit.exportToStream(level);check(level.str().find("Heart")==std::string::npos,"editor maps do not persist combat damage");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
inline = '\n'.join(function(header, sig) for sig in
                   ('bool canSeatSellBuilding(', 'bool isAttackable(', 'double takeDamage('))
methods = '\n'.join(function(source, sig) for sig in (
    'double RoomDungeonTemple::getHP(', 'double RoomDungeonTemple::getHeartMaxHP(', 'bool RoomDungeonTemple::canAttackHeart(',
    'double RoomDungeonTemple::takeHeartDamage(', 'bool RoomDungeonTemple::removeCoveredTile(',
    'void RoomDungeonTemple::doUpkeep(', 'void RoomDungeonTemple::exportToStream(',
    'bool RoomDungeonTemple::importFromStream('))
methods = 'const double RoomDungeonTemple::HEART_HP_PER_TILE = ' + \
    source.split('const double RoomDungeonTemple::HEART_HP_PER_TILE = ')[1].split(';')[0] + ';\n' + methods
probe = probe.replace('INLINE_METHODS', inline).replace('METHODS', methods)
probe = probe.replace('HEART_OBJECT', function(source, 'class DungeonHeartObject :'))
with tempfile.TemporaryDirectory(prefix='odp-heart-combat-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
