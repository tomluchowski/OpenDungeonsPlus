"""Exercise the browser's production look switch with installed CEGUI."""
from pathlib import Path
import os
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
source = (repo / 'source/modes/MenuModeLoad.cpp').read_text()
start = source.index('    sheet->getChild("LevelWindowFrame")->setLookNFeel(')
block = source[start:source.index('    if(mInGame)', start)]
probe = r'''
#include <CEGUI/CEGUI.h>
#include <CEGUI/RendererModules/Null/Renderer.h>
#include <CEGUI/RendererModules/Ogre/ResourceProvider.h>
#include <CEGUI/RendererModules/Ogre/ImageCodec.h>
#include <OgreRoot.h>
#include <OgreResourceGroupManager.h>
#include <iostream>
void apply(CEGUI::Window* sheet,bool mInGame){BLOCK}
int main(int argc,char** argv){try{
 Ogre::Root ogre("","","load-menu-Ogre.log");ogre.loadPlugin(std::string(argv[2])+"/bin/Codec_STBI");
 auto& resources=Ogre::ResourceGroupManager::getSingleton();
 for(const char* path:{"gui","gui/fonts","gui/schemas"})resources.addResourceLocation(std::string(argv[1])+"/"+path,"FileSystem","GUI");
 CEGUI::OgreResourceProvider provider;provider.setDefaultResourceGroup("GUI");CEGUI::OgreImageCodec codec;codec.setImageFileDataType("png");
 auto& renderer=CEGUI::NullRenderer::create();renderer.setDisplaySize(CEGUI::Sizef(1920,1200));
 auto& system=CEGUI::System::create(renderer,&provider,nullptr,&codec,nullptr,"","load-menu-CEGUI.log");
 CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");
 auto& windows=CEGUI::WindowManager::getSingleton();auto* sheet=windows.loadLayoutFromFile("MenuLoad.layout");
 system.getDefaultGUIContext().setRootWindow(sheet);
 int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 auto* frame=sheet->getChild("LevelWindowFrame");auto* loading=sheet->getChild("LoadingText");
 auto* list=static_cast<CEGUI::Listbox*>(frame->getChild("SaveGameSelect"));
 list->addItem(new CEGUI::ListboxTextItem("Saved dungeon"));
 for(bool game:{true,false,true,false}){
  apply(sheet,game);check(frame->getLookNFeel()==(game?"OD/GameSettingsWindow":"OD/MenuPageWindow"));
  check(loading->isInFront(*frame));check(list->getItemCount()==1);
  check(frame->getChild("BackButton")->isVisible());
  check(frame->getPixelSize().d_width==1920&&frame->getPixelSize().d_height==1200);
  system.getDefaultGUIContext().draw();
 }
 windows.destroyWindow(sheet);CEGUI::System::destroy();CEGUI::NullRenderer::destroy(renderer);
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('BLOCK', block)
with tempfile.TemporaryDirectory(prefix='odp-load-menu-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    cegui = prefix.parent / 'src/cegui/cegui'
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/DCEGUINULLRENDERER_EXPORTS',
        f'/I{prefix / "include/cegui-0"}', f'/I{cegui / "include"}', f'/I{prefix / "include/OGRE"}',
        'check.cpp', *[str(cegui / 'src/RendererModules/Null' / (name+'.cpp')) for name in ('Renderer','GeometryBuffer','Texture','TextureTarget')],
        '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'CEGUIBase-0.lib', 'CEGUIOgreRenderer-0.lib', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, check=True)
