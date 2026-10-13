"""Check actual badge dismissal handlers, including unread ownership cleanup."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'source/modes/GameMode.cpp').read_text()
methods = source[source.index('void GameMode::dismissEventMessage('):
                 source.index('void GameMode::updateEventMessageIndicator(')]
code = r'''
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>
int deleted=0,destroyed=0;
struct EventMessage {~EventMessage(){++deleted;}};
namespace CEGUI {
enum MouseButton {LeftButton,RightButton,MiddleButton};
struct Window {void* data=nullptr;bool hidden=false;std::string text="message";
 Window* getChild(const char*){return this;}void hide(){hidden=true;}void setText(const char* s){text=s;}void* getUserData(){return data;}};
struct EventArgs {};
struct MouseEventArgs:EventArgs {MouseButton button;Window* window;};
struct WindowManager {static WindowManager& getSingleton(){static WindowManager m;return m;}
 void destroyWindow(Window*){++destroyed;}};
}
struct GameMode {
 struct MessageTab {EventMessage* message;CEGUI::Window* window;bool read;};
 std::vector<MessageTab> mMessageTabs;std::vector<EventMessage*> mEventMessages;
 EventMessage* mSelectedEventMessage=nullptr;CEGUI::Window* mRootWindow;int updates=0;
 void updateEventMessageIndicator(float){++updates;}
 void dismissEventMessage(EventMessage*);bool onEventMessagesClicked(const CEGUI::EventArgs&);
};
METHODS
int main(){int checks=0,failures=0;auto check=[&](bool pass,const char* name){++checks;if(!pass){++failures;std::cout<<"FAIL "<<name<<'\n';}};
 for(bool read:{false,true})for(bool selected:{false,true}){
  CEGUI::Window root,a,b;GameMode mode;mode.mRootWindow=&root;
  auto* first=new EventMessage;auto* second=new EventMessage;a.data=first;b.data=second;
  mode.mMessageTabs={{first,&a,read},{second,&b,false}};mode.mEventMessages={first,second};
  mode.mSelectedEventMessage=selected?first:second;
  for(auto button:{CEGUI::LeftButton,CEGUI::MiddleButton}){
   CEGUI::MouseEventArgs args;args.window=&a;args.button=button;mode.onEventMessagesClicked(args);
   check(mode.mMessageTabs.size()==2,"non-right clicks do not dismiss");}
  const int before=deleted,windows=destroyed;
  CEGUI::MouseEventArgs args;args.window=&a;args.button=CEGUI::RightButton;mode.onEventMessagesClicked(args);
  check(mode.mMessageTabs.size()==1&&mode.mMessageTabs[0].message==second,"only clicked badge removed");
  check(mode.mEventMessages.size()==1&&mode.mEventMessages[0]==second,"only clicked owned message removed");
  check(deleted==before+1&&destroyed==windows+1,"message and badge deleted exactly once");
  check(!mode.mMessageTabs[0].read,"other unread state retained");
  check(mode.mSelectedEventMessage==(selected?nullptr:second),"other selection retained");
  check(root.hidden==selected,"only dismissed selected panel closes");
  check(root.text==(selected?"":"message"),"other open text retained");
  check(mode.updates==1,"queue refreshes once");
  mode.dismissEventMessage(nullptr);check(mode.updates==1,"unknown message ignored");
  mode.dismissEventMessage(second);check(mode.mMessageTabs.empty()&&mode.mEventMessages.empty(),"unread last message removable");
 }
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
'''.replace('METHODS', methods)
with tempfile.TemporaryDirectory(prefix='odp-notice-dismiss-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(code)
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
