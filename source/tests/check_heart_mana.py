"""Exercise the production mana path: the income formula, the worker upkeep, the worker
redemption on a heart drop, and the remaining-mana transfer on a single player defeat."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text()


temple = read('source/rooms/RoomDungeonTemple.cpp')
player = read('source/game/Player.cpp')
player_header = read('source/game/Player.h')
game_map = read('source/gamemap/GameMap.cpp')
seat = read('source/game/Seat.cpp')
seat_data = read('source/game/SeatData.cpp')


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


probe = r"""
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
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
enum class NodeType {MTILES_NODE};
enum class GameEntityType {creature, other};
struct ODPacket {std::vector<std::string> texts;std::vector<int32_t> ints;
 ODPacket& operator<<(const char* t){texts.push_back(t);return *this;}
 ODPacket& operator<<(const std::string& t){texts.push_back(t);return *this;}
 ODPacket& operator<<(EventShortNoticeType){return *this;}
 ODPacket& operator<<(int32_t v){ints.push_back(v);return *this;}
 ODPacket& operator<<(uint32_t){return *this;}ODPacket& operator<<(bool){return *this;}};
struct ODApplication {static double turnsPerSecond;};double ODApplication::turnsPerSecond=4.0;
struct ConfigManager {double maxManaPerSeat=200000.0;
 static ConfigManager& getSingleton(){static ConfigManager manager;return manager;}
 double getMaxManaPerSeat()const{return maxManaPerSeat;}};
struct SeatStatistics {uint32_t mKeepersDefeated=0,mCreaturesKilled=0,mHeroesDestroyed=0,mRoomsCaptured=0,mItemsMade=0,mCreaturesConverted=0;};
struct Player;
struct GameMap;
struct ServerNotification {ServerNotificationType type;Player* player;ODPacket mPacket;
 ServerNotification(ServerNotificationType t,Player* p):type(t),player(p){}};
struct ODServer {std::vector<ServerNotification*> queue;
 static ODServer& getSingleton(){static ODServer server;return server;}
 void queueServerNotification(ServerNotification* n){queue.push_back(n);}};
struct Tile{int x=3,y=4;Tile(){}Tile(int xi,int yi):x(xi),y(yi){}
 int getX()const{return x;}int getY()const{return y;}};
struct Seat;
struct Seat {
 int team;int id;Player* mPlayer=nullptr;GameMap* mGameMap=nullptr;void* mCurrentSkill=nullptr;SeatStatistics stats;
 std::vector<uint32_t> mNbRooms=std::vector<uint32_t>(static_cast<uint32_t>(RoomType::nbRooms),0);
 double mMana=0.0,mManaDelta=0.0,mManaIncomePerSecond=0.0,mManaUpkeepPerSecond=0.0;
 unsigned int mNumClaimedTiles=0;int mNumCreaturesWorkers=0;
 Seat(int t,int i):team(t),id(i){}
 void addSkillPoints(int){}
 SeatStatistics& getStatistics(){return stats;}Player* getPlayer(){return mPlayer;}int getId()const{return id;}
 bool isRogueSeat()const{return id==0;}bool isAlliedSeat(Seat* s){return s&&team==s->team;}
 double getMana()const{return mMana;}void addMana(double mana);
 unsigned int getNumClaimedTiles()const{return mNumClaimedTiles;}
 int getNumCreaturesWorkers()const{return mNumCreaturesWorkers;}
 void computeSeatBeginTurn();uint32_t getNbRooms(RoomType roomType) const;};
