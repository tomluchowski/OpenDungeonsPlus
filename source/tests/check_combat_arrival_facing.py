"""Exercise production movement completion and queued attack facing without a game."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/entities/MovableGameEntity.cpp').read_text()
movement = source.split('    // Move the entity\n', 1)[1].split('\nvoid MovableGameEntity::setPosition(', 1)[0]
stop = source.split('void MovableGameEntity::stopWalking()\n', 1)[1].split('\nvoid MovableGameEntity::setWalkDirection(', 1)[0]
probe = r'''
#include <OgreVector.h>
#include <deque>
#include <string>
#include <iostream>
namespace ODApplication {const float turnsPerSecond=1;}
struct MovableGameEntity {
 Ogre::Vector3 position{0,0,0},mWalkDirection{0,-1,0},mDestinationAnimationDirection;
 std::deque<Ogre::Vector2> mWalkQueue;
 std::string mDestinationAnimationState,animation;
 bool mDestinationAnimationLoop=false,mDestinationPlayIdleWhenAnimationEnds=true;
 float getMoveSpeed(){return 1;} const Ogre::Vector3& getPosition(){return position;}
 void setPosition(const Ogre::Vector3& p){position=p;}
 void setWalkDirection(const Ogre::Vector3& d){mWalkDirection=d;}
 void setAnimationState(const std::string& s,bool,const Ogre::Vector3& d,bool){animation=s;if(d!=Ogre::Vector3::ZERO)setWalkDirection(d);}
 void update(Ogre::Real timeSinceLastFrame);void stopWalking();
};
void MovableGameEntity::update(Ogre::Real timeSinceLastFrame){MOVEMENT
void MovableGameEntity::stopWalking()STOP
int main(){int checks=0,failures=0;auto check=[&](bool v,const char* reason){++checks;if(!v){++failures;std::cout<<"FAIL "<<reason<<'\n';}};
 for(const std::string state:{"CombatAttack","RangedAttack","Idle","Sleep"})
 for(float dt:{1.f,2.f})for(bool directed:{false,true}){
  MovableGameEntity c;c.mWalkQueue={{0,-.25f},{.25f,-.25f}};
  c.mDestinationAnimationState=state;
  c.mDestinationAnimationDirection=directed?Ogre::Vector3::UNIT_Y:Ogre::Vector3::ZERO;
  c.update(dt);
  check(c.mWalkQueue.empty(),"final waypoint consumed");
  check(c.position==Ogre::Vector3(.25f,-.25f,0),"final position retained");
  check(c.animation==state,"queued end animation starts");
  check(c.mWalkDirection==(directed?Ogre::Vector3::UNIT_Y:Ogre::Vector3::UNIT_X),"arrival retains requested facing or final travel heading");
  check(c.mDestinationAnimationState.empty(),"queued state clears");
 }
 MovableGameEntity moving;moving.mWalkQueue={{0,-1}};moving.mDestinationAnimationState="CombatAttack";
 moving.mDestinationAnimationDirection=Ogre::Vector3::UNIT_X;moving.update(.25f);
 check(moving.mWalkDirection==Ogre::Vector3::NEGATIVE_UNIT_Y,"moving creature faces its path");
 check(moving.animation.empty(),"attack remains queued before arrival");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('MOVEMENT', movement).replace('STOP', stop)
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
with tempfile.TemporaryDirectory(prefix='odp-combat-facing-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
