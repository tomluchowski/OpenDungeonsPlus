"""Exercise production low-nest elevation and renderer lifecycle without a game."""
from pathlib import Path
import os
import sys
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
methods = renderer.split('void RenderManager::updateCreatureStep(', 1)[1].split('\nvoid RenderManager::rrMoveMapLightFlicker(', 1)[0]
probe = r'''
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include "gamemap/RoomObjectStep.h"
#include <set>
#include <iostream>
enum class GameEntityType {buildingObject,creature};
struct RenderedMovableEntity {
 Ogre::SceneNode* node;std::string mesh="GoblinBed";Ogre::Vector3 pos;float angle=0;
 GameEntityType getObjectType(){return GameEntityType::buildingObject;}
 Ogre::SceneNode* getEntityNode(){return node;}const auto& getMeshName(){return mesh;}
 const auto& getPosition(){return pos;}float getRotationAngle(){return angle;}
};
struct BuildingObject:RenderedMovableEntity {Ogre::Vector2 scale;auto getFurnitureScale(){return scale;}};
struct GameMap {std::vector<RenderedMovableEntity*> objects;const auto& getRenderedMovableEntities(){return objects;}};
struct Creature {
 Ogre::SceneNode* node;GameMap* map;std::string mesh="Kobold.mesh";int level=30;
 Ogre::Vector3 pos{2,5,0},direction{1,0,0};bool moving=true,onMap=true;
 auto* getEntityNode(){return node;}bool getIsOnMap(){return onMap;}bool isMoving(){return moving;}
 auto* getGameMap(){return map;}const auto& getPosition(){return pos;}const auto& getWalkDirection(){return direction;}
 const auto& getMeshName(){return mesh;}int getLevel(){return level;}
};
struct RenderManager {
 std::set<Creature*> mSteppingCreatures;
 void updateCreatureStep(Creature*);void cancelCreatureStep(Creature* = nullptr);
};
void RenderManager::updateCreatureStep(METHODS
void renderPreview(const std::string& repo,const std::string& prefix){
 Ogre::Root root("","","low-step-preview.log");root.loadPlugin("RenderSystem_GL3Plus");root.loadPlugin("Codec_STBI");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";options["vsync"]="false";
 auto* window=root.createRenderWindow("Low nest traversal",640,360,false,&options);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 for(const auto& path:{repo+"/models",repo+"/materials/textures",prefix+"/Media/Main",prefix+"/Media/RTShaderLib/GLSL"})groups.addResourceLocation(path,"FileSystem","Graphics",true);
 Ogre::RTShader::ShaderGenerator::initialize();groups.initialiseAllResourceGroups();
 auto& materials=Ogre::MaterialManager::getSingleton();auto* shaders=Ogre::RTShader::ShaderGenerator::getSingletonPtr();
 OgreBites::SGTechniqueResolverListener resolver(shaders);materials.addListener(&resolver);
 auto* scene=root.createSceneManager();shaders->addSceneManager(scene);scene->setAmbientLight(Ogre::ColourValue(.8f,.8f,.8f));
 auto* camera=scene->createCamera("Camera");camera->setNearClipDistance(.01f);camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);camera->setOrthoWindow(2.8f,1.575f);camera->setAspectRatio(640.f/360);
 auto* cameraNode=scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(6.8f,2.5f,1.8f));cameraNode->attachObject(camera);
 cameraNode->setOrientation(Ogre::Vector3::NEGATIVE_UNIT_Z.getRotationTo((Ogre::Vector3(5,5,.25f)-cameraNode->getPosition()).normalisedCopy()));
 auto* viewport=window->addViewport(camera);viewport->setBackgroundColour(Ogre::ColourValue(.12f,.12f,.12f));viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
 for(const auto& name:{"Kobold","GoblinBed"}){auto material=materials.create(std::string(name)+"Preview","Graphics");auto* pass=material->getTechnique(0)->getPass(0);pass->setLightingEnabled(false);pass->createTextureUnitState(std::string(name)+".png");pass->setCullingMode(Ogre::CULL_NONE);}
 auto* workerEntity=scene->createEntity("Kobold.mesh");workerEntity->setMaterialName("KoboldPreview","Graphics");
 auto* workerNode=scene->getRootSceneNode()->createChildSceneNode();workerNode->attachObject(workerEntity);workerNode->setScale(1.6f,1.6f,1.6f);workerNode->setOrientation(Ogre::Vector3::NEGATIVE_UNIT_Y.getRotationTo(Ogre::Vector3::UNIT_X));
 auto* bedEntity=scene->createEntity("GoblinBed.mesh");bedEntity->setMaterialName("GoblinBedPreview","Graphics");
 auto* bedNode=scene->getRootSceneNode()->createChildSceneNode();bedNode->attachObject(bedEntity);
 GameMap map;BuildingObject nest;nest.node=bedNode;
 for(const auto& bounds:RoomObjectPath::meshBounds)if(std::string(bounds.name)=="GoblinBed"){
  const auto placed=RoomObjectPath::bedPlacement(bounds,5,5,1,1,0,"Creature1");nest.pos={placed.x,placed.y,0};nest.angle=placed.angle;nest.scale={placed.scale.x,placed.scale.y};
 }
 bedNode->setPosition(nest.pos);bedNode->setScale(nest.scale.x,nest.scale.y,1);bedNode->setOrientation(Ogre::Quaternion(Ogre::Degree(nest.angle),Ogre::Vector3::UNIT_Z));
 map.objects={&nest};Creature creature;creature.node=workerNode;creature.map=&map;RenderManager renderer;
 auto* walk=workerEntity->getAnimationState("Walk");walk->setEnabled(true);walk->setLoop(true);
 for(int frame=0;frame<=120;++frame){
  creature.pos={4.f+frame/60.f,5,0};walk->setTimePosition(frame/60.f);renderer.updateCreatureStep(&creature);
  root._fireFrameStarted();root._fireFrameRenderingQueued();window->update(false);
  if(frame==15||frame==30||frame==60||frame==90){Ogre::Image pixels;pixels.create(Ogre::PF_BYTE_RGBA,640,360);window->copyContentsToMemory(pixels.getPixelBox(),Ogre::RenderTarget::FB_BACK);pixels.save(repo+"/build/windows/low-step-preview-"+std::to_string(frame)+".png");}
  window->swapBuffers();root._fireFrameEnded();
 }
 renderer.cancelCreatureStep();shaders->removeSceneManager(scene);root.destroySceneManager(scene);materials.removeListener(&resolver);Ogre::RTShader::ShaderGenerator::destroy();
}
int main(int argc,char** argv){
 int checks=0,failures=0;auto check=[&](bool ok,const char* reason){++checks;if(!ok){++failures;std::cout<<"FAIL "<<reason<<'\n';}};
 using namespace RoomObjectPath;
 for(const auto& body:lowWalkingBounds)for(int level:{1,30})for(float angle:{0.f,.07f,1.5707963f}){
  Obstacle bed{{-.35f,-.35f},{.35f,.35f},{5,5},std::cos(angle),std::sin(angle)};bed.maximumHeight=.073802f;
  const float scale=1+.02f*level;const float rise=prepareLowStep(bed,body.name,scale,0);
  check(body.empty?rise==0:rise>0,"only known ground bodies require a step");
  for(int heading=0;heading<8;++heading){
   const Ogre::Vector2 direction(std::cos(heading*.785398163f),std::sin(heading*.785398163f));
   const auto shape=bed.forHeading(direction);float previous=lowStepElevation(bed,Ogre::Vector2(5,5)-direction*2.f,direction,rise);
   for(int i=0;i<=400;++i){
    const auto point=Ogre::Vector2(5,5)+direction*(-2.f+i*.01f);
    const float lift=lowStepElevation(bed,point,direction,rise);
    check(lift>=0&&lift<=rise+.00001f,"step remains within measured clearance");
    if(!body.empty&&shape.contains(point))check(lift+(body.minZ-lowWalkingMargin)*scale>=bed.maximumHeight-.00001f,"lowest animated body clears the complete bed while crossing");
    check(std::abs(lift-previous)<.016f,"entry and exit height remain continuous at one-centimeter samples");previous=lift;
   }
  }
  bed.maximumHeight=.2f;check(prepareLowStep(bed,body.name,scale,0)==0,"higher bed parts remain solid");
 }
 Ogre::SceneNode creatureNode(nullptr),bedNode(nullptr);GameMap map;BuildingObject nest;nest.node=&bedNode;
 for(const auto& bounds:meshBounds)if(std::string(bounds.name)=="GoblinBed"){
  const auto placed=bedPlacement(bounds,5,5,1,1,0,"Creature1");nest.pos={placed.x,placed.y,0};nest.angle=placed.angle;nest.scale={placed.scale.x,placed.scale.y};
 }
 map.objects={&nest};Creature creature;creature.node=&creatureNode;creature.map=&map;RenderManager renderer;
 renderer.updateCreatureStep(&creature);check(creatureNode.getPosition()==creature.pos,"flat-ground movement remains unchanged");
 creature.pos={4.8f,5,0};renderer.updateCreatureStep(&creature);
 check(creatureNode.getPosition().z>.08f&&renderer.mSteppingCreatures.count(&creature)==1,"renderer visibly raises a worker before crossing the nest");
 creature.moving=false;renderer.updateCreatureStep(&creature);check(creatureNode.getPosition().z>.08f,"stopping on the nest retains support");
 map.objects.clear();renderer.updateCreatureStep(&creature);check(creatureNode.getPosition()==creature.pos&&renderer.mSteppingCreatures.empty(),"removing the nest restores floor height");
 map.objects={&nest};creature.moving=true;renderer.updateCreatureStep(&creature);creature.onMap=false;renderer.updateCreatureStep(&creature);
 check(creatureNode.getPosition()==creature.pos&&renderer.mSteppingCreatures.empty(),"pickup clears step height and active tracking");
 creature.onMap=true;renderer.updateCreatureStep(&creature);renderer.cancelCreatureStep();check(renderer.mSteppingCreatures.empty()&&creatureNode.getPosition()==creature.pos,"map reset restores every tracked creature");
 creature.level=1;creature.pos={5.35f,5,0};creature.direction={0,1,0};renderer.updateCreatureStep(&creature);
 check(creatureNode.getPosition().z==0,"walking parallel through a free strip does not trigger a step");
 creature.mesh="CaveHornet.mesh";creature.pos={4.8f,5,0};renderer.updateCreatureStep(&creature);check(creatureNode.getPosition().z==0,"flying animation is not lifted as if it had ground feet");
 if(argc>1)renderPreview(argv[1],argv[2]);
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHODS', methods)
with tempfile.TemporaryDirectory(prefix='odp-low-step-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/O2', '/std:c++14', f'/I{repo / "source"}',
                    f'/I{prefix / "include/OGRE"}', f'/I{prefix / "include/OGRE/RTShaderSystem"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib'], cwd=work, check=True)
    result = subprocess.run([str(work / 'check.exe')] + ([str(repo), str(prefix)] if '--render' in sys.argv else []), cwd=work, capture_output=True, text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if not line.startswith('GL_EXTENSIONS') and any(tag in line for tag in ('FAIL ', 'CHECKS=', 'Error', 'error'))))
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
