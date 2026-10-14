"""Exercise the heart defence: while an enemy creature is within seven tiles of the
living heart of the seat, the runners defend the heart, the alarm call sounds once
when the defence starts (not the combat music), and everyone returns to work when
the enemy leaves or the heart is destroyed."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text()


game_map = read("source/gamemap/GameMap.cpp")


def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


defence = function(game_map, "void GameMap::updateSeatHeartDefense(")
target = function(game_map, "Tile* GameMap::getHeartDefenceTargetTile(")
constants = game_map[game_map.index("const int HEART_DEFENCE_RANGE_SQUARED")
    : game_map.index("HEART_DEFENCE_RANGE_SQUARED = 7 * 7;") + len("HEART_DEFENCE_RANGE_SQUARED = 7 * 7;")]

probe = r"""
#include <iostream>
#include <string>
#include <vector>
enum class RoomType {dungeonTemple, other, nbRooms};
struct Tile {int x=0,y=0;Tile(){}Tile(int xi,int yi):x(xi),y(yi){}
 int getX()const{return x;}int getY()const{return y;}};
namespace Pathfinding {
 template <typename T> int squaredDistanceTile(const T& a, const T& b)
 {int dx=a.getX()-b.getX();int dy=a.getY()-b.getY();return dx*dx+dy*dy;}
}
namespace SoundRelativeKeeperStatements
{
    const std::string WeAreUnderAttack = "Keeper/WeAreUnderAttack";
}
struct CreatureDefinition {
 bool runner=false;
 bool isWorker()const{return false;}
 bool isHeartDefenceRunner()const{return runner;}
};
struct Player {
 bool human=true;bool lost=false;
 bool getIsHuman()const{return human;}
 bool getHasLost()const{return lost;}
};
struct Seat {
 int team;
 Player* player=nullptr;
 bool mHeartDefenceActive=false;
 bool mHeartDefenceHeartDamaged=false;
 Seat(int t):team(t){}
 Player* getPlayer()const{return player;}
 bool getHeartDefenceActive()const{return mHeartDefenceActive;}
 bool getHeartDefenceHeartDamaged()const{return mHeartDefenceHeartDamaged;}
 bool isAlliedSeat(const Seat* other)const
 {return other!=nullptr&&(other==this||other->team==team);}
};
struct Creature {
 Seat* seat;bool alive=true;bool onMap=true;Tile* tile;
 const CreatureDefinition* definition;
 Creature(Seat* s,const CreatureDefinition* d,Tile* t):seat(s),definition(d),tile(t){}
 Seat* getSeat()const{return seat;}
 bool isAlive()const{return alive;}
 bool getIsOnMap()const{return onMap;}
 Tile* getPositionTile()const{return tile;}
 const CreatureDefinition* getDefinition()const{return definition;}
};
struct Room {Seat* seat;double hp=250.0;RoomType type=RoomType::other;
 Room(Seat* s):seat(s){}virtual ~Room()=default;
 Seat* getSeat()const{return seat;}
 virtual RoomType getType()const{return type;}
 virtual double getHP(Tile*)const{return hp;}};