struct GameEntity;
struct Player {
 Player(Seat* s,bool h):mSeat(s),mIsHuman(h){}
 Seat* mSeat;bool mIsHuman;bool mHasLost=false;GameMap* mGameMap=nullptr;
 int32_t mConquerorSeatId=-1,mDefeatHeartTileX=-1,mDefeatHeartTileY=-1;
 std::vector<GameEntity*> mObjectsInHand;
 Seat* getSeat(){return mSeat;}bool getIsHuman()const{return mIsHuman;}bool getHasLost()const{return mHasLost;}
 RECORD
 void removeEntityFromHand(GameEntity* entity);
 bool redemWorkerInHeart(GameEntity* entity,Tile* tile);
 void notifyNoMoreDungeonTemple();
};
struct CreatureDefinition {bool worker=false;bool isWorker()const{return worker;}};
struct GameEntity {Seat* seat;int deaths=0;
 static int removed,removedFromTile,removedFromMap,removedVision;
 GameEntity(Seat* s=nullptr):seat(s){}virtual ~GameEntity()=default;
 virtual GameEntityType getObjectType()const{return GameEntityType::other;}
 Seat* getSeat(){return seat;}void setSeat(Seat* s){seat=s;}void fireEntityDead(){++deaths;}
 void fireRemoveEntityToSeatsWithVision(GameMap* g=nullptr){(void)g;++removedVision;}
 void removeEntityFromPositionTile(GameMap* g=nullptr){(void)g;++removedFromTile;}
 void removeFromGameMap(GameMap* g=nullptr){(void)g;++removedFromMap;}
 void deleteYourself(GameMap* g=nullptr,NodeType nt=NodeType::MTILES_NODE){(void)g;(void)nt;++removed;}};
int GameEntity::removed=0,GameEntity::removedFromTile=0,GameEntity::removedFromMap=0,GameEntity::removedVision=0;
struct Creature:GameEntity {using GameEntity::GameEntity;
 GameEntityType getObjectType()const override{return GameEntityType::creature;}
 CreatureDefinition definition;const CreatureDefinition* getDefinition()const{return &definition;}};
struct BuildingObject;
struct Room {Seat* seat;double hp=250.0;std::vector<Tile*> mCoveredTiles;
 Room(Seat* s):seat(s){}virtual ~Room()=default;
 Seat* getSeat()const{return seat;}virtual RoomType getType()const{return RoomType::other;}
 virtual double getHP(Tile*)const{return hp;}
 uint32_t numCoveredTiles()const{return static_cast<uint32_t>(mCoveredTiles.size());}};
struct PlainRoom:Room {PlainRoom(Seat* s):Room(s){}};
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;double mHeartHP=10000.0;
 RoomDungeonTemple(Seat* s):Room(s){}
 RoomType getType()const override{return RoomType::dungeonTemple;}
 double getHP(Tile*)const override{return mHeartHP;}
 Tile* getHeartTile() const;
};
struct GameMap {std::vector<Room*> mRooms;std::vector<Seat*> seats;
 std::vector<Room*>& getRooms(){return mRooms;}std::vector<Seat*>& getSeats(){return seats;}
 int64_t getTurnNumber()const{return 0;}NodeType getNodeType()const{return NodeType::MTILES_NODE;}
 void fireRelativeSound(std::vector<Seat*>&,SoundRelativeKeeperStatements){}
 std::vector<Room*> getRoomsByType(RoomType type) const;void updateSeatMana(Seat* seat);};
struct SpellSummonWorker {static int32_t nextPrice;
 static int32_t getNextWorkerPriceForPlayer(GameMap*,Player*){return nextPrice;}};
int32_t SpellSummonWorker::nextPrice=1500;
struct BuildingObject:GameEntity {Tile* tile;BuildingObject(Tile* t):GameEntity(nullptr),tile(t){}
 Tile* getPositionTile(){return tile;}};
MANA_HELPERS
METHODS
int checks=0,failures=0;
void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}}
int count(ODServer& server,Player* p,ServerNotificationType t){int n=0;
 for(ServerNotification* s:server.queue)if(s->player==p&&s->type==t)++n;return n;}
