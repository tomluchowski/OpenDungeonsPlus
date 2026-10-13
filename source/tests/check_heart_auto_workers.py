"""Exercise the automatic worker path: the living heart creates one worker
of the seat every five seconds until the seat has four, and none otherwise."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text()


temple = read("source/rooms/RoomDungeonTemple.cpp")
player = read("source/game/Player.cpp")
game_map = read("source/gamemap/GameMap.cpp")
seat = read("source/game/Seat.cpp")


def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


probe = r"""
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace Ogre {
typedef float Real;
struct Vector3 {float x,y,z;Vector3(){}Vector3(float a,float b,float c):x(a),y(b),z(c){}};
}
enum class RoomType {dungeonTemple, other, nbRooms};
struct Tile {int x=0,y=0;Tile(){}Tile(int xi,int yi):x(xi),y(yi){}
 int getX()const{return x;}int getY()const{return y;}};
struct CreatureDefinition {std::string name="worker";};
struct Seat;
struct GameMap;
struct Creature {
 static int spawned,addToMap,meshes,effects,positions;
 static int lastX,lastY;
 static Creature* first;
 const CreatureDefinition* definition;Seat* seat;
 Creature(GameMap* g,const CreatureDefinition* d,Seat* s):definition(d),seat(s)
 {++spawned;if(first==nullptr)first=this;}
 void addToGameMap(GameMap* g=nullptr){(void)g;++addToMap;}
 void addParticleEffect(const std::string& e,uint32_t t){(void)e;(void)t;++effects;}
 void createMesh(){++meshes;}
 void setPosition(const Ogre::Vector3& v){lastX=static_cast<int>(v.x);lastY=static_cast<int>(v.y);++positions;}
};
int Creature::spawned=0, Creature::addToMap=0, Creature::meshes=0, Creature::effects=0, Creature::positions=0;
int Creature::lastX=-1, Creature::lastY=-1;
Creature* Creature::first=nullptr;
struct Seat {
 int id;
 double mAutoWorkerTimer=0.0;
 int mNumCreaturesWorkers=0;
 const CreatureDefinition* mDefaultWorkerClass=nullptr;
 Seat(int i):id(i){}
 int getNumCreaturesWorkers()const{return mNumCreaturesWorkers;}
 const CreatureDefinition* getWorkerClassToSpawn()const{return mDefaultWorkerClass;}
};
struct BuildingObject;
struct Room {Seat* seat;double hp=250.0;
 Room(Seat* s):seat(s){}virtual ~Room()=default;
 Seat* getSeat()const{return seat;}virtual RoomType getType()const{return RoomType::other;}
 virtual double getHP(Tile*)const{return hp;}};
struct RoomDungeonTemple:Room {
 BuildingObject* mTempleObject=nullptr;double mHeartHP=10000.0;
 RoomDungeonTemple(Seat* s):Room(s){}
 RoomType getType()const override{return RoomType::dungeonTemple;}
 double getHP(Tile*)const override{return mHeartHP;}
 Tile* getHeartTile() const;
};
struct BuildingObject {Tile* tile;BuildingObject(Tile* t):tile(t){}
 Tile* getPositionTile(){return tile;}};
