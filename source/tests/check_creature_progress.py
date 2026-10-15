"""Compile production progress, packet and clock methods; never launch a game."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])


def function(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


creature = 'source/entities/Creature.cpp'
methods = '\n'.join(function(creature, sig) for sig in (
    'double Creature::getExperienceProgress()', 'void Creature::exportProgressToPacket(',
    'void Creature::importProgressFromPacket(', 'void Creature::receiveExp('))
clock = function('source/render/CreatureOverlayStatus.cpp', 'void CreatureOverlayStatus::updateProgress(')
source = (root / creature).read_text()
for sig in ('void Creature::exportToPacket(', 'void Creature::exportToPacketForUpdate('):
    assert 'exportProgressToPacket(os, seat);' in function(creature, sig)
for sig in ('void Creature::importFromPacket(', 'void Creature::updateFromPacket('):
    assert 'importProgressFromPacket(is);' in function(creature, sig)
assert '--mAttackRecoveryTurns;' in source
assert 'mAttackRecoveryDuration = std::max(skillData.mWarmup, skillData.mCooldown)' in source
server = (root / 'source/network/ODServer.cpp').read_text()
assert server.count('packetSend << clientSocket->supportsCreatureProgress();') == 1
assert server.count('packetSend << client->supportsCreatureProgress();') == 1
client = (root / 'source/network/ODClient.cpp').read_text()
assert 'if(!packetReceived.endOfPacket())\n                OD_ASSERT_TRUE(packetReceived >> creatureProgress);' in client
assert 'mSupportsCreatureProgress = false;' in (root / 'source/network/ODSocketClient.cpp').read_text()

probe = r'''
#include "network/ODPacket.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#define OD_LOG_ERR(x) ((void)0)
#define OD_ASSERT_TRUE(x) do { if(!(x)) return; } while(0)
constexpr unsigned MAX_LEVEL = 30;
struct Player {};
struct Seat {Player player; Player* getPlayer() const {return const_cast<Player*>(&player);}};
struct ODServer {bool enabled=true;static ODServer& getSingleton(){static ODServer s;return s;}bool supportsCreatureProgress(Player*){return enabled;}};
struct ODClient {bool enabled=true;static ODClient& getSingleton(){static ODClient s;return s;}bool supportsCreatureProgress(){return enabled;}};
struct Definition {double needed=100;double getXPNeededWhenLevel(unsigned)const{return needed;}};
struct Creature {
 bool server=true,mHasProgressInformation=false,mNeedFireRefresh=false;unsigned mLevel=1;
 double mExp=0,mExperienceProgress=0;Definition def;Definition* mDefinition=&def;
 uint32_t mAttackRecoveryTurns=0,mAttackRecoveryDuration=0,mAttackRecoverySerial=0;
 bool getIsOnServerMap()const{return server;}std::string getName()const{return "probe";}
 double getExperienceProgress()const;void receiveExp(double);
 void exportProgressToPacket(ODPacket&,const Seat*)const;void importProgressFromPacket(ODPacket&);
 bool hasProgressInformation()const{return mHasProgressInformation;}
 uint32_t getAttackRecoveryTurns()const{return mAttackRecoveryTurns;}
 uint32_t getAttackRecoveryDuration()const{return mAttackRecoveryDuration;}
 uint32_t getAttackRecoverySerial()const{return mAttackRecoverySerial;}
};
METHODS
namespace Ogre {using Real=float;}
struct ODApplication {static double turnsPerSecond;};double ODApplication::turnsPerSecond=1.4;
enum class CreatureOverlays {health,experience,recovery,status};
struct MovableTextOverlay {
 unsigned frames[4]={};bool shown[4]={};
 void displayOverlay(uint32_t id,int t){shown[id]=t!=0;}
 void setAtlasFrame(uint32_t id,uint32_t frame,uint32_t cols){if(cols!=8||frame>63)throw 1;frames[id]=frame;}
};
struct CreatureOverlayStatus {
 Creature* mCreature;MovableTextOverlay* mMovableTextOverlay;std::vector<uint32_t> mOverlayIds{0,1,2,3};
 uint32_t mRecoveryTurns=0,mRecoverySerial=0;float mRecoveryElapsed=0;
 void updateProgress(float);
};
CLOCK
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<why<<'\n';}};
 Seat seat;Creature a,b;b.server=false;
 for(unsigned level=1;level<=30;++level)for(double xp:{0.,25.,100.,150.}){
  a.mLevel=level;a.mExp=xp;a.mAttackRecoveryDuration=8;a.mAttackRecoveryTurns=5;a.mAttackRecoverySerial=42;
  ODPacket packet;a.exportProgressToPacket(packet,&seat);packet<<uint32_t(0x12345678);b.importProgressFromPacket(packet);
  check(b.mHasProgressInformation&&b.getExperienceProgress()==(level==30?1.:std::min(1.,xp/100.)),"experience round trip and cap");
  check(b.mAttackRecoveryTurns==5&&b.mAttackRecoveryDuration==8&&b.mAttackRecoverySerial==42,"recovery round trip");
  uint32_t sentinel=0;packet>>sentinel;check(sentinel==0x12345678&&packet.endOfPacket(),"concatenated payload remains aligned");
 }
 ODServer::getSingleton().enabled=false;ODPacket legacy;a.exportProgressToPacket(legacy,&seat);legacy<<uint32_t(27);
 ODClient::getSingleton().enabled=false;b.importProgressFromPacket(legacy);uint32_t sentinel=0;legacy>>sentinel;
 check(!b.mHasProgressInformation&&sentinel==27,"old client/server payload untouched");
 ODClient::getSingleton().enabled=true;ODServer::getSingleton().enabled=true;
 for(double invalid:{-.1,1.1,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
  ODPacket packet;packet<<invalid<<uint32_t(0)<<uint32_t(0)<<uint32_t(0);b.importProgressFromPacket(packet);check(!b.mHasProgressInformation,"invalid XP rejected");
 }
 ODPacket bad;bad<<.5<<uint32_t(8)<<uint32_t(2)<<uint32_t(1);b.importProgressFromPacket(bad);check(!b.mHasProgressInformation,"invalid recovery rejected");
 ODPacket shortPacket;shortPacket<<.5;b.importProgressFromPacket(shortPacket);check(!b.mHasProgressInformation,"truncated payload rejected");
 a.mExp=0;a.receiveExp(-1);check(a.mExp==0,"negative gain rejected");a.receiveExp(20);check(a.mExp==20&&a.mNeedFireRefresh,"XP changes schedule replication");
 MovableTextOverlay overlay;CreatureOverlayStatus status{&b,&overlay};b.mHasProgressInformation=true;b.mExperienceProgress=.5;b.mAttackRecoveryDuration=4;b.mAttackRecoveryTurns=4;b.mAttackRecoverySerial=1;
 status.updateProgress(0);check(overlay.frames[1]==31&&overlay.frames[2]==1,"half XP and fresh attack");
 status.updateProgress(.35f);check(overlay.frames[2]>1,"recovery sweeps between turns");auto frozen=overlay.frames[2];
 for(int i=0;i<100;++i)status.updateProgress(0);check(overlay.frames[2]==frozen,"pause freezes clock");
 status.updateProgress(1000);check(overlay.frames[2]<17,"missing server tick cannot fake readiness");
 b.mAttackRecoveryTurns=1;status.updateProgress(0);check(overlay.frames[2]==47,"server corrects recovery");
 b.mAttackRecoveryTurns=0;status.updateProgress(0);check(overlay.frames[2]==0,"ready hides clock");
 b.mAttackRecoveryTurns=4;++b.mAttackRecoverySerial;status.updateProgress(0);check(overlay.frames[2]==1,"repeated attack resets clock");
 b.mHasProgressInformation=false;status.updateProgress(0);check(!overlay.shown[1]&&overlay.frames[2]==0,"legacy replay does not invent progress");
 std::cout<<"CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
'''.replace('METHODS', methods).replace('CLOCK', clock)
with tempfile.TemporaryDirectory(prefix='odp-progress-') as directory:
    work = Path(directory)
    (work / 'probe.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{root / "source"}',
                    f'/I{prefix / "include"}', f'/I{prefix / "include/OGRE"}', 'probe.cpp',
                    str(root / 'source/network/ODPacket.cpp'), '/Feprobe.exe', '/link',
                    f'/LIBPATH:{prefix / "lib"}', 'sfml-network.lib', 'sfml-system.lib', 'OgreMain.lib'], cwd=work, check=True)
    subprocess.run([str(work / 'probe.exe')], cwd=work, check=True)
