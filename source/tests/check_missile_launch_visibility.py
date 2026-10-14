"""Exercise production missile creation and visibility ordering without a game."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
launch_source = (repo / 'source/creatureskill/CreatureSkillMissileLaunch.cpp').read_text()
launch = launch_source.split('bool CreatureSkillMissileLaunch::tryUseFight(', 1)[1].split('\nCreatureSkillMissileLaunch* CreatureSkillMissileLaunch::clone()', 1)[0]
entity_source = (repo / 'source/entities/GameEntity.cpp').read_text()
visibility = entity_source.split('void GameEntity::notifySeatsWithVision(', 1)[1].split('\nvoid GameEntity::addSeatWithVision(', 1)[0]
probe = r'''
#include <OgreVector.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_ERR(...) ((void)0)
namespace Helper {std::string toString(const Ogre::Vector3&){return "";}}
enum class NodeType {MTILES_NODE};
struct Player {bool human;bool getIsHuman(){return human;}};
struct Seat {Player* player;Player* getPlayer(){return player;}};
struct Tile {std::vector<Seat*> visible;int getX(){return 2;}int getY(){return 3;}const auto& getSeatsWithVision(){return visible;}};
struct Event {std::string kind;Seat* seat;Ogre::Vector3 position;};
struct GameMap {std::vector<Event> events;};
struct GameEntity {
 std::vector<Seat*> mSeatsWithVisionNotified;GameMap* map;Ogre::Vector3 position;
 void notifySeatsWithVision(const std::vector<Seat*>&,NodeType=NodeType::MTILES_NODE);
 void fireRemoveEntity(Seat* s){map->events.push_back({"remove",s,position});}
 void fireAddEntity(Seat* s,bool,NodeType){map->events.push_back({"add",s,position});}
};
void GameEntity::notifySeatsWithVision(VISIBILITY
struct Weapon {double getPhysicalDamage(){return 0;}double getMagicalDamage(){return 0;}double getElementDamage(){return 0;}};
struct Creature {
 Tile tile;Seat* seat;Ogre::Vector3 position{2.25f,3.2f,0};bool invalid=false;
 Tile* getPositionTile(){return invalid?nullptr:&tile;}const auto& getPosition(){return position;}
 std::string getName(){return "Caster";}int getLevel(){return 1;}Seat* getSeat(){return seat;}
 Weapon* getWeaponL(){return nullptr;}Weapon* getWeaponR(){return nullptr;}
};
struct MissileOneHit:GameEntity {
 static std::vector<MissileOneHit*> created;int upkeep=0;Ogre::Vector3 direction;double speed;
 MissileOneHit(GameMap* game,Seat*,const std::string&,const std::string&,const std::string&,const Ogre::Vector3& d,double s,double,double,double,GameEntity*,bool,bool,bool):direction(d),speed(s){map=game;created.push_back(this);}
 void addToGameMap(){}void createMesh(){}void setPosition(const Ogre::Vector3& p){position=p;}
 void doUpkeep(){++upkeep;for(auto* seat:mSeatsWithVisionNotified)if(seat->getPlayer()&&seat->getPlayer()->getIsHuman())map->events.push_back({"path",seat,position});}
};
std::vector<MissileOneHit*> MissileOneHit::created;
const Ogre::Real CANNON_MISSILE_HEIGHT=.3f;
struct CreatureSkillMissileLaunch {
 std::string mMissileMesh,mMissilePartScript="MissileMagic";double mMissileSpeed=3,mPhyAtk=0,mPhyAtkPerLvl=0,mMagAtk=13,mMagAtkPerLvl=.25,mEleAtkPerLvl=0;
 bool tryUseFight(GameMap&,Creature*,float,GameEntity*,Tile*,bool,bool)const;
};
bool CreatureSkillMissileLaunch::tryUseFight(LAUNCH
int main(){int checks=0,failures=0;auto check=[&](bool v,const char* why){++checks;if(!v){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 Player human{true},ai{false};Seat owner{&human},observer{&human},hidden{&human},computer{&ai},unassigned{nullptr};
 for(bool visible:{false,true}){
  GameMap map;Creature caster;caster.seat=&owner;
  caster.tile.visible=visible?std::vector<Seat*>{&owner,&observer,&computer,&unassigned}:std::vector<Seat*>{};
  CreatureSkillMissileLaunch skill;GameEntity target;Tile targetTile;
  check(skill.tryUseFight(map,&caster,1,&target,&targetTile,false,true),"launch succeeds");
  auto* missile=MissileOneHit::created.back();
  check(missile->position==Ogre::Vector3(caster.position.x,caster.position.y,.3f),"projectile starts at actual caster XY");
  check(missile->upkeep==1&&missile->speed==3,"first flight upkeep and speed unchanged");
  check(map.events.size()==(visible?4:0),"visible humans receive creation and first path in the launch turn");
  if(visible&&map.events.size()==4){
   check(map.events[0].kind=="add"&&map.events[1].kind=="add"&&map.events[2].kind=="path"&&map.events[3].kind=="path","creation precedes movement notification");
   check(map.events[0].position==missile->position&&map.events[1].position==missile->position,"creation packet preserves launch origin");
  }
  for(const auto& event:map.events)check(event.seat!=&hidden&&event.seat!=&computer&&event.seat!=&unassigned,"no reveal to hidden or nonhuman seats");
  const auto previous=map.events.size();missile->notifySeatsWithVision(caster.tile.visible);
  check(map.events.size()==previous,"next visibility pass does not duplicate creation");
  delete missile;MissileOneHit::created.clear();
 }
 GameMap map;Creature invalid;invalid.invalid=true;CreatureSkillMissileLaunch skill;GameEntity target;Tile tile;
 check(!skill.tryUseFight(map,&invalid,1,&target,&tile,false,true)&&MissileOneHit::created.empty(),"invalid caster tile creates no missile");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('VISIBILITY', visibility).replace('LAUNCH', launch)
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
with tempfile.TemporaryDirectory(prefix='odp-missile-launch-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
