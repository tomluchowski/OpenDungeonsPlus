"""Exercise production enemy discovery for attackable persistent heart objects."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/gamemap/GameMap.cpp').read_text()
start = source.index('std::vector<GameEntity*> GameMap::getVisibleForce(')
end = source.index('\nstd::vector<GameEntity*> GameMap::getVisibleCreatures', start)
method = source[start:end]
probe = r'''
#include <algorithm>
#include <iostream>
#include <vector>
#define OD_LOG_ERR(x) ((void)0)
enum class GameEntityType{creature,room,persistentObject,buildingObject};
enum class SelectionEntityWanted{creatureAliveEnemyAttackable,creatureAliveAllied};
struct Seat {int team;bool isAlliedSeat(Seat* s){return team==s->team;}Seat* getPlayer(){return this;}};
struct Tile;
struct GameEntity {
 GameEntityType type;Seat* owner;bool alive=true,attackable=true;Tile* position=nullptr;
 GameEntityType getObjectType(){return type;}Seat* getSeat(){return owner;}
 bool isAttackable(Tile* tile,Seat* viewer){return alive&&attackable&&owner&&!owner->isAlliedSeat(viewer)&&(!position||position==tile);}
};
using Building=GameEntity;
struct Tile {
 Building* building=nullptr;std::vector<GameEntity*> entities;
 Building* getCoveringBuilding(){return building;}
 const std::vector<GameEntity*>& getEntitiesInTile(){return entities;}
 void fillWithEntities(std::vector<GameEntity*>& out,SelectionEntityWanted wanted,Seat* viewer){
  for(auto* e:entities)if(e->type==GameEntityType::creature&&e->alive&&
    (e->owner->isAlliedSeat(viewer)==(wanted==SelectionEntityWanted::creatureAliveAllied)))out.push_back(e);}
};
struct GameMap {std::vector<GameEntity*> getVisibleForce(const std::vector<Tile*>&,Seat*,bool);};
METHOD
int main(){int checks=0,failures=0;
 auto check=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::cout<<"FAIL "<<msg<<'\n';}};
 Seat owner{1},ally{1},enemy{2};GameMap map;Tile centre,outer,roomTile;
 GameEntity floor{GameEntityType::room,&owner,true,false};centre.building=&floor;outer.building=&floor;
 GameEntity heart{GameEntityType::persistentObject,&owner,true,true,&centre};
 GameEntity portal{GameEntityType::persistentObject,&owner,true,false,&centre};
 GameEntity furniture{GameEntityType::buildingObject,&owner,true,true,&centre};
 centre.entities={&heart,&portal,&furniture};
 for(auto* viewer:{&owner,&ally,&enemy}){
  auto seen=map.getVisibleForce({nullptr,&outer,&centre,&centre},viewer,true);
  check(seen==(viewer==&enemy?std::vector<GameEntity*>{&heart}:std::vector<GameEntity*>{}),"only enemy discovers one heart, never floor or portal");
  check(map.getVisibleForce({&outer},viewer,true).empty(),"visible floor does not reveal a hidden heart");
 }
 heart.alive=false;check(map.getVisibleForce({&centre},&enemy,true).empty(),"dead heart excluded");heart.alive=true;
 centre.building=nullptr;check(map.getVisibleForce({&centre},&enemy,true)==std::vector<GameEntity*>{&heart},"heart remains targetable if old save lacks its centre floor");
 GameEntity room{GameEntityType::room,&owner};roomTile.building=&room;
 GameEntity creature{GameEntityType::creature,&owner};roomTile.entities={&creature};
 auto seen=map.getVisibleForce({&roomTile,&centre},&enemy,true);
 check(seen==std::vector<GameEntity*>({&creature,&room,&heart}),"ordinary creature and room targeting preserved");
 check(map.getVisibleForce({&roomTile},&ally,false)==std::vector<GameEntity*>({&creature,&room}),"allied discovery preserved");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='odp-heart-targeting-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
