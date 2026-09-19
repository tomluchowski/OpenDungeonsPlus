"""Compile actual missile traversal/collision against Ogre without launching a game."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref', help='Read an older missile implementation with git show')
args = parser.parse_args()
source = (repo / 'source/entities/MissileObject.cpp').read_text()
if args.source_ref:
    source = subprocess.check_output(['git', 'show', args.source_ref + ':source/entities/MissileObject.cpp'], cwd=repo, text=True)

def extract(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

probe = r'''
#include <OgreVector.h>
#include <cmath>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#define OD_LOG_ERR(message) do {} while(false)
#define OD_LOG_INF(message) do {} while(false)
class Seat {};
struct GameEntity { int hits=0; std::string getName() const {return "target";} };
struct Tile {
    int x,y; double fullness=0; std::vector<GameEntity*> enemies,allies;
    Tile(int xx,int yy):x(xx),y(yy){}
    int getX() const {return x;} int getY() const {return y;}
    double getFullness() const {return fullness;}
    bool isEntityOnTile(GameEntity* e) const {
        for(auto* entry:enemies)if(entry==e)return true;
        for(auto* entry:allies)if(entry==e)return true;
        return false;
    }
};
struct TileContainer {
    std::vector<Tile> tiles;
    TileContainer(){for(int y=0;y<100;++y)for(int x=0;x<100;++x)tiles.emplace_back(x,y);}
    Tile* getTile(int x,int y) const {return x<0||y<0||x>=100||y>=100?nullptr:const_cast<Tile*>(&tiles[y*100+x]);}
    int getMapSizeX() const {return 100;} int getMapSizeY() const {return 100;}
    std::list<Tile*> tilesBetween(int,int,int,int) const;
};
struct GameMap:TileContainer {
    std::vector<GameEntity*> getVisibleCreatures(const std::vector<Tile*>& t,Seat*,bool enemy){return enemy?t[0]->enemies:t[0]->allies;}
};
namespace Helper {int round(double x){return int(std::round(x));}}
namespace EntityAnimation {const std::string idle_anim="Idle";}
struct MissileObject {
    GameMap* map; Ogre::Vector3 pos,mDirection;
    bool mIsMissileAlive=true,mDamageAllies=false,onMap=true,moving=false,deleted=false,distortion=true,piercing=false;
    GameEntity* mEntityTarget=nullptr; double speed=3;
    std::vector<Ogre::Vector2> queued;
    MissileObject(GameMap& m):map(&m),pos(10,10,.3f),mDirection(1,0,0){}
    bool getIsOnMap() const{return onMap;} bool isMoving() const{return moving;}
    void removeFromGameMap(){onMap=false;} void deleteYourself(){deleted=true;}
    Tile* getPositionTile(){return map->getTile(Helper::round(pos.x),Helper::round(pos.y));}
    Ogre::Vector3 getPosition() const{return pos;} double getMoveSpeed() const{return speed;}
    GameMap* getGameMap() const{return map;} Seat* getSeat() const{return nullptr;}
    std::string getName() const{return "missile";}
    void setWalkPath(const std::string&,const std::string&,bool,bool,const std::vector<Ogre::Vector2>& p,bool d){queued=p;distortion=d;}
    void hitTargetEntity(Tile*,GameEntity* e){++e->hits;}
    bool hitCreature(Tile*,GameEntity* e){++e->hits;return piercing;}
    void doUpkeep();
    bool computeDestination(const Ogre::Vector3&,double,const Ogre::Vector3&,Ogre::Vector3&,std::list<Tile*>&);
    WALL
};
TILES
UPKEEP
DESTINATION
int checks=0,failures=0;
void check(bool value,const char* label){++checks;if(!value){++failures;std::cout<<"FAIL "<<label<<'\n';}}
void endpoint(MissileObject& shot,float x,float y){
    check(shot.queued.size()==1,"one final endpoint");
    check(!shot.queued.empty() && shot.queued.back().distance(Ogre::Vector2(x,y))<.001f,"flight ends on the collision tile");
    check(!shot.distortion,"projectile endpoints are not randomly displaced");
}
int main(){
    for(int distance:{0,1,2,3}){
        GameMap map; GameEntity target,bystander;
        map.getTile(10+distance,10)->enemies={&target,&bystander};
        MissileObject shot(map);shot.mEntityTarget=&target;shot.doUpkeep();
        check(target.hits==1,"target receives exactly one damage callback");
        check(bystander.hits==0,"single-hit projectile cannot also hit a shared-tile bystander");
        check(!shot.mIsMissileAlive,"target impact stops projectile");endpoint(shot,10+distance,10);
        shot.moving=true;shot.doUpkeep();check(!shot.deleted,"expired projectile keeps its visual flight");
        shot.moving=false;shot.doUpkeep();check(shot.deleted && !shot.onMap,"expired projectile is removed after flight");
    }
    for(int axis=0;axis<3;++axis){
        GameMap map;GameEntity target;
        MissileObject shot(map);
        const int x=axis==1?10:11,y=axis==0?10:11;
        shot.mDirection=Ogre::Vector3(float(x-10),float(y-10),0);shot.mDirection.normalise();
        map.getTile(x,y)->enemies={&target};shot.doUpkeep();
        check(target.hits==1 && !shot.mIsMissileAlive,"incidental hit applies once and stops");endpoint(shot,x,y);
    }
    for(bool damageAllies:{false,true}){
        GameMap map;GameEntity ally;
        map.getTile(11,10)->allies={&ally};MissileObject shot(map);shot.mDamageAllies=damageAllies;shot.doUpkeep();
        check(ally.hits==(damageAllies?1:0),"ally damage flag preserved");endpoint(shot,damageAllies?11:13,10);
    }
    {
        GameMap map;GameEntity front,target;
        map.getTile(11,10)->enemies={&front};map.getTile(12,10)->enemies={&target};
        MissileObject shot(map);shot.mEntityTarget=&target;shot.doUpkeep();
        check(front.hits==1 && target.hits==0,"first obstacle intercepts targeted shot");endpoint(shot,11,10);
    }
    {
        GameMap map;GameEntity first,second;
        map.getTile(11,10)->enemies={&first};map.getTile(12,10)->enemies={&second};
        MissileObject shot(map);shot.piercing=true;shot.doUpkeep();
        check(first.hits==1 && second.hits==1 && shot.mIsMissileAlive,"piercing behavior remains intact");endpoint(shot,13,10);
    }
    for(int distance:{0,1,2}){
        GameMap map;map.getTile(10+distance,10)->fullness=100;
        MissileObject shot(map);shot.doUpkeep();
        check(!shot.mIsMissileAlive,"wall stops missile");endpoint(shot,10+std::max(0,distance-1),10);
    }
    {
        GameMap map;MissileObject shot(map);shot.pos=Ogre::Vector3(98,10,.3f);shot.doUpkeep();
        endpoint(shot,99,10);
    }
    std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''
replacements = {
    'WALL': extract((repo / 'source/entities/MissileObject.h').read_text(), 'virtual bool wallHitNextDirection('),
    'TILES': extract((repo / 'source/gamemap/TileContainer.cpp').read_text(), 'std::list<Tile*> TileContainer::tilesBetween('),
    'UPKEEP': extract(source, 'void MissileObject::doUpkeep()'),
    'DESTINATION': extract(source, 'bool MissileObject::computeDestination('),
}
for key, value in replacements.items():
    probe = probe.replace(key, value)
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
with tempfile.TemporaryDirectory(prefix='odp-projectile-collision-') as temporary:
    work = Path(temporary)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/Od', '/std:c++14',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe',
                    '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
