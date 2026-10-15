"""Exercise the H6 treasury ring: a 5x5 heart stores 1000 gold on each of its 16 outer
tiles, a 3x3 heart stores none, and the gold flows through the same deposit/withdraw
virtuals the carry system already calls."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


temple = read('source/rooms/RoomDungeonTemple.cpp')
temple_header = read('source/rooms/RoomDungeonTemple.h')
treasury_object = read('source/entities/TreasuryObject.cpp')

constants = temple[
    temple.index('const double RoomDungeonTemple::HEART_MAX_HP'):
    temple.index('RoomDungeonTemple::RoomDungeonTemple(')
]
get_heart_tile = function(temple, 'Tile* RoomDungeonTemple::getHeartTile(')
get_heart_max_hp = function(temple, 'double RoomDungeonTemple::getHeartMaxHP(')
get_hp = function(temple, 'double RoomDungeonTemple::getHP(')
get_ring_center_tile = function(temple, 'Tile* RoomDungeonTemple::getRingCenterTile(')
is_treasury_tile = function(temple, 'bool RoomDungeonTemple::isTreasuryTile(')
get_total_gold_storage = function(temple, 'int RoomDungeonTemple::getTotalGoldStorage(')
get_total_gold_stored = function(temple, 'int RoomDungeonTemple::getTotalGoldStored(')
deposit_gold = function(temple, 'int RoomDungeonTemple::depositGold(')
withdraw_gold = function(temple, 'int RoomDungeonTemple::withdrawGold(')
update_meshes = function(temple, 'void RoomDungeonTemple::updateTreasuryMeshesForTile(')
split_room = function(temple, 'void RoomDungeonTemple::splitRoom(')
create_tile_data = function(temple, 'RoomTreasuryTileData* RoomDungeonTemple::createTileData(')
has_carry_spot = function(temple, 'bool RoomDungeonTemple::hasCarryEntitySpot(')
ask_carry_spot = function(temple, 'Tile* RoomDungeonTemple::askSpotForCarriedEntity(')
remove_covered_tile = function(temple, 'bool RoomDungeonTemple::removeCoveredTile(')
do_upkeep = function(temple, 'void RoomDungeonTemple::doUpkeep(')
mesh_name = function(treasury_object, 'const char* TreasuryObject::getMeshNameForGold(')

# The ring is part of the heart: fixed capacity, no treasury skill involved.
assert 'static const int treasuryTileCapacity = 1000;' in temple
assert 'static const int HEART_CORE_RADIUS = 1;' in temple
assert 'dx >= -HEART_CORE_RADIUS && dx <= HEART_CORE_RADIUS' in temple
assert 'center = getCentralTile();' in temple
# The gold virtuals are declared in the header so the carry system sees them.
for symbol in ('getTotalGoldStorage', 'getTotalGoldStored', 'depositGold', 'withdrawGold'):
    assert symbol in temple_header, symbol
assert 'GoldstackLv1' in treasury_object and 'GoldstackLv4' in treasury_object

probe = r"""
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#define OD_LOG_ERR(x)
#define OD_LOG_INF(x)
#define OD_ASSERT_TRUE_MSG(cond,msg) do{if(!(cond)){std::cout<<"ASSERT FAILED: "<<(msg)<<'\n';std::exit(1);}}while(0)
namespace Helper {template<typename T> std::string toString(const T& v){return std::to_string(v);}}

