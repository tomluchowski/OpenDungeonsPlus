"""Exercise production corpse animation and crypt placement without starting the game."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
crypt = (repo / 'source/rooms/RoomCrypt.cpp').read_text()


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


models = sorted(set(re.findall(r'^\s*MeshName\s+(\S+)',
                              (repo / 'config/creatures.cfg').read_text(), re.M)))
probe = r'''
#include <Ogre.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#define OD_LOG_ERR(x) do {} while(false)
int checks=0,failures=0;
void check(bool pass,const char* label){++checks;if(!pass){++failures;std::cout<<"FAIL "<<label<<'\n';}}
bool sameRotation(const Ogre::Quaternion& a,const Ogre::Quaternion& b){
 // Component-space comparison avoids acos(dot(q,q)) rejecting q itself after float rounding.
 return std::min((a-b).Norm(),(a+b).Norm())<=1e-10f;
}
HELPERS
enum class GameEntityType {creature,other};
namespace EntityAnimation {const std::string rot_anim="Rot",die_anim="Die";}
struct Tile {int x,y;int getX(){return x;}int getY(){return y;}};
struct GameMap {Tile destination{4,5};Tile* getTile(int x,int y){return x==4&&y==5?&destination:nullptr;}};
struct GameEntity {GameEntityType type=GameEntityType::creature;GameEntityType getObjectType(){return type;}
 std::string getName(){return "test";}};
struct Creature:GameEntity {
 Tile* tile=nullptr;std::string state,cleared;bool onTile=true;bool loop=true,idle=true;
 Tile* getPositionTile(){return tile;}
 void clearDestinations(const std::string& s,bool l,bool i){cleared=s;loop=l;idle=i;}
 void setAnimationState(const std::string& s,bool l,const Ogre::Vector3&,bool i){state=s;loop=l;idle=i;}
 void removeEntityFromPositionTile(){onTile=false;}void addEntityToPositionTile(){onTile=true;}
};
enum class ActiveSpotPlace {activeSpotCenter,other};
struct Room {void notifyActiveSpotRemoved(ActiveSpotPlace,Tile*){}};
struct RoomCrypt:Room {
 GameMap map;std::map<Tile*,std::pair<Creature*,int32_t>> mRottingCreatures;
 GameMap* getGameMap(){return &map;}std::string getName(){return "crypt";}
 void notifyCarryingStateChanged(Creature*,GameEntity*);void notifyActiveSpotRemoved(ActiveSpotPlace,Tile*);
};
OFFSETS
ROOM_METHODS
int main(int argc,char** argv){try{
 check(sameRotation(Ogre::Quaternion::IDENTITY,-Ogre::Quaternion::IDENTITY),"quaternion sign does not change a rotation");
 check(!sameRotation(Ogre::Quaternion::IDENTITY,Ogre::Quaternion(Ogre::Radian(.001f),Ogre::Vector3::UNIT_X)),"rotation check rejects a genuine angular error");
 RoomCrypt room;Tile spot{4-OFFSET_TILE_X,5-OFFSET_TILE_Y},wrong{9,9};Creature carrier,corpse;
 carrier.tile=corpse.tile=&room.map.destination;
 room.mRottingCreatures[&spot]={&corpse,-1};
 room.notifyCarryingStateChanged(&carrier,&corpse);
 check(corpse.state=="Rot"&&corpse.cleared=="Rot","placement persists decay state and clears stale walking");
 check(!corpse.loop&&!corpse.idle&&!corpse.onTile,"corpse never returns to idle or normal death upkeep");
 check(room.mRottingCreatures[&spot].second==0,"existing rot counter begins at zero");
 room.notifyActiveSpotRemoved(ActiveSpotPlace::activeSpotCenter,&spot);
 check(corpse.onTile&&corpse.state=="Die"&&corpse.cleared=="Die","removed crypt spot restores ordinary corpse state");
 check(room.mRottingCreatures.empty(),"removed spot releases reservation");
 corpse.state="unchanged";corpse.onTile=true;carrier.tile=&wrong;
 room.mRottingCreatures[&spot]={&corpse,-1};room.notifyCarryingStateChanged(&carrier,&corpse);
 check(corpse.state=="unchanged"&&corpse.onTile,"interrupted delivery does not start decay");
 check(room.mRottingCreatures[&spot].first==nullptr,"interrupted delivery frees reservation");
 room.mRottingCreatures[&spot]={&corpse,-1};room.notifyActiveSpotRemoved(ActiveSpotPlace::activeSpotCenter,&spot);
 check(corpse.state=="unchanged","removing a reserved but unused spot leaves carried corpse unchanged");

 Ogre::Root root("","","crypt-decay.log");
 Ogre::DefaultHardwareBufferManager buffers;
 Ogre::MaterialManager::getSingleton().initialise();
 Ogre::ParticleSystemManager::getSingleton()._initialise();
 root.loadPlugin(std::string(argv[2])+"/bin/Plugin_ParticleFX");
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 groups.addResourceLocation(std::string(argv[1])+"/models","FileSystem","Graphics",true);
 groups.initialiseResourceGroup("Graphics");
 auto* scene=root.createSceneManager();
 for(const std::string model:{MODELS}){
  auto* entity=scene->createEntity(model,model,"Graphics");
  auto* skeleton=entity->getMesh()->getSkeleton().get();
  std::string pose=needsCreatureDropFallback(entity)?"Sleep":"Die";
  if(!skeleton->hasAnimation(pose))pose=skeleton->hasAnimation("Sleep")?"Sleep":"Idle";
  const auto name=createCreatureDecayAnimation(entity,pose,80);
  check(name=="CorpseDecay"&&entity->hasAnimationState(name),"every configured creature gets a decay state");
  const auto* source=skeleton->getAnimation(pose);auto* decay=skeleton->getAnimation(name);
  check(decay->getLength()==80,"decay uses the configured turn duration");
  for(unsigned short bone=0;bone<skeleton->getNumBones();++bone){
   Ogre::TransformKeyFrame expected(nullptr,0);
   if(source->hasNodeTrack(bone))source->getNodeTrack(bone)->getInterpolatedKeyFrame(Ogre::TimeIndex(source->getLength()),&expected);
   for(float progress:{0.f,.5f,1.f}){
    Ogre::TransformKeyFrame actual(nullptr,0);decay->getNodeTrack(bone)->getInterpolatedKeyFrame(Ogre::TimeIndex(80*progress),&actual);
    check(sameRotation(actual.getRotation(),expected.getRotation()),"corpse retains final ground-pose rotation");
    check(actual.getTranslate().positionEquals(expected.getTranslate(),.0001f),"corpse retains final ground-pose translation");
    const float scale=skeleton->getBone(bone)->getParent()?1.f:1.f-.18f*progress;
    check(actual.getScale().positionEquals(expected.getScale()*scale,.0001f),"settling changes root scale only");
   }
  }
  const auto count=skeleton->getNumAnimations();createCreatureDecayAnimation(entity,pose,80);
  check(skeleton->getNumAnimations()==count,"repeated decay reuses cached animation");
  auto* second=scene->createEntity(model+"_second",model,"Graphics");createCreatureDecayAnimation(second,pose,80);
  check(second->hasAnimationState(name),"another creature sharing the skeleton has its own animation state");
  scene->destroyEntity(second);scene->destroyEntity(entity);
 }
 // Material rendering is verified by the installed-renderer fixture separately.
 Ogre::MaterialManager::getSingleton().create("CorpseFlies","Graphics");
 std::ifstream stream(std::string(argv[1])+"/particles/CorpseDecay.particle");
 Ogre::DataStreamPtr data(new Ogre::FileStreamDataStream("CorpseDecay.particle",&stream,false));
 Ogre::ParticleSystemManager::getSingleton().parseScript(data,"Graphics");
 auto* effect=scene->createParticleSystem("corpse","CorpseDecay");
 check(effect->getParticleQuota()==24,"fly count is bounded");
 check(effect->getNumEmitters()==1&&effect->getNumAffectors()==1,"fly script parses with emitter and movement affector");
 check(effect->getAffector(0)->getType()=="DirectionRandomiser","flies have animated erratic movement");
 check(effect->getEmitter(0)->getEmissionRate()>0,"swarm emits throughout corpse decay");
 check(effect->getMaterialName()=="CorpseFlies","flies use their winged silhouette, not blood flares");
 check(effect->getDefaultWidth()>=.1f&&effect->getDefaultHeight()>=.1f,"flies remain readable at dungeon-camera scale");
 scene->destroyParticleSystem(effect);root.destroySceneManager(scene);
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
probe = probe.replace('HELPERS', '\n'.join(function(renderer, signature) for signature in (
    'bool needsCreatureDropFallback(', 'std::string createCreatureDecayAnimation(')))
probe = probe.replace('ROOM_METHODS', '\n'.join(function(crypt, signature) for signature in (
    'void RoomCrypt::notifyCarryingStateChanged(', 'void RoomCrypt::notifyActiveSpotRemoved(')))
probe = probe.replace('OFFSETS', '\n'.join(re.findall(r'^.*const.*OFFSET_TILE_[XY].*;$', crypt, re.M)))
probe = probe.replace('MODELS', ','.join('"' + model + '"' for model in models))
with tempfile.TemporaryDirectory(prefix='odp-crypt-decay-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'], cwd=work, check=True)
    result = subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work,
                            capture_output=True, text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if 'CHECKS=' in line or 'FAIL ' in line))
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