struct GameMap {std::vector<Room*> mRooms;
 std::vector<Room*>& getRooms(){return mRooms;}
 void updateSeatAutoWorkers(Seat* seat, double timeSinceLastTurn);
};
GET_HEART_TILE
AUTO_WORKERS
int checks=0,failures=0;
void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<"\n";}}
int main(){
 GameMap map;
 Seat owner(1);
 Tile centre(5,7);
 RoomDungeonTemple heart(&owner);
 heart.mTempleObject=new BuildingObject(&centre);
 map.mRooms.push_back(&heart);
 CreatureDefinition workerClass;workerClass.name="worker";
 owner.mDefaultWorkerClass=&workerClass;

 // From zero workers to four, one every five seconds
 for(int i=0;i<7;++i)map.updateSeatAutoWorkers(&owner,0.7);
 check(Creature::spawned==0,"below the five seconds no worker appears yet");
 map.updateSeatAutoWorkers(&owner,0.7);
 check(Creature::spawned==1,"at about five and a half seconds the first worker appears");
 check(Creature::addToMap==1&&Creature::meshes==1&&Creature::effects==1&&Creature::positions==1,
  "the worker joins the map, gets its mesh, the summon effect and its position");
 check(Creature::first->definition==&workerClass,"the worker is the seat own worker class");
 check(Creature::lastX==5&&Creature::lastY==7,"the worker stands on the heart tile");
 for(int i=0;i<21;++i)map.updateSeatAutoWorkers(&owner,0.7);
 check(Creature::spawned==4,"four workers have appeared by about twenty seconds");
 check(owner.mNumCreaturesWorkers==4,"the seat count follows the spawns");
 for(int i=0;i<10;++i)map.updateSeatAutoWorkers(&owner,0.7);
 check(Creature::spawned==4,"at four workers the heart creates no more");

 // Four or more workers: none, and the clock resets
 owner.mNumCreaturesWorkers=6;
 map.updateSeatAutoWorkers(&owner,10.0);
 check(Creature::spawned==4&&owner.mAutoWorkerTimer==0.0,
  "with six workers the heart creates none and the clock resets");
 owner.mNumCreaturesWorkers=0;

 // A destroyed heart creates none
 heart.mHeartHP=0.0;
 map.updateSeatAutoWorkers(&owner,60.0);
 check(Creature::spawned==4&&owner.mAutoWorkerTimer==0.0,"a destroyed heart creates none");
 heart.mHeartHP=10000.0;

 // A heart without its object creates none
 heart.mTempleObject=nullptr;
 map.updateSeatAutoWorkers(&owner,60.0);
 check(Creature::spawned==4&&owner.mAutoWorkerTimer==0.0,"a heart without its object creates none");
 heart.mTempleObject=new BuildingObject(&centre);

 // A seat without a worker class creates none
 owner.mDefaultWorkerClass=nullptr;
 map.updateSeatAutoWorkers(&owner,60.0);
 check(Creature::spawned==4&&owner.mAutoWorkerTimer==0.0,"a seat without a worker class creates none");
 owner.mDefaultWorkerClass=&workerClass;

 // One worker already there: three more appear
 owner.mNumCreaturesWorkers=1;
 int spawnedBefore=Creature::spawned;
 for(int i=0;i<40;++i)map.updateSeatAutoWorkers(&owner,0.7);
 check(Creature::spawned==spawnedBefore+3,"one worker already there, three more appear");
 check(owner.mNumCreaturesWorkers==4,"the seat count ends at four");

 // A long turn still creates only up to the target
 int burstBefore=Creature::spawned;
 owner.mNumCreaturesWorkers=0;owner.mAutoWorkerTimer=0.0;
 map.updateSeatAutoWorkers(&owner,1000.0);
 check(Creature::spawned==burstBefore+4,"a long turn still creates only up to the target");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<"\n";return failures?1:0;
}
"""

get_heart_tile = function(temple, "Tile* RoomDungeonTemple::getHeartTile(")
auto_workers = function(game_map, "void GameMap::updateSeatAutoWorkers(")
constants = game_map[game_map.index("const int AUTO_WORKERS_TARGET")
    : game_map.index("AUTO_WORKER_INTERVAL_SECONDS = 5.0;") + len("AUTO_WORKER_INTERVAL_SECONDS = 5.0;")]
probe = probe.replace("GET_HEART_TILE", get_heart_tile)
probe = probe.replace("AUTO_WORKERS", "namespace {\n" + constants + "\n}\n" + auto_workers)

# Static wiring checks on the production sources the fixture does not execute.
assert "new Creature(this, classToSpawn, seat)" in auto_workers, "the worker is created with the seat"
assert "newCreature->addToGameMap();" in auto_workers, "the worker joins the game map"
assert "addParticleEffect(\"SummonWorker\", 3)" in auto_workers, "the worker gets the summon effect"
assert "newCreature->createMesh();" in auto_workers, "the mesh is created so the clients are told of it"
assert "++seat->mNumCreaturesWorkers;" in auto_workers, "the seat count follows the spawn"
assert "AUTO_WORKERS_TARGET" in auto_workers and "AUTO_WORKER_INTERVAL_SECONDS" in auto_workers

misc = function(game_map, "unsigned long int GameMap::doMiscUpkeep(")
assert "updateSeatMana(seat, nbManaWellTiles, timeSinceLastTurn);" in misc and "updateSeatAutoWorkers(seat, timeSinceLastTurn);" in misc, \
    "each seat gets its automatic workers every turn"
assert misc.index("updateSeatMana(seat, nbManaWellTiles, timeSinceLastTurn);") < misc.index("updateSeatAutoWorkers(seat, timeSinceLastTurn);"), \
    "the workers are created after the mana upkeep, with the fresh worker count"
assert "void updateSeatAutoWorkers(Seat* seat, double timeSinceLastTurn);" in read("source/gamemap/GameMap.h")
assert "#include \"rooms/RoomDungeonTemple.h\"" in game_map, "the heart tile needs the temple room type"

seat_cpp = read("source/game/Seat.cpp")
seat_h = read("source/game/Seat.h")
assert "mAutoWorkerTimer(0.0)" in seat_cpp, "the timer starts at zero"
assert "double mAutoWorkerTimer;" in seat_h

assert "You have no worker to fulfill your dark wishes." in player, "the no worker notice stays"
spell = read("source/spells/SpellSummonWorker.cpp")
assert "getWorkerClassToSpawn()" in spell and "newCreature->addToGameMap();" in spell, \
    "summoning a worker by spell still works"

with tempfile.TemporaryDirectory(prefix="odp-heart-auto-workers-") as directory:
    work = Path(directory)
    (work / "check.cpp").write_text(probe)
    subprocess.run(["cl", "/nologo", "/EHsc", "/MD", "/std:c++14", "check.cpp", "/Fecheck.exe"], cwd=work, check=True)
    subprocess.run([str(work / "check.exe")], cwd=work, check=True)
