"""Check actual menu-entry hand restoration with isolated scene-node services.

Run in the Windows compiler environment; no game is launched.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-ref", help="Read the menu entry from a Git revision")
args = parser.parse_args()
renderer = (root / "source/render/RenderManager.cpp").read_text(encoding="utf-8")
menu = (root / "source/renderscene/RenderSceneMenu.cpp").read_text(encoding="utf-8")
if args.source_ref:
    menu = subprocess.check_output(["git", "show", args.source_ref + ":source/renderscene/RenderSceneMenu.cpp"], cwd=root, text=True)
header = (root / "source/render/RenderManager.h").read_text(encoding="utf-8")
start = renderer.index("void RenderManager::rrToggleHandSelectorVisibility()")
toggle = renderer[start:renderer.index("void RenderManager::setEntityOpacity(", start)]
start = header.index("bool isKeeperHandVisible()")
visible = header[start:header.index("}", start) + 1]
start = menu.index("{", menu.index("void RenderSceneMenu::resetMenu(")) + 1
entry = menu[start:menu.index("    Ogre::Rectangle2D* background", start)]
probe = r'''
#include <cstdint>
#include <iostream>
#include <stdexcept>
struct Node {Node* parent=nullptr;bool visible=true;Node* getParentSceneNode(){return parent;}void removeChild(Node* n){n->parent=nullptr;}void addChild(Node* n){if(n->parent)throw std::runtime_error("duplicate parent");n->parent=this;}void setVisible(bool v){visible=v;}};
struct RenderManager {
 Node hand,grip;Node* mHandKeeperNode=&hand;Node* mHeldCreatureGrip=&grip;uint32_t mHandKeeperHandVisibility=0;
 Node prop;Node* mHandIdleProp=&prop;bool idlePlaying=false;
 void rrCancelIdleHandAnimation(){idlePlaying=false;prop.setVisible(false);}
 RenderManager(){hand.addChild(&grip);}
 VISIBLE
 void rrToggleHandSelectorVisibility();
};
TOGGLE
void resetMenu(RenderManager& renderManager){ENTRY}
int main(){int checks=0;try{
 auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("menu hand restoration failed");};
 RenderManager r;
 for(int repeat=0;repeat<20;++repeat){
  r.idlePlaying=true;r.prop.setVisible(true);
  r.rrToggleHandSelectorVisibility();check(!r.isKeeperHandVisible() && !r.hand.visible && !r.grip.parent);
  check(!r.idlePlaying&&!r.prop.visible);
  resetMenu(r);check(r.isKeeperHandVisible() && r.hand.visible && r.grip.parent==&r.hand);
  check(!r.prop.visible);
  resetMenu(r);check(r.isKeeperHandVisible() && r.hand.visible && r.grip.parent==&r.hand);
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES=0\n";
}catch(const std::exception& e){std::cerr<<"CHECK "<<checks<<": "<<e.what()<<"\n";return 1;}}
'''.replace("VISIBLE", visible).replace("TOGGLE", toggle).replace("ENTRY", entry)
with tempfile.TemporaryDirectory(prefix="menu-hand-") as directory:
    work = Path(directory)
    cpp = work / "check.cpp"
    cpp.write_text(probe, encoding="utf-8")
    executable = work / "check.exe"
    subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++14", str(cpp), f"/Fe:{executable}"], cwd=work, check=True)
    subprocess.run([str(executable)], cwd=work, check=True)
