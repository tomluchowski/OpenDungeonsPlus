"""Render the production feather material and verify cutout/fade pixels.

Run in the Windows developer environment; uses a hidden isolated Ogre window,
not the game or a manual QA session.
Pass --combat-blood to run the same cutout/fade checks on the blood material.
"""
from pathlib import Path
import os
import sys
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
prefix = Path(os.environ["CMAKE_PREFIX_PATH"])
probe = r'''
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <iostream>
int main(int argc,char** argv){try{
 Ogre::Root root("","","feather.log");root.loadPlugin("RenderSystem_GL3Plus");root.loadPlugin("Codec_STBI");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";options["vsync"]="false";
 auto* window=root.createRenderWindow("Feather check",128,128,false,&options);
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("Graphics");
 std::string repo=argv[1],prefix=argv[2];
 for(const auto& path:{repo+"/shaders",repo+"/materials/textures",prefix+"/Media/Main",prefix+"/Media/RTShaderLib/GLSL"})
  groups.addResourceLocation(path,"FileSystem","Graphics");
 Ogre::RTShader::ShaderGenerator::initialize();groups.initialiseAllResourceGroups();
 groups.addResourceLocation(repo+"/materials/scripts","FileSystem","Graphics");
 auto& materials=Ogre::MaterialManager::getSingleton();materials.parseScript(groups.openResource("ChickenFeathers.material","Graphics"),"Graphics");
 auto* shaders=Ogre::RTShader::ShaderGenerator::getSingletonPtr();OgreBites::SGTechniqueResolverListener resolver(shaders);materials.addListener(&resolver);
 auto* scene=root.createSceneManager();shaders->addSceneManager(scene);
 auto* camera=scene->createCamera("Camera");camera->setNearClipDistance(.01f);camera->setAspectRatio(1);
 auto* cameraNode=scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0,0,4));cameraNode->attachObject(camera);
 auto* viewport=window->addViewport(camera);viewport->setBackgroundColour(Ogre::ColourValue(.15f,.25f,.2f));
 viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
 auto* billboards=scene->createBillboardSet("Feather",1);billboards->setMaterialName("ChickenFeathers","Graphics");billboards->setDefaultDimensions(2,2);
 auto* billboard=billboards->createBillboard(Ogre::Vector3::ZERO,Ogre::ColourValue(1,.95f,.82f,1));
 scene->getRootSceneNode()->createChildSceneNode()->attachObject(billboards);
 int checks=0;
 auto near=[&](Ogre::ColourValue a,Ogre::ColourValue b){return std::abs(a.r-b.r)<.02f && std::abs(a.g-b.g)<.02f && std::abs(a.b-b.b)<.02f;};
 auto check=[&](bool v,const char* text){++checks;if(!v)throw std::runtime_error(text);};
 for(float alpha:{1.f,0.f}){
  billboard->setColour(Ogre::ColourValue(1,.95f,.82f,alpha));window->update(false);
  Ogre::Image image;image.create(Ogre::PF_BYTE_RGBA,128,128);window->copyContentsToMemory(image.getPixelBox(),Ogre::RenderTarget::FB_BACK);
  auto background=image.getColourAt(2,2,0);int offset=static_cast<int>(.8f*128/(8*Ogre::Math::Tan(camera->getFOVy()*.5f)));
  for(int x:{-1,1})for(int y:{-1,1})check(near(image.getColourAt(64+x*offset,64+y*offset,0),background),"opaque rectangle around feather");
  auto center=image.getColourAt(64,64,0);check(alpha>0?center.r>background.r+.2f:near(center,background),"feather body or alpha fade incorrect");
  window->swapBuffers();
 }
 shaders->removeSceneManager(scene);root.destroySceneManager(scene);materials.removeListener(&resolver);Ogre::RTShader::ShaderGenerator::destroy();
 std::cout<<"CHECKS="<<checks<<" FAILURES=0\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
'''
if "--combat-blood" in sys.argv:
    probe = probe.replace("ChickenFeathers.material", "CombatImpact.material").replace('"ChickenFeathers"', '"CombatBlood"')

with tempfile.TemporaryDirectory(prefix="feather-material-") as directory:
    work = Path(directory)
    cpp = work / "check.cpp"
    cpp.write_text(probe, encoding="utf-8")
    executable = work / "check.exe"
    subprocess.run(["cl", "/nologo", "/EHsc", "/MD", "/std:c++14", f"/I{prefix / 'include/OGRE'}",
                    f"/I{prefix / 'include/OGRE/RTShaderSystem'}", str(cpp), f"/Fe:{executable}",
                    "/link", f"/LIBPATH:{prefix / 'lib'}", "OgreMain.lib", "OgreRTShaderSystem.lib", "OgreBites.lib"], cwd=work, check=True)
    result = subprocess.run([str(executable), str(root), str(prefix)], cwd=work, capture_output=True, text=True)
    print("\n".join(line for line in result.stdout.splitlines() if "CHECKS=" in line))
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
