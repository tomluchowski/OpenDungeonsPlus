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
#include <functional>
#include <iostream>
#include <sstream>
#include <vector>
struct Tile{};
struct Seat {int team;void* getPlayer(){return this;}bool isAlliedSeat(Seat* s){return s&&team==s->team;}};
struct GameEntity {Seat* seat;int deaths=0;GameEntity(Seat* s=nullptr):seat(s){}
 Seat* getSeat(){return seat;}void setSeat(Seat* s){seat=s;}void fireEntityDead(){++deaths;}
 virtual bool isAttackable(Tile*,Seat*)const{return false;}virtual double getHP(Tile*)const{return 0;}
 virtual double takeDamage(GameEntity*,double,double,double,double,Tile*,bool){return 0;}};
struct GameMap {bool editor=false;int fights=0;
 bool isInEditorMode(){return editor;}void playerIsFighting(void*,Tile*){++fights;}};
struct BuildingObject:GameEntity {Tile* tile;BuildingObject(Tile* t):tile(t){}Tile* getPositionTile(){return tile;}};
struct Building {double floorHP=250;double getHP(Tile*)const{return floorHP;}};
struct Room:Building {
 GameMap* map;Seat* seat;int dead=0,removed=0,upkeep=0;std::vector<Tile*> mCoveredTiles;
 Room(GameMap* m,Seat* s):map(m),seat(s){}virtual ~Room()=default;
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
 BuildingObject* mTempleObject=nullptr;double mHeartHP=-1;
 RoomDungeonTemple(GameMap* m,Seat* s):Room(m,s){}
 INLINE_METHODS
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
 std::function<void(bool,const char*)> check=[&](bool ok,const char* msg){
  ++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}};
 Seat owner{1},ally{1},enemy{2};Tile centre,floor;BuildingObject object{&centre};
 GameMap map;RoomDungeonTemple heart(&map,&owner);DungeonHeartObject core(&map,heart,&centre);
 heart.mTempleObject=&core;heart.mCoveredTiles={&centre,&floor};
 check(core.getSeat()==&owner,"heart entity carries room ownership");
 check(heart.getHP(nullptr)==250,"legacy total durability preserved");
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
 check(heart.getHP(nullptr)==234&&heart.floorHP==250,"heart damage never reduces floor health");
 check(heart.dead==0&&map.fights==1,"nonlethal hit reports combat without death");
 heart.doUpkeep();check(heart.removed==0&&heart.upkeep==1,"living heart retains all floor tiles");
 std::stringstream save;heart.exportToStream(save);save<<"[/Room]\n";
 RoomDungeonTemple loaded(&map,&owner);check(loaded.importFromStream(save)&&loaded.getHP(nullptr)==234,"damaged heart round trip");
 std::string next;save>>next;check(next=="[/Room]","save parser preserves room boundary");
 std::stringstream legacy("80\n[/Room]\n");RoomDungeonTemple old(&map,&owner);
 check(old.importFromStream(legacy)&&old.getHP(nullptr)==80,"legacy remaining durability retained");
 legacy>>next;check(next=="[/Room]","legacy boundary not consumed");
 for(const char* text:{"250\nHeartHP -1\n", "250\nHeartHP nan\n", "250\nHeartHP nope\n"}){
  std::stringstream bad(text);RoomDungeonTemple invalid(&map,&owner);check(!invalid.importFromStream(bad),"invalid health rejected");}
 check(core.takeDamage(&attacker,999,0,0,0,&centre,false)==234,"lethal damage clamped");
 check(core.deaths==1&&core.getHP(nullptr)==0,"object death listeners notified");
 check(heart.dead==1&&!heart.canAttackHeart(&centre,&enemy),"death fires once and disables targeting");
 check(heart.takeHeartDamage(&attacker,999,0,0,0,&centre)==0&&heart.dead==1,"dead heart cannot be hit twice");
 heart.doUpkeep();check(heart.removed==2&&heart.mCoveredTiles.empty(),"heart death releases the existing room lifecycle");
 map.editor=true;RoomDungeonTemple edit(&map,&owner);edit.mCoveredTiles={&floor};
 check(edit.removeCoveredTile(&floor),"editor editing preserved");
 std::stringstream level;edit.exportToStream(level);check(level.str().find("HeartHP")==std::string::npos,"editor maps do not persist combat damage");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
inline = '\n'.join(function(header, sig) for sig in
                   ('bool canSeatSellBuilding(', 'bool isAttackable(', 'double takeDamage('))
methods = '\n'.join(function(source, sig) for sig in (
    'double RoomDungeonTemple::getHP(', 'bool RoomDungeonTemple::canAttackHeart(',
    'double RoomDungeonTemple::takeHeartDamage(', 'bool RoomDungeonTemple::removeCoveredTile(',
    'void RoomDungeonTemple::doUpkeep(', 'void RoomDungeonTemple::exportToStream(',
    'bool RoomDungeonTemple::importFromStream('))
probe = probe.replace('INLINE_METHODS', inline).replace('METHODS', methods)
probe = probe.replace('HEART_OBJECT', function(source, 'class DungeonHeartObject :'))
with tempfile.TemporaryDirectory(prefix='odp-heart-combat-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
