"""Exercise the production destroyed-heart (ruin) path: the platform stays, the heart object goes."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text()


temple = read('source/rooms/RoomDungeonTemple.cpp')
temple_header = read('source/rooms/RoomDungeonTemple.h')
player = read('source/game/Player.cpp')
player_header = read('source/game/Player.h')
game_map = read('source/gamemap/GameMap.cpp')
seat = read('source/game/Seat.cpp')
seat_data = read('source/game/SeatData.cpp')
tile = read('source/entities/Tile.cpp')
building = read('source/entities/Building.cpp')


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
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
int errorsLogged = 0;
#define OD_LOG_ERR(x) ++errorsLogged
#define OD_LOG_INF(x)
namespace Helper {template<typename T> std::string toString(T v){return std::to_string(v);}}
enum class ServerNotificationType {chatServer, playerDefeated, levelStatistics};
enum class EventShortNoticeType {majorGameEvent};
enum class RoomType {dungeonTemple, other, nbRooms};
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
struct Tile{int getX()const{return 3;}int getY()const{return 4;}static std::string displayAsString(const Tile*){return "tile";}};
struct Seat;
struct TileData{std::vector<Seat*> mSeatsVision;};
struct GameMap;
struct Room;
struct Seat {
 int team;int id;Player* mPlayer=nullptr;GameMap* mGameMap=nullptr;void* mCurrentSkill=nullptr;SeatStatistics stats;
 std::vector<uint32_t> mNbRooms=std::vector<uint32_t>(static_cast<uint32_t>(RoomType::nbRooms),0);
 Seat(int t,int i):team(t),id(i){}
 void addSkillPoints(int){}
 SeatStatistics& getStatistics(){return stats;}Player* getPlayer(){return mPlayer;}int getId()const{return id;}
 bool isRogueSeat()const{return id==0;}bool isAlliedSeat(Seat* s){return s&&team==s->team;}
 void computeSeatBeginTurn();uint32_t getNbRooms(RoomType roomType) const;};
struct Player {
 Player(Seat* s,bool h):mSeat(s),mIsHuman(h){}
 Seat* mSeat;bool mIsHuman;bool mHasLost=false;GameMap* mGameMap=nullptr;
 int32_t mConquerorSeatId=-1,mDefeatHeartTileX=-1,mDefeatHeartTileY=-1;
 Seat* getSeat(){return mSeat;}bool getIsHuman()const{return mIsHuman;}bool getHasLost()const{return mHasLost;}
 RECORD
 void notifyNoMoreDungeonTemple();
};
enum class GameEntityType {creature, other};
struct CreatureDefinition {bool isWorker()const{return false;}};
struct GameEntity {Seat* seat;int deaths=0;GameEntity(Seat* s=nullptr):seat(s){}virtual ~GameEntity()=default;
 virtual GameEntityType getObjectType()const{return GameEntityType::other;}
 Seat* getSeat(){return seat;}void setSeat(Seat* s){seat=s;}void fireEntityDead(){++deaths;}
 virtual bool isAttackable(Tile*,Seat*)const{return false;}virtual double getHP(Tile*)const{return 0;}
 virtual double takeDamage(GameEntity*,double,double,double,double,Tile*,bool){return 0;}};
struct Creature:GameEntity {CreatureDefinition definition;const CreatureDefinition* getDefinition()const{return &definition;}};
struct Building {double floorHP=250;virtual ~Building()=default;virtual double getHP(Tile*)const{return floorHP;}};
struct BuildingObject;
struct Room:Building {
 GameMap* map;Seat* seat;int dead=0,doUpkeeps=0,restored=0,added=0,objectsRemoved=0;
 std::vector<Tile*> mCoveredTiles,mCoveredTilesDestroyed;std::map<Tile*,BuildingObject*> mBuildingObjects;
 std::map<Tile*,TileData*> mTileData;Tile* central=nullptr;
 Room(GameMap* m,Seat* s):map(m),seat(s){}
 GameMap* getGameMap()const{return map;}Seat* getSeat()const{return seat;}
 double getPhysicalDefense(){return 1;}double getMagicalDefense(){return 2;}double getElementDefense(){return 3;}
 void fireEntityDead(){++dead;}
 virtual RoomType getType()const{return RoomType::other;}
 virtual bool canSeatSellBuilding(Seat*)const{return true;}
 virtual bool isAttackable(Tile*,Seat*)const{return true;}
 virtual double takeDamage(GameEntity*,double,double,double,double,Tile*,bool){return 99;}
 virtual bool removeCoveredTile(Tile* t){
  mCoveredTiles.erase(std::remove(mCoveredTiles.begin(),mCoveredTiles.end(),t),mCoveredTiles.end());mCoveredTilesDestroyed.push_back(t);return true;}
 virtual void doUpkeep(){++doUpkeeps;}
 virtual void restoreInitialEntityState(){++restored;}
 virtual void exportToStream(std::ostream& os)const{os<<floorHP<<'\n';}
 virtual bool importFromStream(std::istream& is){return bool(is>>floorHP);}
 bool getIsOnServerMap()const{return true;}Tile* getCentralTile(){return central;}
 uint32_t numCoveredTiles()const{return static_cast<uint32_t>(mCoveredTiles.size());}
 void addBuildingObject(Tile* t,BuildingObject* o){++added;mBuildingObjects[t]=o;}
 void removeAllBuildingObjects(){objectsRemoved+=static_cast<int>(mBuildingObjects.size());mBuildingObjects.clear();}
};
struct PlainRoom:Room {PlainRoom(GameMap* m,Seat* s):Room(m,s){}};
struct GameMap {bool editor=false;int fights=0,sounds=0;std::vector<Room*> mRooms;std::vector<Seat*> seats;
 bool isInEditorMode(){return editor;}void playerIsFighting(Player*,Tile*){++fights;}
 int64_t getTurnNumber()const{return 0;}std::vector<Seat*>& getSeats(){return seats;}std::vector<Room*>& getRooms(){return mRooms;}
 void fireRelativeSound(std::vector<Seat*>&,SoundRelativeKeeperStatements){++sounds;}
 std::vector<Room*> getRoomsByType(RoomType type) const;unsigned int numRoomsByTypeAndSeat(RoomType type, const Seat* seat) const;};
bool g_removeAllowed=true;int g_removeAsked=0;
struct BuildingObject:GameEntity {Tile* tile;BuildingObject(Tile* t):tile(t){}Tile* getPositionTile(){return tile;}
 virtual bool notifyRemoveAsked(){++g_removeAsked;return g_removeAllowed;}
 void notifySeatsWithVision(const std::vector<Seat*>&){}};
struct PersistentObject:BuildingObject {
 PersistentObject(GameMap*,Room&,const char*,Tile* t,double,bool):BuildingObject(t){}
};
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;double mHeartHP=-1;bool mCriticalWarningSent=false;
 RoomDungeonTemple(GameMap* m,Seat* s):Room(m,s){}
 RoomType getType() const override{return RoomType::dungeonTemple;}
 INLINE_METHODS
 static const double HEART_HP_PER_TILE;double getHeartMaxHP()const;
 bool canAttackHeart(Tile*,Seat*)const;double getHP(Tile*)const override;
 double takeHeartDamage(GameEntity*,double,double,double,double,Tile*);
 bool removeCoveredTile(Tile*)override;void doUpkeep()override;
 void exportToStream(std::ostream&)const override;bool importFromStream(std::istream&)override;
 void updateActiveSpots(GameMap* gameMap);void updateTemplePosition();void restoreInitialEntityState()override;
};
HEART_OBJECT;
METHODS
int checks=0,failures=0;
void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}}
int count(ODServer& server,Player* p,ServerNotificationType t){int n=0;for(ServerNotification* s:server.queue)if(s->player==p&&s->type==t)++n;return n;}
void turn(GameMap& map,Seat& seat){
 // Same order as GameMap::doMiscUpkeep: entity upkeep first, then the per-seat temple check
 for(Room* room:map.mRooms)room->doUpkeep();
 seat.computeSeatBeginTurn();
 if(seat.getNbRooms(RoomType::dungeonTemple)==0)seat.mPlayer->notifyNoMoreDungeonTemple();
}
void addFloor(RoomDungeonTemple& heart,Tile* centre,Tile* floor,TileData* centreData,TileData* floorData){
 heart.central=centre;heart.mCoveredTiles.push_back(centre);heart.mCoveredTiles.push_back(floor);
 heart.mTileData[centre]=centreData;heart.mTileData[floor]=floorData;
}
int main(){
 ODServer& server=ODServer::getSingleton();
 GameMap map;Seat owner(1,1),enemy(2,5);Player ownerPlayer(&owner,true),enemyPlayer(&enemy,false);
 owner.mPlayer=&ownerPlayer;owner.mGameMap=&map;enemy.mPlayer=&enemyPlayer;enemy.mGameMap=&map;
 ownerPlayer.mGameMap=&map;enemyPlayer.mGameMap=&map;map.seats.push_back(&owner);map.seats.push_back(&enemy);
 Tile centre,floor;TileData centreData,floorData;
 RoomDungeonTemple heart(&map,&owner);addFloor(heart,&centre,&floor,&centreData,&floorData);
 PlainRoom other(&map,&owner);map.mRooms.push_back(&other);map.mRooms.push_back(&heart);

 // Living heart: object created once, counted as a temple, floor untouched
 heart.updateActiveSpots(&map);
 check(heart.added==1&&heart.mBuildingObjects.size()==1&&heart.mTempleObject!=nullptr,"living heart creates its object once");
 check(heart.mTempleObject->getPositionTile()==&centre,"heart object stands on the central tile");
 turn(map,owner);
 check(owner.getNbRooms(RoomType::dungeonTemple)==1&&owner.getNbRooms(RoomType::other)==1,"living heart counts as the seat temple");
 check(heart.objectsRemoved==0&&heart.mCoveredTiles.size()==2&&g_removeAsked==0,"living heart keeps object and floor");
 check(server.queue.empty(),"no defeat traffic while the heart lives");

 // The heart dies
 GameEntity attacker(&enemy);
 check(heart.takeHeartDamage(&attacker,99999,0,0,0,&centre)==20000,"lethal blow clamped to the heart health (10000 per room tile)");
 check(heart.dead==1&&heart.getHP(nullptr)==0,"heart death fires once");
 owner.computeSeatBeginTurn();
 check(owner.getNbRooms(RoomType::dungeonTemple)==0,"seat has no temple as soon as the heart is dead");
 check(map.getRoomsByType(RoomType::dungeonTemple).empty()&&map.numRoomsByTypeAndSeat(RoomType::dungeonTemple,&owner)==0,"dead heart is invisible to temple queries");
 check(map.numRoomsByTypeAndSeat(RoomType::other,&owner)==1,"other rooms are still counted");

 // Upkeep turns after the death: object released once, platform stays
 turn(map,owner);turn(map,owner);turn(map,owner);
 check(heart.objectsRemoved==1&&heart.mBuildingObjects.empty()&&heart.mTempleObject==nullptr,"heart object released exactly once");
 check(heart.mCoveredTiles.size()==2&&heart.mCoveredTilesDestroyed.empty(),"platform tiles stay covered by the ruin room");
 check(!heart.removeCoveredTile(&floor)&&!heart.removeCoveredTile(&centre)&&heart.mCoveredTiles.size()==2,"ruin floor cannot be released in game mode");
 check(heart.doUpkeeps==3+1,"generic room upkeep still runs");
 heart.updateActiveSpots(&map);heart.updateActiveSpots(&map);
 check(heart.added==1&&heart.mBuildingObjects.empty()&&heart.mTempleObject==nullptr,"ruin never gets a new heart object");
 check(heart.mHeartHP==0&&owner.getNbRooms(RoomType::dungeonTemple)==0,"ruin still counts as no temple");

 // Not attackable
 Seat* whos[3]={&owner,&enemy,static_cast<Seat*>(nullptr)};
 for(int i=0;i<3;++i){
  Seat* who=whos[i];
  GameEntity hitter(who);
  check(!heart.canAttackHeart(&centre,who)&&!heart.canAttackHeart(&floor,who),"ruin is not a heart target");
  check(!heart.isAttackable(&centre,who)&&!heart.isAttackable(&floor,who),"ruin floor is not a room target");
  check(heart.takeHeartDamage(&hitter,999,999,999,999,&centre)==0&&heart.takeDamage(&hitter,999,999,999,999,&centre,false)==0,"ruin takes no damage");
  check(!heart.canSeatSellBuilding(who),"ruin cannot be sold");
 }
 check(heart.dead==1&&enemy.stats.mKeepersDefeated==1,"no second death and one defeated keeper");

 // Defeat chain fires exactly as before, once
 check(count(server,&ownerPlayer,ServerNotificationType::playerDefeated)==1,"playerDefeated sent exactly once");
 check(count(server,&ownerPlayer,ServerNotificationType::levelStatistics)==1,"levelStatistics sent exactly once");
 check(count(server,&ownerPlayer,ServerNotificationType::chatServer)==1,"defeat chat text sent once");
 check(count(server,&enemyPlayer,ServerNotificationType::playerDefeated)==0,"the killer is not defeated");
 int found=0;
 for(size_t i=0;i<server.queue.size();++i){
  ServerNotification* n=server.queue[i];
  if(n->type!=ServerNotificationType::playerDefeated)continue;
  ++found;
  check(n->mPacket.ints.size()==3&&n->mPacket.ints[0]==5&&n->mPacket.ints[1]==3&&n->mPacket.ints[2]==4,"defeat payload keeps conqueror and heart tile");}
 check(found==1,"one defeat packet in total");

 // Save and load of the destroyed state
 std::stringstream save;heart.exportToStream(save);save<<"[/Room]\n";
 check(save.str().find("HeartHealth 0")!=std::string::npos,"destroyed state is written with the room");
 RoomDungeonTemple loaded(&map,&owner);TileData loadedCentre,loadedFloor;addFloor(loaded,&centre,&floor,&loadedCentre,&loadedFloor);
 check(loaded.importFromStream(save)&&loaded.getHP(nullptr)==0,"destroyed heart loads");
 std::string next;save>>next;check(next=="[/Room]","load keeps the room boundary");
 map.mRooms.clear();map.mRooms.push_back(&other);map.mRooms.push_back(&loaded);
 int errorsBefore=errorsLogged;
 loaded.updateActiveSpots(&map);loaded.restoreInitialEntityState();
 check(loaded.added==0&&loaded.mTempleObject==nullptr&&loaded.mBuildingObjects.empty(),"loaded ruin gets no heart object");
 check(loaded.restored==1&&errorsLogged==errorsBefore,"loaded ruin restores its floor without an error");
 int chatBefore=count(server,&ownerPlayer,ServerNotificationType::chatServer);
 ownerPlayer.mHasLost=false;
 turn(map,owner);
 check(owner.getNbRooms(RoomType::dungeonTemple)==0&&loaded.mCoveredTiles.size()==2&&loaded.objectsRemoved==0,"loaded ruin counts as no temple and keeps its floor");
 check(count(server,&ownerPlayer,ServerNotificationType::chatServer)==chatBefore+1,"a loaded game with a ruin runs the defeat chain once, as it did without a room");

 // A damaged living heart still loads with its object
 {RoomDungeonTemple living(&map,&owner);TileData livingCentre,livingFloor;addFloor(living,&centre,&floor,&livingCentre,&livingFloor);
  std::stringstream damaged("250\nHeartHealth 100\n[/Room]\n");
  check(living.importFromStream(damaged)&&living.getHP(nullptr)==100,"damaged heart round trip");
  int asked=g_removeAsked;living.updateActiveSpots(&map);living.restoreInitialEntityState();
  check(living.added==1&&living.mTempleObject!=nullptr&&living.restored==1,"living loaded heart gets its object back");
  check(g_removeAsked==asked&&errorsLogged==errorsBefore,"living loaded heart is not asked to disappear");}

 // Removal is retried while a seat that saw the heart has no vision on it
 {map.mRooms.clear();RoomDungeonTemple late(&map,&owner);TileData lateCentre,lateFloor;addFloor(late,&centre,&floor,&lateCentre,&lateFloor);
  map.mRooms.push_back(&late);late.updateActiveSpots(&map);
  g_removeAllowed=false;late.takeHeartDamage(&attacker,99999,0,0,0,&centre);
  late.doUpkeep();late.doUpkeep();
  check(late.objectsRemoved==0&&late.mTempleObject!=nullptr&&late.mCoveredTiles.size()==2,"object stays while removal is refused");
  g_removeAllowed=true;late.doUpkeep();late.doUpkeep();
  check(late.objectsRemoved==1&&late.mTempleObject==nullptr&&late.mCoveredTiles.size()==2,"object released once removal is allowed, floor kept");}

 // Editor is unchanged
 {map.editor=true;RoomDungeonTemple edit(&map,&owner);TileData editCentre,editFloor;addFloor(edit,&centre,&floor,&editCentre,&editFloor);
  check(edit.removeCoveredTile(&floor)&&edit.mCoveredTiles.size()==1,"editor can still remove floor tiles");
  edit.updateActiveSpots(&map);check(edit.added==1&&edit.mTempleObject!=nullptr,"editor still places the heart object");
  std::stringstream level;edit.exportToStream(level);check(level.str().find("Heart")==std::string::npos,"editor maps do not persist heart health");
  map.editor=false;}
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
inline = '\n'.join(function(temple_header, sig) for sig in
                   ('bool canSeatSellBuilding(', 'bool isAttackable(', 'double takeDamage('))
methods = 'const double RoomDungeonTemple::HEART_HP_PER_TILE = ' + \
    temple.split('const double RoomDungeonTemple::HEART_HP_PER_TILE = ')[1].split(';')[0] + ';\n'
methods += '\n'.join(function(temple, sig) for sig in (
    'double RoomDungeonTemple::getHP(', 'double RoomDungeonTemple::getHeartMaxHP(', 'bool RoomDungeonTemple::canAttackHeart(',
    'double RoomDungeonTemple::takeHeartDamage(', 'bool RoomDungeonTemple::removeCoveredTile(',
    'void RoomDungeonTemple::doUpkeep(', 'void RoomDungeonTemple::exportToStream(',
    'bool RoomDungeonTemple::importFromStream(', 'void RoomDungeonTemple::updateActiveSpots(',
    'void RoomDungeonTemple::updateTemplePosition(', 'void RoomDungeonTemple::restoreInitialEntityState('))
methods += '\n' + '\n'.join([
    function(game_map, 'std::vector<Room*> GameMap::getRoomsByType('),
    function(game_map, 'unsigned int GameMap::numRoomsByTypeAndSeat('),
    function(seat, 'void Seat::computeSeatBeginTurn('),
    function(seat_data, 'uint32_t SeatData::getNbRooms(').replace('SeatData::', 'Seat::'),
    function(player, 'void Player::notifyNoMoreDungeonTemple('),
])
probe = probe.replace('INLINE_METHODS', inline).replace('RECORD', function(player_header, 'inline void recordHeartDestroyed('))
probe = probe.replace('HEART_OBJECT', function(temple, 'class DungeonHeartObject :'))
probe = probe.replace('METHODS', methods)

# Static wiring checks on the production sources the fixture does not execute.
assert temple.count('Room::removeCoveredTile(tile);') == 1, 'the heart floor must only be released through removeCoveredTile'
assert 'if(seat->getNbRooms(RoomType::dungeonTemple) == 0)' in game_map
assert 'seat->getPlayer()->notifyNoMoreDungeonTemple();' in game_map
assert game_map.count('room->getHP(nullptr) > 0.0') >= 3, 'temple queries must skip dead rooms'
assert 'if(room->getHP(nullptr) <= 0.0)' in function(seat, 'void Seat::computeSeatBeginTurn(')
assert tile.count('mTileVisual = TileVisual::dungeonTempleRoom;') == 2, 'the floor emblem follows the covering room'
assert 'mCoveredTiles.empty()' not in function(building, 'void Building::doUpkeep(')
print('WIRING OK: platform look follows the covering room, dead rooms are skipped by every temple count')

with tempfile.TemporaryDirectory(prefix='odp-heart-ruin-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