struct RoomDungeonTemple:Room {
 Tile* heartTile=nullptr;double maxHP=10000.0;double heartHP=10000.0;
 RoomDungeonTemple(Seat* s):Room(s){type=RoomType::dungeonTemple;}
 RoomType getType()const override{return RoomType::dungeonTemple;}
 double getHP(Tile*)const override{return heartHP;}
 Tile* getHeartTile()const{return heartTile;}
 double getHeartMaxHP()const{return maxHP;}
};
struct GameMap {
 std::vector<Room*> mRooms;
 std::vector<Creature*> mCreatures;
 int alarms=0;
 std::string lastSound="";
 std::vector<Room*>& getRooms(){return mRooms;}
 std::vector<Creature*>& getCreatures(){return mCreatures;}
 void fireRelativeSound(const std::vector<Seat*>& seats, const std::string& soundFamily)
 {(void)seats;++alarms;lastSound=soundFamily;}
 void updateSeatHeartDefense(Seat* seat);
 Tile* getHeartDefenceTargetTile(Creature& runner, Seat* seat);
};
DEFENCE
TARGET
int checks=0,failures=0;
void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<"\n";}}
int main(){
 GameMap map;
 Player human;
 Seat owner(1);
 owner.player=&human;
 Seat enemySeat(2);
 Tile heart(0,0);
 RoomDungeonTemple temple(&owner);
 temple.heartTile=&heart;
 map.mRooms.push_back(&temple);
 CreatureDefinition worker;worker.runner=true;
 CreatureDefinition fighter;fighter.runner=false;
 Tile at1(1,0),at2(2,0),at3(3,0),at5(5,0),at6(6,0),at7(7,0),at71(7,1),at8(8,0),at10(10,0);
 // No enemy in range: no defence
 map.updateSeatHeartDefense(&owner);
 check(!owner.mHeartDefenceActive&&map.alarms==0,"no enemy means no defence");
 map.alarms=0;
 map.updateSeatHeartDefense(&owner);

 // An enemy six tiles away: the defence starts and the alarm call sounds once
 Creature enemy(&enemySeat,&fighter,&at6);
 map.mCreatures.push_back(&enemy);
 map.updateSeatHeartDefense(&owner);
 check(owner.mHeartDefenceActive,"an enemy six tiles from the heart triggers the defence");
 check(map.alarms==1&&map.lastSound=="Keeper/WeAreUnderAttack","the defence start sounds the alarm call");
 map.updateSeatHeartDefense(&owner);
 check(map.alarms==1,"the alarm call does not sound again while the defence is on");

 // An enemy eight tiles away: out of range, everyone returns to work
 enemy.tile=&at8;
 map.updateSeatHeartDefense(&owner);
 check(!owner.mHeartDefenceActive,"an enemy eight tiles from the heart is out of range");
 map.updateSeatHeartDefense(&owner);

 // A dead enemy does not trigger the defence
 enemy.alive=false;
 enemy.tile=&at6;
 map.updateSeatHeartDefense(&owner);
 check(!owner.mHeartDefenceActive,"a dead enemy does not trigger the defence");
 enemy.alive=true;

 // An allied creature does not trigger the defence
 map.mCreatures.clear();
 Creature ally(&owner,&fighter,&at6);
 map.mCreatures.push_back(&ally);
 map.updateSeatHeartDefense(&owner);
 check(!owner.mHeartDefenceActive,"an allied creature does not trigger the defence");
 map.mCreatures.clear();

 // A non human owner gets the defence without the alarm call
 map.mCreatures.clear();
 human.human=false;
 map.alarms=0;
 map.mCreatures.push_back(&enemy);
 enemy.tile=&at6;
 map.updateSeatHeartDefense(&owner);
 check(owner.mHeartDefenceActive,"a non human owner gets the defence");
 check(map.alarms==0,"a non human owner gets no alarm call");
 human.human=true;
 owner.mHeartDefenceActive=false;
 map.alarms=0;
 map.mCreatures.clear();

 // A destroyed heart is not defended
 temple.heartHP=0.0;
 map.mCreatures.push_back(&enemy);
 map.updateSeatHeartDefense(&owner);
 check(!owner.mHeartDefenceActive,"a destroyed heart is not defended");
 temple.heartHP=10000.0;
 map.mCreatures.clear();

 // A healthy heart: the runners rally the nearest fighter, not another runner
 Tile run(1,0);
 Creature runner(&owner,&worker,&run);
 Creature fighterA(&owner,&fighter,&at2);
 Creature fighterB(&owner,&fighter,&at10);
 map.mCreatures.push_back(&runner);
 map.mCreatures.push_back(&fighterA);
 map.mCreatures.push_back(&fighterB);
 Tile* t=map.getHeartDefenceTargetTile(runner,&owner);
 check(t==&at2,"with a healthy heart the runner rallies the nearest fighter");
 check(t!=&run&&t!=&at10,"the runner does not rally itself or the farther fighter");

 // No fighter to rally: hold the heart
 map.mCreatures.clear();
 map.mCreatures.push_back(&runner);
 t=map.getHeartDefenceTargetTile(runner,&owner);
 check(t==&heart,"with no fighter the runner holds the heart");

 // A damaged heart takes the runners to itself, even when a fighter is closer
 temple.heartHP=5000.0;
 map.mCreatures.push_back(&fighterA);
 map.mCreatures.push_back(&enemy);
 map.updateSeatHeartDefense(&owner);
 check(owner.mHeartDefenceActive&&owner.mHeartDefenceHeartDamaged,
  "a damaged heart is in defence with the heart damaged");
 t=map.getHeartDefenceTargetTile(runner,&owner);
 check(t==&heart,"with a damaged heart the runner goes to the heart");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<"\n";return failures?1:0;
}
"""
probe = probe.replace("DEFENCE", "namespace {\n" + constants + "\n}\n" + defence)
probe = probe.replace("TARGET", target)

# Static wiring checks on the production sources the fixture does not execute.
assert "WeAreUnderAttack" in defence, "the defence start sounds the alarm call"
assert "playerIsFighting" not in defence and "playerFighting" not in defence, \
    "the defence does not start the combat music"
assert "HEART_DEFENCE_RANGE_SQUARED" in defence, "the seven tile reach is a named constant"
assert "getHeartDefenceTargetTile" in target, "the runner target comes from the game map"
assert "isHeartDefenceRunner()" in target, "the runners are not rallied as fighters"

game_map_h = read("source/gamemap/GameMap.h")
assert "void updateSeatHeartDefense(Seat* seat);" in game_map_h
assert "Tile* getHeartDefenceTargetTile(Creature& runner, Seat* seat);" in game_map_h

misc = function(game_map, "unsigned long int GameMap::doMiscUpkeep(")
assert "updateSeatAutoWorkers(seat, timeSinceLastTurn);" in misc
assert misc.index("updateSeatAutoWorkers(seat, timeSinceLastTurn);") < misc.index("updateSeatHeartDefense(seat);"), \
    "each seat gets its heart defence checked every turn"

seat_h = read("source/game/Seat.h")
assert "bool mHeartDefenceActive;" in seat_h
assert "bool mHeartDefenceHeartDamaged;" in seat_h
assert "getHeartDefenceActive" in seat_h and "getHeartDefenceHeartDamaged" in seat_h
seat_cpp = read("source/game/Seat.cpp")
assert "mHeartDefenceActive(false)" in seat_cpp
assert "mHeartDefenceHeartDamaged(false)" in seat_cpp

creature_def = read("source/entities/CreatureDefinition.h")
runner_fn = function(creature_def, "inline bool isHeartDefenceRunner()")
assert "isWorker()" in runner_fn and "CaveHornet" in runner_fn and "Goblin" in runner_fn, \
    "the runners are the workers plus the hornet and goblin scouts"

creature_h = read("source/entities/Creature.h")
assert "void handleHeartDefence();" in creature_h
creature_cpp = read("source/entities/Creature.cpp")
upkeep = function(creature_cpp, "void Creature::doUpkeep(")
assert "decidePrioritaryAction();" in upkeep
assert upkeep.index("decidePrioritaryAction();") < upkeep.index("handleHeartDefence();"), \
    "the heart defence is decided right after the prioritary action"
heart_defence_creature = function(creature_cpp, "void Creature::handleHeartDefence()")
assert "isHeartDefenceRunner()" in heart_defence_creature
assert "getHeartDefenceActive()" in heart_defence_creature
assert "isActionInList(CreatureActionType::goDefendHeart)" in heart_defence_creature
assert "clearActionQueue();" in heart_defence_creature, "the runner drops its current job"
assert "CreatureActionGoDefendHeart" in heart_defence_creature
assert "#include \"creatureaction/CreatureActionGoDefendHeart.h\"" in creature_cpp

action_h = read("source/creatureaction/CreatureAction.h")
assert "goDefendHeart," in action_h
assert action_h.index("goDefendHeart") < action_h.index("nb // Must be the last value"), \
    "the new action type stays inside the enum"
action_cpp = read("source/creatureaction/CreatureAction.cpp")
assert "case CreatureActionType::goDefendHeart:" in action_cpp, "the new action has a name"

go = read("source/creatureaction/CreatureActionGoDefendHeart.cpp")
assert "creature.popAction();" in go, "when the defence is over the action pops"
assert "result.resize(3)" in go, "the runner walks in short steps"
assert "CreatureActionWalkToTile" in go
assert "setWalkPath" in go
assert "getHeartDefenceTargetTile" in go

assert "${SRC}/creatureaction/CreatureActionGoDefendHeart.cpp" in read("CMakeLists.txt"), \
    "the new action file is built"

# The defence state lives only in these files: it is server side and not saved,
# so old maps and saves keep working.
offenders = []
for pattern in ("source/**/*.cpp", "source/**/*.h"):
    for path in repo.glob(pattern):
        if path.is_file() and "mHeartDefence" in path.read_text():
            offenders.append("/".join(path.relative_to(repo).parts))
assert sorted(offenders) == ["source/game/Seat.cpp", "source/game/Seat.h", "source/gamemap/GameMap.cpp"], \
    "the heart defence state is not saved with the level"

with tempfile.TemporaryDirectory(prefix="odp-heart-defence-") as directory:
    work = Path(directory)
    (work / "check.cpp").write_text(probe)
    subprocess.run(["cl", "/nologo", "/EHsc", "/MD", "/std:c++14", "check.cpp", "/Fecheck.exe"], cwd=work, check=True)
    subprocess.run([str(work / "check.exe")], cwd=work, check=True)
