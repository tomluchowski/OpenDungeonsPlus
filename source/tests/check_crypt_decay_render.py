"""Render the production crypt decay and fly assets in a hidden Ogre scene."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
source = (repo / 'source/render/RenderManager.cpp').read_text()
start = source.index('std::string createCreatureDecayAnimation(')
end = source.index('\n}\n}', start) + 2
helper = source[start:end]
cleanup_start = source.index('void RenderManager::clearCreatureDecay(')
cleanup_end = source.index('\nvoid RenderManager::rrSetCreaturesTextOverlay', cleanup_start)
cleanup = source[cleanup_start:cleanup_end]
probe = r'''
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <iostream>
int checks=0,failures=0;
void check(bool value,const char* label){++checks;if(!value){++failures;std::cout<<"FAIL "<<label<<'\n';}}
HELPER
struct Creature {
 std::string name;std::string getName(){return name;}std::string getOgreNamePrefix(){return "";}
};
struct RenderManager {
 Ogre::SceneManager* mSceneManager;
 std::map<Creature*,std::vector<Ogre::MaterialPtr>> mCreatureDecayMaterials;
 void clearCreatureDecay(Creature*);
};
CLEANUP
int main(int argc,char** argv){try{
 Ogre::Root root("","","crypt-render.log");
 const std::string repo=argv[1],prefix=argv[2];
 for(const char* plugin:{"RenderSystem_GL3Plus","Codec_STBI","Plugin_ParticleFX"})root.loadPlugin(prefix+"/bin/"+plugin);
 auto* rs=root.getAvailableRenderers().front();root.setRenderSystem(rs);rs->setConfigOption("Full Screen","No");root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";
 auto* window=root.createRenderWindow("CryptDecayPreview",960,540,false,&options);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 for(const char* dir:{"models","materials/textures","materials/scripts","shaders","particles"})groups.addResourceLocation(repo+"/"+dir,"FileSystem","Graphics",true);
 groups.addResourceLocation(prefix+"/Media/Main","FileSystem","OgreInternal");
 groups.addResourceLocation(prefix+"/Media/Main","FileSystem","Graphics");
 groups.addResourceLocation(prefix+"/Media/RTShaderLib/GLSL","FileSystem","OgreInternal");
 groups.addResourceLocation(prefix+"/Media/RTShaderLib/GLSL","FileSystem","Graphics");
 Ogre::RTShader::ShaderGenerator::initialize();auto* generator=Ogre::RTShader::ShaderGenerator::getSingletonPtr();
 groups.initialiseAllResourceGroups();
 OgreBites::SGTechniqueResolverListener listener(generator);Ogre::MaterialManager::getSingleton().addListener(&listener);
 auto* scene=root.createSceneManager();generator->addSceneManager(scene);scene->setAmbientLight(Ogre::ColourValue(.55f,.55f,.55f));
 auto* camera=scene->createCamera("camera");camera->setNearClipDistance(.1f);camera->setFarClipDistance(50);
 camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);camera->setOrthoWindow(12,6.75f);
 auto* camNode=scene->getRootSceneNode()->createChildSceneNode();camNode->attachObject(camera);
 camNode->setPosition(0,-9,12);camNode->lookAt({0,0,0},Ogre::Node::TS_WORLD);
 auto* viewport=window->addViewport(camera);viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
 viewport->setBackgroundColour(Ogre::ColourValue(.12f,.11f,.1f));
 std::vector<Ogre::Entity*> bodies;std::vector<Ogre::AnimationState*> states;
 std::vector<Ogre::ParticleSystem*> swarms;
 std::vector<std::vector<Ogre::MaterialPtr>> originals(3);
 std::vector<Creature> creatures(3);RenderManager renderer;renderer.mSceneManager=scene;
 for(int i=0;i<3;++i){
  auto* entity=scene->createEntity("body"+std::to_string(i),"Dwarf1.mesh","Graphics");
  auto* node=scene->getRootSceneNode()->createChildSceneNode();node->attachObject(entity);node->setPosition(float(i-1)*3,0,0);
  const auto name=createCreatureDecayAnimation(entity,"Die",30);
  auto* animation=entity->getAnimationState(name);animation->setEnabled(true);animation->setLoop(false);animation->setTimePosition(i*15.f);
  bodies.push_back(entity);states.push_back(animation);
  for(unsigned int sub=0;sub<entity->getNumSubEntities();++sub){
   auto* part=entity->getSubEntity(sub);originals[i].push_back(part->getMaterial());
   part->setMaterial(part->getMaterial()->clone("decay"+std::to_string(i)+"_"+std::to_string(sub)));
  }
  setCreatureDecayProgress(entity,i*.5f);
  creatures[i].name=entity->getName();renderer.mCreatureDecayMaterials[&creatures[i]]=originals[i];
  auto* swarm=scene->createParticleSystem(entity->getName()+"_decay","CorpseDecay");
  auto* flyNode=scene->getRootSceneNode()->createChildSceneNode();flyNode->setPosition(float(i-1)*3,0,.35f);flyNode->attachObject(swarm);
  swarms.push_back(swarm);
 }
 root.renderOneFrame();
 for(auto* swarm:swarms)swarm->fastForward(1.f,.02f);
 const auto capture=[&](){
  window->update(false);std::vector<unsigned char> pixels(960*540*4);
  window->copyContentsToMemory(Ogre::PixelBox(960,540,1,Ogre::PF_BYTE_RGBA,pixels.data()),Ogre::RenderTarget::FB_BACK);return pixels;
 };
 const auto save=[&](std::vector<unsigned char> pixels,const std::string& file){
  Ogre::Image image;image.loadDynamicImage(pixels.data(),960,540,1,Ogre::PF_BYTE_RGBA,false);
  image.save(repo+"/build/review-followups/"+file);
 };
 const auto first=capture();
 save(first,"crypt-decay-preview.png");
 for(size_t i=0;i<bodies.size();++i){
  auto* entity=bodies[i];entity->_updateAnimation();
  check(swarms[i]->getNumParticles()>0&&swarms[i]->getNumParticles()<=24,"visible swarm remains bounded");
  for(unsigned int sub=0;sub<entity->getNumSubEntities();++sub){
   auto* pass=entity->getSubEntity(sub)->getMaterial()->getTechnique(0)->getPass(0);
   check(pass->getFragmentProgram()->isSupported(),"real creature fragment program compiles");
   auto original=originals[i][sub]->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
   const auto* constant=original->_findNamedConstantDefinition("corpseDecay",false);
   check(constant&&*original->getFloatPointer(constant->physicalIndex)==0,"living creature's shared material remains unchanged");
  }
  std::cout<<"STAGE="<<i<<" PARTICLES="<<swarms[i]->getNumParticles()<<" ROOT_SCALE="<<entity->getSkeleton()->getRootBones().front()->getScale()
   <<" BOX="<<swarms[i]->getBoundingBox()<<" VISIBLE="<<swarms[i]->isVisible()<<" MATERIAL="<<swarms[i]->getMaterialName()
   <<" FLAGS="<<swarms[i]->getVisibilityFlags()<<" MASK="<<scene->getVisibilityMask()
   <<" IN_VIEW="<<camera->isVisible(swarms[i]->getWorldBoundingBox(true))
   <<" MATERIAL_LOADED="<<Ogre::MaterialManager::getSingleton().getByName(swarms[i]->getMaterialName())->isLoaded()<<'\n';
 }
 for(auto* swarm:swarms)swarm->fastForward(.25f,.02f);
 const auto second=capture();
 check(first!=second,"swarm motion changes rendered pixels");
 save(second,"crypt-decay-motion-preview.png");
 for(auto* entity:bodies)entity->setVisible(false);
 auto flyMaterial=Ogre::MaterialManager::getSingleton().getByName("CorpseFlies");
 std::cout<<"FLY_TECHNIQUES="<<flyMaterial->getNumSupportedTechniques()<<" REASON="<<flyMaterial->getUnsupportedTechniquesExplanation()<<'\n';
 const auto flies=capture();
 save(flies,"crypt-flies-only.png");
 for(auto* swarm:swarms)swarm->setVisible(false);
 const auto empty=capture();size_t visibleFlyPixels=0;
 for(size_t p=0;p<flies.size();p+=4)if(std::abs(int(flies[p])-empty[p])+std::abs(int(flies[p+1])-empty[p+1])+std::abs(int(flies[p+2])-empty[p+2])>20)++visibleFlyPixels;
 check(visibleFlyPixels>100,"flies are visibly rendered, not just live particles");
 bodies[0]->setVisible(true);
 setCreatureDecayProgress(bodies[0],0);const auto intact=capture();
 setCreatureDecayProgress(bodies[0],.5f);const auto rotting=capture();
 setCreatureDecayProgress(bodies[0],1);const auto gone=capture();
 check(intact!=rotting&&rotting!=gone,"same corpse changes visibly during decay");
 check(gone==empty,"final decay leaves no intact body awaiting deletion");
 for(unsigned int sub=0;sub<bodies[0]->getNumSubEntities();++sub)bodies[0]->getSubEntity(sub)->setMaterial(originals[0][sub]);
 check(capture()==intact,"restoring original material restores the unchanged creature appearance");
 // Restore the cloned part before executing the actual production cleanup.
 for(unsigned int sub=0;sub<bodies[0]->getNumSubEntities();++sub)bodies[0]->getSubEntity(sub)->setMaterial(
  Ogre::MaterialManager::getSingleton().getByName("decay0_"+std::to_string(sub)));
 for(int i=0;i<3;++i){
  renderer.clearCreatureDecay(&creatures[i]);
  check(!scene->hasParticleSystem(bodies[i]->getName()+"_decay"),"cleanup removes the corpse's fly system");
  for(unsigned int sub=0;sub<bodies[i]->getNumSubEntities();++sub){
   check(bodies[i]->getSubEntity(sub)->getMaterial()==originals[i][sub],"production cleanup restores each original material");
   check(!Ogre::MaterialManager::getSingleton().resourceExists("decay"+std::to_string(i)+"_"+std::to_string(sub),"Graphics"),"production cleanup releases cloned materials");
  }
  renderer.clearCreatureDecay(&creatures[i]);
 }
 check(renderer.mCreatureDecayMaterials.empty(),"repeated cleanup leaves no corpse state");
 std::cout<<"VISIBLE_FLY_PIXELS="<<visibleFlyPixels<<" CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
 Ogre::MaterialManager::getSingleton().removeListener(&listener);generator->removeSceneManager(scene);root.destroySceneManager(scene);
 Ogre::RTShader::ShaderGenerator::destroy();return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('HELPER', helper).replace('CLEANUP', cleanup)
with tempfile.TemporaryDirectory(prefix='odp-crypt-render-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14',
                    f'/I{prefix / "include/OGRE"}', f'/I{prefix / "include/OGRE/RTShaderSystem"}',
                    'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'OgreRTShaderSystem.lib',
                    'OgreBites.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, check=True)
