"""Check production overlay caption ownership and placement with installed Ogre."""
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
source = (repo / 'source/render/MovableTextOverlay.cpp').read_text()
source = source.replace('#include "utils/Helper.h"', '').replace('#include "utils/LogManager.h"', '')
probe = r'''
#include <Ogre.h>
#include <Overlay/OgreOverlaySystem.h>
#include <iostream>
#include <sstream>
namespace Helper {template<class T>std::string toString(T v){std::ostringstream s;s<<v;return s.str();}}
#define OD_LOG_ERR(x) do {} while(false)
SOURCE
int main(int argc,char** argv){try{
 Ogre::Root root("","","caption-outline.log");root.loadPlugin(std::string(argv[2])+"/bin/RenderSystem_GL3Plus");
 root.setRenderSystem(root.getAvailableRenderers().front());root.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";
 root.createRenderWindow("Caption fixture",64,64,false,&options);
 Ogre::OverlaySystem overlaySystem;
 auto& groups=Ogre::ResourceGroupManager::getSingleton();groups.createResourceGroup("GUI");
 groups.addResourceLocation(std::string(argv[1])+"/gui/fonts","FileSystem","GUI");groups.initialiseResourceGroup("GUI");
 auto& manager=Ogre::OverlayManager::getSingleton();int checks=0,failures=0;
 auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::cout<<"FAIL "<<label<<'\n';}};
 {
  MovableTextOverlay label("OutlineProbe",nullptr,nullptr);
  auto id=label.createChildOverlay("MedievalSharp",24,Ogre::ColourValue(.04f,.04f,.04f),"");
  label.forceTextArea(id,48,48);label.centerCaption(id);
  auto* main=manager.getOverlayElement("OutlineProbe0_OvTxt");
  check(!manager.hasOverlayElement("OutlineProbe0_OvOutline0"),"ordinary captions allocate no outline");
  label.setCaptionOutline(id,Ogre::ColourValue(1,.91f,.66f));
  auto* panel=manager.getOverlayElement("OutlineProbe0_OvC");
  for(int level=1;level<=30;++level){
   label.setCaptionSize(id,level<10?24:18);label.setCaption(id,std::to_string(level));
   for(unsigned i=0;i<4;++i){
    auto* edge=manager.getOverlayElement("OutlineProbe0_OvOutline"+std::to_string(i));
    check(edge->getCaption()==main->getCaption(),"all level glyphs share their foreground caption");
    check(edge->getParameter("char_height")==main->getParameter("char_height"),"one- and two-digit sizes stay synchronized");
    const float dx=i==0?-.75f:i==1?.75f:0,dy=i==2?-.75f:i==3?.75f:0;
    check(std::abs(edge->getLeft()-main->getLeft()-dx)<.0001f&&std::abs(edge->getTop()-main->getTop()-dy)<.0001f,"outline stays subpixel-centred around glyph");
    check(edge->getZOrder()<main->getZOrder(),"outline renders behind the dark foreground");
   }
  }
  auto* original=manager.getOverlayElement("OutlineProbe0_OvOutline0");
  label.setCaptionOutline(id,Ogre::ColourValue::White);
  check(original==manager.getOverlayElement("OutlineProbe0_OvOutline0"),"repeated setup reuses existing elements");
  label.setCaption(id,"");
  for(unsigned i=0;i<4;++i)check(manager.getOverlayElement("OutlineProbe0_OvOutline"+std::to_string(i))->getCaption().empty(),"mood replacement also removes outlined level");
  label.displayOverlay(id,-1);check(panel->isVisible(),"outline and foreground share the visible panel");
  label.displayOverlay(id,0);check(!panel->isVisible(),"hidden lifetime hides the complete caption");
  label.setVisible(false);check(!label.isVisible(),"Alt hides the complete overlay");
 }
 for(unsigned i=0;i<4;++i)check(!manager.hasOverlayElement("OutlineProbe0_OvOutline"+std::to_string(i)),"destruction releases every outline element");
 check(!manager.hasOverlayElement("OutlineProbe0_OvTxt"),"foreground cleanup is preserved");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('SOURCE', source)
with tempfile.TemporaryDirectory(prefix='odp-caption-outline-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{repo / "source"}',
                    f'/I{prefix / "include/OGRE"}', 'check.cpp', '/Fecheck.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'OgreOverlay.lib'], cwd=work, check=True)
    if args.compile_only:
        print('COMPILE ONLY: fixture built; runtime checks were not executed')
    else:
        subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, check=True)
