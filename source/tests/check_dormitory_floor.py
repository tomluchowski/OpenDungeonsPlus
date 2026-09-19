"""Render all sixteen dormitory neighbour masks with the shipped mesh/materials."""
from pathlib import Path
import os
import re
import subprocess

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
section = (repo / 'config/tilesets.cfg').read_text().split('[dormitoryRoom]', 1)[1].split('[/dormitoryRoom]', 1)[0]
tiles = []
for line in section.splitlines():
    fields = line.split()
    if fields:
        bits, mesh, material, rx, ry, rz = fields
        tiles.append((int(bits, 2), material, float(rz)))
assert sorted(mask for mask, _, _ in tiles) == list(range(16))
probe = r'''
#include <Ogre.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <iostream>
int main(int argc,char** argv){try{
 Ogre::Root root("","","dormitory.log");root.loadPlugin("RenderSystem_GL3Plus");root.loadPlugin("Codec_STBI");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";options["vsync"]="false";
 auto* window=root.createRenderWindow("Dormitory tiles",256,256,false,&options);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 std::string repo=argv[1],prefix=argv[2];
 for(const auto& path:{repo+"/models",repo+"/shaders",repo+"/materials/textures",prefix+"/Media/Main"})
  groups.addResourceLocation(path,"FileSystem","Graphics");
 groups.initialiseAllResourceGroups();groups.addResourceLocation(repo+"/materials/scripts","FileSystem","Graphics");
 auto& materials=Ogre::MaterialManager::getSingleton();
 for(const auto* name:{"Dormitory.material","Dormitory1100.material","Dormitory1011.material","Dormitory1111.material"})
  materials.parseScript(groups.openResource(name,"Graphics"),"Graphics");
 // Pixel assertions isolate the actual fragment shader from world deformation;
 // the second render uses the unchanged production vertex shader as well.
 auto vertex=Ogre::HighLevelGpuProgramManager::getSingleton().createProgram("FlatTile","Graphics","glsl",Ogre::GPT_VERTEX_PROGRAM);
 vertex->setSource(R"(#version 330 core
 uniform mat4 worldViewProj;uniform mat4 world;
 layout(location=0) in vec4 position;layout(location=8) in vec2 uv0;
 out vec2 out_UV0;out vec2 out_UV1;out vec3 FragPos;out vec4 VertexPos;out mat3 TBN;
 void main(){gl_Position=worldViewProj*position;FragPos=(world*position).xyz;
 out_UV0=out_UV1=uv0;VertexPos=vec4(0);TBN=mat3(1);})");vertex->load();
 auto* scene=root.createSceneManager();scene->setAmbientLight(Ogre::ColourValue(.7f,.7f,.7f));
 auto* camera=scene->createCamera("Camera");camera->setNearClipDistance(.01f);
 camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);camera->setOrthoWindow(1.04f,1.04f);
 auto* cameraNode=scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0,0,3));cameraNode->attachObject(camera);
 auto* viewport=window->addViewport(camera);viewport->setBackgroundColour(Ogre::ColourValue(0,0,1));
 auto* node=scene->getRootSceneNode()->createChildSceneNode();
 auto* tile=scene->createEntity("Floor","Room.mesh","Graphics");node->attachObject(tile);
 struct Variant{int mask;const char* material;float rotation;};
 const Variant variants[]={VARIANTS};
 int checks=0,failures=0;auto check=[&](bool ok,const std::string& why){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<why<<'\n';}};
 for(const auto& variant:variants){
  auto material=materials.getByName(variant.material,"Graphics");material->load();
  check(material->getBestTechnique()!=nullptr,"supported material "+std::string(variant.material));
  auto diagnostic=material->clone("Diagnostic"+std::to_string(variant.mask));
  auto* pass=diagnostic->getTechnique(0)->getPass(0);pass->setVertexProgram("FlatTile");
  auto params=pass->getVertexProgramParameters();params->setNamedAutoConstant("worldViewProj",Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
  params->setNamedAutoConstant("world",Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);
  tile->setMaterialName(diagnostic->getName(),"Graphics");
  node->setOrientation(Ogre::Quaternion(Ogre::Degree(variant.rotation),Ogre::Vector3::UNIT_Z));
  root.renderOneFrame();root.renderOneFrame();
  Ogre::Image image;image.create(Ogre::PF_BYTE_RGB,256,256);
  window->copyContentsToMemory(image.getPixelBox(),Ogre::RenderTarget::FB_FRONT);
  image.save("mask-"+std::to_string(variant.mask)+".png");
  for(int direction=0;direction<4;++direction){
   Ogre::ColourValue average(0,0,0,0);
   for(int along=96;along<160;++along){
    int x=along,y=direction==0?241:14;
    if(direction==1||direction==3){x=direction==1?241:14;y=along;}
    average+=image.getColourAt(x,y,0);
   }
   const bool carpet=average.r>average.g*1.65f;
   check(carpet==bool(variant.mask&(1<<direction)),"mask "+std::to_string(variant.mask)+" boundary "+std::to_string(direction));
  }
  const auto center=image.getColourAt(128,128,0);
  check(center.r>center.g*1.65f&&center.r>.2f,"woven carpet center "+std::to_string(variant.mask));
  tile->setMaterialName(material->getName(),"Graphics");root.renderOneFrame();root.renderOneFrame();
  window->writeContentsToFile("production-"+std::to_string(variant.mask)+".png");
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
 root.destroySceneManager(scene);return failures?1:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
'''
probe = probe.replace('VARIANTS', ','.join('{%d,"%s",%sf}' % (mask, material, rotation) for mask, material, rotation in tiles))
out = repo / 'build/dormitory-floor-check'
out.mkdir(parents=True, exist_ok=True)
(out / 'check.cpp').write_text(probe)
with (out / 'compile.log').open('w') as log:
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
                    'check.cpp', '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib'],
                   cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
with (out / 'result.log').open('w') as log:
    result = subprocess.run([str(out / 'check.exe'), str(repo), str(prefix)], cwd=out, stdout=log, stderr=subprocess.STDOUT)
log = (out / 'result.log').read_text(errors='replace')
print('\n'.join(line for line in log.splitlines() if re.search(r'^FAIL|^CHECKS=|Error:|error:', line)))
assert 'Error: ScriptCompiler' not in log, log
result.check_returncode()
