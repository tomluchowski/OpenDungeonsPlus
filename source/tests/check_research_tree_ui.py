"""Run production research-button and dependency rendering against installed CEGUI."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--render', action='store_true', help='Save an isolated Ogre/CEGUI graph preview without launching the game')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
game = (repo / 'source/modes/GameMode.cpp').read_text()
manager = (repo / 'source/game/SkillManager.cpp').read_text()
skill_names = (repo / 'source/game/SkillType.cpp').read_text()
names_start = skill_names.index('std::string skillTypeToPlayerVisibleString(')
names_end = skill_names.index('\n}\n}', names_start) + 2

def function(signature):
    start = game.index(signature)
    end = game.index('{', start) + 1
    depth = 1
    while depth:
        depth += (game[end] == '{') - (game[end] == '}')
        end += 1
    return game[start:end]

# Read the actual constructor's ordered dependency lists, not diagram row guesses.
dependencies = {'emptyDepends': []}
model = {}
for line in manager[manager.index('SkillManager::SkillManager()'):manager.index('SkillManager::~SkillManager()')].splitlines():
    if match := re.search(r'(lvl\ddepends)\.clear\(\)', line):
        dependencies[match[1]] = []
    if match := re.search(r'resType = SkillType::(\w+);', line):
        current = match[1]
    if match := re.search(r'new Skill\(resType, .*, (\w+)\)', line):
        parents = list(dependencies.get(match[1], []))
    if match := re.search(r'new SkillDef\w+\("([^"]+)", "([^"]+)"', line):
        model[current] = (match[1] + match[2], parents)
    if match := re.search(r'(lvl\ddepends)\.push_back\(skill\)', line):
        dependencies.setdefault(match[1], []).append(current)
assert len(model) == 27
initializers = '\n'.join('data[SkillType::%s] = {SkillType::%s, "%s", {%s}};' %
    (key, key, path, ','.join('&data[SkillType::'+parent+']' for parent in parents))
    for key, (path, parents) in model.items())
probe = r'''
#include <CEGUI/CEGUI.h>
#include <CEGUI/RendererModules/Null/Renderer.h>
#include <CEGUI/RendererModules/Ogre/ResourceProvider.h>
#include <CEGUI/RendererModules/Ogre/ImageCodec.h>
#include <OgreRoot.h>
#include <OgreResourceGroupManager.h>
#include "game/SkillType.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
namespace Helper {template<class T>std::string toString(T v){return std::to_string(v);}}
struct CreatureDefinition {std::string getMeshName()const{return "Kobold.mesh";}};
const CEGUI::Image& getCreatureHandIconImage(const std::string&){return CEGUI::ImageManager::getSingleton().get("OpenDungeonsIcons/WorkerButton");}
struct Seat {uint32_t level=0,queue=0;bool current=false;std::vector<SkillType> denied;std::map<SkillType,uint32_t> levels;SkillType currentType=SkillType::roomTrainingHall;
 uint32_t getSkillLevel(SkillType t)const{auto i=levels.find(t);return i==levels.end()?level:i->second;}
 const CreatureDefinition* getWorkerClassToSpawn(){return nullptr;}
 const std::vector<SkillType>& getSkillNotAllowed()const{return denied;}
 uint32_t isSkillPending(SkillType)const{return queue;}
 bool getCurrentSkillProgress(SkillType& t,float& p){t=currentType;p=.5f;return current;}};
struct Player {Seat seat;Seat* getSeat(){return &seat;}};
struct GameMap {Player player;Player* getLocalPlayer(){return &player;}};
struct Skill {SkillType type;std::string path;std::vector<const Skill*> parents;
 SkillType getType()const{return type;}const std::vector<const Skill*>& getDependencies()const{return parents;}
 int getNeededSkillPoints(uint32_t level)const{return 100*level;}};
struct SkillManager {static std::map<SkillType,Skill> data;
 static const Skill* getSkill(SkillType t){return &data.at(t);}
 static std::string getResearchDescription(SkillType,uint32_t){return "Test research description";}
 template<class F>static void listAllSkills(F f){for(auto& p:data)f(p.second.path,"cast"+std::to_string(int(p.first)),p.second.path+"/"+p.second.path.substr(p.second.path.find('/')+1)+"ProgressBar",p.first);}};
std::map<SkillType,Skill> SkillManager::data;
namespace Skills {NAMES}
struct GameMode {CEGUI::Window* mRootWindow;GameMap* mGameMap;bool mIsSkillWindowOpen=false;std::vector<SkillType> mSkillPending;
 struct Progress {CEGUI::ProgressBar* mProgressBar=nullptr;float mCompleteness=0;void setValue(CEGUI::ProgressBar* b,float p){mProgressBar=b;mCompleteness=p;}}mSkillCurrentCompletion;
 void refreshSkillButtonState(const std::string&,const std::string&,const std::string&,SkillType);
 void refreshSkillConnections();};
METHODS
int main(int argc,char** argv){try{
 Ogre::Root ogre("","","research-tree-Ogre.log");ogre.loadPlugin(std::string(argv[2])+"/bin/Codec_STBI");
 auto& resources=Ogre::ResourceGroupManager::getSingleton();
 for(const char* path:{"gui","gui/fonts","gui/schemas"})resources.addResourceLocation(std::string(argv[1])+"/"+path,"FileSystem","GUI");
 CEGUI::OgreResourceProvider provider;provider.setDefaultResourceGroup("GUI");CEGUI::OgreImageCodec codec;codec.setImageFileDataType("png");
 auto& renderer=CEGUI::NullRenderer::create();renderer.setDisplaySize(CEGUI::Sizef(1920,1200));
 auto& system=CEGUI::System::create(renderer,&provider,nullptr,&codec,nullptr,"","research-tree-CEGUI.log");
 CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");
 auto& windows=CEGUI::WindowManager::getSingleton();auto* root=windows.createWindow("DefaultWindow","Root");
 root->addChild(windows.loadLayoutFromFile("WindowSkillTree.layout"));system.getDefaultGUIContext().setRootWindow(root);
 std::map<CEGUI::Window*,CEGUI::URect> authored;
 std::function<void(CEGUI::Window*)> remember=[&](CEGUI::Window* w){authored[w]=w->getArea();for(size_t i=0;i<w->getChildCount();++i)remember(w->getChildAtIdx(i));};
 remember(root->getChild("SkillTreeWindow"));
 auto& data=SkillManager::data;INITIALIZERS
 GameMap map;GameMode game{root,&map};int checks=0,failures=0;
 auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cout<<"FAIL "<<why<<'\n';}};
 for(const auto& pair:data)root->addChild(windows.createWindow("OD/GameTabButton","cast"+std::to_string(int(pair.first))));
 for(uint32_t level=0;level<=3;++level){
  map.player.seat.level=level;map.player.seat.queue=2;map.player.seat.current=level<3;
  SkillManager::listAllSkills([&](const std::string& name,const std::string& cast,const std::string& bar,SkillType type){
   game.refreshSkillButtonState(name,cast,bar,type);auto* b=root->getChild("SkillTreeWindow/Skills/"+name);
   check(b->getText().empty(),"queue numbers stay absent from the icon");
   auto* badge=b->getChild("ResearchLevel");
   check(badge->getText()==std::to_string(level)+"/3","actual current and maximum level visible on every node");
   check(badge->isVisible()&&badge->isMousePassThroughEnabled()&&!badge->isClippedByParent(),"badge visible below icon without intercepting clicks");
   check(b->getProperty("ButtonImageColour")== (level==0?"FF666666":"FFFFFFFF"),"unresearched icons are greyed and researched icons retain colour");
   check(b->getProperty("ResearchLevelColour")== (level==3?"FFFFC947":level==2?"FFD5DFE8":"00FFFFFF"),"level has no, silver or gold frame");
   check(b->isDisabled()==(level==3),"max-level selection rule preserved");
   check(root->getChild(cast)->isVisible()==(level>0),"unlocked actions stay available during upgrades");
   check(b->getUserString("ResearchDetails").find(level==3?"Maximum level":"Queue position: 2")!=CEGUI::String::npos,"queue and level information remains accessible");
   check(b->getUserString("ResearchDetails").find("Requires all (level 1):")!=CEGUI::String::npos || data.at(type).parents.empty(),"prerequisites are explicitly all required");
   check(b->getTooltipText()==b->getUserString("ResearchDetails"),"descriptions are optional hover help, not persistent panels");
   check(b->getChild(b->getName()+"ProgressBar")->isVisible()==(type==SkillType::roomTrainingHall&&level<3),"only the current node displays progress");
  });
  game.refreshSkillConnections();game.refreshSkillConnections();
  for(const auto& p:data){auto* button=root->getChild("SkillTreeWindow/Skills/"+p.second.path);auto* parent=button->getParent();
   for(auto* dependency:p.second.parents)for(int part=0;part<3;++part){
    auto* line=parent->getChild("ResearchLink_"+std::to_string(int(dependency->type))+"_"+std::to_string(int(p.first))+"_"+std::to_string(part));
    check(line->isMousePassThroughEnabled(),"connections do not intercept clicks");
    check(line->getPixelSize().d_width>0&&line->getPixelSize().d_height>0,"each immediate edge has positive geometry");
    check(line->getProperty("ImageColours").find(level>0?"FFD28B54":"FF656A70")!=CEGUI::String::npos,"permanent paths reflect actual prerequisite completion");
   }
  }
  system.getDefaultGUIContext().draw();
 }
 auto* tree=root->getChild("SkillTreeWindow");
 check(!tree->isChild("CurrentResearch")&&!tree->isChild("ResearchDetails"),"no text banner or explanation panel");
 map.player.seat.level=0;
 for(const auto& p:data)if(p.second.parents.size()>1){
  auto* parent=root->getChild("SkillTreeWindow/Skills/"+p.second.path)->getParent();
  map.player.seat.levels.clear();map.player.seat.levels[p.second.parents.front()->type]=1;
  for(bool complete:{false,true}){
   if(complete)for(auto* dependency:p.second.parents)map.player.seat.levels[dependency->type]=1;
   game.refreshSkillConnections();
   for(auto* dependency:p.second.parents)for(int part=0;part<3;++part){
    auto* line=parent->getChild("ResearchLink_"+std::to_string(int(dependency->type))+"_"+std::to_string(int(p.first))+"_"+std::to_string(part));
    bool ready=part==0?map.player.seat.getSkillLevel(dependency->type)>0:complete;
    check(line->getProperty("ImageColours").find(ready?"FFD28B54":"FF656A70")!=CEGUI::String::npos,"shared bus unlocks only when ALL prerequisites are complete");
   }
  }
 }
 map.player.seat.levels.clear();
 map.player.seat.level=0;map.player.seat.current=true;map.player.seat.queue=2;
 SkillManager::listAllSkills([&](const std::string& n,const std::string& c,const std::string& b,SkillType t){game.refreshSkillButtonState(n,c,b,t);});
 game.mIsSkillWindowOpen=true;game.refreshSkillConnections();
 for(const auto size:{CEGUI::Sizef(800,600),CEGUI::Sizef(1280,720),CEGUI::Sizef(1920,1200)})for(float user:{.8f,1.f,1.2f}){
  renderer.setDisplaySize(size);const float scale=std::min(size.d_width/1024.f,size.d_height/768.f)*user;
  auto fonts=CEGUI::FontManager::getSingleton().getIterator();
  while(!fonts.isAtEnd()){fonts.getCurrentValue()->setNativeResolution(CEGUI::Sizef(800/user,600/user));fonts.getCurrentValue()->setAutoScaled(CEGUI::ASM_Min);++fonts;}
  CEGUI::FontManager::getSingleton().notifyDisplaySizeChanged(size);
  for(const auto& p:authored){auto area=p.second;area.d_min.d_x.d_offset*=scale;area.d_min.d_y.d_offset*=scale;area.d_max.d_x.d_offset*=scale;area.d_max.d_y.d_offset*=scale;p.first->setArea(area);}
  game.refreshSkillConnections();
  for(const auto& p:data){auto* button=root->getChild("SkillTreeWindow/Skills/"+p.second.path);auto* parent=button->getParent();
   auto rect=button->getUnclippedOuterRect().get();auto bounds=parent->getUnclippedOuterRect().get();
   auto* badge=button->getChild("ResearchLevel");auto label=badge->getUnclippedOuterRect().get();
   check(label.top()>=rect.bottom()-.5f&&label.bottom()<=bounds.bottom(),"level badge below icon and inside graph");
   check(label.getWidth()>=badge->getFont()->getTextExtent(badge->getText())&&label.getHeight()>=badge->getFont()->getLineSpacing(),"level text fits at every scale");
   check(label.left()>=bounds.left()&&label.right()<=bounds.right(),"level badge stays in its category");
   check(rect.left()>=bounds.left()&&rect.right()<=bounds.right()&&rect.top()>=bounds.top()&&rect.bottom()<=bounds.bottom(),"symbol stays inside its graph column");
   check(std::abs(rect.getWidth()-rect.getHeight())<2,"square symbols at all viewport scales");
   check(!parent->isChild(button->getName()+"Label")&&!parent->isChild(button->getName()+"Status"),"no prose around symbol nodes");
   for(const auto& other:data)if(other.first!=p.first){auto r=root->getChild("SkillTreeWindow/Skills/"+other.second.path)->getUnclippedOuterRect().get();
    check(rect.right()<=r.left()||rect.left()>=r.right()||rect.bottom()<=r.top()||rect.top()>=r.bottom(),"node hit areas never overlap");}
   auto* bar=static_cast<CEGUI::ProgressBar*>(button->getChild(button->getName()+"ProgressBar"));
   check(bar->getUnclippedOuterRect().get().bottom()<=label.top(),"level count never covers current progress");
   if(p.first==SkillType::roomTrainingHall){
    check(button->getProperty("ResearchBackgroundColour")=="FFB76A23","active node highlighted");
    check(bar->isVisible()&&bar->getProgress()==.5f&&bar->getAlpha()==1,"real progress visible on active node");}
   for(auto* dep:p.second.parents)for(int part=0;part<3;++part){auto* line=parent->getChild("ResearchLink_"+std::to_string(int(dep->type))+"_"+std::to_string(int(p.first))+"_"+std::to_string(part));
    check(line->isVisible(),"all prerequisite paths stay visible without hover");
    auto edge=line->getUnclippedOuterRect().get();
    check(edge.getWidth()>0&&edge.getHeight()>0,"positive edge geometry");
    for(const auto& other:data){auto r=root->getChild("SkillTreeWindow/Skills/"+other.second.path)->getUnclippedOuterRect().get();
     check(edge.right()<=r.left()+1||edge.left()>=r.right()-1||edge.bottom()<=r.top()+1||edge.top()>=r.bottom()-1,"connections never cross symbol interiors");}
   }
  }
 }
 windows.destroyWindow(root);CEGUI::System::destroy();CEGUI::NullRenderer::destroy(renderer);
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('METHODS', function('void GameMode::refreshSkillButtonState(')+'\n'+function('void GameMode::refreshSkillConnections(')).replace('INITIALIZERS', initializers).replace('NAMES', skill_names[names_start:names_end])
if args.render:
    native = (repo / 'source/render/Gui.cpp').read_text()
    icon_methods = native[native.index('void shadeNavigationIcon('):native.index('void createNavigationImages(')]
    probe = probe.replace('#include <CEGUI/RendererModules/Null/Renderer.h>', '#include <CEGUI/RendererModules/Ogre/Renderer.h>\n#include <CEGUI/BasicImage.h>\n#include <Ogre.h>\n#include <OgreRenderTexture.h>\n#include <OgreHardwarePixelBuffer.h>\n#include <RTShaderSystem/OgreShaderGenerator.h>\n#include <Bites/OgreSGTechniqueResolverListener.h>')
    probe = probe.replace('int main(int argc,char** argv)', icon_methods + '\nint main(int argc,char** argv)')
    probe = probe.replace('auto& renderer=CEGUI::NullRenderer::create();renderer.setDisplaySize(CEGUI::Sizef(1920,1200));', r'''
 ogre.loadPlugin(std::string(argv[2])+"/bin/RenderSystem_GL3Plus");
 auto* renderSystem=ogre.getAvailableRenderers().front();ogre.setRenderSystem(renderSystem);
 renderSystem->setConfigOption("Full Screen","No");ogre.initialise(false);
 Ogre::NameValuePairList options;options["hidden"]="true";
 auto* window=ogre.createRenderWindow("Research graph preview",1280,960,false,&options);window->setAutoUpdated(false);
 resources.addResourceLocation(std::string(argv[2])+"/Media/Main","FileSystem","OgreInternal");
 resources.addResourceLocation(std::string(argv[2])+"/Media/RTShaderLib/GLSL","FileSystem","General");
 resources.initialiseAllResourceGroups();
 Ogre::RTShader::ShaderGenerator::initialize();auto* shaders=Ogre::RTShader::ShaderGenerator::getSingletonPtr();
 OgreBites::SGTechniqueResolverListener listener(shaders);Ogre::MaterialManager::getSingleton().addListener(&listener);
 auto* scene=ogre.createSceneManager("DefaultSceneManager");auto* camera=scene->createCamera("Camera");
 shaders->addSceneManager(scene);Ogre::MaterialManager::getSingleton().setActiveScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
 scene->getRootSceneNode()->createChildSceneNode()->attachObject(camera);
 window->addViewport(camera)->setBackgroundColour(Ogre::ColourValue(.02f,.03f,.04f));
 auto& renderer=CEGUI::OgreRenderer::create(*window);renderer.setFrameControlExecutionEnabled(false);
 renderer.setDisplaySize(CEGUI::Sizef(1920,1200));''')
    probe = probe.replace('CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");',
                          'CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");colourNavigationAtlas();createSummonWorkerIcon();')
    probe = probe.replace(' windows.destroyWindow(root);', r'''
 renderer.setDisplaySize(CEGUI::Sizef(1280,960));
 auto fonts=CEGUI::FontManager::getSingleton().getIterator();
 while(!fonts.isAtEnd()){fonts.getCurrentValue()->setNativeResolution(CEGUI::Sizef(800,600));fonts.getCurrentValue()->setAutoScaled(CEGUI::ASM_Min);++fonts;}
 CEGUI::FontManager::getSingleton().notifyDisplaySizeChanged(CEGUI::Sizef(1280,960));
 for(const auto& p:authored){auto area=p.second;area.d_min.d_x.d_offset*=1.25f;area.d_min.d_y.d_offset*=1.25f;area.d_max.d_x.d_offset*=1.25f;area.d_max.d_y.d_offset*=1.25f;p.first->setArea(area);}
 auto& seat=map.player.seat;seat.currentType=SkillType::roomArena;seat.current=true;seat.level=0;
 for(const auto& p:data)if(p.second.parents.empty())seat.levels[p.first]=1;
 seat.levels[SkillType::roomTrainingHall]=2;seat.levels[SkillType::roomTreasury]=3;
 seat.levels[SkillType::spellCallToWar]=1;seat.levels[SkillType::spellCreatureExplosion]=1;seat.levels[SkillType::roomPrison]=1;
 game.mSkillPending={SkillType::roomArena,SkillType::roomTorture};
 SkillManager::listAllSkills([&](const std::string& n,const std::string& c,const std::string& b,SkillType t){game.refreshSkillButtonState(n,c,b,t);});
 game.refreshSkillConnections();
 for(const auto& p:data)root->getChild("cast"+std::to_string(int(p.first)))->hide();
 system.getDefaultGUIContext().injectMousePosition(0,0);
 auto texture=Ogre::TextureManager::getSingleton().createManual("ResearchPreview","General",Ogre::TEX_TYPE_2D,1280,960,0,Ogre::PF_BYTE_RGBA,Ogre::TU_RENDERTARGET);
 auto* target=texture->getBuffer()->getRenderTarget();target->setAutoUpdated(false);
 target->addViewport(camera)->setBackgroundColour(Ogre::ColourValue(.02f,.03f,.04f));
 renderer.setDefaultRootRenderTarget(*target);renderSystem->setScissorTest(false);
 target->update(false);system.renderAllGUIContexts();
 target->writeContentsToFile(std::string(argv[1])+"/build/review-followups/research-visual-graph-preview.png");
 renderer.setDefaultRootRenderTarget(*window);target->removeAllViewports();Ogre::TextureManager::getSingleton().remove(texture->getHandle());
 windows.destroyWindow(root);''')
    probe = probe.replace('CEGUI::NullRenderer::destroy(renderer);', 'CEGUI::OgreRenderer::destroy(renderer);shaders->removeSceneManager(scene);ogre.destroySceneManager(scene);Ogre::MaterialManager::getSingleton().removeListener(&listener);Ogre::RTShader::ShaderGenerator::destroy();')
with tempfile.TemporaryDirectory(prefix='odp-research-tree-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    cegui_source = prefix.parent / 'src/cegui/cegui'
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/DCEGUINULLRENDERER_EXPORTS',
        f'/I{repo / "source"}', f'/I{prefix / "include/cegui-0"}', f'/I{cegui_source / "include"}', f'/I{prefix / "include/OGRE"}', f'/I{prefix / "include/OGRE/RTShaderSystem"}',
        'check.cpp', *[str(cegui_source / 'src/RendererModules/Null' / (name+'.cpp')) for name in ('Renderer','GeometryBuffer','Texture','TextureTarget')],
        '/Fecheck.exe', '/link', f'/LIBPATH:{prefix / "lib"}', 'CEGUIBase-0.lib', 'CEGUIOgreRenderer-0.lib', 'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe'), str(repo), str(prefix)], cwd=work, check=True)