struct Tile {
 int mX=0,mY=0;
 Tile(int x=0,int y=0):mX(x),mY(y){}
 int getX()const{return mX;}
 int getY()const{return mY;}
};
struct Seat;
struct TileData {
 double mHP=0.0;
 virtual ~TileData()=default;
};
struct RoomTreasuryTileData:TileData {
 int mGoldInTile=0;
 std::string mMeshOfTile;
};
namespace Ogre {
 typedef double Real;
 struct Vector3 {
  Real x,y,z;
  Vector3(Real a,Real b,Real c):x(a),y(b),z(c){}
 };
}
struct Random {
 static double Double(double min,double max){return (min+max)*0.5;}
};
struct ODApplication {static double turnsPerSecond;};
double ODApplication::turnsPerSecond=4.0;
struct GameMap {
 bool editor=false;
 bool isInEditorMode()const{return editor;}
};
enum class GameEntityType {creature, giftBoxEntity, skillEntity, treasuryObject, other};
struct GameEntity {
 std::string mName;
 GameEntity(Seat* s=nullptr){(void)s;}
 virtual ~GameEntity()=default;
 const std::string& getName()const{return mName;}
 virtual GameEntityType getObjectType()const{return GameEntityType::other;}
};
struct Building:GameEntity {
 Building():GameEntity(nullptr){}
};
struct Room;
struct BuildingObject:GameEntity {
 Tile* tile;
 BuildingObject(Tile* t):GameEntity(nullptr),tile(t){}
 BuildingObject(GameMap*,Room&,const std::string&,Tile* t,double,double,double,double,bool):GameEntity(nullptr),tile(t){}
 Tile* getPositionTile()const{return tile;}
 virtual bool notifyRemoveAsked(){return true;}
};
struct TreasuryObject:GameEntity {
 int mGoldValue=0;
 static std::vector<TreasuryObject*> spawned;
 TreasuryObject(GameMap*,int v):GameEntity(nullptr),mGoldValue(v){spawned.push_back(this);}
 void addToGameMap(){}
 void createMesh(){}
 void setPosition(Ogre::Vector3){}
 static const char* getMeshNameForGold(int gold);
};
std::vector<TreasuryObject*> TreasuryObject::spawned;
struct Room:Building {
 GameMap* map;
 int doUpkeeps=0;
 std::vector<Tile*> mCoveredTiles;
 std::vector<Tile*> mCoveredTilesDestroyed;
 std::map<Tile*,BuildingObject*> mBuildingObjects;
 std::map<Tile*,TileData*> mTileData;
 Tile* central=nullptr;
 Room(GameMap* m):map(m){}
 GameMap* getGameMap()const{return map;}
 Tile* getCentralTile()const{return central;}
 virtual void doUpkeep(){++doUpkeeps;}
 virtual bool removeCoveredTile(Tile* t){
  auto it=std::find(mCoveredTiles.begin(),mCoveredTiles.end(),t);
  if(it==mCoveredTiles.end())return false;
  mCoveredTiles.erase(it);
  mCoveredTilesDestroyed.push_back(t);
  return true;
 }
 void addBuildingObject(Tile* t,BuildingObject* o){mBuildingObjects[t]=o;}
 void removeBuildingObject(Tile* t){mBuildingObjects.erase(t);}
 void removeAllBuildingObjects(){mBuildingObjects.clear();}
 static int roomSounds;
 static void fireRoomSound(Tile&,const std::string&){++roomSounds;}
};
int Room::roomSounds=0;
"""

probe += r"""
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;
 double mHeartHP=-1;
 bool mCriticalWarningSent=false;
 bool mGoldChanged=false;
 RoomDungeonTemple(GameMap* m):Room(m){}
 static const double HEART_MAX_HP;
 static const double HEART_HEAL_PER_SECOND;
 double getHeartMaxHP()const;
 double getHP(Tile*)const;
 Tile* getHeartTile() const;
 Tile* getRingCenterTile() const;
 bool isTreasuryTile(Tile*) const;
 int getTotalGoldStorage() const;
 int getTotalGoldStored() const;
 int depositGold(int,Tile*);
 int withdrawGold(int);
 void updateTreasuryMeshesForTile(Tile*,RoomTreasuryTileData*);
 void splitRoom(Room&,const std::vector<Tile*>&);
 RoomTreasuryTileData* createTileData(Tile*);
 bool hasCarryEntitySpot(GameEntity*);
 Tile* askSpotForCarriedEntity(GameEntity*);
 void doUpkeep()override;
 bool removeCoveredTile(Tile*)override;
};
__CONSTANTS__
__GET_HEART_TILE__
__GET_HEART_MAX_HP__
__GET_HP__
__GET_RING_CENTER_TILE__
__IS_TREASURY_TILE__
__GET_TOTAL_GOLD_STORAGE__
__GET_TOTAL_GOLD_STORED__
__DEPOSIT_GOLD__
__WITHDRAW_GOLD__
__UPDATE_MESHES__
__SPLIT_ROOM__
__CREATE_TILE_DATA__
__HAS_CARRY_SPOT__
__ASK_CARRY_SPOT__
__REMOVE_COVERED_TILE__
__DO_UPKEEP__
__MESH_NAME__
struct CarriedTreasure:GameEntity {
 CarriedTreasure():GameEntity(nullptr){}
 virtual GameEntityType getObjectType()const{return GameEntityType::treasuryObject;}
};
struct CarriedGift:GameEntity {
 CarriedGift():GameEntity(nullptr){}
 virtual GameEntityType getObjectType()const{return GameEntityType::giftBoxEntity;}
};
struct CarriedSkill:GameEntity {
 CarriedSkill():GameEntity(nullptr){}
 virtual GameEntityType getObjectType()const{return GameEntityType::skillEntity;}
};
"""

probe += r"""
int checks=0;int failures=0;
void check(bool ok,const std::string& what){
 ++checks;
 if(!ok){++failures;std::cout<<"FAIL: "<<what<<'\n';}
}
RoomTreasuryTileData* dataOf(RoomDungeonTemple& h,Tile* t){
 return static_cast<RoomTreasuryTileData*>(h.mTileData[t]);
}
int goldIn(RoomDungeonTemple& h,Tile* t){return dataOf(h,t)->mGoldInTile;}
Tile* tileAt(RoomDungeonTemple& h,int x,int y){
 for(Tile* t:h.mCoveredTiles)if(t->getX()==x&&t->getY()==y)return t;
 return nullptr;
}
int ringCount(RoomDungeonTemple& h){
 int n=0;for(Tile* t:h.mCoveredTiles)if(h.isTreasuryTile(t))++n;
 return n;
}
void addHeartTiles(RoomDungeonTemple& h,int x0,int x1,int y0,int y1,int cx,int cy,Tile*& centre){
 for(int x=x0;x<=x1;++x)
  for(int y=y0;y<=y1;++y){
   Tile* t=new Tile(x,y);
   h.mCoveredTiles.push_back(t);
   RoomTreasuryTileData* d=h.createTileData(t);
   d->mHP=10.0;
   h.mTileData[t]=d;
   if(x==cx&&y==cy)centre=t;
  }
 h.central=centre;
 h.mTempleObject=new BuildingObject(centre);
}

