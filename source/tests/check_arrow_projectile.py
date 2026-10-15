"""Inspect and render the shipped arrow mesh/material in a hidden Ogre window."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
arrow_scale = re.search(r'    if\(meshName == "ArrowProjectile"\)\n[^\n]+;', renderer)[0]
probe = r'''
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <iostream>
int main(int argc,char** argv){try{
 Ogre::Root root("","","arrow.log");root.loadPlugin("RenderSystem_GL3Plus");root.loadPlugin("Codec_STBI");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";options["vsync"]="false";
 auto* window=root.createRenderWindow("Arrow check",512,256,false,&options);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 const std::string repo=argv[1],prefix=argv[2];
 for(const auto& path:{repo+"/models",repo+"/materials/textures",prefix+"/Media/Main",prefix+"/Media/RTShaderLib/GLSL"})groups.addResourceLocation(path,"FileSystem","Graphics");
 Ogre::RTShader::ShaderGenerator::initialize();groups.initialiseAllResourceGroups();
 groups.addResourceLocation(repo+"/materials/scripts","FileSystem","Graphics");
 auto& materials=Ogre::MaterialManager::getSingleton();materials.parseScript(groups.openResource("ArrowProjectile.material","Graphics"),"Graphics");
 auto* shaders=Ogre::RTShader::ShaderGenerator::getSingletonPtr();OgreBites::SGTechniqueResolverListener resolver(shaders);materials.addListener(&resolver);
 auto* scene=root.createSceneManager();shaders->addSceneManager(scene);scene->setAmbientLight(Ogre::ColourValue(.7,.7,.7));
 auto* camera=scene->createCamera("Camera");camera->setNearClipDistance(.01f);camera->setAspectRatio(2);camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);camera->setOrthoWindow(4,2);
 auto* cameraNode=scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0,0,4));cameraNode->attachObject(camera);
 auto* viewport=window->addViewport(camera);viewport->setBackgroundColour(Ogre::ColourValue(.1f,.08f,.06f));viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
 auto* entity=scene->createEntity("ArrowProjectile.mesh");auto* node=scene->getRootSceneNode()->createChildSceneNode();node->attachObject(entity);
 const std::string meshName="ArrowProjectile";
 ARROW_SCALE
 std::cout<<"ARROW_BOUNDS="<<entity->getBoundingBox()<<'\n';
 for(unsigned sub=0;sub<entity->getNumSubEntities();++sub)std::cout<<"ARROW_MATERIAL="<<entity->getSubEntity(sub)->getMaterialName()<<'\n';
 int checks=0,failures=0;auto check=[&](bool v,const char* reason){++checks;if(!v){++failures;std::cout<<"FAIL "<<reason<<'\n';}};
 for(int frame=0;frame<8;++frame){
  node->setOrientation(Ogre::Quaternion(Ogre::Degree(frame*45),Ogre::Vector3::UNIT_Z));
  window->update(false);Ogre::Image pixels;pixels.create(Ogre::PF_BYTE_RGBA,512,256);window->copyContentsToMemory(pixels.getPixelBox(),Ogre::RenderTarget::FB_BACK);
  int visible=0,minX=512,maxX=0,minY=256,maxY=0;
  for(int y=0;y<256;++y)for(int x=0;x<512;++x){const auto c=pixels.getColourAt(x,y,0);if(c.r>.2f||c.g>.2f||c.b>.2f){++visible;minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);}}
  check(visible>=30 && std::hypot(float(maxX-minX),float(maxY-minY))>=60,"arrow shaft and fletching remain readable along the projectile");
  std::cout<<"ARROW_PIXELS="<<frame<<" count="<<visible<<" extent="<<maxX-minX<<','<<maxY-minY<<'\n';
  if(frame==0||frame==2)pixels.save(repo+"/build/windows/arrow-preview-"+std::to_string(frame)+".png");
  window->swapBuffers();
 }
 auto diagnostic=materials.create("ArrowShape","Graphics");auto* pass=diagnostic->getTechnique(0)->getPass(0);pass->setLightingEnabled(false);pass->setDiffuse(.8f,.8f,.8f,1);
 entity->setMaterialName("ArrowShape","Graphics");node->setOrientation(Ogre::Quaternion::IDENTITY);
 window->update(false);Ogre::Image shape;shape.create(Ogre::PF_BYTE_RGBA,512,256);window->copyContentsToMemory(shape.getPixelBox(),Ogre::RenderTarget::FB_BACK);shape.save(repo+"/build/windows/arrow-shape.png");
 shaders->removeSceneManager(scene);root.destroySceneManager(scene);materials.removeListener(&resolver);Ogre::RTShader::ShaderGenerator::destroy();
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('ARROW_SCALE', arrow_scale)
with tempfile.TemporaryDirectory(prefix='odp-arrow-projectile-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    f'/I{prefix / "include/OGRE/RTShaderSystem"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib'], cwd=work, check=True)
    result = subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, capture_output=True, text=True)
    print('\n'.join(line for line in result.stdout.splitlines() if not line.startswith('GL_EXTENSIONS') and any(token in line for token in ('CHECKS=', 'FAIL ', 'ARROW_', 'Error', 'error'))))
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
