"""Execute the room-selection feedback block and verify its existing look states."""
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/modes/GameMode.cpp').read_text()
start = source.index('    SkillManager::listAllRooms([&]', source.index('void GameMode::refreshActionFeedback'))
block = source[start:source.index('    const bool prohibited', start)]
look = ET.parse(repo / 'gui/OD.looknfeel').getroot().find("WidgetLook[@name='OD/GameTabButton']")
for state in ('Normal', 'Hover', 'Pushed', 'PushedOff'):
    assert look.find(f"StateImagery[@name='{state}']/Layer/Section[@section='selected']") is not None
probe = r'''
#include <iostream>
#include <map>
#include <string>
namespace CEGUI {using String=std::string;struct Window {
 std::string colour="00FFFFFF";int writes=0;std::map<std::string,Window> children;
 Window* getChild(const std::string& path){return &children[path];}
 std::string getProperty(const char*)const{return colour;}
 void setProperty(const char*,const std::string& value){colour=value;++writes;}};}
enum class RoomType {room};enum class SelectedAction {none,buildRoom,castSpell,buildTrap};
struct Selection {SelectedAction action;SelectedAction getCurrentAction(){return action;}};
struct SkillManager {template<class F>static void listAllRooms(F f){for(int i=0;i<13;++i)f(RoomType::room,"room"+std::to_string(i));}};
void refresh(CEGUI::Window* mRootWindow,Selection& mPlayerSelection,const std::string& button){BLOCK}
int main(){CEGUI::Window root;int checks=0,failures=0;auto check=[&](bool ok){++checks;if(!ok)++failures;};
 for(auto action:{SelectedAction::buildRoom,SelectedAction::none,SelectedAction::castSpell,SelectedAction::buildTrap})
 for(int selected=0;selected<13;++selected){Selection selection{action};const auto name="room"+std::to_string(selected);
  refresh(&root,selection,name);
  for(int i=0;i<13;++i){auto* b=root.getChild("room"+std::to_string(i));check(b->colour==(action==SelectedAction::buildRoom&&i==selected?"C0FFD060":"00FFFFFF"));}
  int writes=root.getChild(name)->writes;refresh(&root,selection,name);check(root.getChild(name)->writes==writes);
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('BLOCK', block)
with tempfile.TemporaryDirectory(prefix='odp-room-highlight-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
