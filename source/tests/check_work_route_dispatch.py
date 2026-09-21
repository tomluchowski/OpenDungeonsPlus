"""Compile actual walk-path dispatch and check prevalidated workstation routes."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-only', action='store_true')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
source = (repo / 'source/entities/MovableGameEntity.cpp').read_text()
method = source[source.index('void MovableGameEntity::setWalkPath('):source.index('\nvoid MovableGameEntity::clearDestinations(')]
code = r'''
#include <OgreVector2.h>
#include <OgreVector3.h>
#include <deque>
#include <string>
#include <vector>
#include <iostream>
enum class GameEntityType {creature,other};
struct Creature;
int refineCalls=0;
namespace RoomObjectNavigation {bool refine(Creature&,std::vector<Ogre::Vector2>& p){++refineCalls;if(!p.empty())p.back()+=Ogre::Vector2(1,1);return true;}}
enum class ServerNotificationType {animatedObjectSetWalkPath};
struct Player {bool getIsHuman(){return true;}};
struct Seat {Player player;Player* getPlayer(){return &player;}};
struct Packet {std::vector<bool> flags;Packet& operator<<(bool v){flags.push_back(v);return *this;}template<class T>Packet& operator<<(const T&){return *this;}};
struct ServerNotification {Packet mPacket;ServerNotification(ServerNotificationType,Player*){}};
struct ODServer {
 std::vector<ServerNotification*> sent;
 static ODServer& getSingleton(){static ODServer instance;return instance;}
 void queueServerNotification(ServerNotification* n){sent.push_back(n);}
 void clear(){for(auto* n:sent)delete n;sent.clear();}
 ~ODServer(){clear();}
};
struct MovableGameEntity {
 bool server=true;GameEntityType type=GameEntityType::creature;std::string name="creature";
 std::deque<Ogre::Vector2> mWalkQueue;std::vector<Seat*> mSeatsWithVisionNotified;
 std::string mDestinationAnimationState,animation;bool mDestinationAnimationLoop=false,mDestinationPlayIdleWhenAnimationEnds=false;
 bool getIsOnServerMap(){return server;}GameEntityType getObjectType(){return type;}const std::string& getName(){return name;}
 void setAnimationState(const std::string& s,bool=true,const Ogre::Vector3& =Ogre::Vector3::ZERO,bool=true){animation=s;}
 void setWalkPath(const std::string&,const std::string&,bool,bool,const std::vector<Ogre::Vector2>&,bool,bool=false);
};
struct Creature:MovableGameEntity {};
METHOD
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 Seat viewer;
 for(bool server:{false,true})for(bool validated:{false,true})for(bool empty:{false,true}){
  Creature c;c.server=server;c.mSeatsWithVisionNotified={&viewer};refineCalls=0;
  std::vector<Ogre::Vector2> route=empty?std::vector<Ogre::Vector2>{}:std::vector<Ogre::Vector2>{{3,4},{5.25f,6.125f}};
  c.setWalkPath("Walk","Idle",true,false,route,true,validated);
  check(refineCalls==int(server&&!validated),"only ordinary server routes are refined");
  check(c.mWalkQueue.size()==route.size(),"queue preserves route length");
  if(!empty)check(c.mWalkQueue.back()==route.back()+((server&&!validated)?Ogre::Vector2(1,1):Ogre::Vector2::ZERO),"validated endpoint is not moved by dispatch");
  check(c.animation==(empty?"Idle":"Walk"),"walk and immediate end animations are preserved");
  auto& sent=ODServer::getSingleton().sent;
  check(sent.size()==size_t(server),"only server sends route notifications");
  if(server)check(sent.back()->mPacket.flags.size()==3&&!sent.back()->mPacket.flags[0]&&sent.back()->mPacket.flags[1]&&!sent.back()->mPacket.flags[2],"validated and normally refined paths preserve flags without jitter");
  ODServer::getSingleton().clear();
 }
 Creature other;other.type=GameEntityType::other;refineCalls=0;
 other.setWalkPath("Walk","Idle",true,true,{{2,3}},true);
 check(refineCalls==0&&other.mWalkQueue.back()==Ogre::Vector2(2,3),"non-creature movement remains unchanged");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('METHOD', method)
with tempfile.TemporaryDirectory(prefix='odp-work-route-dispatch-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    if args.compile_only:
        print('COMPILE ONLY: fixture built; runtime checks were not executed')
    else:
        subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
