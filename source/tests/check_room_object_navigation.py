"""Exercise the actual room-navigation integration with lightweight map fixtures."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
source = (repo / 'source/gamemap/RoomObjectNavigation.cpp').read_text()
source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
parser = argparse.ArgumentParser()
parser.add_argument('--food-source-ref')
parser.add_argument('--trace-food', action='store_true')
parser.add_argument('--trace-work', action='store_true')
parser.add_argument('--benchmark', action='store_true')
parser.add_argument('--packed-beds', action='store_true', help='Check the reported full-capacity dormitory transit regression')
parser.add_argument('--room-layouts', action='store_true', help='Check furnished room transit with production placement offsets')
parser.add_argument('--saved-terrain', type=Path)
parser.add_argument('--saved-worker', default='Kobold8', help='Exact saved worker name used for approach benchmarks')
parser.add_argument('--furniture-log', type=Path)
parser.add_argument('--saved-food-cases', action='store_true')
parser.add_argument('--saved-beds', action='store_true', help='Include exact saved bed positions, rotations and configured dimensions')
parser.add_argument('--source-ref')
args = parser.parse_args()
def read_source(relative):
    return (subprocess.check_output(['git', 'show', args.source_ref + ':' + relative], cwd=repo, text=True)
            if args.source_ref else (repo / relative).read_text())
if args.source_ref:
    source = re.sub(r'^#include[^\n]*\n', '', read_source('source/gamemap/RoomObjectNavigation.cpp'), flags=re.M)
if args.trace_food:
    source = source.replace('std::stable_sort(candidates.begin()', 'std::cout << "CANDIDATES " << creature.getMeshName() << " level=" << creature.getLevel() << " count=" << candidates.size() << "\\n"; std::stable_sort(candidates.begin()')
if args.trace_work:
    source = source.replace('const auto tiles = map.path(&creature, stagingTile);', 'if(creature.getLevel()==30&&(creature.getMeshName()=="Dragon.mesh"||creature.getMeshName()=="PitDemon.mesh"))std::cout<<"WORK_CAND "<<creature.getMeshName()<<" "<<object.getMeshName()<<" "<<point<<" stage="<<staging<<"\\n"; const auto tiles = map.path(&creature, stagingTile);')
    source = source.replace('path.clear();\n            return true;', 'if(creature.getLevel()==30&&(creature.getMeshName()=="Dragon.mesh"||creature.getMeshName()=="PitDemon.mesh"))std::cout<<"BAD_LEG "<<previous<<" -> "<<target<<"\\n"; path.clear();\n            return true;')
food_source = (subprocess.check_output(['git', 'show', args.food_source_ref + ':source/creatureaction/CreatureActionEatChicken.cpp'], cwd=repo, text=True)
               if args.food_source_ref else read_source('source/creatureaction/CreatureActionEatChicken.cpp'))
food_handler = food_source[food_source.index('bool CreatureActionEatChicken::handleEatChicken('):food_source.index('\nstd::string CreatureActionEatChicken::getListenerName()')]
work_gates = []
for room_class, kind in [('RoomWorkshop', 'workshop'), ('RoomLibrary', 'library'), ('RoomTrainingHall', 'trainingHall'), ('RoomCasino', 'casino')]:
    room_source = read_source(f'source/rooms/{room_class}.cpp')
    gate_start = room_source.index('    std::vector<Ogre::Vector2> approach;', room_source.index(f'bool {room_class}::useRoom('))
    gate_end = room_source.index('    // This creature is ready.' if kind == 'casino' else '    Ogre::Vector3 walkDirection', gate_start)
    work_gates.append(f'case RoomType::{kind}: {{\n' + room_source[gate_start:gate_end] + '\n break; }')
probe = r'''
#include "gamemap/RoomObjectNavigation.h"
#include "gamemap/RoomObjectBounds.h"
#include "gamemap/RoomObjectStep.h"
#include "rooms/RoomType.h"
#include "gamemap/Pathfinding.h"
#include <OgreVector3.h>
#include <map>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <iostream>
#include <chrono>
enum class CreatureActionType {sleep,leaveDungeon,useRoom};
namespace Helper {int round(float v){return int(std::round(v));}}
struct Room;
struct Tile {int x,y;bool walkable=true;Room* room=nullptr;Room* getCoveringRoom(){return room;}int getX()const{return x;}int getY()const{return y;}};
struct BuildingObject {
 std::string mesh="ChickenCoop";Ogre::Vector3 pos{5,5,0};float angle=0;
 const std::string& getMeshName()const{return mesh;}const Ogre::Vector3& getPosition()const{return pos;}
 float getRotationAngle()const{return angle;}
 Ogre::Vector2 furnitureScale=Ogre::Vector2::ZERO;const Ogre::Vector2& getFurnitureScale()const{return furnitureScale;}
};
struct Creature;
void placeBed(BuildingObject& bed,int x,int y,int width,int height,float rotation,const std::string& owner){
 for(const auto& bounds:RoomObjectPath::meshBounds)if(bed.mesh==bounds.name){
  const auto placed=RoomObjectPath::bedPlacement(bounds,x,y,width,height,rotation,owner);
  bed.pos={placed.x,placed.y,0};bed.angle=placed.angle;bed.furnitureScale={placed.scale.x,placed.scale.y};return;
 }
}
struct Room {
 RoomType type=RoomType::hatchery;std::map<Tile*,BuildingObject*> objects;std::vector<Creature*> users;
 RoomType getType()const{return type;}const auto& getBuildingObjects()const{return objects;}
 Creature* getCreatureUsingRoom(unsigned i){return i<users.size()?users[i]:nullptr;}
};
struct RoomPrison : Room {
 std::map<Tile*,BuildingObject*> fences;const auto& getFencingObjects()const{return fences;}
};
struct GameMap {
 int sizeX,sizeY;
 std::vector<Tile> tiles;std::vector<Room*> rooms;
 GameMap(int xCount=16,int yCount=16):sizeX(xCount),sizeY(yCount){for(int y=0;y<sizeY;++y)for(int x=0;x<sizeX;++x)tiles.push_back({x,y});}
 Tile* getTile(int x,int y){return x>=0&&x<sizeX&&y>=0&&y<sizeY?&tiles[y*sizeX+x]:nullptr;}
 const auto& getRooms()const{return rooms;}int getMapSizeX()const{return sizeX;}int getMapSizeY()const{return sizeY;}
 std::list<Tile*> path(Creature*,Tile*);
 bool pathExists(Creature* creature,Tile*,Tile* target){return !path(creature,target).empty();}
};
struct Creature {
 GameMap* map;Ogre::Vector3 pos{1,5,0};Tile* home=nullptr;std::set<CreatureActionType> actions;int level=1;
 std::string mesh="Kobold.mesh";const std::string& getMeshName()const{return mesh;}
 Ogre::Vector3 direction{0,-1,0};const Ogre::Vector3& getWalkDirection()const{return direction;}
 int cooldown=0,popped=0,walkActions=0,feeding=0,workReady=0;double food=0,hp=10;
 std::vector<Ogre::Vector2> walk;bool distortion=true;std::string animation;
 GameMap* getGameMap(){return map;}const Ogre::Vector3& getPosition()const{return pos;}
 Tile* getHomeTile()const{return home;}bool isActionInList(CreatureActionType a)const{return actions.count(a)>0;}
 int getLevel()const{return level;}bool canGoThroughTile(Tile* t)const{return t&&t->walkable;}
 Tile* getPositionTile(){return map->getTile(Helper::round(pos.x),Helper::round(pos.y));}
 static void tileToVector2(const std::list<Tile*>& tiles,std::vector<Ogre::Vector2>& path,bool skip,float){
  for(auto* tile:tiles){if(skip){skip=false;continue;}path.push_back({float(tile->x),float(tile->y)});}
 }
 bool decreaseJobCooldown(){if(cooldown>0){--cooldown;return false;}return true;}
 void popAction(){++popped;}void foodEaten(double value){food+=value;}
 void setJobCooldown(int value){cooldown=value;}double getHP()const{return hp;}void setHP(double value){hp=value;}
 void computeCreatureOverlayHealthValue(){}void fireChickenFeeding(const std::string&,const Ogre::Vector3&){++feeding;}
 void clearDestinations(const std::string& state,bool,bool){walk.clear();animation=state;}
 void setAnimationState(const std::string& state,bool,const Ogre::Vector3&,bool){animation=state;}
 void setWalkPath(const std::string&,const std::string&,bool,bool,const std::vector<Ogre::Vector2>&,bool);
 template<typename T>void pushAction(std::unique_ptr<T>){++walkActions;}
};
std::list<Tile*> GameMap::path(Creature* creature,Tile* target){
 auto* start=creature->getPositionTile();if(!start||!target)return {};
 std::vector<int> parents(sizeX*sizeY,-2);std::queue<int> open;int first=start->y*sizeX+start->x;
 parents[first]=-1;open.push(first);
 while(!open.empty()){
  int i=open.front();open.pop();if(&tiles[i]==target){std::list<Tile*> result;for(int p=i;p>=0;p=parents[p])result.push_front(&tiles[p]);return result;}
  for(auto direction:{Ogre::Vector2(-1,0),Ogre::Vector2(1,0),Ogre::Vector2(0,-1),Ogre::Vector2(0,1)}){
   auto* next=getTile(tiles[i].x+int(direction.x),tiles[i].y+int(direction.y));if(!next||!next->walkable)continue;
   int n=next->y*sizeX+next->x;if(parents[n]!=-2)continue;parents[n]=i;open.push(n);
  }
 }return {};
}
SOURCE
void Creature::setWalkPath(const std::string&,const std::string&,bool,bool,const std::vector<Ogre::Vector2>& path,bool jitter){
 walk=path;distortion=jitter;if(RoomObjectNavigation::refine(*this,walk))distortion=false;
}
struct ChickenEntity {
 GameMap* map;Ogre::Vector3 pos;int consumed=0;
 Tile* getPositionTile(){return map->getTile(Helper::round(pos.x),Helper::round(pos.y));}
 const Ogre::Vector3& getPosition()const{return pos;}std::string getName()const{return "Chicken";}
 bool eatChicken(Creature*){if(consumed)return false;++consumed;return true;}
};
struct CreatureActionWalkToTile {CreatureActionWalkToTile(Creature&){}};
struct CreatureActionEatChicken {static bool handleEatChicken(Creature&,ChickenEntity*);};
struct ConfigManager {
 static ConfigManager& getSingleton(){static ConfigManager config;return config;}
 double getRoomConfigDouble(const char*){return 10;}unsigned getRoomConfigUInt32(const char*){return 1;}
};
namespace Random {int Int(int first,int){return first;}}
namespace Utils {using std::make_unique;}
namespace EntityAnimation {const std::string walk_anim="Walk",idle_anim="Idle",eat_chicken_anim="EatChicken";}
#define OD_LOG_ERR(...) ((void)0)
FOOD_HANDLER
bool roomWorkGate(RoomType type,Creature& creature,BuildingObject* ro,const Ogre::Vector2& wanted){
 auto* object=ro;float wantedX=wanted.x,wantedY=wanted.y;
 switch(type){WORK_GATES default:return false;}
 ++creature.workReady;return false;
}
int checks=0,failures=0;
void check(bool ok,const char* reason){++checks;if(!ok){++failures;std::cout<<"FAIL "<<reason<<'\n';}}
int main(){
 GameMap map;Room room;map.rooms.push_back(&room);for(auto& tile:map.tiles)tile.room=&room;
 BuildingObject object;room.objects[map.getTile(5,5)]=&object;Creature creature{&map};
 for(const auto& row:RoomObjectPath::meshBounds)for(float rotation:{0.f,30.f,45.f,90.f,180.f,270.f}){
  object.mesh=row.name;object.angle=rotation;object.pos={5,5,0};
  const float angle=rotation*.01745329252f;
  const auto placed=RoomObjectNavigation::collect(map,0);
  if(object.mesh=="PortalObject"||object.mesh=="DungeonTempleObject"){
   check(placed.empty(),"portal and dungeon heart do not create furniture blockers");
   for(const auto& model:RoomObjectPath::walkingRadii)for(int level:{1,30}){
    creature.mesh=model.name;creature.level=level;creature.pos={1,5,0};
    std::vector<Ogre::Vector2> transit{{5,5},{10,5}};
    check(!RoomObjectNavigation::blocked(creature,transit),"every creature can traverse portal and heart");
    RoomObjectNavigation::refine(creature,transit);
    check(transit.size()==2&&transit.back()==Ogre::Vector2(10,5),"landmark transit retains its destination");
   }
   creature.mesh="Kobold.mesh";creature.level=1;
   continue;
  }
  const auto furnitureScale=RoomObjectPath::furnitureScale(row);
  check(placed.size()==1&&placed.front().minimum==Ogre::Vector2(row.minX*furnitureScale.x,row.minY*furnitureScale.y)&&
   placed.front().maximum==Ogre::Vector2(row.maxX*furnitureScale.x,row.maxY*furnitureScale.y),
   "placed navigation bounds match the shared visible furniture scale");
  const float crossingY=5+std::sin(angle)*(row.minX+row.maxX)*.5f+std::cos(angle)*(row.minY+row.maxY)*.5f;
  creature.pos={1,crossingY,0};
  std::vector<Ogre::Vector2> path;for(int x=2;x<=10;++x)path.push_back({float(x),crossingY});
  check(RoomObjectNavigation::blocked(creature,path)!=(std::string(row.name)=="GoblinBed"),"straight crossing is permitted only over the measured low nest");
  check(RoomObjectNavigation::refine(creature,path),"furniture routes suppress independent client jitter");
  if(path.empty())std::cout<<"NO_ROUTE "<<row.name<<" rotation="<<rotation<<'\n';
  check(!path.empty()&&path.back()==Ogre::Vector2(10,crossingY),"route keeps accessible destination");
  check(!RoomObjectNavigation::blocked(creature,path),"all routed segments clear placed furniture");
 }
 object.mesh="ChickenCoop";object.angle=0;
 auto obstacles=RoomObjectNavigation::collect(map,.1f);Ogre::Vector2 spawn;
 check(RoomObjectNavigation::standingPosition(obstacles,{5,5},spawn),"chicken can spawn beside its coop");
 check(RoomObjectPath::clearPoint(obstacles,spawn)&&Helper::round(spawn.x)==5&&Helper::round(spawn.y)==5,"spawn clears real off-center coop and remains in hatchery tile");
 for(auto start:{Ogre::Vector3(6,5,0),Ogre::Vector3(5.4f,5,0),Ogre::Vector3(4,5,0)}){
  creature.pos=start;std::vector<Ogre::Vector2> approach;
  check(RoomObjectNavigation::foodApproach(creature,spawn,approach),"free food approach exists from either side and the same logical tile");
  check(!approach.empty()&&!RoomObjectNavigation::blocked(creature,approach),"food approach clears furniture throughout");
  if(!approach.empty())check(RoomObjectPath::clearSegment(RoomObjectNavigation::collect(map,0),approach.back(),spawn),"eater ends on chicken side of coop");
 }
 std::vector<Ogre::Vector2> inaccessible;
 check(!RoomObjectNavigation::foodApproach(creature,{5.2f,5},inaccessible)&&inaccessible.empty(),"food still inside solid coop is not reachable through its wall");
 for(const auto& model:RoomObjectPath::walkingRadii)for(int level:{1,30}){
  creature=Creature{&map};creature.mesh=model.name;creature.level=level;creature.pos={8,5,0};
  std::vector<Ogre::Vector2> approach;
  bool found=RoomObjectNavigation::foodApproach(creature,spawn,approach);
  // At maximum size these forward walking envelopes cannot fit anywhere in
  // the existing five-tile eating reach while facing food against the coop.
  // Extending eating range or shrinking their measured bodies is not this fix.
  const bool tooClose=level==30&&(creature.mesh=="Defender.mesh"||creature.mesh=="Dragon.mesh");
  check(found!=tooClose,"food against coop is reached only when the body fits within existing eating reach");
  check(!RoomObjectNavigation::blocked(creature,approach),"body-sized food approach has no furniture crossing");
  if(tooClose){
   ChickenEntity tight{&map,{spawn.x,spawn.y,0}};
   CreatureActionEatChicken::handleEatChicken(creature,&tight);
   check(tight.consumed==0&&creature.popped==1&&creature.food==0&&creature.feeding==0,"oversized eater releases inaccessible wall-side chicken without reward or animation");
  }
  approach.clear();
  check(RoomObjectNavigation::foodApproach(creature,{4,5},approach),"all models and endpoint levels reach food once it wanders clear of the coop");
  check(!approach.empty()&&!RoomObjectNavigation::blocked(creature,approach),"free-food approach preserves full measured body clearance");
  if(!approach.empty()){
   creature.pos={approach.back().x,approach.back().y,0};ChickenEntity clearFood{&map,{4,5,0}};
   CreatureActionEatChicken::handleEatChicken(creature,&clearFood);
   check(clearFood.consumed==1&&creature.feeding==1&&creature.food==10,"each body-sized arrival can consume once through the actual food action");
  }
 }
 creature=Creature{&map};
 std::vector<Ogre::Vector2> far{{2,0},{3,0},{4,0}};creature.pos={1,0,0};const auto unchanged=far;
 check(!RoomObjectNavigation::refine(creature,far)&&far==unchanged,"distant paths retain prior jitter behavior");
 creature.pos={1,5,0};
 for(RoomType type:{RoomType::workshop,RoomType::library,RoomType::trainingHall}){
  room.type=type;object.pos={5,type==RoomType::library?5.3f:5.2f,0};object.angle=type==RoomType::library?45.f:30.f;
  object.mesh=type==RoomType::workshop?"WorkshopMachine1":type==RoomType::library?"Bookcase":"TrainingDummy1";
  const Ogre::Vector2 target=type==RoomType::workshop?Ogre::Vector2(5.7f,5):Ogre::Vector2(5,4.7f);
  std::vector<Ogre::Vector2> path{{2,5},{3,5},{4,5},{5,5},target};RoomObjectNavigation::refine(creature,path);
  check(!path.empty(),"existing workstation remains accessible");
  if(!path.empty())check(Helper::round(path.back().x)==Helper::round(target.x)&&Helper::round(path.back().y)==Helper::round(target.y),"working destination stays on its existing logical tile");
  check(!RoomObjectNavigation::blocked(creature,path),"working approach does not pass through its machine");
 }
 for(const auto& model:RoomObjectPath::walkingRadii)for(int level:{1,30})for(RoomType type:{RoomType::workshop,RoomType::library,RoomType::trainingHall,RoomType::casino}){
  creature=Creature{&map};creature.mesh=model.name;creature.level=level;
  room.type=type;object.pos={5,type==RoomType::library?5.3f:5.2f,0};object.angle=type==RoomType::library?45.f:30.f;
  object.mesh=type==RoomType::workshop?"WorkshopMachine1":type==RoomType::library?"Bookcase":type==RoomType::casino?"CasinoPokerTable":"TrainingDummy1";
  const Ogre::Vector2 target=type==RoomType::workshop?Ogre::Vector2(5.7f,5):Ogre::Vector2(5,4.7f);
  const Ogre::Vector2 offset=type==RoomType::workshop?Ogre::Vector2(-1,1):Ogre::Vector2::ZERO;
  std::vector<Ogre::Vector2> path;bool found=RoomObjectNavigation::workApproach(creature,object,target,offset,path);
  if(path.empty())std::cout<<"NO_WORK_ACCESS "<<model.name<<" level="<<level<<" object="<<object.mesh<<'\n';
  check(found&&!path.empty(),"body-sized workstation remains accessible");
  check(!RoomObjectNavigation::blocked(creature,path),"every work-approach segment clears full walking body");
  if(!path.empty()){
   creature.pos={path.back().x,path.back().y,0};
   check(RoomObjectPath::clearPoint(RoomObjectNavigation::bodyObstacles(creature),path.back(),Ogre::Vector2(object.pos.x,object.pos.y)+offset-path.back()),"work facing does not rotate the creature back into furniture");
   const bool arrived=RoomObjectNavigation::workApproach(creature,object,target,offset,path)&&path.empty();
   if(!arrived)std::cout<<"WORK_RESELECT "<<model.name<<" level="<<level<<" object="<<object.mesh<<" position="<<creature.pos<<" wanted="<<target<<" facing="<<Ogre::Vector2(object.pos.x,object.pos.y)+offset<<'\n';
   check(arrived,"arrival becomes work-ready instead of repeatedly returning to obstructed tile center");
   roomWorkGate(type,creature,&object,target);
   check(creature.workReady==1&&creature.walkActions==0,"actual room arrival gate permits work at clearance-adjusted position");
   creature.pos={1,5,0};creature.workReady=0;
   roomWorkGate(type,creature,&object,target);
   check(creature.workReady==0&&creature.walkActions==1&&!creature.walk.empty()&&!creature.distortion,"actual room gate walks before work and suppresses client offsets");
  }
 }
 creature=Creature{&map};
 for(auto& tile:map.tiles)tile.walkable=false;
 map.getTile(1,5)->walkable=true;object.mesh="Bookcase";object.pos={5,5.3f,0};object.angle=45;room.type=RoomType::library;
 for(auto type:{RoomType::library,RoomType::workshop,RoomType::trainingHall,RoomType::casino}){
  creature.popped=0;
  check(!roomWorkGate(type,creature,&object,{5,4.7f}),"failed room approach ends this action tick instead of immediate reselection");
 }
 check(creature.popped==1&&creature.workReady==0&&creature.walk.empty(),"inaccessible workstation releases its job instead of looping or awarding work");
 for(auto& tile:map.tiles)tile.walkable=true;
 creature=Creature{&map};
 object.pos={5,5,0};object.angle=0;object.mesh="Bed";room.type=RoomType::dormitory;
 creature.home=map.getTile(5,5);creature.actions.insert(CreatureActionType::sleep);
 std::vector<Ogre::Vector2> sleep{{2,5},{3,5},{4,5},{5,5}};
 RoomObjectNavigation::refine(creature,sleep);
 check(!sleep.empty()&&sleep.back()==Ogre::Vector2(5,5),"intentional own-bed entry retains accepted sleep endpoint");
 creature.actions.clear();sleep={{2,5},{3,5},{4,5},{5,5},{6,5},{7,5}};
 RoomObjectNavigation::refine(creature,sleep);
 check(!sleep.empty()&&!RoomObjectNavigation::blocked(creature,sleep),"same bed is solid during ordinary traversal");
 room.objects.clear();std::vector<Ogre::Vector2> restored{{2,5},{3,5},{4,5},{5,5}};
 check(!RoomObjectNavigation::refine(creature,restored),"removing furniture immediately reopens original route");
 room.objects[map.getTile(5,5)]=&object;
 check(RoomObjectNavigation::blocked(creature,restored),"placing furniture invalidates existing crossing route");
 // A previously jittered path can overlap a newly placed object even when the
 // server's centerline is clear. Placement must invalidate that larger envelope.
 creature.pos={1,6.4f,0};restored={{9,6.4f}};object.angle=45;
 check(!RoomObjectNavigation::blocked(creature,restored)&&RoomObjectNavigation::blocked(creature,restored,true),"placement accounts for client offsets around rotated furniture");
 creature.pos={5,5,0};restored={{9,5}};
 check(RoomObjectNavigation::blocked(creature,restored,true),"new furniture overlapping a path start invalidates it before an explicit escape is planned");
 creature.pos={1,5,0};object.angle=0;
 RoomPrison prison;prison.type=RoomType::prison;BuildingObject fence;fence.mesh="FenceStraight";fence.pos={5,5,0};fence.angle=90;
 prison.fences[map.getTile(5,5)]=&fence;map.rooms={&prison};restored={{9,5}};
 check(RoomObjectNavigation::blocked(creature,restored),"separately stored prison fences are solid navigation obstacles");
 RoomObjectNavigation::refine(creature,restored);
 check(!restored.empty()&&!RoomObjectNavigation::blocked(creature,restored),"prison-fence route goes around its physical footprint");
 map.rooms={&room};
 for(int y=0;y<16;++y)map.getTile(6,y)->walkable=false;
 restored={{2,5},{3,5},{4,5},{5,5},{6,5},{7,5}};
 RoomObjectNavigation::refine(creature,restored);
 check(restored.empty(),"failed refinement never sends a route through blocked terrain");
 for(auto& tile:map.tiles)tile.walkable=true;
 object.mesh="ChickenCoop";object.pos={5,5,0};object.angle=0;room.type=RoomType::hatchery;
 for(auto start:{Ogre::Vector3(6,5,0),Ogre::Vector3(5.4f,5.2f,0),Ogre::Vector3(4.7f,5,0)}){
  creature=Creature{&map};creature.pos=start;ChickenEntity chicken{&map,{4.6875f,4.9375f,0}};
  CreatureActionEatChicken::handleEatChicken(creature,&chicken);
  check(chicken.consumed==0&&creature.feeding==0&&creature.food==0&&creature.hp==10,"blocked or overlapping eater receives no food or animation before walking");
  check(creature.walkActions==1&&!creature.walk.empty()&&!creature.distortion,"food action requests the checked route without client jitter");
  for(int turn=0;turn<20&&!creature.walk.empty()&&chicken.consumed==0;++turn){
   check(!RoomObjectNavigation::blocked(creature,creature.walk),"every approach step is collision-free");
   creature.pos={creature.walk.back().x,creature.walk.back().y,0};creature.walk.clear();
   CreatureActionEatChicken::handleEatChicken(creature,&chicken);
  }
  check(chicken.consumed==1&&creature.feeding==1&&creature.food==10&&creature.hp==20,"eater eventually reaches free side and eats exactly once");
  check(creature.cooldown==1&&creature.animation=="EatChicken","accepted feeding animation and cooldown are preserved");
 }
 creature=Creature{&map};creature.pos={6,5,0};ChickenEntity trapped{&map,{5.2f,5,0}};
 check(!CreatureActionEatChicken::handleEatChicken(creature,&trapped),"failed food approach ends this action tick instead of immediate reselection");
 check(trapped.consumed==0&&creature.popped==1&&creature.walk.empty(),"unreachable chicken releases the action rather than consuming through furniture");
 room.objects.clear();creature=Creature{&map};creature.pos={4,5,0};ChickenEntity adjacent{&map,{5,5,0}};
 CreatureActionEatChicken::handleEatChicken(creature,&adjacent);
 check(adjacent.consumed==1&&creature.walkActions==0&&creature.food==10,"unobstructed adjacent feeding remains immediate");
 creature=Creature{&map};ChickenEntity distant{&map,{8,5,0}};
 CreatureActionEatChicken::handleEatChicken(creature,&distant);
 check(distant.consumed==0&&creature.distortion&&creature.walk.size()==5&&creature.walk.back()==Ogre::Vector2(6,5),"unobstructed distant chase retains original tile path and 80 percent truncation");
 PACKED_BEDS
 ROOM_LAYOUTS
 BENCHMARK
 SAVED_TERRAIN
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('SOURCE', source).replace('FOOD_HANDLER', food_handler).replace('WORK_GATES', '\n'.join(work_gates))
probe = probe.replace('PACKED_BEDS', r'''
 {
  // A completely furnished room with two opposing doorways: an open-map
  // benchmark can route around the room and misses the reported regression.
  GameMap packed(11,11);Room dormitory;dormitory.type=RoomType::dormitory;
  packed.rooms={&dormitory};
  for(auto& tile:packed.tiles)tile.walkable=false;
  std::vector<BuildingObject> beds(9);
  for(int y=4;y<=6;++y)for(int x=4;x<=6;++x){
   auto* tile=packed.getTile(x,y);tile->walkable=true;tile->room=&dormitory;
   auto& bed=beds[(y-4)*3+x-4];bed.mesh="ImpBed";bed.pos={float(x),float(y),0};
   placeBed(bed,x,y,1,1,0,"Creature"+std::to_string((y-4)*3+x-4));
   dormitory.objects[tile]=&bed;
  }
  for(int x:{2,3,7,8})packed.getTile(x,5)->walkable=true;
  for(int level:{1,30})for(bool reverse:{false,true}){
   Creature walker{&packed};walker.level=level;walker.pos={reverse?8.f:2.f,5,0};
   auto* destination=packed.getTile(reverse?2:8,5);
   auto coarse=packed.path(&walker,destination);
   check(!coarse.empty(),"packed dormitory terrain connects its opposing doorways");
   std::vector<Ogre::Vector2> transit;
   Creature::tileToVector2(coarse,transit,true,0);
   RoomObjectNavigation::refine(walker,transit);
   check(!transit.empty(),"packed worker beds must not make the dormitory impassable");
   if(transit.empty())std::cout<<"PACKED_BLOCKED level="<<level<<" reverse="<<reverse<<'\n';
   if(!transit.empty()){
    check(transit.back()==Ogre::Vector2(float(destination->x),float(destination->y)),"packed-bed transit reaches the opposite doorway");
    auto previous=Ogre::Vector2(walker.pos.x,walker.pos.y);
    for(const auto& point:transit){
     check(terrainClear(walker,previous,point),"packed-bed transit cannot escape through the surrounding walls");
     check(RoomObjectPath::clearSegment(RoomObjectNavigation::bodyObstacles(walker),previous,point),
      "packed-bed transit keeps the full walking body outside visible furniture");
     previous=point;
    }
   }
   check(dormitory.objects.size()==9,"packed-bed transit preserves all nine placed beds");
  }
 }
 {
  struct BedLayout{const char* name;int width,height;};
  const BedLayout layouts[]={
   {"Bed",1,2},{"ImpBed",1,1},{"GoblinBed",1,1},{"SpiderBed",1,1},
   {"TentacleBed",1,1},{"KnightCoffin",1,2},{"StoneCoffin",1,2},
   {"LizardmanBed",1,2},{"OrcBed",1,2},{"RangerBed",1,2},
   {"DragonBed",2,2},{"TrollBed",2,2}
  };
  for(const auto& layout:layouts)for(bool rotated:{false,true})for(bool vertical:{false,true}){
   const int w=rotated?layout.height:layout.width,h=rotated?layout.width:layout.height;
   GameMap packed(3*w+8,3*h+8);Room dormitory;dormitory.type=RoomType::dormitory;
   packed.rooms={&dormitory};for(auto& tile:packed.tiles)tile.walkable=false;
   for(int y=4;y<4+3*h;++y)for(int x=4;x<4+3*w;++x){
    auto* tile=packed.getTile(x,y);tile->walkable=true;tile->room=&dormitory;
   }
   std::vector<BuildingObject> beds(9);
   for(int y=0;y<3;++y)for(int x=0;x<3;++x){
    auto& bed=beds[y*3+x];bed.mesh=layout.name;bed.angle=rotated?90.f:0.f;
    bed.pos={4+x*w+w*.5f-.5f,4+y*h+h*.5f-.5f,0};
    placeBed(bed,4+x*w,4+y*h,w,h,bed.angle,"Creature"+std::to_string(y*3+x));
    dormitory.objects[packed.getTile(4+x*w,4+y*h)]=&bed;
   }
   const int centerX=4+w,centerY=4+h;
   for(int i:{2,3}){
    packed.getTile(vertical?centerX:i,vertical?i:centerY)->walkable=true;
    packed.getTile(vertical?centerX:3*w+i+2,vertical?3*h+i+2:centerY)->walkable=true;
   }
   for(int level:{1,30})for(bool reverse:{false,true}){
    const Ogre::Vector3 first(vertical?float(centerX):2.f,vertical?2.f:float(centerY),0);
    const Ogre::Vector3 last(vertical?float(centerX):float(3*w+5),vertical?float(3*h+5):float(centerY),0);
    Creature walker{&packed};walker.level=level;walker.pos=reverse?last:first;
    const auto target=reverse?first:last;
    auto coarse=packed.path(&walker,packed.getTile(int(target.x),int(target.y)));
    std::vector<Ogre::Vector2> path;Creature::tileToVector2(coarse,path,true,0);
    RoomObjectNavigation::refine(walker,path);
    if(path.empty())std::cout<<"BED_LAYOUT_BLOCKED "<<layout.name<<" rotated="<<rotated<<" vertical="<<vertical<<" level="<<level<<" reverse="<<reverse<<'\n';
    check(!path.empty()&&path.back()==Ogre::Vector2(target.x,target.y),"every full-capacity bed layout retains worker transit");
    const auto obstacles=RoomObjectNavigation::bodyObstacles(walker);
    auto previous=Ogre::Vector2(walker.pos.x,walker.pos.y);
    for(const auto& point:path){
     bool clear=true;
     for(auto obstacle:obstacles)if(obstacle.intersects(previous,point)){
      const float rise=RoomObjectPath::prepareLowStep(obstacle,walker.mesh,1+.02f*walker.level,walker.pos.z);
      if(rise<=0){clear=false;break;}
      for(int sample=0;sample<=64;++sample){
       const auto at=previous+(point-previous)*(sample/64.f);
       if(obstacle.contains(at,point-previous)&&RoomObjectPath::lowStepElevation(obstacle,at,point-previous,rise)<rise-.00001f)clear=false;
      }
     }
     check(terrainClear(walker,previous,point)&&clear,
      "bed-layout transit stays clear on the ground or visibly above an eligible low nest");
     previous=point;
    }
    check(dormitory.objects.size()==9,"bed layouts retain all placed objects");
   }
  }
 }
''' if args.packed_beds else '')
probe = probe.replace('ROOM_LAYOUTS', r'''
 for(bool vertical:{false,true})for(bool reverse:{false,true}){
  GameMap packed(12,12);Room dormitory;dormitory.type=RoomType::dormitory;packed.rooms={&dormitory};
  for(auto& tile:packed.tiles){tile.walkable=false;tile.room=&dormitory;}
  for(int y=4;y<=6;++y)for(int x=4;x<=6;++x)packed.getTile(x,y)->walkable=true;
  for(int i:{2,3,7,8})packed.getTile(vertical?5:i,vertical?i:5)->walkable=true;
  std::vector<BuildingObject> beds(9);
  for(int y=0;y<3;++y)for(int x=0;x<3;++x){
   auto& bed=beds[y*3+x];bed.mesh="ImpBed";
   placeBed(bed,4+x,4+y,1,1,0,"Creature"+std::to_string(y*3+x));dormitory.objects[packed.getTile(4+x,4+y)]=&bed;
  }
  Creature walker{&packed};walker.mesh="Rat.mesh";walker.level=2;
  walker.pos={vertical?5.f:reverse?8.f:2.f,vertical?(reverse?8.f:2.f):5.f,0};
  const Ogre::Vector2 food(vertical?5.f:reverse?2.f:8.f,vertical?(reverse?2.f:8.f):5.f);
  std::vector<Ogre::Vector2> direct{food};RoomObjectNavigation::refine(walker,direct);
  check(!direct.empty(),"rat fits the measured tall-bed lane in both directions");
  std::vector<Ogre::Vector2> approach;
  check(RoomObjectNavigation::foodApproach(walker,food,approach),"food search must use the same usable bed lane as ordinary movement");
  if(!approach.empty())check(!RoomObjectNavigation::blocked(walker,approach),"food lane keeps the actual rat body outside tall beds");
 }
 for(bool lowNest:{false,true})for(bool reverse:{false,true}){
  GameMap packed(12,12);Room dormitory;dormitory.type=RoomType::dormitory;packed.rooms={&dormitory};
  for(auto& tile:packed.tiles){tile.walkable=false;tile.room=&dormitory;}
  for(int y=4;y<=6;++y)for(int x=4;x<=6;++x)packed.getTile(x,y)->walkable=true;
  for(int x:{2,3,7,8})packed.getTile(x,5)->walkable=true;
  std::vector<BuildingObject> beds(9);
  for(int y=0;y<3;++y)for(int x=0;x<3;++x){
   auto& bed=beds[y*3+x];bed.mesh=lowNest?"GoblinBed":"ImpBed";
   placeBed(bed,4+x,4+y,1,1,0,"Creature"+std::to_string(y*3+x));dormitory.objects[packed.getTile(4+x,4+y)]=&bed;
  }
  Creature walker{&packed};walker.level=30;walker.pos={reverse?8.f:2.f,5,0};
  const Ogre::Vector2 food(reverse?2.f:8.f,5);std::vector<Ogre::Vector2> approach;
  const bool reached=RoomObjectNavigation::foodApproach(walker,food,approach);
  check(reached==lowNest,"food approach can cross low nests but not higher bed parts");
  if(reached){
   check(!approach.empty()&&!RoomObjectNavigation::blocked(walker,approach),"food transit uses the same server step permission");
   check(RoomObjectPath::clearPoint(RoomObjectNavigation::bodyObstacles(walker),approach.back(),food-approach.back()),"food interaction still stands outside the bed");
   auto previous=Ogre::Vector2(walker.pos.x,walker.pos.y);
   for(const auto& point:approach){check(terrainClear(walker,previous,point),"food stepping cannot bypass corridor walls");previous=point;}
  }
 }
 for(bool lowNest:{false,true}){
  // Actual Rat bodies and Kobold lower-body triangles fit their respective
  // lanes; neither a root-aligned grid nor upper-body overhang may hide them.
  GameMap packed(12,12);Room dormitory;dormitory.type=RoomType::dormitory;
  packed.rooms={&dormitory};for(auto& tile:packed.tiles)tile.room=&dormitory;
  std::vector<BuildingObject> beds(9);
  for(int y=0;y<3;++y)for(int x=0;x<3;++x){
   auto& bed=beds[y*3+x];bed.mesh=lowNest?"GoblinBed":"ImpBed";
   placeBed(bed,4+x,4+y,1,1,0,"Creature"+std::to_string(y*3+x));
   dormitory.objects[packed.getTile(4+x,4+y)]=&bed;
  }
  for(bool vertical:{false,true})for(bool reverse:{false,true})for(bool coarseDetour:{false,true}){
   Creature walker{&packed};walker.mesh=lowNest?"Kobold.mesh":"Rat.mesh";
   walker.pos={vertical?5.f:reverse?8.f:2.f,vertical?(reverse?8.f:2.f):5.f,0};
   const Ogre::Vector2 goal(vertical?5.f:reverse?2.f:8.f,vertical?(reverse?2.f:8.f):5.f);
   std::vector<Ogre::Vector2> path;
   if(coarseDetour){path.push_back({2,3});path.push_back({8,3});}
   path.push_back(goal);RoomObjectNavigation::refine(walker,path);
   bool crossedGap=false;
   float length=0;auto previous=Ogre::Vector2(walker.pos.x,walker.pos.y);
   for(const auto& point:path){length+=previous.distance(point);
    const int along=vertical?1:0,across=1-along;
    if(std::min(previous[along],point[along])<=5&&std::max(previous[along],point[along])>=5&&std::abs(point[along]-previous[along])>.00001f){
     const float crossing=previous[across]+(point[across]-previous[across])*(5-previous[along])/(point[along]-previous[along]);
     crossedGap|=crossing>3.5f&&crossing<6.5f;
    }
    check(terrainClear(walker,previous,point)&&RoomObjectPath::clearSegment(RoomObjectNavigation::bodyObstacles(walker),previous,point),"free-strip route retains full body clearance");previous=point;}
   std::cout<<"GAP_ROUTE reverse="<<reverse<<" length="<<length<<'\n';
   check(!path.empty()&&path.back()==goal&&length<7.f&&crossedGap,"usable bed strips are preferred to an outside detour");
  }
 }
 {
  GameMap map(12,12);Room room;map.rooms={&room};BuildingObject nest;nest.mesh="GoblinBed";
  room.objects[map.getTile(5,5)]=&nest;Creature walker{&map};
  const auto low=RoomObjectNavigation::bodyObstacles(walker);
  check(low.size()==1&&!RoomObjectPath::clearPoint(low,{5,5}),"low bed still blocks the worker's feet");
  nest.pos.z=.1f;const auto raised=RoomObjectNavigation::bodyObstacles(walker);
  check(raised.size()==1&&low.front().bodyMaximum.x-low.front().bodyMinimum.x<
   raised.front().bodyMaximum.x-raised.front().bodyMinimum.x,"only furniture entirely below the measured body band uses lower-body clearance");
  nest.pos.z=0;walker.mesh="CaveHornet.mesh";
  check(RoomObjectNavigation::bodyObstacles(walker).empty(),"flying body above the low nest is not projected into it");
  nest.pos.z=.1f;
  check(!RoomObjectNavigation::bodyObstacles(walker).empty(),"higher obstacles still block the same flyer");
  nest.pos.z=0;walker.mesh="Unknown.mesh";
  check(!RoomObjectPath::clearPoint(RoomObjectNavigation::bodyObstacles(walker),{5,5}),"unknown bodies retain conservative collision");
  walker.mesh="Spider.mesh";const auto spider=RoomObjectNavigation::bodyObstacles(walker);
  check(spider.size()==1&&spider.front().bodyMaximum.x-spider.front().bodyMinimum.x>.7f,"wide ground legs are not narrowed to manufacture a route");
 }
 {
  struct Layout{const char* mesh;float angle,yOffset;int spacing;};
  const Layout layouts[]={
   {"ChickenCoop",0,0,2},{"Bookcase",45,.3f,2},{"Podium",45,.3f,2},
   {"TrainingDummy1",0,.3f,2},{"TrainingDummy2",0,.3f,2},
   {"TrainingDummy3",0,.3f,2},{"TrainingDummy4",0,.3f,2},
   {"WorkshopMachine1",30,.2f,2},{"WorkshopMachine2",30,.2f,2},
   {"GoldstackLv1",0,0,1},{"GoldstackLv2",45,0,1},
   {"GoldstackLv3",90,0,1},{"GoldstackLv4",135,0,1},
   {"CasinoPokerTable",0,0,2},{"Roulette",0,0,2},{"TortureObject",0,0,2},
   {"CelticCross",0,0,2},{"KnightStatue",0,0,2},{"KnightStatue2",0,0,2}
  };
  for(const auto& layout:layouts)for(bool vertical:{false,true})for(int placement:{0,1,2}){
   if(placement!=0&&layout.spacing!=1)continue;
   GameMap packed(13,13);Room room;packed.rooms={&room};
   for(auto& tile:packed.tiles)tile.walkable=false;
   for(int y=3;y<=9;++y)for(int x=3;x<=9;++x){
    auto* tile=packed.getTile(x,y);tile->walkable=true;tile->room=&room;
   }
   std::vector<BuildingObject> furniture;
   furniture.reserve(49);
   const int edge=layout.spacing==1?3:4;
   for(int y=edge;y<=12-edge;y+=layout.spacing)for(int x=edge;x<=12-edge;x+=layout.spacing){
    furniture.emplace_back();auto& item=furniture.back();item.mesh=layout.mesh;
    item.angle=layout.angle;item.pos={float(x),float(y)+layout.yOffset,0};
    if(placement!=0){
     // Treasury placement permits +/-0.2 on each axis and arbitrary angles.
     // Exercise opposite extreme offsets, not only centered demonstration piles.
     const float sign=placement==1?1.f:-1.f;
     item.pos.x+=sign*(x%2?.2f:-.2f);item.pos.y+=sign*(y%2?.2f:-.2f);
     item.angle+=float((x*17+y*31)%90);
    }
    room.objects[packed.getTile(x,y)]=&item;
   }
   for(int i:{1,2,10,11})packed.getTile(vertical?6:i,vertical?i:6)->walkable=true;
   for(int level:{1,30})for(bool reverse:{false,true}){
    Creature walker{&packed};walker.level=level;
    walker.pos={vertical?6.f:reverse?11.f:1.f,vertical?(reverse?11.f:1.f):6.f,0};
    const Ogre::Vector2 target(vertical?6.f:reverse?1.f:11.f,vertical?(reverse?1.f:11.f):6.f);
    auto coarse=packed.path(&walker,packed.getTile(int(target.x),int(target.y)));
    std::vector<Ogre::Vector2> path;Creature::tileToVector2(coarse,path,true,0);
    RoomObjectNavigation::refine(walker,path);
    if(path.empty())std::cout<<"ROOM_LAYOUT_BLOCKED "<<layout.mesh<<" placement="<<placement<<" vertical="<<vertical<<" level="<<level<<" reverse="<<reverse<<'\n';
    check(!path.empty()&&path.back()==target,"furnished room retains worker transit");
    const auto obstacles=RoomObjectNavigation::bodyObstacles(walker);
    auto previous=Ogre::Vector2(walker.pos.x,walker.pos.y);
    for(const auto& point:path){
     check(terrainClear(walker,previous,point)&&RoomObjectPath::clearSegment(obstacles,previous,point),
      "room-layout route clears visible furniture without crossing walls");
     previous=point;
    }
    check(room.objects.size()==furniture.size(),"room transit retains every object");
   }
  }
 }
''' if args.room_layouts else '')
probe = probe.replace('BENCHMARK', r'''
 std::vector<BuildingObject> crowd(100);room.type=RoomType::library;
 for(int i=0;i<100;++i){crowd[i].mesh="Bookcase";crowd[i].pos={float(2+i%10),float(2+i/10),0};crowd[i].angle=45;room.objects[map.getTile(2+i%10,2+i/10)]=&crowd[i];}
 creature=Creature{&map};creature.pos={1,1,0};
 const auto began=std::chrono::steady_clock::now();
 for(int i=0;i<20;++i){std::vector<Ogre::Vector2> route{{14,14}};RoomObjectNavigation::refine(creature,route);check(!route.empty()&&!RoomObjectNavigation::blocked(creature,route),"dense-room route remains traversable and collision-free");}
 const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-began).count();
 std::cout<<"DENSE_ROOM_20_ROUTES_MS="<<elapsed<<'\n';
 // Use the actual library placement offset for the work-approach measurements.
 for(auto& station:crowd)station.pos.y+=.3f;
 long long workTotal=0,workMax=0;int workCalls=0,workReached=0;
 for(const auto& model:RoomObjectPath::walkingRadii)for(int level:{1,30})for(int index:{0,55}){
  creature=Creature{&map};creature.mesh=model.name;creature.level=level;creature.pos={1,1,0};
  auto& station=crowd[index];
  // Library furniture is 0.3 above the tile center; the working target is 0.3 below it.
  const Ogre::Vector2 wanted(station.pos.x,station.pos.y-.6f);
  std::vector<Ogre::Vector2> route;
  const auto started=std::chrono::steady_clock::now();
  const bool found=RoomObjectNavigation::workApproach(creature,station,wanted,{0,0},route);
  const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started).count();
  workTotal+=us;workMax=std::max(workMax,us);++workCalls;workReached+=found;
  check(!found||!RoomObjectNavigation::blocked(creature,route),"dense work approach does not cross furniture");
  check(found||route.empty(),"failed dense work approach leaves no route");
  if(us>10000)std::cout<<"SLOW_WORK "<<model.name<<" level="<<level<<" station="<<index<<" found="<<found<<" ms="<<us/1000.0<<'\n';
 }
 std::cout<<"DENSE_WORK_CALLS="<<workCalls<<" REACHED="<<workReached<<" TOTAL_MS="<<workTotal/1000.0<<" MAX_MS="<<workMax/1000.0<<'\n';
''' if args.benchmark else '')
saved_probe = ''
if args.saved_terrain:
    assert args.furniture_log, '--saved-terrain requires --furniture-log'
    saved = args.saved_terrain.read_text()
    section = saved.split('[Tiles]', 1)[1].split('[/Tiles]', 1)[0]
    rows = [line.split('#', 1)[0].split() for line in section.splitlines()]
    rows = [row for row in rows if row]
    sx, sy = int(rows[0][0]), int(rows[1][0])
    floor = [(int(row[0]), int(row[1])) for row in rows[2:] if int(row[2]) in (1, 2, 3, 6) and float(row[3]) == 0]
    # Only use objects whose placed orientation is fixed by their room source;
    # randomized treasury angles and saved bed rotations are not guessed.
    angles = {'ChickenCoop': 0, 'Bookcase': 45, 'Podium': 45, 'DungeonTempleObject': 0, 'PortalObject': 0,
              'WorkshopMachine1': 30, 'WorkshopMachine2': 30}
    objects = {}
    saved_bed_info = {}
    for x, y, mesh in re.findall(r'SERVER - Adding rendered object [^\n]*?\[([0-9]+),([0-9]+)\][^\n]*?,MeshName=(\w+)', args.furniture_log.read_text()):
        if mesh in angles:
            offset = .3 if mesh in ('Bookcase', 'Podium') else .2 if mesh.startswith('WorkshopMachine') else 0
            objects[(int(x), int(y))] = (mesh, angles[mesh], float(x), float(y) + offset)
    if args.saved_beds:
        bed_definitions = {}
        for section in (repo / 'config/creatures.cfg').read_text().split('[Creature]')[1:]:
            section = section.split('[/Creature]', 1)[0]
            mesh = re.search(r'^\s*MeshName\s+(\S+)', section, re.M)
            bed = re.search(r'^\s*BedMeshName\s+(\S+)', section, re.M)
            dimensions = re.search(r'^\s*BedDim\s+(\d+)\s+(\d+)', section, re.M)
            if mesh and bed and dimensions:
                bed_definitions[mesh[1]] = (bed[1], int(dimensions[1]), int(dimensions[2]))
        saved_creatures = dict(re.findall(r'^\d+\t(\w+)\t(\w+\.mesh)\t', saved, re.M))
        rooms = saved.split('[Rooms]', 1)[1].split('[/Rooms]', 1)[0]
        bed_count = 0
        for name, x, y, rotation in re.findall(r'^(\w+)\t(\d+)\t(\d+)\t(0|90)\s*$', rooms, re.M):
            if name not in saved_creatures:
                continue
            mesh, width, height = bed_definitions[saved_creatures[name]]
            if rotation != '0':
                width, height = height, width
            px, py = int(x) + width * .5 - .5, int(y) + height * .5 - .5
            objects[(int(x), int(y))] = (mesh, int(rotation), px, py)
            saved_bed_info[(int(x), int(y))] = (name, width, height)
            bed_count += 1
        assert bed_count > 0, 'Saved-bed fixture found no bedroom records'
        print(f'SAVED_BEDS={bed_count}', flush=True)
    assert objects and floor
    worker = re.search(r'^1\t' + re.escape(args.saved_worker) + r'\tKobold.mesh\t([^\t]+)\t([^\t]+)\t[^\t]+\tKobold\t(\d+)\t', saved, re.M)
    assert worker, f'Save fixture requires the selected worker {args.saved_worker}'
    saved_probe = f'GameMap savedMap({sx},{sy});Room savedRoom;savedMap.rooms={{&savedRoom}};\n'
    saved_probe += 'for(auto& tile:savedMap.tiles){tile.walkable=false;tile.room=&savedRoom;}\n'
    saved_probe += ''.join(f'savedMap.getTile({x},{y})->walkable=true;\n' for x, y in floor)
    saved_probe += f'std::vector<BuildingObject> savedObjects({len(objects)});\n'
    for i, ((x, y), (mesh, angle, px, py)) in enumerate(objects.items()):
        saved_probe += f'savedObjects[{i}].mesh="{mesh}";savedObjects[{i}].pos={{float({px}),float({py}),0}};savedObjects[{i}].angle={angle};savedRoom.objects[savedMap.getTile({x},{y})]=&savedObjects[{i}];\n'
        if (x, y) in saved_bed_info:
            owner, width, height = saved_bed_info[(x, y)]
            saved_probe += f'placeBed(savedObjects[{i}],{x},{y},{width},{height},{angle},"{owner}");\n'
    saved_probe += f'Creature savedWorker{{&savedMap}};savedWorker.pos={{{worker[1]}f,{worker[2]}f,0}};savedWorker.level={worker[3]};\n'.replace(f'{worker[1]}f', f'float({worker[1]})').replace(f'{worker[2]}f', f'float({worker[2]})')
    saved_probe += r'''
    long long totalMicros=0,maxMicros=0;int searches=0;
    for(auto& object:savedObjects)for(int repeat=0;repeat<3;++repeat){
      auto* tile=savedMap.getTile(Helper::round(object.pos.x),Helper::round(object.pos.y));
      auto coarse=savedMap.path(&savedWorker,tile);if(coarse.empty())continue;
      std::vector<Ogre::Vector2> path;Creature::tileToVector2(coarse,path,true,0);
      const auto began=std::chrono::steady_clock::now();RoomObjectNavigation::refine(savedWorker,path);
      const auto micros=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();
      totalMicros+=micros;maxMicros=std::max(maxMicros,micros);++searches;
      check(!RoomObjectNavigation::blocked(savedWorker,path),"saved-terrain routes never cross recorded furniture");
    }
    check(searches>0,"saved terrain exercises real worker approach destinations");
    std::cout<<"SAVED_TERRAIN_OBJECTS="<<savedObjects.size()<<" SEARCHES="<<searches<<" TOTAL_MS="<<totalMicros/1000.0<<" MAX_MS="<<maxMicros/1000.0<<'\n';
    '''
    if args.saved_food_cases:
        creatures = re.findall(r'^\d+\t(\w+)\t(\w+\.mesh)\t([^\t]+)\t([^\t]+)\t[^\t]+\t\w+\t(\d+)\t', saved, re.M)
        chickens = [line.split() for line in saved.split('[Chickens]')[1].split('[/Chickens]')[0].splitlines() if line.startswith('-1')]
        cases = []
        for name, mesh, x, y, level in creatures:
            for chicken in chickens:
                cx, cy = chicken[3:5]
                if (float(x) - float(cx)) ** 2 + (float(y) - float(cy)) ** 2 <= 400:
                    cases.append(f'{{"{name}","{mesh}",{level},{{float({x}),float({y})}},{{float({cx}),float({cy})}}}}')
        assert cases
        saved_probe += 'struct FoodCase{const char* name;const char* mesh;int level;Ogre::Vector2 start,food;};\n'
        saved_probe += 'const FoodCase cases[]={' + ',\n'.join(cases) + '};\n'
        saved_probe += r'''
        long long foodTotal=0,foodMax=0;int foodCalls=0,foodReached=0;
        for(const auto& item:cases){
          savedWorker.mesh=item.mesh;savedWorker.pos={item.start.x,item.start.y,0};savedWorker.level=item.level;
          std::vector<Ogre::Vector2> result;
          const auto began=std::chrono::steady_clock::now();
          const bool found=RoomObjectNavigation::foodApproach(savedWorker,item.food,result);
          const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();
          foodTotal+=us;foodMax=std::max(foodMax,us);++foodCalls;foodReached+=found;
          check(!found||(!result.empty()&&!RoomObjectNavigation::blocked(savedWorker,result)),"saved food approach never crosses furniture");
          if(us>10000)std::cout<<"SLOW "<<item.name<<" food="<<item.food<<" found="<<found<<" ms="<<us/1000.0<<std::endl;
        }
        std::cout<<"FOOD_CALLS="<<foodCalls<<" REACHED="<<foodReached<<" TOTAL_MS="<<foodTotal/1000.0<<" MAX_MS="<<foodMax/1000.0<<std::endl;
        check(foodMax<100000,"saved food search stays below 100 ms in the local performance fixture");
        '''
probe = probe.replace('SAVED_TERRAIN', saved_probe)
with tempfile.TemporaryDirectory(prefix='odp-room-navigation-') as directory:
    work = Path(directory)
    if args.source_ref:
        (work / 'gamemap').mkdir()
        (work / 'gamemap/RoomObjectPath.h').write_text(read_source('source/gamemap/RoomObjectPath.h'))
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/O2', '/std:c++14', f'/I{work}', f'/I{repo / "source"}',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
