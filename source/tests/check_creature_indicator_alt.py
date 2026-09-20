"""Run production overlay lifetime and Alt modifier paths without a game session."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def function(path, signature):
    source = (repo / path).read_text()
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


game = (repo / 'source/modes/GameMode.cpp').read_text()
activation = function('source/modes/GameMode.cpp', 'void GameMode::activate()')
assert 'mCreatureIndicatorsVisible);' in activation
for key in ('LMENU', 'RMENU'):
    assert f'getKeyboard()->isKeyDown(OIS::KC_{key})' in activation
    assert f'updateCreatureIndicatorAlt(OIS::KC_{key}, false);' in function('source/modes/GameMode.cpp', 'void GameMode::onFrameStarted(')
for signature, pressed in (('bool GameMode::keyPressed(', 'true'), ('bool GameMode::keyReleased(', 'false')):
    assert f'updateCreatureIndicatorAlt(arg.key, {pressed});' in function('source/modes/GameMode.cpp', signature)
renderer = (repo / 'source/render/RenderManager.cpp').read_text()
assert 'creatureOverlay->displayHealthOverlay(mCreatureTextOverlayDisplayed ? -1.0 : 0.0)' in renderer

probe = r'''
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_ERR(message) ((void)0)
namespace Ogre { using Real=float; }
namespace Helper { template<class T> std::string toString(T value){return std::to_string(value);} }
struct Container { bool visible=false;void show(){visible=true;}void hide(){visible=false;} };
struct ChildOverlay {
 float mTimeToDisplay=0;Container container;Container* mOverlayContainer=&container;
 void displayOverlay(float);void update(float);bool isDisplayed();
};
CHILD_METHODS
struct MovableTextOverlay {
 std::vector<ChildOverlay> mChildOverlays=std::vector<ChildOverlay>(2);bool visible=false;
 void displayOverlay(uint32_t,float);bool isDisplayed(uint32_t);
 void setVisible(bool value){visible=value;}
 void update(float dt){for(auto& child:mChildOverlays)child.update(dt);}
};
MOVABLE_METHODS
enum class CreatureOverlays { health,status };
struct CreatureOverlayStatus;
struct Creature {
 bool onMap=true,alive=true;CreatureOverlayStatus* overlay=nullptr;
 bool getIsOnMap(){return onMap;}bool isAlive(){return alive;}
 CreatureOverlayStatus* getOverlayStatus(){return overlay;}
};
struct CreatureOverlayStatus {
 Creature* mCreature;MovableTextOverlay* mMovableTextOverlay;
 std::vector<uint32_t> mOverlayIds{0,1};
 void updateHealth(){}void updateProgress(float){}void updateStatus(float){mMovableTextOverlay->displayOverlay(1,-1);}
 void displayHealthOverlay(float);void update(float);
};
STATUS_METHODS
struct GameMap { std::vector<Creature*> creatures;const std::vector<Creature*>& getCreatures(){return creatures;} };
struct RenderManager { bool mCreatureTextOverlayDisplayed=false;void rrSetCreaturesTextOverlay(GameMap&,bool);
 static RenderManager* instance;static RenderManager& getSingleton(){return *instance;} };
RenderManager* RenderManager::instance=nullptr;
RENDER_METHOD
namespace OIS { enum KeyCode {KC_LMENU,KC_RMENU,KC_A};struct Keyboard {
 enum Modifier {Alt,Ctrl,Shift};bool held=false;bool isModifierDown(Modifier){return held;}
}; }
namespace sf { struct Keyboard {
 enum Key {LAlt,RAlt,LControl,RControl,LShift,RShift};
 static bool keys[6];static bool isKeyPressed(Key key){return keys[key];}
};bool Keyboard::keys[6]={}; }
struct Keyboard { OIS::Keyboard backend;OIS::Keyboard* mKeyboard=&backend;bool isModifierDown(OIS::Keyboard::Modifier); };
KEYBOARD_METHOD
struct GameMode { GameMap* mGameMap;bool mCreatureIndicatorsVisible=false;
 bool mIndicatorLeftAltDown=false,mIndicatorRightAltDown=false;
 void updateCreatureIndicatorAlt(OIS::KeyCode,bool); };
TOGGLE_METHOD
int main(){int checks=0,failures=0;const auto check=[&](bool value,const char* why){++checks;if(!value){++failures;std::cerr<<why<<'\n';}};
 Creature c;MovableTextOverlay display;CreatureOverlayStatus overlay{&c,&display};c.overlay=&overlay;
 Creature notRendered;GameMap map{{&c,&notRendered}};RenderManager renderer;Keyboard keys;
 for(bool left:{false,true})for(bool right:{false,true}){
  sf::Keyboard::keys[sf::Keyboard::LAlt]=left;sf::Keyboard::keys[sf::Keyboard::RAlt]=right;keys.backend.held=left||right;
  const bool alt=keys.isModifierDown(OIS::Keyboard::Alt);
  check(alt==(left||right),"either Alt key requests display");
  renderer.rrSetCreaturesTextOverlay(map,alt);overlay.update(.01f);
  check(display.visible==alt,"health and need symbols obey held Alt");
  renderer.rrSetCreaturesTextOverlay(map,alt);overlay.update(0);
  check(display.visible==alt,"same key state and pause preserve visibility");
  for(bool alive:{false,true})for(bool onMap:{false,true}){c.alive=alive;c.onMap=onMap;overlay.update(0);
   check(display.visible==(alt&&alive&&onMap),"dead and held creatures stay hidden");}
 }
 c.alive=c.onMap=true;renderer.rrSetCreaturesTextOverlay(map,false);overlay.update(0);
 check(!display.visible,"release hides immediately while paused");
 overlay.displayHealthOverlay(.5f);overlay.update(.2f);check(display.visible,"editor hover remains temporary");
 renderer.rrSetCreaturesTextOverlay(map,false);overlay.update(.1f);check(display.visible,"unchanged request does not erase hover timer");
 overlay.update(.21f);check(!display.visible,"hover expiry hides both children");
 renderer.rrSetCreaturesTextOverlay(map,true);overlay.displayHealthOverlay(.5f);overlay.update(2);
 check(display.visible,"temporary hover cannot shorten held Alt");
 renderer.rrSetCreaturesTextOverlay(map,false);overlay.update(.1f);check(!display.visible,"release hides persistent need child too");
 check(!display.isDisplayed(99),"invalid child is not displayed");
 RenderManager::instance=&renderer;GameMode mode{&map};
 for(auto key:{OIS::KC_LMENU,OIS::KC_RMENU}) {
  for(int press=0;press<4;++press) {
   const bool expected=press%2==0;
   mode.updateCreatureIndicatorAlt(key,true);overlay.update(0);
   check(display.visible==expected,"each Alt press toggles visibility");
   for(int repeat=0;repeat<10;++repeat)mode.updateCreatureIndicatorAlt(key,true);
   overlay.update(0);check(display.visible==expected,"holding and key repeat cannot toggle again");
   mode.updateCreatureIndicatorAlt(key,false);overlay.update(0);
   check(display.visible==expected,"release preserves selected visibility while paused");
  }
 }
 mode.updateCreatureIndicatorAlt(OIS::KC_A,true);check(!mode.mCreatureIndicatorsVisible,"other keys do not toggle");
 mode.updateCreatureIndicatorAlt(OIS::KC_LMENU,true);
 mode.updateCreatureIndicatorAlt(OIS::KC_RMENU,true);
 mode.updateCreatureIndicatorAlt(OIS::KC_LMENU,false);
 mode.updateCreatureIndicatorAlt(OIS::KC_RMENU,true);
 check(mode.mCreatureIndicatorsVisible,"overlapping Alt keys and repeats form one held interval");
 mode.updateCreatureIndicatorAlt(OIS::KC_RMENU,false);
 mode.updateCreatureIndicatorAlt(OIS::KC_RMENU,true);
 check(!mode.mCreatureIndicatorsVisible,"next press toggles after both Alt keys are released");
 // Activation samples already-held keys but restores the saved per-mode choice.
 mode.mIndicatorLeftAltDown=true;mode.mIndicatorRightAltDown=false;
 mode.updateCreatureIndicatorAlt(OIS::KC_LMENU,true);
 check(!mode.mCreatureIndicatorsVisible,"activation with Alt held cannot create a new press");
 mode.updateCreatureIndicatorAlt(OIS::KC_LMENU,false);
 mode.updateCreatureIndicatorAlt(OIS::KC_LMENU,true);
 check(mode.mCreatureIndicatorsVisible,"missed-release recovery permits next press");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''
probe = probe.replace('CHILD_METHODS', '\n'.join(function('source/render/MovableTextOverlay.cpp', name)
    for name in ('void ChildOverlay::displayOverlay(', 'void ChildOverlay::update(', 'bool ChildOverlay::isDisplayed(')))
probe = probe.replace('MOVABLE_METHODS', '\n'.join(function('source/render/MovableTextOverlay.cpp', name)
    for name in ('void MovableTextOverlay::displayOverlay(', 'bool MovableTextOverlay::isDisplayed(')))
probe = probe.replace('STATUS_METHODS', '\n'.join(function('source/render/CreatureOverlayStatus.cpp', name)
    for name in ('void CreatureOverlayStatus::displayHealthOverlay(', 'void CreatureOverlayStatus::update(')))
probe = probe.replace('RENDER_METHOD', function('source/render/RenderManager.cpp', 'void RenderManager::rrSetCreaturesTextOverlay('))
probe = probe.replace('KEYBOARD_METHOD', function('source/modes/Keyboard.cpp', 'bool Keyboard::isModifierDown('))
probe = probe.replace('TOGGLE_METHOD', function('source/modes/GameMode.cpp', 'void GameMode::updateCreatureIndicatorAlt('))
with tempfile.TemporaryDirectory(prefix='odp-indicator-alt-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    for backend in ('OIS', 'SFML'):
        definitions = ['/DOD_USE_SFML_WINDOW'] if backend == 'SFML' else []
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', *definitions,
                        'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
        subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