bool near(double a,double b){return std::fabs(a-b)<=0.0001;}
int main(){
 ODServer& server=ODServer::getSingleton();
 ConfigManager& config=ConfigManager::getSingleton();

 // The income formula, below and above the tile cap, the heart tiles never counted twice
 check(manaIncomePerSecond(0,0)==30.0,"the heart alone produces 30 per second");
 check(manaIncomePerSecond(100,0)==130.0,"one mana per second per claimed tile");
 check(manaIncomePerSecond(500,0)==530.0,"the tile part reaches the cap at 500 tiles");
 check(manaIncomePerSecond(600,0)==530.0,"the tile part stays capped above 500 tiles");
 check(manaIncomePerSecond(9,9)==30.0,"a 3x3 heart yields 30, not 30 plus its 9 tiles");
 check(manaIncomePerSecond(100,9)==121.0,"the heart tiles are not counted again");
 check(manaIncomePerSecond(509,9)==530.0,"the cap applies after the heart tiles are removed");
 check(manaIncomePerSecond(3,9)==30.0,"fewer claimed tiles than heart tiles is just the heart");

 // The worker upkeep
 check(manaUpkeepPerSecond(0)==0.0,"no worker costs nothing");
 check(manaUpkeepPerSecond(1)==7.0,"one worker costs 7 per second");
 check(manaUpkeepPerSecond(4)==28.0,"four workers cost 28 per second");

 // A seat with a living heart
 GameMap map;
 Seat owner(1,1),enemy(2,5);
 Player ownerPlayer(&owner,true),enemyPlayer(&enemy,false);
 owner.mPlayer=&ownerPlayer;owner.mGameMap=&map;
 enemy.mPlayer=&enemyPlayer;enemy.mGameMap=&map;
 ownerPlayer.mGameMap=&map;enemyPlayer.mGameMap=&map;
 map.seats.push_back(&owner);map.seats.push_back(&enemy);
 Tile centre;
 RoomDungeonTemple heart(&owner);
 Tile floorTiles[9];
 for(int i=0;i<9;++i)heart.mCoveredTiles.push_back(&floorTiles[i]);
 heart.mTempleObject=new BuildingObject(&centre);
 PlainRoom storage(&owner);
 map.mRooms.push_back(&storage);map.mRooms.push_back(&heart);
 owner.mNumClaimedTiles=9+100;owner.mNumCreaturesWorkers=4;

 owner.computeSeatBeginTurn();
 check(owner.getNbRooms(RoomType::dungeonTemple)==1,"the living heart counts as the seat temple");
 owner.mMana=0.0;
 map.updateSeatMana(&owner);
 check(near(owner.mManaIncomePerSecond,130.0),"income is the heart plus 100 tiles, the 9 heart tiles not added");
 check(near(owner.mManaUpkeepPerSecond,28.0),"upkeep is the four workers at 7 per second");
 check(near(owner.mManaDelta,(130.0-28.0)/ODApplication::turnsPerSecond),"the delta converts the net of both to turns");
 check(near(owner.mMana,(130.0-28.0)/ODApplication::turnsPerSecond),"the accrued mana lands on the seat");

 // A seat without a heart gains nothing and loses nothing
 Seat orphan(3,9);Player orphanPlayer(&orphan,false);
 orphan.mPlayer=&orphanPlayer;orphan.mGameMap=&map;orphanPlayer.mGameMap=&map;
 orphan.mNumClaimedTiles=200;orphan.mNumCreaturesWorkers=1;
 orphan.mMana=42.0;orphan.mManaDelta=5.0;orphan.mManaIncomePerSecond=8.0;orphan.mManaUpkeepPerSecond=2.0;
 orphan.computeSeatBeginTurn();
 check(orphan.getNbRooms(RoomType::dungeonTemple)==0,"a seat without a heart has no temple");
 map.updateSeatMana(&orphan);
 check(orphan.mMana==42.0&&orphan.mManaDelta==0.0
  &&orphan.mManaIncomePerSecond==0.0&&orphan.mManaUpkeepPerSecond==0.0,
  "without a heart there is no income, no upkeep and the mana is untouched");

 // Upkeep never brings the mana below 0
 owner.mMana=1.0;owner.mNumClaimedTiles=9;owner.mNumCreaturesWorkers=8;
 map.updateSeatMana(&owner);
 check(near(owner.mMana,0.0),"the mana stops at 0, the upkeep never makes it negative");
 owner.mNumClaimedTiles=9+100;owner.mNumCreaturesWorkers=4;

 // The stored mana has a maximum
 owner.mMana=199990.0;owner.mNumClaimedTiles=500;owner.mNumCreaturesWorkers=0;
 map.updateSeatMana(&owner);
 check(near(owner.mMana,200000.0),"the stored mana is clamped at the maximum");
 owner.mNumClaimedTiles=9+100;owner.mNumCreaturesWorkers=4;

 // A worker redeemed on the heart
 Tile otherTile(7,8);
 SpellSummonWorker::nextPrice=1500;
 owner.mMana=0.0;
 Creature worker(&owner);worker.definition.worker=true;
 Creature notWorker(&owner);
 GameEntity plain(&owner);
 int removedBefore=GameEntity::removed;
 int chatBefore=count(server,&ownerPlayer,ServerNotificationType::chatServer);
 check(ownerPlayer.redemWorkerInHeart(&worker,&centre),"a human own worker on the living own heart is redeemed");
 check(near(owner.mMana,750.0),"half of the summoning price at the current worker count is paid back");
 check(GameEntity::removed==removedBefore+1&&GameEntity::removedFromTile==removedBefore+1
  &&GameEntity::removedFromMap==removedBefore+1&&GameEntity::removedVision==removedBefore+1,
  "the redeemed worker is removed from the game for every seat");
 check(count(server,&ownerPlayer,ServerNotificationType::chatServer)==chatBefore+1,"the redemption chat is sent");
 ServerNotification* lastChat=nullptr;
 for(size_t i=0;i<server.queue.size();++i){
  ServerNotification* n=server.queue[i];
  if(n->player==&ownerPlayer&&n->type==ServerNotificationType::chatServer)lastChat=n;
 }
 check(lastChat!=nullptr&&lastChat->mPacket.texts.size()==1
  &&lastChat->mPacket.texts[0]=="You redeemed your worker into 750 mana","the redemption chat names the refund");

 // Nothing is redeemed unless every condition holds, and nothing is removed then either
 removedBefore=GameEntity::removed;
 Creature worker2(&owner);worker2.definition.worker=true;
 check(!ownerPlayer.redemWorkerInHeart(&plain,&centre),"a non creature drop is left to the regular drop");
 check(!ownerPlayer.redemWorkerInHeart(&notWorker,&centre),"a non worker drop is left to the regular drop");
 check(!ownerPlayer.redemWorkerInHeart(&worker2,&otherTile),"a worker on another tile is left to the regular drop");
 Creature enemyWorker(&enemy);enemyWorker.definition.worker=true;
 check(!ownerPlayer.redemWorkerInHeart(&enemyWorker,&centre),"a foreign worker is not redeemed");
 check(!enemyPlayer.redemWorkerInHeart(&worker2,&centre),"an AI drop is left to the regular drop");
 heart.mHeartHP=0.0;
 Creature deadHeartWorker(&owner);deadHeartWorker.definition.worker=true;
 check(!ownerPlayer.redemWorkerInHeart(&deadHeartWorker,&centre),"a worker on a destroyed heart is left to the regular drop");
 heart.mHeartHP=10000.0;
 heart.mTempleObject=nullptr;
 Creature ruinWorker(&owner);ruinWorker.definition.worker=true;
 check(!ownerPlayer.redemWorkerInHeart(&ruinWorker,&centre),"a heart without its object cannot redeem");
 heart.mTempleObject=new BuildingObject(&centre);
 check(GameEntity::removed==removedBefore,"no refused drop removes anything");

 // The refund is clamped at the stored maximum
 config.maxManaPerSeat=1000.0;owner.mMana=900.0;
 Creature cappedWorker(&owner);cappedWorker.definition.worker=true;
 check(ownerPlayer.redemWorkerInHeart(&cappedWorker,&centre),"redemption still works near the mana cap");
 check(near(owner.mMana,1000.0),"the refund is clamped at the stored maximum");
 config.maxManaPerSeat=200000.0;

 // The local hand loses the redeemed worker
 GameEntity handA(&owner),handB(&owner),handC(&owner),stranger(&owner);
 ownerPlayer.mObjectsInHand.clear();
 ownerPlayer.mObjectsInHand.push_back(&handA);
 ownerPlayer.mObjectsInHand.push_back(&handB);
 ownerPlayer.mObjectsInHand.push_back(&handC);
 ownerPlayer.removeEntityFromHand(&handB);
 check(ownerPlayer.mObjectsInHand.size()==2
  &&ownerPlayer.mObjectsInHand[0]==&handA&&ownerPlayer.mObjectsInHand[1]==&handC,
  "the redeemed worker leaves the local hand");
 ownerPlayer.removeEntityFromHand(&handA);
 check(ownerPlayer.mObjectsInHand.size()==1&&ownerPlayer.mObjectsInHand[0]==&handC,"removal again");
 ownerPlayer.removeEntityFromHand(&stranger);
 check(ownerPlayer.mObjectsInHand.size()==1,"an unknown entity changes nothing");

 // The remaining mana of the destroyed heart goes to the conqueror, single player only
 heart.mHeartHP=0.0;
 owner.mMana=123.0;enemy.mMana=0.0;
 ownerPlayer.mHasLost=false;
 ownerPlayer.recordHeartDestroyed(enemy.getId(),centre.getX(),centre.getY());
 int defeatedBefore=count(server,&ownerPlayer,ServerNotificationType::playerDefeated);
 ownerPlayer.notifyNoMoreDungeonTemple();
 check(near(owner.mMana,0.0),"the defeated seat is left with no mana");
 check(near(enemy.mMana,123.0),"the remaining mana goes to the conqueror");
 check(count(server,&ownerPlayer,ServerNotificationType::playerDefeated)==defeatedBefore+1,
  "the defeat chain still runs after the transfer");

 // The transfer is clamped at the stored maximum
 config.maxManaPerSeat=100.0;
 owner.mMana=80.0;enemy.mMana=50.0;
 ownerPlayer.mHasLost=false;
 ownerPlayer.notifyNoMoreDungeonTemple();
 check(near(owner.mMana,0.0)&&near(enemy.mMana,100.0),"the transfer is clamped at the stored maximum");
 config.maxManaPerSeat=200000.0;

 // The defeat guard keeps the whole chain, transfer included, single
 owner.mMana=55.0;enemy.mMana=10.0;
 ownerPlayer.notifyNoMoreDungeonTemple();
 check(near(owner.mMana,55.0)&&near(enemy.mMana,10.0),"the defeat guard stops a second transfer");

 // A self conquest transfers nothing
 owner.mMana=55.0;enemy.mMana=0.0;
 ownerPlayer.mHasLost=false;
 ownerPlayer.recordHeartDestroyed(owner.getId(),1,2);
 ownerPlayer.notifyNoMoreDungeonTemple();
 check(near(owner.mMana,55.0)&&near(enemy.mMana,0.0),"a self conquest transfers nothing");

 // Two humans: no transfer
 Seat ally(1,7);Player allyPlayer(&ally,true);
 ally.mPlayer=&allyPlayer;ally.mGameMap=&map;allyPlayer.mGameMap=&map;
 map.seats.push_back(&ally);
 Tile allyCentre;
 RoomDungeonTemple allyHeart(&ally);
 allyHeart.mTempleObject=new BuildingObject(&allyCentre);
 map.mRooms.push_back(&allyHeart);
 owner.mMana=123.0;enemy.mMana=0.0;
 ownerPlayer.mHasLost=false;
 ownerPlayer.recordHeartDestroyed(enemy.getId(),centre.getX(),centre.getY());
 ownerPlayer.notifyNoMoreDungeonTemple();
 check(near(owner.mMana,123.0),"in a multi human game the remaining mana is not transferred");
 check(near(enemy.mMana,0.0),"... and the conqueror gains nothing");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
"""

methods = function(temple, 'Tile* RoomDungeonTemple::getHeartTile(')
methods += '\n' + '\n'.join([
    function(game_map, 'std::vector<Room*> GameMap::getRoomsByType('),
    function(game_map, 'void GameMap::updateSeatMana('),
    function(seat, 'void Seat::computeSeatBeginTurn('),
    function(seat, 'void Seat::addMana('),
    function(seat_data, 'uint32_t SeatData::getNbRooms(').replace('SeatData::', 'Seat::'),
    function(player, 'void Player::removeEntityFromHand('),
    function(player, 'bool Player::redemWorkerInHeart('),
    function(player, 'void Player::notifyNoMoreDungeonTemple('),
])
mana_helpers = game_map[game_map.index('const double MANA_HEART_INCOME_PER_SECOND')
    : game_map.index('double manaUpkeepPerSecond(') + len(function(game_map, 'double manaUpkeepPerSecond('))]
probe = probe.replace('RECORD', function(player_header, 'inline void recordHeartDestroyed('))
probe = probe.replace('MANA_HELPERS', 'namespace {\n' + mana_helpers + '\n}')
probe = probe.replace('METHODS', methods)

# Static wiring checks on the production sources the fixture does not execute.
drop = function(player, 'void Player::dropHand(')
assert 'if(redemWorkerInHeart(entity, t))' in drop, 'the server drop must try the redemption first'
assert drop.index('redemWorkerInHeart(entity, t)') < drop.index('entity->drop(pos);'), 'redemption comes before the regular drop'
assert 'mGameMap->isServerGameMap()' in drop, 'the redemption is a server side rule'
misc = function(game_map, 'unsigned long int GameMap::doMiscUpkeep(')
assert 'seat->computeSeatBeginTurn();' in misc and 'updateSeatMana(seat);' in misc, 'each seat gets its mana every turn'
assert 'seat->getPlayer()->notifyNoMoreDungeonTemple();' in misc, 'a lost temple still starts the defeat path'
assert 'void updateSeatMana(Seat* seat);' in read('source/gamemap/GameMap.h')

client = read('source/network/ODClient.cpp')
assert '#include <OgreSceneNode.h>' in client, 'the scene node needs its full type'
assert 'localPlayer->removeEntityFromHand(entity);' in client, 'the redeemed worker leaves the local hand'
assert 'entityNode->getParentSceneNode()->removeChild(entityNode);' in client, 'the held node is detached'

seat_data_cpp = read('source/game/SeatData.cpp')
assert 'mManaIncomePerSecond(0.0)' in seat_data_cpp and 'mManaUpkeepPerSecond(0.0)' in seat_data_cpp
assert seat_data_cpp.count('mManaIncomePerSecond') == 5, 'the new fields are serialized at every site'
assert seat_data_cpp.count('mManaUpkeepPerSecond') == 5, 'the new fields are serialized at every site'

assert 'mMaxManaPerSeat(200000.0)' in read('source/utils/ConfigManager.cpp')
cfg_lines = [l.strip() for l in read('config/global.cfg').splitlines() if l.strip().startswith('MaxManaPerSeat')]
assert any('200000' in l for l in cfg_lines), 'the stored maximum is 200000 in the config'

game_mode = read('source/modes/GameMode.cpp')
assert 'getManaIncomePerSecond()' in game_mode and 'getManaUpkeepPerSecond()' in game_mode, \
    'the mana display shows income and upkeep per second'

player_header_text = read('source/game/Player.h')
assert 'void removeEntityFromHand(GameEntity* entity);' in player_header_text
assert 'bool redemWorkerInHeart(GameEntity* entity, Tile* tile);' in player_header_text
print('WIRING OK: redemption on the server drop, per second mana on the HUD, transfer on a single player defeat')

with tempfile.TemporaryDirectory(prefix='odp-heart-mana-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
