"""Exercise extracted server shutdown with real SFML threads, without a game."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-ref', help='Compare an older implementation')
args = parser.parse_args()


def read(path):
    if args.source_ref:
        return subprocess.check_output(['git', 'show', args.source_ref + ':' + path],
                                       cwd=repo, text=True)
    return (repo / path).read_text()


def extract(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


server = read('source/network/ODServer.cpp')
socket = read('source/network/ODSocketServer.cpp')
header = read('source/network/ODSocketServer.h')
probe = r'''
#include <SFML/System.hpp>
#include <atomic>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_INF(message) do {} while(false)
#define OD_LOG_ERR(message) do {} while(false)
#define OD_ASSERT_TRUE_MSG(condition,message) do {} while(false)
int checks=0,failures=0;
void check(bool value,const char* name){++checks;if(!value){++failures;std::cout<<"FAIL "<<name<<std::endl;}}
struct ODSocketClient { static int disconnected; void disconnect(){++disconnected;} };
int ODSocketClient::disconnected=0;
struct Selector {int cleared=0;void clear(){++cleared;}};
struct Listener {int closed=0;void close(){++closed;}};
class ODSocketServer {
public:
    sf::Thread* mThread=nullptr;
    RUNNING_TYPE mIsConnected{false};
    Selector mSockSelector; Listener mSockListener;
    std::vector<ODSocketClient*> mSockClients;
    ~ODSocketServer(); bool isConnected(); void requestStop(); virtual void stopServer();
};
enum class ServerNotificationType {turnStarted,entityPickedUp,entityDropped,entitySlapped,exit,other};
struct Player {};
struct ServerNotification {
    ServerNotificationType mType;Player* mConcernedPlayer;int mPacket=0;
    static int destroyed;
    ServerNotification(ServerNotificationType t,Player* p):mType(t),mConcernedPlayer(p){}
    ~ServerNotification(){++destroyed;}
};
int ServerNotification::destroyed=0;
struct GameMap {int cleared=0;void clearAll(){++cleared;}};
enum class ServerState {StateNone,StateGame};
struct ODServer:ODSocketServer {
    GameMap map;GameMap* mGameMap=&map;
    ServerState mServerState=ServerState::StateGame;bool mSeatsConfigured=true;
    std::vector<Player*> mDisconnectedPlayers;Player* mPlayerConfig=nullptr;
    std::deque<ServerNotification*> mServerNotificationQueue;
    std::atomic<bool> ready{false},release{false},finished{false};int sent=0;
    void sendMsg(Player*,int){++sent;}
    void queueServerNotification(ServerNotification* e){mServerNotificationQueue.push_back(e);}
    void processServerNotifications();void notifyExit();void stopServer() override;
    void work(){
        ready=true;
        while(!release)sf::sleep(sf::milliseconds(1));
        processServerNotifications();
        finished=true;
    }
    void launch(){mIsConnected=true;mThread=new sf::Thread(&ODServer::work,this);mThread->launch();
        while(!ready)sf::sleep(sf::milliseconds(1));}
};
FUNCTIONS
int main(int argc,char** argv){
    const std::string scenario=argc>1?argv[1]:"";
    ODServer server;
    server.mSockClients.push_back(new ODSocketClient);
    if(scenario=="queued-exit"){
        server.queueServerNotification(new ServerNotification(ServerNotificationType::other,nullptr));
        server.queueServerNotification(new ServerNotification(ServerNotificationType::exit,nullptr));
        server.queueServerNotification(new ServerNotification(ServerNotificationType::other,nullptr));
        server.launch();server.release=true;
        while(!server.finished)sf::sleep(sf::milliseconds(1));
        check(!server.isConnected(),"worker exit stops loop");
        check(server.sent==1,"events before exit delivered and later events not sent");
        check(server.mServerNotificationQueue.size()==1,"pending events retained for owner cleanup");
        check(server.map.cleared==0 && ODSocketClient::disconnected==0,"worker cannot destroy map or sockets");
    }else if(scenario=="notify-exit"){
        server.queueServerNotification(new ServerNotification(ServerNotificationType::other,nullptr));
        server.launch();server.notifyExit();server.notifyExit();
        check(!server.isConnected(),"repeated exit request stops loop immediately");
        check(server.mServerNotificationQueue.size()==1 &&
            server.mServerNotificationQueue.front()->mType==ServerNotificationType::other,
            "application exit request does not mutate worker queue");
        check(ServerNotification::destroyed==0,"request does not delete worker events");
        server.release=true;
    }else if(scenario=="owner-stop"){
        server.launch();server.release=true;
    }else return 2;
    server.stopServer();
    check(server.finished,"owner waits until worker returns");
    check(server.mThread==nullptr && !server.isConnected(),"owner releases thread");
    check(server.mServerNotificationQueue.empty(),"owner drains pending events");
    check(server.map.cleared==1 && server.mServerState==ServerState::StateNone,"owner clears game state");
    check(ODSocketClient::disconnected==1 && server.mSockClients.empty(),"owner disconnects clients once");
    check(server.mSockSelector.cleared==1 && server.mSockListener.closed==1,"owner clears sockets after join");
    server.stopServer();
    check(server.mThread==nullptr && ODSocketClient::disconnected==1,"repeated owner stop is safe");
    server.ready=false;server.release=false;server.finished=false;
    server.launch();server.release=true;server.stopServer();
    check(server.finished && !server.isConnected(),"next server lifecycle can finish");
    std::cout<<scenario<<" CHECKS="<<checks<<" FAILURES="<<failures<<std::endl;
    return failures?1:0;
}
'''
functions = [extract(socket, name) for name in (
    'ODSocketServer::~ODSocketServer()', 'bool ODSocketServer::isConnected()',
    'void ODSocketServer::stopServer()')]
if 'void ODSocketServer::requestStop()' in socket:
    functions.append(extract(socket, 'void ODSocketServer::requestStop()'))
functions += [extract(server, name) for name in (
    'void ODServer::processServerNotifications()', 'void ODServer::notifyExit()',
    'void ODServer::stopServer()')]
probe = probe.replace('FUNCTIONS', '\n'.join(functions)).replace('RUNNING_TYPE',
    'std::atomic<bool>' if 'std::atomic<bool> mIsConnected' in header else 'bool')
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
failed = False
with tempfile.TemporaryDirectory(prefix='odp-server-shutdown-') as temporary:
    work = Path(temporary)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/O2', '/DNDEBUG', '/std:c++14',
                    f'/I{prefix / "include"}', 'check.cpp', '/Fecheck.exe',
                    '/link', f'/LIBPATH:{prefix / "lib"}', 'sfml-system.lib'],
                   cwd=work, check=True)
    for scenario in ('queued-exit', 'notify-exit', 'owner-stop'):
        try:
            result = subprocess.run([str(work / 'check.exe'), scenario], cwd=work, timeout=5)
            failed |= result.returncode != 0
        except subprocess.TimeoutExpired:
            print('FAIL', scenario, 'hung for 5 seconds', flush=True)
            failed = True

# Guard the actual production loop, not just the isolated worker fixture.
loop = extract(server, 'void ODServer::serverThread()')
poll = loop.index('doTask(static_cast<int32_t>(turnLengthMs));')
next_turn = loop.index('startNewTurn(', poll)
if 'if(!isConnected())\n            break;' not in loop[poll:next_turn]:
    print('FAIL server loop can start a turn after shutdown was requested')
    failed = True
if 'std::atomic<bool> mIsConnected' not in header:
    print('FAIL shared running flag is not atomic')
    failed = True
raise SystemExit(1 if failed else 0)
