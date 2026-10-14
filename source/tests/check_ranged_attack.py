"""Exercise the actual attack dispatch without a game or renderer."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/entities/Creature.cpp').read_text()
start = source.index('void Creature::useAttack(')
end = source.index('\nbool Creature::isActionInList(', start)
method = source[start:end]
probe = r'''
#include <OgreVector.h>
#include <cmath>
#include <iostream>
#include <string>
struct GameMap {};
struct Tile {int x,y;int getX()const{return x;}int getY()const{return y;}};
enum class GameEntityType {creature,building};
struct GameEntity {
 GameEntityType type=GameEntityType::building;Ogre::Vector3 position;
 GameEntityType getObjectType()const{return type;}
 const Ogre::Vector3& getPosition()const{return position;}
};
struct Creature;
namespace EntityAnimation {const std::string combat_attack_anim="CombatAttack",ranged_attack_anim="RangedAttack";}
namespace CreatureSound {enum {Attack};}
namespace Pathfinding {float distanceTile(const Tile& a,const Tile& b){return std::hypot(float(a.x-b.x),float(a.y-b.y));}}
struct Skill {
    double maximum;int calls=0;float distance=-1;bool ko=false,notify=false;
    double getRangeMax(Creature*,GameEntity*)const{return maximum;}
    bool tryUseFight(GameMap&,Creature*,float d,GameEntity*,Tile*,bool k,bool n){++calls;distance=d;ko=k;notify=n;return true;}
    int getWarmupNbTurns()const{return 2;}int getCooldownNbTurns()const{return 3;}
};
struct CreatureSkillData {Skill* mSkill;int mWarmup=0,mCooldown=0;};
struct Creature {
    Ogre::Vector3 position=Ogre::Vector3::ZERO,direction;Tile tile{0,0};GameMap map;
    std::string animation;int sounds=0,turns=-1;double tired=0,xp=0;
    const Ogre::Vector3& getPosition()const{return position;}
    Tile* getPositionTile(){return &tile;}GameMap* getGameMap(){return &map;}
    void setAnimationState(const std::string& a,bool,const Ogre::Vector3& d,bool){animation=a;direction=d;}
    void fireCreatureSound(int){++sounds;}void setNbTurnsWithoutBattle(int t){turns=t;}
    void decreaseWakefulness(double v){tired+=v;}void receiveExp(double v){xp+=v;}
    void useAttack(CreatureSkillData&,GameEntity&,Tile&,bool,bool);
};
METHOD
int main(){int checks=0,failures=0;auto check=[&](bool v){++checks;if(!v)++failures;};
 for(double maximum:{1.,7.})for(int distance:{1,3}){
    if(maximum==1 && distance==3)continue;
    Creature c;Skill skill{maximum};CreatureSkillData data{&skill};GameEntity target;Tile tile{distance,0};
    c.useAttack(data,target,tile,true,false);
    check(c.animation==(maximum>1?"RangedAttack":"CombatAttack"));
    check(c.direction==Ogre::Vector3::UNIT_X);
    check(skill.calls==1 && skill.distance==distance && skill.ko && !skill.notify);
    check(data.mWarmup==2 && data.mCooldown==3);
    check(c.sounds==1 && c.turns==0 && c.tired==.5 && c.xp==1.5);
 }
 for(double maximum:{1.,7.})for(const Ogre::Vector3 offset:{Ogre::Vector3(.3f,.2f,0),Ogre::Vector3(-.3f,-.2f,0)}){
    Creature c;c.position={.1f,-.1f,0};Skill skill{maximum};CreatureSkillData data{&skill};
    GameEntity target;target.type=GameEntityType::creature;target.position=offset;Tile tile{0,0};
    c.useAttack(data,target,tile,true,false);
    auto expected=target.position-c.position;expected.normalise();
    check(c.direction==expected);
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHOD', method)
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
with tempfile.TemporaryDirectory(prefix='odp-ranged-attack-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
