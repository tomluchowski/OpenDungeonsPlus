"""Run the production key handler with OS calls stubbed; never change desktop focus."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


source = (repo / 'source/modes/AbstractApplicationMode.cpp').read_text()
handler = function(source, 'bool AbstractApplicationMode::handleDesktopKey(')
for mode in ('AbstractApplicationMode', 'GameMode', 'EditorMode'):
    text = (repo / f'source/modes/{mode}.cpp').read_text()
    pressed = function(text, f'bool {mode}::keyPressed(')
    if 'handleScreenshotKey(arg)' in pressed:
        assert pressed.index('handleDesktopKey(arg)') < pressed.index('handleScreenshotKey(arg)')
    assert pressed.index('handleDesktopKey(arg)') < pressed.index('injectKeyDown(')

probe = r'''
#include <cstddef>
#include <iostream>
namespace OIS {enum KeyCode{KC_LWIN,KC_RWIN,KC_A,KC_ESCAPE,KC_SYSRQ};struct KeyEvent{KeyCode key;};}
using HWND=void*;const int SW_MINIMIZE=6;HWND foreground=nullptr;int calls=0;HWND target=nullptr;int action=0;
HWND GetForegroundWindow(){return foreground;}
void ShowWindow(HWND window,int command){++calls;target=window;action=command;foreground=nullptr;}
struct Window{size_t handle=42;int queries=0;void getCustomAttribute(const char*,size_t* out){*out=handle;++queries;}};
struct ODFrameListener{Window window;static ODFrameListener& getSingleton(){static ODFrameListener value;return value;}Window* getRenderWindow(){return &window;}};
struct AbstractApplicationMode{bool handleDesktopKey(const OIS::KeyEvent&);};
HANDLER
int main(){AbstractApplicationMode mode;auto& window=ODFrameListener::getSingleton().window;int checks=0,failures=0;
 auto check=[&](bool ok){++checks;if(!ok)++failures;};
 for(auto key:{OIS::KC_LWIN,OIS::KC_RWIN,OIS::KC_A,OIS::KC_ESCAPE,OIS::KC_SYSRQ})
 for(size_t handle:{size_t(0),size_t(42),size_t(73)})for(size_t active:{size_t(0),size_t(42),size_t(73)}){
  window.handle=handle;window.queries=0;foreground=reinterpret_cast<HWND>(active);calls=0;target=nullptr;action=0;
  const bool winKey=key==OIS::KC_LWIN||key==OIS::KC_RWIN;
#if defined OIS_WIN32_PLATFORM
  const bool handled=winKey;const bool minimize=winKey&&handle!=0&&active==handle;
#else
  const bool handled=false,minimize=false;
#endif
  check(mode.handleDesktopKey({key})==handled);check(calls==(minimize?1:0));
  check(window.queries==(handled?1:0));
  check(!minimize||(target==reinterpret_cast<HWND>(handle)&&action==SW_MINIMIZE));
  mode.handleDesktopKey({key});check(calls==(minimize?1:0)); // Ignore repeats after focus loss.
 }
 // Restoration of focus permits the next real press without recreating input.
 window.handle=42;foreground=reinterpret_cast<HWND>(size_t(42));calls=0;
 mode.handleDesktopKey({OIS::KC_LWIN});foreground=reinterpret_cast<HWND>(size_t(42));mode.handleDesktopKey({OIS::KC_RWIN});
#if defined OIS_WIN32_PLATFORM
 check(calls==2);
#else
 check(calls==0);
#endif
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('HANDLER', handler)
with tempfile.TemporaryDirectory(prefix='odp-desktop-key-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    for name, flags in [('windows', ['/DOIS_WIN32_PLATFORM']), ('other', [])]:
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', *flags,
            'check.cpp', f'/Fe{name}.exe'], cwd=work, check=True)
        subprocess.run([str(work / f'{name}.exe')], cwd=work, check=True)
