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
origin_source = (repo / 'source/entities/MissileObject.cpp').read_text()


# Extract the actual save/load methods for the integrator's serialization probe.
def origin_method(signature):
    text = origin_source[origin_source.index(signature):]
    return text[:text.index('\n}\n') + 3].replace('MissileObject::', 'SavedMissile::')


origin_methods = origin_method('void MissileObject::exportToStream(') + origin_method('bool MissileObject::importFromStream(')
probe = r'''
#include <OgreVector.h>
#include "entities/CreatureProgression.h"
#include <algorithm>
#include <functional>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_ERR(...) ((void)0)
namespace Helper {std::string toString(const Ogre::Vector3&){return "";}}
enum class NodeType {MTILES_NODE};
struct Player {bool human;bool getIsHuman(){return human;}};
struct Seat {Player* player;Player* getPlayer(){return player;}};
struct Tile {std::vector<Seat*> visible;int getX(){return 2;}int getY(){return 3;}const std::vector<Seat*>& getSeatsWithVision(){return visible;}};
struct Event {std::string kind;Seat* seat;Ogre::Vector3 position;};
struct GameMap {std::vector<Event> events;};
enum class GameEntityType {creature,missileObject};
enum class MissileObjectType {oneHit};
struct GameEntity {
 std::vector<Seat*> mSeatsWithVisionNotified;GameMap* map;Ogre::Vector3 position;
 GameEntityType getObjectType()const{return GameEntityType::creature;}std::string getName()const{return "Target";}
 void notifySeatsWithVision(const std::vector<Seat*>&,NodeType=NodeType::MTILES_NODE);
 void fireRemoveEntity(Seat* s){map->events.push_back({"remove",s,position});}
 void fireAddEntity(Seat* s,bool,NodeType){map->events.push_back({"add",s,position});}
};
void GameEntity::notifySeatsWithVision(VISIBILITY
namespace CosmeticEventType {const int missileLaunch=6;}
struct CosmeticEvent {CosmeticEvent(int){} std::string mSubject,mObject,mText;int mValue=0,mValue2=0;Ogre::Vector3 mPosition;};
struct Weapon {double getPhysicalDamage(){return 0;}double getMagicalDamage(){return 0;}double getElementDamage(){return 0;}};
// Creature launch state used by the actual skill method.
struct Creature {
 Tile tile;Seat* seat;Ogre::Vector3 position{2.25f,3.2f,0};bool invalid=false,possessed=false;
 // Report actual possession at the launch boundary.
 bool isPossessed(){return possessed;}
 Tile* getPositionTile(){return invalid?nullptr:&tile;}const Ogre::Vector3& getPosition(){return position;}
 std::string getName(){return "Caster";}int getLevel(){return 1;}Seat* getSeat(){return seat;}
 void fireCosmeticEvent(const CosmeticEvent&,bool){}
 Weapon* getWeaponL(){return nullptr;}Weapon* getWeaponR(){return nullptr;}
};
// Projectile spy records the launch metadata and visibility order.
struct MissileOneHit:GameEntity {
 static std::vector<MissileOneHit*> created;int upkeep=0;Ogre::Vector3 direction;double speed;bool creatureShot=false,possessedShot=false;
 // Capture provenance before initial upkeep in the production launch method.
 void setCreatureShot(bool possessed){creatureShot=true;possessedShot=possessed;}
 MissileOneHit(GameMap* game,Seat*,const std::string&,const std::string&,const std::string&,const Ogre::Vector3& d,double s,double,double,double,GameEntity*,bool,bool,bool):direction(d),speed(s){map=game;created.push_back(this);}
 std::string getName(){return "Missile";}
 MissileObjectType getMissileType()const{return MissileObjectType::oneHit;}
 void setShooter(const std::string&,const std::string&){}
 void addToGameMap(){}void createMesh(){}void setPosition(const Ogre::Vector3& p){position=p;}
 void doUpkeep(){++upkeep;for(Seat* seat:mSeatsWithVisionNotified)if(seat->getPlayer()&&seat->getPlayer()->getIsHuman())map->events.push_back({"path",seat,position});}
};
std::vector<MissileOneHit*> MissileOneHit::created;
const Ogre::Real CANNON_MISSILE_HEIGHT=.3f;
struct CreatureSkillMissileLaunch {
 std::string mMissileMesh,mMissilePartScript="MissileMagic";double mMissileSpeed=3,mPhyAtk=0,mPhyAtkPerLvl=0,mMagAtk=13,mMagAtkPerLvl=.25,mEleAtk=0,mEleAtkPerLvl=0;
 bool tryUseFight(GameMap&,Creature*,float,GameEntity*,Tile*,bool,bool)const;
};
bool CreatureSkillMissileLaunch::tryUseFight(LAUNCH

// Isolate missile-specific fields from unrelated entity serialization.
struct RenderedMovableEntity {
 // The base entity fields are outside the missile-specific serialization under test.
 void exportToStream(std::ostream&)const{}
 bool importFromStream(std::istream&){return true;}
};
// Minimal host for the actual production save/load methods.
struct SavedMissile:RenderedMovableEntity {
 Ogre::Vector3 mDirection{1,2,3};bool mIsMissileAlive=true,mDamageAllies=false,mKoEnemyCreature=false;
 double mSpeed=5;bool mCreatureShot=false,mPossessedShot=false;
 void exportToStream(std::ostream&)const;
 bool importFromStream(std::istream&);
};
ORIGIN_METHODS
// Run real base-missile serialization with old rows, subtype suffixes and both control modes.
void checkShotSave(const std::function<void(bool,const char*)>& check){
 for(bool possessed:{false,true}){
  SavedMissile original;original.mCreatureShot=true;original.mPossessedShot=possessed;
  std::ostringstream out;original.exportToStream(out);out<<"12 13 14";
  SavedMissile loaded;std::istringstream input(out.str());int first=0,second=0,third=0;
  check(loaded.importFromStream(input),"new missile save loads");input>>first>>second>>third;
  check(bool(input)&&first==12&&second==13&&third==14,"subtype fields follow optional origin unchanged");
  check(loaded.mCreatureShot&&loaded.mPossessedShot==possessed,"shot possession survives a save");
 }
 for(const std::string tail:{"12 13 14",""}){
  SavedMissile legacy;legacy.mCreatureShot=true;legacy.mPossessedShot=true;
  std::istringstream input("1 2 3 1 0 0 5 "+tail);
  check(legacy.importFromStream(input),"legacy missile row still loads");
  check(!legacy.mCreatureShot&&!legacy.mPossessedShot,"legacy row cannot invent a controlled shot");
  if(!tail.empty()){int first=0;input>>first;check(bool(input)&&first==12,"legacy subtype fields are not consumed");}
 }
 for(const std::string tail:{"ShotOrigin 2", "ShotOrigin -1", "ShotOrigin", "ShotOther 1"}){
  SavedMissile invalid;std::istringstream input("1 2 3 1 0 0 5 "+tail);
  check(!invalid.importFromStream(input),"invalid origin extension fails");
 }
}
int main(){int checks=0,failures=0;std::function<void(bool,const char*)> check=[&](bool v,const char* why){++checks;if(!v){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 checkShotSave(check);
 Player human{true},ai{false};Seat owner{&human},observer{&human},hidden{&human},computer{&ai},unassigned{nullptr};
 for(bool visible:{false,true})for(bool possessed:{false,true}){
  GameMap map;Creature caster;caster.seat=&owner;caster.possessed=possessed;
  caster.tile.visible=visible?std::vector<Seat*>{&owner,&observer,&computer,&unassigned}:std::vector<Seat*>{};
  CreatureSkillMissileLaunch skill;GameEntity target;Tile targetTile;
  check(skill.tryUseFight(map,&caster,1,&target,&targetTile,false,true),"launch succeeds");
  MissileOneHit* missile=MissileOneHit::created.back();
  check(missile->position==Ogre::Vector3(caster.position.x,caster.position.y,.3f),"projectile starts at actual caster XY");
  check(missile->upkeep==1&&missile->speed==3,"first flight upkeep and speed unchanged");
  check(missile->creatureShot&&missile->possessedShot==possessed,"launch captures creature origin and exact possession state");
  check(map.events.size()==(visible?4:0),"visible humans receive creation and first path in the launch turn");
  if(visible&&map.events.size()==4){
   check(map.events[0].kind=="add"&&map.events[1].kind=="add"&&map.events[2].kind=="path"&&map.events[3].kind=="path","creation precedes movement notification");
   check(map.events[0].position==missile->position&&map.events[1].position==missile->position,"creation packet preserves launch origin");
  }
  for(const Event& event:map.events)check(event.seat!=&hidden&&event.seat!=&computer&&event.seat!=&unassigned,"no reveal to hidden or nonhuman seats");
  const std::vector<Event>::size_type previous=map.events.size();missile->notifySeatsWithVision(caster.tile.visible);
  check(map.events.size()==previous,"next visibility pass does not duplicate creation");
  delete missile;MissileOneHit::created.clear();
 }
 GameMap map;Creature invalid;invalid.invalid=true;CreatureSkillMissileLaunch skill;GameEntity target;Tile tile;
 check(!skill.tryUseFight(map,&invalid,1,&target,&tile,false,true)&&MissileOneHit::created.empty(),"invalid caster tile creates no missile");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('VISIBILITY', visibility).replace('LAUNCH', launch).replace('ORIGIN_METHODS', origin_methods)
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
with tempfile.TemporaryDirectory(prefix='odp-missile-launch-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{repo / "source"}', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
