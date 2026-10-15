"""Compile the actual research rules, Seat methods and packet/save paths.

Run in the maintained Windows developer environment; no game is launched.
World/UI services are fixtures; research methods and wire codec are production code.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
prefix = Path(os.environ["CMAKE_PREFIX_PATH"])


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


seat = (root / "source/game/Seat.cpp").read_text(encoding="utf-8")
manager = (root / "source/game/SkillManager.cpp").read_text(encoding="utf-8")
skill = (root / "source/game/Skill.cpp").read_text(encoding="utf-8")
client = (root / "source/network/ODClient.cpp").read_text(encoding="utf-8")
server = (root / "source/network/ODServer.cpp").read_text(encoding="utf-8")
definitions = skill[skill.index("Skill::Skill("):]
definitions += "\n" + "\n".join(function(manager, signature) for signature in [
    "SkillManager::SkillManager()", "const Skill* SkillManager::getSkill(",
    "bool SkillManager::isAllSkillsDoneForSeat(",
    "double SkillManager::getResearchValue(const Seat*", "double SkillManager::getResearchValue(SkillType",
    "void SkillManager::buildRandomPendingSkillsForSeat("])
definitions += "\n" + "\n".join(function(seat, signature) for signature in [
    "bool Seat::addSkill(", "bool Seat::isSkillDone(", "uint32_t Seat::getSkillLevel(",
    "void Seat::setResearchLevels(", "void Seat::completeResearch(", "void Seat::addSkillPoints(",
    "void Seat::setNextSkill(", "void Seat::setSkillsDone(", "void Seat::setSkillTree("])
load = seat[seat.index("    uint32_t nbSkill =", seat.index("bool Seat::importSeatFromStream")):
            seat.index("    // Note: At this point, we are reading seats")]
save_start = seat.index('    os << "[SkillDone]"')
save = seat[save_start:seat.index("    // In editor mode, we don't save tile states", save_start)]
decode = function(client, "case ServerNotificationType::skillsDone:")
decode = decode[decode.index("{") + 1:-1].replace("            break;", "            return true;")
request = function(server, "case ClientNotificationType::askSetSkillTree:")
request = request[request.index("{") + 1:-1].replace("            break;", "            return true;")

probe = r'''
#include "game/Skill.h"
#include "game/SkillType.h"
#include "rooms/RoomType.h"
#include "traps/TrapType.h"
#include "spells/SpellType.h"
#include "network/ODPacket.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#define OD_LOG_INF(x) ((void)0)
#define OD_LOG_ERR(x) ((void)0)
#define OD_ASSERT_TRUE(x) do {if(!(x)) return false;} while(0)
namespace Helper {std::string toString(int value){return std::to_string(value);} template<class T> std::string toString(T value){std::ostringstream s;s<<value;return s.str();}}
class ConfigManager {
public:
    std::map<std::string,int> points;
    static ConfigManager& getSingleton(){static ConfigManager c;return c;}
    int getSkillPoints(const std::string& key) const {return points.at(key);}
};
enum class SkillFamily {rooms,traps,spells,nb};
struct SkillDef {
    const Skill* mSkill;
    SkillDef(const Skill* s):mSkill(s){}
    void mapSkill(std::vector<std::vector<SkillType>>&){ }
};
struct SkillDefRoom:SkillDef {SkillDefRoom(const char*,const char*,const Skill* s,RoomType):SkillDef(s){}};
struct SkillDefTrap:SkillDef {SkillDefTrap(const char*,const char*,const Skill* s,TrapType):SkillDef(s){}};
struct SkillDefSpell:SkillDef {SkillDefSpell(const char*,const char*,const Skill* s,SpellType):SkillDef(s){}};
struct Seat;
struct SkillManager {
    std::vector<const SkillDef*> mSkills;
    std::vector<std::vector<SkillType>> mSkillsFamily;
    SkillManager();
    static const Skill* getSkill(SkillType);
    static bool isAllSkillsDoneForSeat(const Seat*);
    static double getResearchValue(SkillType,uint32_t,double,bool=false);
    static double getResearchValue(const Seat*,SkillType,double,bool=false);
    static void buildRandomPendingSkillsForSeat(std::vector<SkillType>&,const Seat*);
};
SkillManager& getSkillManager(){static SkillManager m;return m;}
struct GameMap {bool server=true;bool isServerGameMap()const{return server;}};
struct Player {
    Seat* seat=nullptr;int warnings=0;
    Seat* getSeat()const{return seat;}
    bool getIsHuman()const{return true;}
    bool getHasLost()const{return false;}
    void notifyNoSkillInQueue(){++warnings;}
};
enum class ServerNotificationType {skillsDone,skillTree,chatServer};
enum class EventShortNoticeType {aboutSkills};
ODPacket& operator<<(ODPacket& packet,EventShortNoticeType type){return packet<<static_cast<int>(type);}
struct ServerNotification {
    ServerNotificationType type; ODPacket mPacket;
    ServerNotification(ServerNotificationType t,Player*):type(t){}
};
struct ODServer {
    std::vector<std::unique_ptr<ServerNotification>> sent;
    static ODServer& getSingleton(){static ODServer s;return s;}
    void queueServerNotification(ServerNotification* n){sent.emplace_back(n);}
};
struct Seat {
    GameMap world;GameMap* mGameMap=&world;Player player;
    int32_t mSkillPoints=0;const Skill* mCurrentSkill=nullptr;
    SkillType mCurrentSkillType=SkillType::nullSkillType;float mCurrentSkillProgress=0;
    bool mGuiSkillNeedsRefresh=false;
    std::vector<SkillType> mSkillDone,mSkillPending,mSkillNotAllowed;
    std::map<SkillType,uint32_t> mResearchLevels;
    Seat(){player.seat=this;}
    Player* getPlayer() {return &player;}
    bool isSkilling()const{return mCurrentSkill!=nullptr;}
    uint32_t getNbRooms(RoomType)const{return 1;}
    const std::vector<SkillType>& getSkillDone()const{return mSkillDone;}
    const std::vector<SkillType>& getSkillPending()const{return mSkillPending;}
    const std::vector<SkillType>& getSkillNotAllowed()const{return mSkillNotAllowed;}
    bool addSkill(SkillType);bool isSkillDone(SkillType)const;uint32_t getSkillLevel(SkillType)const;
    void setResearchLevels(const std::map<SkillType,uint32_t>&);
    void completeResearch(SkillType);void addSkillPoints(int32_t);void setNextSkill(SkillType);
    void setSkillsDone(const std::vector<SkillType>&);void setSkillTree(const std::vector<SkillType>&);
    bool load(std::istream& is);void save(std::ostream& os)const;
};
DEFINITIONS
bool Seat::load(std::istream& is){std::string str;LOAD return true;}
void Seat::save(std::ostream& os)const{SAVE}
Player* activePlayer;
Player* getPlayer(){return activePlayer;}
bool decodeSkills(ODPacket& packetReceived){DECODE}
struct ClientSocket {Player* getPlayer(){return activePlayer;}};
bool requestSkills(ODPacket& packetReceived){ClientSocket socket;ClientSocket* clientSocket=&socket;REQUEST}
int checks=0;
void check(bool value,const std::string& name){++checks;if(!value)throw std::runtime_error(name);}
void near(double a,double b,const std::string& name){check(std::abs(a-b)<1e-7,name);}
int main(int argc,char** argv){try {
    std::ifstream config(argv[1]);std::string line;
    while(std::getline(config,line)){std::istringstream row(line);std::string key;int points;
        if(row>>key>>points && key[0]!='#')ConfigManager::getSingleton().points[key]=points;}
    std::vector<SkillType> all;
    for(uint32_t i=1;i<static_cast<uint32_t>(SkillType::countSkill);++i)all.push_back(static_cast<SkillType>(i));
    check(all.size()==27,"all 27 current research entries");
    for(SkillType type:all){
        const Skill* skill=SkillManager::getSkill(type);check(skill!=nullptr,"catalog completeness");
        const int base=skill->getNeededSkillPoints();
        const int anchor=base?base:((type==SkillType::roomLibrary||type==SkillType::spellSummonWorker)?150:100);
        check(skill->getNeededSkillPoints(2)==2*anchor,"level II cost");
        check(skill->getNeededSkillPoints(3)==4*anchor,"level III cost");
        Seat s;s.setSkillsDone(all);check(s.getSkillLevel(type)==1,"legacy unlock means I");
        s.setSkillTree({type});s.addSkillPoints(2*anchor-1);
        check(s.getSkillLevel(type)==1 && s.mSkillPoints==2*anchor-1,"no premature upgrade");
        s.addSkillPoints(1);check(s.getSkillLevel(type)==2 && s.mSkillPoints==0 && s.mSkillPending.empty(),"II charged once and consumed");
        s.setSkillTree({type});s.addSkillPoints(4*anchor+13);
        check(s.getSkillLevel(type)==3 && s.mSkillPoints==13 && s.mSkillPending.empty(),"III and surplus points");
        s.setSkillTree({type});check(s.mSkillPending.empty(),"cap rejects fourth level");
        check(!s.addSkill(type) && s.getSkillLevel(type)==3,"gift unlock cannot alter upgrade");
        std::stringstream saved;s.save(saved);Seat loaded;
        check(loaded.load(saved) && loaded.getSkillLevel(type)==3 && loaded.mSkillPoints==13,"level and points save roundtrip");
    }
    Seat fresh;
    Seat gifted;gifted.setSkillTree({SkillType::roomTrainingHall,SkillType::roomLibrary});
    gifted.addSkill(SkillType::roomTrainingHall);gifted.addSkillPoints(200);
    check(gifted.getSkillLevel(SkillType::roomTrainingHall)==1 && gifted.mSkillPending.empty(),"gift cannot silently start an unselected upgrade");
    fresh.setSkillTree({SkillType::roomTrainingHall});fresh.addSkillPoints(100);
    check(fresh.getSkillLevel(SkillType::roomTrainingHall)==1,"actual first unlock");
    fresh.setSkillTree({SkillType::roomTrainingHall,SkillType::roomTrainingHall});
    check(fresh.mSkillPending.empty(),"duplicate target rejected");
    fresh.mSkillNotAllowed={SkillType::roomTrainingHall};fresh.setSkillTree({SkillType::roomTrainingHall});
    check(fresh.mSkillPending.empty(),"map-disabled upgrade rejected");
    fresh.mSkillNotAllowed.clear();fresh.setSkillTree({SkillType::trapBoulder});
    check(fresh.mSkillPending.empty(),"locked dependencies rejected");
    std::vector<SkillType> automatic;SkillManager::buildRandomPendingSkillsForSeat(automatic,&fresh);
    fresh.setSkillTree(automatic);check(fresh.mSkillPending==automatic && !automatic.empty(),"autofill includes legal unlocks and upgrades");
    fresh.addSkillPoints(100000);check(fresh.mSkillPending.empty(),"large book surplus drains full selected queue");
    check(fresh.getSkillLevel(SkillType::roomTrainingHall)==2,"autofill upgrades unlocked item only once");
    Seat resumed;resumed.setSkillsDone(all);resumed.setSkillTree({SkillType::trapCannon,SkillType::trapSpike});resumed.addSkillPoints(125);
    std::stringstream saved;resumed.save(saved);Seat loaded;check(loaded.load(saved),"pending upgrade save loads");
    auto pending=loaded.mSkillPending;loaded.setSkillTree(pending);loaded.addSkillPoints(275);
    check(loaded.getSkillLevel(SkillType::trapCannon)==2 && loaded.mSkillPending==std::vector<SkillType>{SkillType::trapSpike},"resume partial upgrade with order retained");
    std::stringstream old("[SkillDone] roomLibrary [/SkillDone] [SkillNotAllowed] [/SkillNotAllowed] [SkillPending] roomTrainingHall [/SkillPending]");
    Seat legacy;check(legacy.load(old) && legacy.getSkillLevel(SkillType::roomLibrary)==1 && legacy.mSkillPoints==0,"old save without extension");
    for(const std::string& block:{"-1 0", "0 1 roomLibrary 4", "0 1 trapCannon 2", "0 2 roomLibrary 2 roomLibrary 3"}){
        std::stringstream bad("[SkillDone] roomLibrary [/SkillDone] [ResearchProgress] "+block+" [/ResearchProgress] [SkillNotAllowed] [/SkillNotAllowed] [SkillPending] [/SkillPending]");
        Seat rejected;check(!rejected.load(bad),"malformed save rejected");
    }
    Seat receiver;receiver.world.server=false;activePlayer=&receiver.player;
    ODPacket packet;packet<<uint32_t(2)<<SkillType::trapCannon<<uint32_t(3)<<SkillType::roomLibrary<<uint32_t(2);
    check(decodeSkills(packet) && receiver.getSkillLevel(SkillType::trapCannon)==3 && receiver.getSkillLevel(SkillType::roomLibrary)==2,"actual client packet parser");
    ODPacket bad;bad<<uint32_t(1)<<SkillType::trapCannon<<uint32_t(4);check(!decodeSkills(bad),"invalid network level rejected");
    ODPacket truncated;truncated<<uint32_t(1)<<SkillType::trapCannon;check(!decodeSkills(truncated),"truncated level packet rejected");
    Seat host;host.setSkillsDone(all);activePlayer=&host.player;
    ODPacket request;request<<uint32_t(1)<<SkillType::trapCannon<<uint32_t(2);
    check(requestSkills(request) && host.mSkillPending==std::vector<SkillType>{SkillType::trapCannon},"actual server accepts II request");
    host.addSkillPoints(400);ODPacket stale;stale<<uint32_t(1)<<SkillType::trapCannon<<uint32_t(2);
    check(requestSkills(stale) && host.mSkillPending.empty(),"stale II request cannot silently queue III");
    using T=SkillType;
    struct Effect{T type;double base,second,third;bool secondary;};
    const Effect effects[]={
        {T::roomTreasury,1000,1250,1500,false},{T::roomHatchery,15,12,10,false},
        {T::roomDormitory,1.5,1.8,2.1,false},{T::roomLibrary,5,6,7,false},
        {T::roomTrainingHall,5,6,7,false},{T::roomWorkshop,5,6,7,false},
        {T::roomBridgeWooden,1,1.25,1.5,false},{T::roomBridgeStone,1,1.5,2,false},
        {T::roomCrypt,30,24,20,false},{T::roomPrison,1,2,3,false},
        {T::roomArena,20,25,30,false},{T::roomCasino,.1,.12,.14,false},{T::roomTorture,.1,.12,.14,false},
        {T::trapCannon,10,12,14,false},{T::trapCannon,15,18,21,true},
        {T::trapSpike,10,13,16,false},{T::trapSpike,15,20,24,true},
        {T::trapBoulder,100,120,140,false},{T::trapBoulder,120,144,168,true},
        {T::trapDoorWooden,10,15,20,false},{T::spellSummonWorker,1500,1350,1200,false},
        {T::spellCallToWar,50,65,80,false},{T::spellCreatureDefense,5,6,7,false},
        {T::spellCreatureExplosion,14,17,20,false},{T::spellCreatureHaste,1.5,1.65,1.8,false},
        {T::spellCreatureHeal,5,6,7,false},{T::spellCreatureSlow,.5,.45,.4,false},
        {T::spellCreatureStrength,1.1,1.15,1.2,false},{T::spellCreatureWeak,.9,.85,.8,false},
        {T::spellEyeEvil,10,15,20,false}};
    for(const auto& effect:effects){
        near(SkillManager::getResearchValue(effect.type,0,effect.base,effect.secondary),effect.base,"preplaced baseline");
        near(SkillManager::getResearchValue(effect.type,1,effect.base,effect.secondary),effect.base,"I unchanged");
        near(SkillManager::getResearchValue(effect.type,2,effect.base,effect.secondary),effect.second,"approved II effect");
        near(SkillManager::getResearchValue(effect.type,3,effect.base,effect.secondary),effect.third,"approved III effect");
    }
    std::cout<<"CHECKS="<<checks<<" FAILURES=0\n";
}catch(const std::exception& e){std::cerr<<"CHECK "<<checks<<": "<<e.what()<<"\n";return 1;}}
'''
for key, value in {"DEFINITIONS": definitions, "LOAD": load, "SAVE": save,
                   "DECODE": decode, "REQUEST": request}.items():
    probe = probe.replace(key, value)

with tempfile.TemporaryDirectory(prefix="research-progression-") as directory:
    work = Path(directory)
    cpp = work / "check.cpp"
    cpp.write_text(probe, encoding="utf-8")
    executable = work / "check.exe"
    subprocess.run([
        "cl", "/nologo", "/EHsc", "/MD", "/std:c++14",
        f"/I{root / 'source'}", f"/I{prefix / 'include'}", f"/I{prefix / 'include/OGRE'}",
        str(cpp), str(root / "source/network/ODPacket.cpp"), str(root / "source/game/SkillType.cpp"),
        f"/Fe:{executable}", "/link", f"/LIBPATH:{prefix / 'lib'}",
        "OgreMain.lib", "sfml-network.lib", "sfml-system.lib"
    ], cwd=work, check=True)
    subprocess.run([str(executable), str(root / "config/skills.cfg")], cwd=work, check=True)
