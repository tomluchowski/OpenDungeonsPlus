"""Exercise the production dungeon heart health ring without a game.

Compiles the real HeartHealthRing.h rules, the real server function notifyHeartHealth, the real
heart health/save methods of RoomDungeonTemple and the real badge drawing of Gui.cpp into one
fixture, and checks the wiring of the notification in the client and the game mode.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]


def read(name):
    return (repo / name).read_text()


server = read('source/network/ODServer.cpp')
temple = read('source/rooms/RoomDungeonTemple.cpp')
temple_header = read('source/rooms/RoomDungeonTemple.h')
gui = read('source/render/Gui.cpp')
gui_header = read('source/render/Gui.h')
client = read('source/network/ODClient.cpp')
client_header = read('source/network/ODClient.h')
game_mode = read('source/modes/GameMode.cpp')
notification_header = read('source/network/ServerNotification.h')
notification_source = read('source/network/ServerNotification.cpp')
layout = read('gui/ModeGame.layout')
rules = repo / 'source/game/HeartHealthRing.h'


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


probe = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "RULES_HEADER"

enum class ServerNotificationType {heartHealth};
enum class RoomType {dungeonTemple, treasury};
struct Seat {int id;};
struct Tile {};
struct Player {
    bool mHuman;
    Seat* mSeat;
    Player(Seat* seat, bool human) : mHuman(human), mSeat(seat) {}
    bool getIsHuman() const {return mHuman;}
    Seat* getSeat() const {return mSeat;}
};
struct ODPacket {
    std::vector<std::string> kinds;
    std::vector<double> values;
    ODPacket& operator<<(float v) {kinds.push_back("float"); values.push_back(v); return *this;}
    ODPacket& operator<<(double v) {kinds.push_back("double"); values.push_back(v); return *this;}
    ODPacket& operator<<(bool v) {kinds.push_back("bool"); values.push_back(v ? 1.0 : 0.0); return *this;}
};
struct ServerNotification {
    ServerNotificationType mType;
    Player* mPlayer;
    ODPacket mPacket;
    ServerNotification(ServerNotificationType t, Player* p) : mType(t), mPlayer(p) {}
};
struct ODServer {
    std::vector<ServerNotification*> mQueue;
    static ODServer& getSingleton() {static ODServer server; return server;}
    void queueServerNotification(ServerNotification* n) {mQueue.push_back(n);}
};
struct ODSocketClient {
    float mSent;
    double mHPSent;
    ODSocketClient() : mSent(-1.0f), mHPSent(-1.0) {}
    float getHeartHealthSent() const {return mSent;}
    void setHeartHealthSent(float f) {mSent = f;}
    double getHeartHPSent() const {return mHPSent;}
    void setHeartHPSent(double hp) {mHPSent = hp;}
};
struct Building {
    double mFloorHP;
    Building() : mFloorHP(250.0) {}
    virtual double getHP(Tile*) const {return mFloorHP;}
    virtual ~Building() {}
};
struct Room : Building {
    RoomType mType;
    Seat* mSeat;
    unsigned mTiles;
    Room(RoomType type, Seat* seat) : mType(type), mSeat(seat), mTiles(9) {}
    unsigned numCoveredTiles() const {return mTiles;}
    RoomType getType() const {return mType;}
    Seat* getSeat() const {return mSeat;}
    virtual void exportToStream(std::ostream& os) const {os << mFloorHP << '\n';}
    virtual bool importFromStream(std::istream& is) {return static_cast<bool>(is >> mFloorHP);}
};
struct GameMap {
    std::vector<Room*> mRooms;
    const std::vector<Room*>& getRooms() const {return mRooms;}
    bool isInEditorMode() const {return false;}
};
struct RoomDungeonTemple : Room {
    GameMap* mMap;
    double mHeartHP;
    RoomDungeonTemple(GameMap* map, Seat* seat) : Room(RoomType::dungeonTemple, seat), mMap(map), mHeartHP(-1.0) {}
    GameMap* getGameMap() const {return mMap;}
    static const double HEART_HP_PER_TILE;
    double getHeartMaxHP() const;
    double getHP(Tile* tile) const override;
    double getHeartHealthFraction() const;
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;
};
TEMPLE_METHODS
SERVER_FUNCTION
BADGE_CODE

int gChecks = 0;
int gFailures = 0;

void check(bool ok, const char* message)
{
    ++gChecks;
    if(!ok)
    {
        ++gFailures;
        std::cout << "FAIL " << message << '\n';
    }
}

bool near(double a, double b)
{
    return std::abs(a - b) < 0.001;
}

// Colour of the badge pixel at an angle (degrees clockwise from the top) and a distance from the centre
void pixelAt(const std::vector<unsigned char>& pixels, float angle, float distance, int& r, int& g, int& b)
{
    const float radians = angle * 3.14159265f / 180.0f;
    const float dx = distance * std::sin(radians);
    const float dy = -distance * std::cos(radians);
    const int x = static_cast<int>((dx + 32.0f) * BADGE_SIZE / 64.0f);
    const int y = static_cast<int>((dy + 32.0f) * BADGE_SIZE / 64.0f);
    const int i = (y * BADGE_SIZE + x) * 4;
    r = pixels[i];
    g = pixels[i + 1];
    b = pixels[i + 2];
}

bool ringLit(const std::vector<unsigned char>& pixels, float angle)
{
    int r, g, b;
    pixelAt(pixels, angle, 23.0f, r, g, b);
    return g > 60 && r < 80;
}

bool glowing(const std::vector<unsigned char>& pixels)
{
    int r, g, b;
    pixelAt(pixels, 0.0f, 19.0f, r, g, b);
    return r > 150 && b > 100 && g < 80;
}

void checkMapping()
{
    check(near(HeartHealthRing::visibleSpanDegrees(0.0f), 0.0), "0 percent shows no arc");
    check(near(HeartHealthRing::visibleSpanDegrees(0.1f), 33.0), "10 percent shows 33 degrees");
    check(near(HeartHealthRing::visibleSpanDegrees(0.5f), 165.0), "50 percent shows 165 degrees");
    check(near(HeartHealthRing::visibleSpanDegrees(1.0f), 330.0), "100 percent shows 330 degrees");
    check(near(HeartHealthRing::visibleSpanDegrees(-0.5f), 0.0), "negative fraction clamps to 0");
    check(near(HeartHealthRing::visibleSpanDegrees(7.0f), 330.0), "fraction above 1 clamps to 330");
    check(near(HeartHealthRing::visibleSpanDegrees(std::numeric_limits<float>::quiet_NaN()), 0.0), "NaN shows nothing");
    check(near(HeartHealthRing::RING_START_DEGREES + HeartHealthRing::visibleSpanDegrees(0.1f), 48.0),
        "10 percent ends near 50 degrees");
    bool monotone = true;
    float previous = -1.0f;
    for(int i = 0; i <= 100; ++i)
    {
        const float span = HeartHealthRing::visibleSpanDegrees(i / 100.0f);
        monotone = monotone && span >= previous;
        previous = span;
    }
    check(monotone, "arc never grows when the health falls");
    check(HeartHealthRing::isRingLit(std::sin(0.35f), -std::cos(0.35f), 1.0f), "full ring lit at 20 degrees");
    check(!HeartHealthRing::isRingLit(std::sin(0.05f), -std::cos(0.05f), 1.0f), "full ring has its gap at the top (3 degrees)");
    check(!HeartHealthRing::isRingLit(-std::sin(0.05f), -std::cos(0.05f), 1.0f), "full ring has its gap at the top (357 degrees)");
    check(HeartHealthRing::isRingLit(0.0f, 1.0f, 1.0f), "full ring lit at the bottom");
    check(!HeartHealthRing::isRingLit(0.0f, 1.0f, 0.0f), "empty ring lit nowhere");
}

void checkNotifyRule()
{
    check(HeartHealthRing::shouldNotify(-1.0f, 1.0f), "first message always sent");
    check(HeartHealthRing::shouldNotify(-1.0f, 0.0f), "first message sent even for a destroyed heart");
    check(!HeartHealthRing::shouldNotify(1.0f, 1.0f), "no change is not sent");
    check(!HeartHealthRing::shouldNotify(1.0f, 0.992f), "less than one point is not sent");
    check(HeartHealthRing::shouldNotify(1.0f, 0.99f), "exactly one point is sent");
    check(HeartHealthRing::shouldNotify(0.5f, 0.489f), "more than one point is sent");
    check(HeartHealthRing::shouldNotify(0.005f, 0.0f), "destroyed heart is sent even below one point");
    check(!HeartHealthRing::shouldNotify(0.0f, 0.0f), "destroyed heart is sent only once");
}

void checkGlowTimer()
{
    HeartHealthRing::BadgeState state;
    bool first = state.takeDirty();
    bool second = state.takeDirty();
    check(first && !second, "fresh state redraws once");
    check(near(state.mFraction, 1.0) && !state.mGlow, "fresh state is a full ring without glow");
    state.receive(0.6f, true);
    check(state.mGlow && near(state.mFraction, 0.6) && state.takeDirty(), "attack message turns the glow on");
    state.update(1.0f);
    state.update(1.9f);
    check(state.mGlow && !state.takeDirty(), "glow still on after 2.9 seconds");
    state.update(0.2f);
    check(!state.mGlow && state.takeDirty(), "glow off after 3 seconds without message");
    state.update(5.0f);
    check(!state.mGlow && !state.takeDirty(), "glow stays off");
    state.receive(0.5f, true);
    state.update(2.5f);
    state.receive(0.45f, true);
    state.update(2.5f);
    check(state.mGlow, "a further message restarts the 3 seconds");
    state.update(0.6f);
    check(!state.mGlow, "and it ends 3 seconds after the last message");
    state.receive(0.4f, false);
    check(!state.mGlow && near(state.mFraction, 0.4), "a message without attack shows no glow");
    state.receive(0.0f, true);
    check(!state.mGlow && near(state.mFraction, 0.0), "destroyed heart never glows");
    state.update(0.1f);
    check(!state.mGlow && near(state.mFraction, 0.0), "destroyed heart stays empty and dark");
    state.receive(3.0f, false);
    check(near(state.mFraction, 1.0), "client clamps a wrong fraction");
}

void checkServer()
{
    ODServer& queue = ODServer::getSingleton();
    Seat mine = {1};
    Seat other = {2};
    Player human(&mine, true);
    Player enemy(&other, true);
    Player robot(&mine, false);
    GameMap map;
    RoomDungeonTemple ownHeart(&map, &mine);
    RoomDungeonTemple enemyHeart(&map, &other);
    Room treasury(RoomType::treasury, &mine);
    map.mRooms.push_back(&treasury);
    map.mRooms.push_back(&enemyHeart);
    map.mRooms.push_back(&ownHeart);
    ODSocketClient socket;

    check(near(ownHeart.getHeartHealthFraction(), 1.0), "undamaged heart has a full fraction");
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1, "one message when the game starts");
    if(queue.mQueue.size() == 1)
    {
        const ServerNotification* n = queue.mQueue[0];
        check(n->mType == ServerNotificationType::heartHealth && n->mPlayer == &human, "start message goes to the owner");
        check(n->mPacket.kinds.size() == 4 && n->mPacket.kinds[0] == "float" && n->mPacket.kinds[1] == "bool"
            && n->mPacket.kinds[2] == "double" && n->mPacket.kinds[3] == "double",
            "payload order is float healthFraction, bool underAttack, double heart HP, double heart max HP");
        check(n->mPacket.values.size() == 4 && near(n->mPacket.values[0], 1.0) && n->mPacket.values[1] == 0.0
            && near(n->mPacket.values[2], 90000.0) && near(n->mPacket.values[3], 90000.0),
            "start message is a full heart of 90000 HP that is not under attack");
    }
    queue.mQueue.clear();
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "nothing is sent while the health does not change");

    ownHeart.mHeartHP = 89280.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[2], 89280.0),
        "a change below one point is sent too, so that the tooltip shows the exact HP");
    queue.mQueue.clear();
    ownHeart.mHeartHP = 89280.4;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "a change below one whole HP is not sent");
    ownHeart.mHeartHP = 88920.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1, "a change of one point is sent");
    if(queue.mQueue.size() == 1)
        check(near(queue.mQueue[0]->mPacket.values[0], 0.988) && queue.mQueue[0]->mPacket.values[1] == 1.0,
            "a hit is sent with its fraction and underAttack");
    queue.mQueue.clear();

    ownHeart.mHeartHP = 9000.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[0], 0.1), "10 percent heart is sent");
    queue.mQueue.clear();
    ownHeart.mHeartHP = 8640.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[2], 8640.0),
        "0.4 point below the last message is sent for the tooltip");
    queue.mQueue.clear();
    ownHeart.mHeartHP = 180.0;
    notifyHeartHealth(&map, &socket, &human);
    queue.mQueue.clear();
    ownHeart.mHeartHP = 0.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1, "a destroyed heart is sent even below one point");
    if(queue.mQueue.size() == 1)
        check(near(queue.mQueue[0]->mPacket.values[0], 0.0) && queue.mQueue[0]->mPacket.values[1] == 1.0,
            "destroyed heart message has fraction 0");
    queue.mQueue.clear();
    notifyHeartHealth(&map, &socket, &human);
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "a destroyed heart is not sent again");

    ODSocketClient robotSocket;
    ownHeart.mHeartHP = 36000.0;
    notifyHeartHealth(&map, &robotSocket, &robot);
    check(queue.mQueue.empty(), "nothing is sent to a non-human player");
    ODSocketClient enemySocket;
    notifyHeartHealth(&map, &enemySocket, &enemy);
    check(queue.mQueue.size() == 1 && queue.mQueue[0]->mPlayer == &enemy
        && near(queue.mQueue[0]->mPacket.values[0], 1.0), "each owner is told about its own heart only");
    queue.mQueue.clear();
    Seat lonely = {3};
    Player nobody(&lonely, true);
    ODSocketClient nobodySocket;
    notifyHeartHealth(&map, &nobodySocket, &nobody);
    check(queue.mQueue.empty(), "a seat without a heart gets nothing");
}

void checkSaveAndLoad()
{
    ODServer& queue = ODServer::getSingleton();
    queue.mQueue.clear();
    Seat mine = {1};
    Player human(&mine, true);
    GameMap map;
    RoomDungeonTemple saved(&map, &mine);
    saved.mHeartHP = 9000.0;
    std::stringstream stream;
    saved.exportToStream(stream);
    stream << "[/Room]\n";

    // A new server process: the socket has not been told anything yet
    GameMap loadedMap;
    RoomDungeonTemple loaded(&loadedMap, &mine);
    check(loaded.importFromStream(stream), "damaged heart loads");
    check(near(loaded.getHeartHealthFraction(), 0.1), "loaded heart keeps its health fraction");
    loadedMap.mRooms.push_back(&loaded);
    ODSocketClient socket;
    notifyHeartHealth(&loadedMap, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[0], 0.1)
        && queue.mQueue[0]->mPacket.values[1] == 0.0, "loading a damaged heart sends the damaged ring once, without glow");
    queue.mQueue.clear();

    std::stringstream legacy("250\n[/Room]\n");
    RoomDungeonTemple old(&loadedMap, &mine);
    check(old.importFromStream(legacy) && near(old.getHeartHealthFraction(), 1.0), "old save without health loads full");
    RoomDungeonTemple dead(&loadedMap, &mine);
    dead.mHeartHP = 0.0;
    check(near(dead.getHeartHealthFraction(), 0.0), "destroyed heart has fraction 0");
    dead.mTiles = 0;
    check(near(dead.getHeartHealthFraction(), 0.0), "a room without tiles gives fraction 0");
    RoomDungeonTemple half(&loadedMap, &mine);
    half.mFloorHP = 90.0;
    half.mHeartHP = 45000.0;
    check(near(half.getHeartMaxHP(), 90000.0) && near(half.getHeartHealthFraction(), 0.5),
        "the ring follows the heart's own 90000 health (10000 per tile), not the floor durability");
}

void checkDrawing()
{
    std::vector<unsigned char> full;
    std::vector<unsigned char> tenth;
    std::vector<unsigned char> half;
    std::vector<unsigned char> empty;
    std::vector<unsigned char> attacked;
    drawBadgePixels(full, 0, 1.0f, false);
    drawBadgePixels(tenth, 0, 0.1f, false);
    drawBadgePixels(half, 0, 0.5f, false);
    drawBadgePixels(empty, 0, 0.0f, false);
    drawBadgePixels(attacked, 0, 0.5f, true);
    check(full.size() == static_cast<size_t>(BADGE_SIZE * BADGE_SIZE * 4), "badge has its full size");
    check(ringLit(full, 20.0f) && ringLit(full, 90.0f) && ringLit(full, 180.0f) && ringLit(full, 270.0f) && ringLit(full, 340.0f),
        "full ring is green all around");
    check(!ringLit(full, 5.0f) && !ringLit(full, 355.0f), "full ring has its gap at the top");
    check(ringLit(tenth, 20.0f) && ringLit(tenth, 45.0f), "10 percent arc starts at the top going clockwise");
    check(!ringLit(tenth, 60.0f) && !ringLit(tenth, 180.0f) && !ringLit(tenth, 300.0f), "10 percent arc ends near 50 degrees");
    check(ringLit(half, 170.0f) && !ringLit(half, 195.0f) && !ringLit(half, 300.0f), "50 percent arc ends at the bottom");
    check(!ringLit(empty, 20.0f) && !ringLit(empty, 90.0f) && !ringLit(empty, 180.0f) && !ringLit(empty, 340.0f),
        "empty ring shows no green");
    check(!glowing(full) && !glowing(empty), "no glow without attack");
    check(glowing(attacked), "magenta glow while under attack");
    check(ringLit(attacked, 90.0f) && !ringLit(attacked, 250.0f), "glow does not change the ring");

    std::vector<unsigned char> goldFull;
    std::vector<unsigned char> goldOther;
    drawBadgePixels(goldFull, 1, 1.0f, false);
    drawBadgePixels(goldOther, 1, 0.0f, true);
    check(goldFull == goldOther, "gold badge ignores heart health and glow");
    std::vector<unsigned char> again;
    drawBadgePixels(again, 0, 1.0f, false);
    check(again == full, "drawing is deterministic");
}

int main()
{
    checkMapping();
    checkNotifyRule();
    checkGlowTimer();
    checkServer();
    checkSaveAndLoad();
    checkDrawing();
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures == 0 ? 0 : 1;
}
'''

temple_methods = 'const double RoomDungeonTemple::HEART_HP_PER_TILE = ' + \
    temple.split('const double RoomDungeonTemple::HEART_HP_PER_TILE = ')[1].split(';')[0] + ';\n'
temple_methods += '\n'.join(function(temple, signature) for signature in (
    'double RoomDungeonTemple::getHP(', 'double RoomDungeonTemple::getHeartMaxHP(', 'double RoomDungeonTemple::getHeartHealthFraction(',
    'void RoomDungeonTemple::exportToStream(', 'bool RoomDungeonTemple::importFromStream('))
badge_start = gui.index('const int BADGE_SIZE = 128;')
badge_code = ('const int BADGE_SIZE = 128;\n' + function(gui, 'bool isInBadgeSymbol(') + '\n'
              + function(gui, 'void drawBadgePixels(') + '\n')
probe = (probe.replace('RULES_HEADER', rules.as_posix())
         .replace('TEMPLE_METHODS', temple_methods)
         .replace('SERVER_FUNCTION', function(server, 'void notifyHeartHealth('))
         .replace('BADGE_CODE', badge_code))

# Wiring of the notification, the client and the game mode
enum_body = notification_header[notification_header.index('enum class ServerNotificationType'):]
enum_body = enum_body[:enum_body.index('};')]
enumerators = re.findall(r'^\s*([A-Za-z_]\w*)\s*,?\s*(?://.*)?$', enum_body, re.M)
assert enumerators[-1] == 'heartHealth', enumerators[-3:]
assert enumerators[-2] == 'levelStatistics' and enumerators[-3] == 'playerDefeated'
assert enumerators.count('heartHealth') == 1
assert 'case ServerNotificationType::heartHealth:\n            return "heartHealth";' in notification_source
case = client[client.index('case ServerNotificationType::heartHealth:'):]
case = case[:case.index('break;')]
assert 'packetReceived >> healthFraction >> underAttack >> heartHP >> heartMaxHP' in case
assert 'mHeartBadge.receive(healthFraction, underAttack);' in case
assert 'mHeartBadge.setPoints(heartHP, heartMaxHP);' in case
assert 'float healthFraction = 1.0f;' in case and 'bool underAttack = false;' in case
accepted = client[client.index('case ServerNotificationType::clientAccepted:'):]
accepted = accepted[:accepted.index('break;')]
assert 'mHeartBadge = HeartHealthRing::BadgeState();' in accepted
assert 'HeartHealthRing::BadgeState mHeartBadge;' in client_header
frame = function(game_mode, 'void GameMode::onFrameStarted(')
assert 'ODClient::getSingleton().getHeartBadge()' in frame
assert 'heartBadge.update(evt.timeSinceLastFrame);' in frame
assert 'heartBadge.takeDirty()' in frame
assert 'getGui().updateHeartBadge(heartBadge.mFraction, heartBadge.mGlow);' in frame
assert 'heartIcon->setTooltipText(heartText.str());' in frame
assert 'void updateHeartBadge(float healthFraction, bool underAttack);' in gui_header
assert 'ODServer::getSingleton().queueServerNotification(serverNotification);\n\n        notifyHeartHealth(gameMap, sock, player);' in server
# Badge geometry and the resource strip are untouched; the heart badge says what it shows
assert layout.count('OpenDungeonsIcons/ManaBadge') == 1 and layout.count('OpenDungeonsIcons/GoldBadge') == 1
assert layout.count('<Property name="TooltipText" value="Your Mana" />') == 1
assert layout.count('<Property name="TooltipText" value="Dungeon Heart Health" />') == 1
assert '{{0,-4},{0,-2},{0,60},{0,62}}' in layout
created = function(gui, 'void createNavigationImages(')
assert 'drawBadgePixels(pixels, badge, 1.0f, false);' in created
assert 'badgeImage.setArea(CEGUI::Rectf(0, 0, badgeSize, badgeSize));' in created
assert 'getTexture("ManaBadge")' in function(gui, 'void Gui::updateHeartBadge(')
assert 'getHeartHealthFraction() const;' in temple_header
print('WIRING OK')


def run(command, cwd):
    """Windows may block a freshly compiled fixture (WinError 4551): try up to 4 times."""
    for attempt in range(4):
        try:
            return subprocess.run(command, cwd=cwd, check=True)
        except OSError as error:
            print('attempt', attempt + 1, 'blocked:', error)
            time.sleep(2)
    raise SystemExit('fixture blocked by application control after 4 attempts')


with tempfile.TemporaryDirectory(prefix='odp-heart-ring-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'], cwd=work, check=True)
    run([str(work / 'check.exe')], work)
