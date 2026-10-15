"""Exercise the production idle-hand timer and input eligibility without a game."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'source/modes/GameMode.cpp').read_text()


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


methods = '\n'.join(function(name) for name in (
    'void GameMode::resetIdleHand(', 'void GameMode::updateIdleHand(', 'void GameMode::deactivate('))
for name in ('mouseMoved', 'mousePressed', 'mouseReleased', 'keyPressed', 'keyReleased'):
    method = function(f'bool GameMode::{name}(')
    assert method.split('{', 1)[1].lstrip().startswith('resetIdleHand();'), name
for signature in ('void GameMode::activate(', 'GameMode::~GameMode('):
    assert function(signature).split('{', 1)[1].lstrip().startswith('resetIdleHand();')
assert 'mIdleHandKeys.insert(arg.key);' in function('bool GameMode::keyPressed(')
assert 'mIdleHandKeys.erase(arg.key);' in function('bool GameMode::keyReleased(')
assert 'getKeyboard()->isKeyDown(static_cast<OIS::KeyCode>(key))' in function('void GameMode::activate(')
feedback = function('void GameMode::refreshActionFeedback(')
assert feedback.index('rrSetHandPose(') < feedback.index('updateIdleHand(')
eligible = re.search(r'updateIdleHand\(elapsed, (.*?)\);', feedback, re.S)[1]
base = (root / 'source/modes/GameEditorModeBase.cpp').read_text()
blocked = base[base.index('bool GameEditorModeBase::cameraInputBlocked()'):base.index('bool GameEditorModeBase::onMinimapClick(')]
assert 'getRenderWindow()->isActive()' in blocked and 'InputModeNormal' in blocked and 'child->isVisible()' in blocked

probe = r'''
#include <iostream>
#include <set>
namespace OIS { enum KeyCode { KC_A,KC_LMENU,KC_RMENU }; }
struct Keyboard {
 std::set<OIS::KeyCode> down;
 bool isKeyDown(OIS::KeyCode key){return down.count(key)!=0;}
};
struct RenderManager {
 bool playing=false,ready=true,visible=true;int starts=0,cancels=0;
 static RenderManager& getSingleton(){static RenderManager value;return value;}
 bool rrIsIdleHandAnimationPlaying(){return playing;}
 bool rrPlayIdleHandAnimation(){if(!ready)return false;playing=true;++starts;return true;}
 void rrCancelIdleHandAnimation(){if(playing)++cancels;playing=false;}
 bool isKeeperHandVisible(){return visible;}
};
struct GameEditorModeBase {int deactivated=0;void deactivate(){++deactivated;} };
struct GameMap {bool paused=false;bool getGamePaused(){return paused;} };
struct GameMode:GameEditorModeBase {
 Keyboard keys;std::set<OIS::KeyCode> mIdleHandKeys;float mIdleHandElapsed=0;
 GameMap map;GameMap* mGameMap=&map;bool blocked=false,connected=true;
 Keyboard* getKeyboard(){return &keys;}
 bool cameraInputBlocked(){return blocked;}bool isConnected(){return connected;}
 void resetIdleHand();void updateIdleHand(float,bool);void deactivate();
 bool eligible(bool overGui,bool holding,bool active,bool digging,bool left,bool right,bool middle){
  struct {bool mLMouseDown,mRMouseDown,mMMouseDown;} inputManager{left,right,middle};
  return ELIGIBLE;
 }
};
METHODS
int main(){
 int checks=0,failures=0;
 const auto check=[&](bool value,const char* label){++checks;if(!value){++failures;std::cerr<<label<<'\n';}};
 GameMode game;auto& renderer=RenderManager::getSingleton();
 for(unsigned mask=0;mask<2048;++mask){
  game.map.paused=mask&16;game.blocked=mask&32;game.connected=!(mask&512);renderer.visible=!(mask&1024);
  check(game.eligible(mask&1,mask&2,mask&4,mask&8,mask&64,mask&128,mask&256)==((mask&~13u)==0),"eligibility excludes held, modal, paused and hidden states but not stationary hover or selection");
 }
 renderer=RenderManager{};
 game.updateIdleHand(29.5f,true);check(renderer.starts==0,"no early start");
 game.updateIdleHand(.5f,true);check(renderer.starts==1&&renderer.playing,"start at thirty seconds");
 game.updateIdleHand(90,true);check(renderer.starts==1&&game.mIdleHandElapsed==0,"playing effect cannot accumulate or restart");
 game.resetIdleHand();check(!renderer.playing&&renderer.cancels==1,"input cancels immediately");
 for(int i=0;i<60;++i){game.updateIdleHand(.5f,true);if(i<59)check(!renderer.playing,"idle delay after cancellation");}
 check(renderer.starts==2,"idle may recur after another thirty seconds");
 renderer.playing=false;game.updateIdleHand(29,true);check(renderer.starts==2,"completion starts a fresh delay");
 game.updateIdleHand(1,true);check(renderer.starts==3,"completed effects can recur");
 game.updateIdleHand(0,false);check(!renderer.playing&&game.mIdleHandElapsed==0,"ineligibility cancels a live effect");
 game.updateIdleHand(29,true);game.updateIdleHand(0,false);game.updateIdleHand(1,true);
 check(!renderer.playing&&game.mIdleHandElapsed==1,"blocked time does not count toward inactivity");
 for(auto key:{OIS::KC_A,OIS::KC_LMENU,OIS::KC_RMENU}){
  game.keys.down.insert(key);game.mIdleHandKeys.insert(key);game.updateIdleHand(40,true);
  check(!renderer.playing&&game.mIdleHandElapsed==0,"held input blocks effect without repeat events");
  game.keys.down.erase(key);game.updateIdleHand(1,true);
  check(game.mIdleHandKeys.empty()&&game.mIdleHandElapsed==1,"missed key release is recovered");
 }
 renderer.ready=false;game.resetIdleHand();game.updateIdleHand(30,true);
 check(!renderer.playing&&game.mIdleHandElapsed==30,"busy renderer is not interrupted");
 renderer.ready=true;game.updateIdleHand(.01f,true);check(renderer.playing,"start once contextual animation permits");
 game.mIdleHandKeys.insert(OIS::KC_A);game.deactivate();
 check(!renderer.playing&&game.mIdleHandElapsed==0&&game.mIdleHandKeys.empty()&&game.deactivated==1,"mode exit cancels and preserves base cleanup");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHODS', methods).replace('ELIGIBLE', eligible)
with tempfile.TemporaryDirectory(prefix='od-idle-hand-') as temporary:
    out = Path(temporary)
    (out / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++14', 'check.cpp', '/Fe:check.exe'], cwd=out, check=True)
    subprocess.run([str(out / 'check.exe')], cwd=out, check=True)