int main(){
 GameMap map;
 CarriedTreasure gold;
 CarriedGift gift;
 CarriedSkill skill;

 // A 5x5 heart: 3x3 core plus 16 ring tiles
 RoomDungeonTemple heart(&map);
 Tile* center=nullptr;
 addHeartTiles(heart,8,12,8,12,10,10,center);

 check(heart.getTotalGoldStorage()==16000,"a 5x5 heart stores 1000 gold on each of its 16 ring tiles");
 check(heart.getTotalGoldStored()==0,"a new ring holds no gold");
 check(ringCount(heart)==16,"the 16 outer tiles form the treasury ring");
 check(heart.isTreasuryTile(center)==false,"the central heart tile is not a ring tile");
 check(heart.getHeartTile()==center,"the heart object tile is the heart tile");
 check(heart.getRingCenterTile()==center,"the ring is anchored on the heart tile");

 // A 3x3 heart: all nine tiles are core, so it has no ring and no storage
 RoomDungeonTemple small(&map);
 Tile* smallCenter=nullptr;
 addHeartTiles(small,8,10,8,10,9,9,smallCenter);
 check(small.getTotalGoldStorage()==0,"a 3x3 heart has no treasury ring");
 check(small.depositGold(500,smallCenter)==0,"a 3x3 heart cannot take gold");
 check(small.getTotalGoldStored()==0,"a 3x3 heart holds no gold");
 check(small.withdrawGold(1)==0,"a 3x3 heart has no gold to give");
 check(small.hasCarryEntitySpot(&gold)==false,"a 3x3 heart never accepts a carried treasure");
 check(small.askSpotForCarriedEntity(&gold)==nullptr,"a 3x3 heart has no spot for a carried treasure");
 check(small.askSpotForCarriedEntity(&gift)==smallCenter,"gifts on a 3x3 heart still land on the central tile");

 Tile* ringA=tileAt(heart,12,10);
 check(heart.isTreasuryTile(ringA),"a tile two east of the heart is a ring tile");
 int soundsBefore=Room::roomSounds;
 check(heart.depositGold(1150,ringA)==1150,"a deposit fills the requested ring tile first");
 check(goldIn(heart,ringA)==1000,"the requested tile is capped at 1000 gold");
 check(heart.getTotalGoldStored()==1150,"the heart counts the stored gold");
 check(goldIn(heart,center)==0,"core tiles never hold ring gold");
 check(Room::roomSounds==soundsBefore+1,"a successful deposit fires the treasury sound");

 check(heart.depositGold(999999,ringA)==14850,"an oversized deposit fills the ring and returns the part that fit");
 check(heart.getTotalGoldStored()==16000,"the ring is full at 16000 gold");
 bool allFull=true;
 for(Tile* t:heart.mCoveredTiles)if(heart.isTreasuryTile(t))allFull=allFull&&(goldIn(heart,t)==1000);
 check(allFull,"every ring tile holds its 1000 gold");
 check(goldIn(heart,center)==0,"the core still holds no gold");
 check(heart.depositGold(1,ringA)==0,"a full ring refuses new gold");
 check(heart.hasCarryEntitySpot(&gold)==false,"a full ring refuses a carried treasure");
 check(heart.askSpotForCarriedEntity(&gold)==nullptr,"a full ring has no spot for a carried treasure");
 check(heart.hasCarryEntitySpot(&gift),"a full ring still accepts gifts");

 check(heart.withdrawGold(1234)==1234,"withdrawal gives back the requested gold");
 check(heart.getTotalGoldStored()==14766,"the stored gold drops accordingly");
 check(heart.withdrawGold(9999999)==14766,"withdrawing more than stored takes everything");
 check(heart.getTotalGoldStored()==0,"the ring is empty again");
 check(heart.withdrawGold(1)==0,"an empty ring gives nothing");
 check(heart.hasCarryEntitySpot(&gold),"an empty ring accepts gold again");
 Tile* spot=heart.askSpotForCarriedEntity(&gold);
 check(spot!=nullptr&&heart.isTreasuryTile(spot),"a carried treasure lands on a ring tile");
 check(heart.askSpotForCarriedEntity(&gift)==center,"gifts land on the central tile");
 check(heart.askSpotForCarriedEntity(&skill)==center,"skills land on the central tile");
"""

probe += r"""
 // Gold stacks appear on ring tiles and follow their amount (doUpkeep refreshes them)
 heart.depositGold(250,ringA);
 heart.doUpkeep();
 BuildingObject* a1=heart.mBuildingObjects[ringA];
 check(goldIn(heart,ringA)==250,"250 gold sits on ringA");
 check(dataOf(heart,ringA)->mMeshOfTile=="GoldstackLv1","250 gold shows a level 1 stack");
 check(a1!=nullptr,"a stack object stands on the tile");
 heart.depositGold(250,ringA);
 heart.doUpkeep();
 check(dataOf(heart,ringA)->mMeshOfTile=="GoldstackLv2"&&heart.mBuildingObjects[ringA]!=a1,"500 gold upgrades the stack");
 BuildingObject* a2=heart.mBuildingObjects[ringA];
 heart.depositGold(250,ringA);
 heart.doUpkeep();
 check(dataOf(heart,ringA)->mMeshOfTile=="GoldstackLv3"&&heart.mBuildingObjects[ringA]!=a2,"750 gold upgrades the stack");
 BuildingObject* a3=heart.mBuildingObjects[ringA];
 heart.depositGold(250,ringA);
 heart.doUpkeep();
 check(dataOf(heart,ringA)->mMeshOfTile=="GoldstackLv4"&&heart.mBuildingObjects[ringA]!=a3,"1000 gold shows a level 4 stack");

 Tile* ringB=tileAt(heart,11,8);
 check(heart.isTreasuryTile(ringB),"a south-west tile two away is a ring tile");
 heart.depositGold(200,ringB);
 heart.doUpkeep();
 BuildingObject* b1=heart.mBuildingObjects[ringB];
 check(dataOf(heart,ringB)->mMeshOfTile=="GoldstackLv1"&&b1!=nullptr,"200 gold on a second tile shows a level 1 stack");
 heart.depositGold(50,ringB);
 heart.doUpkeep();
 check(goldIn(heart,ringB)==250&&dataOf(heart,ringB)->mMeshOfTile=="GoldstackLv1"&&heart.mBuildingObjects[ringB]==b1,"250 gold keeps the level 1 stack");

 check(heart.withdrawGold(9999999)==1250,"draining the ring takes the 1250 stored");
 heart.doUpkeep();
 check(heart.getTotalGoldStored()==0,"the ring is empty");
 check(heart.mBuildingObjects.empty(),"every gold stack object is removed");
 check(dataOf(heart,ringA)->mMeshOfTile.empty()&&dataOf(heart,ringB)->mMeshOfTile.empty(),"the tile data forgets its stack");

 // Editor: releasing a ring tile turns its gold into a treasury object
 int spawnedBefore=static_cast<int>(TreasuryObject::spawned.size());
 map.editor=true;
 heart.depositGold(777,ringA);
 heart.doUpkeep();
 check(heart.getTotalGoldStored()==777,"777 gold stored on ringA");
 check(heart.removeCoveredTile(ringA),"the editor releases a ring tile");
 check(TreasuryObject::spawned.size()==spawnedBefore+1&&TreasuryObject::spawned.back()->mGoldValue==777,"the released gold becomes a treasury object");
 check(goldIn(heart,ringA)==0&&dataOf(heart,ringA)->mMeshOfTile.empty(),"the released tile data is zeroed");
 check(std::find(heart.mCoveredTiles.begin(),heart.mCoveredTiles.end(),ringA)==heart.mCoveredTiles.end(),"the released tile leaves the room");
 check(heart.getTotalGoldStored()==0,"the heart no longer counts the released gold");
 map.editor=false;
 check(heart.removeCoveredTile(heart.mCoveredTiles[0])==false,"game mode never releases heart tiles");

 // splitRoom: handed-over tiles lose their gold in the old room
 Tile* ringC=tileAt(heart,8,11);
 check(heart.isTreasuryTile(ringC),"a west tile two away is a ring tile");
 heart.depositGold(500,ringC);
 check(goldIn(heart,ringC)==500,"500 gold stored on ringC");
 heart.mGoldChanged=false;
 RoomDungeonTemple other(&map);
 heart.splitRoom(other,std::vector<Tile*>{ringC});
 check(goldIn(heart,ringC)==0,"split tiles lose their gold in the old room");
 check(dataOf(heart,ringC)->mMeshOfTile.empty(),"split tiles lose their stack in the old room");
 check(heart.mGoldChanged,"the split marks the ring for a mesh refresh");
 check(heart.getTotalGoldStored()==0,"the old room counts no gold after the split");

 // New tile data of the heart carries the treasury fields
 RoomTreasuryTileData* fresh=static_cast<RoomTreasuryTileData*>(heart.createTileData(new Tile(99,99)));
 check(fresh!=nullptr,"heart tile data is treasury aware");

 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
 return failures==0?0:1;
}
"""

probe = probe.replace('__CONSTANTS__', constants)
probe = probe.replace('__GET_HEART_TILE__', get_heart_tile)
probe = probe.replace('__GET_HEART_MAX_HP__', get_heart_max_hp)
probe = probe.replace('__GET_HP__', get_hp)
probe = probe.replace('__GET_RING_CENTER_TILE__', get_ring_center_tile)
probe = probe.replace('__IS_TREASURY_TILE__', is_treasury_tile)
probe = probe.replace('__GET_TOTAL_GOLD_STORAGE__', get_total_gold_storage)
probe = probe.replace('__GET_TOTAL_GOLD_STORED__', get_total_gold_stored)
probe = probe.replace('__DEPOSIT_GOLD__', deposit_gold)
probe = probe.replace('__WITHDRAW_GOLD__', withdraw_gold)
probe = probe.replace('__UPDATE_MESHES__', update_meshes)
probe = probe.replace('__SPLIT_ROOM__', split_room)
probe = probe.replace('__CREATE_TILE_DATA__', create_tile_data)
probe = probe.replace('__HAS_CARRY_SPOT__', has_carry_spot)
probe = probe.replace('__ASK_CARRY_SPOT__', ask_carry_spot)
probe = probe.replace('__REMOVE_COVERED_TILE__', remove_covered_tile)
probe = probe.replace('__DO_UPKEEP__', do_upkeep)
probe = probe.replace('__MESH_NAME__', mesh_name)

with tempfile.TemporaryDirectory(prefix='odp-heart-ring-') as tmp:
    probe_path = Path(tmp) / 'check.cpp'
    probe_path.write_text(probe)
    exe = Path(tmp) / 'check.exe'
    cmd = (
        'cl /nologo /EHsc /MD /std:c++14 /I source /Fo"{}" {} /Fe"{}"'
    ).format(
        str(Path(tmp) / 'check.obj'),
        str(probe_path),
        str(exe),
    )
    build = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if build.returncode != 0:
        print(build.stdout)
        print(build.stderr)
        raise SystemExit(1)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    print(run.stdout)
    if run.returncode != 0:
        print(run.stderr)
        raise SystemExit(1)

print('WIRING OK')
