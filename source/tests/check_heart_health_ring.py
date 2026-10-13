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
struct ODApplication {static double turnsPerSecond;};
double ODApplication::turnsPerSecond = 1.4;
struct ODSocketClient {
    float mSent;
    double mHPSent;
    int64_t mTurn;
    ODSocketClient() : mSent(-1.0f), mHPSent(-1.0), mTurn(-1) {}
    int64_t getHeartMessageTurn() const {return mTurn;}
    void setHeartMessageTurn(int64_t t) {mTurn = t;}
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
    int64_t mTurn;
    GameMap() : mTurn(0) {}
    int64_t getTurnNumber() const {return mTurn;}
    const std::vector<Room*>& getRooms() const {return mRooms;}
    bool isInEditorMode() const {return false;}
};
struct RoomDungeonTemple : Room {
    GameMap* mMap;
    double mHeartHP;
    RoomDungeonTemple(GameMap* map, Seat* seat) : Room(RoomType::dungeonTemple, seat), mMap(map), mHeartHP(-1.0) {}
    GameMap* getGameMap() const {return mMap;}
    static const double HEART_MAX_HP;
    static const double HEART_HEAL_PER_SECOND;
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
    return g - r > 40;
}

// A spoke is a bar of warm bronze: red over green over blue and clearly lit, neither a green gem nor the dark socket
bool spokeAt(const std::vector<unsigned char>& pixels, float angle)
{
    int r, g, b;
    pixelAt(pixels, angle, 23.0f, r, g, b);
    return r > 60 && r > g && g > b;
}

bool litAt(float degrees, float fraction)
{
    const float radians = degrees * 3.14159265f / 180.0f;
    return HeartHealthRing::isRingLit(std::sin(radians), -std::cos(radians), fraction);
}

bool glowing(const std::vector<unsigned char>& pixels)
{
    int r, g, b;
    pixelAt(pixels, 0.0f, 19.0f, r, g, b);
    return r > 150 && b > 100 && g < 80;
}

void checkMapping()
{
    const float mids[6] = {30.0f, 90.0f, 150.0f, 210.0f, 270.0f, 330.0f};
    int lit0 = 0, lit17 = 0, lit50 = 0, lit100 = 0;
    for(int k = 0; k < 6; ++k)
    {
        lit0 += litAt(mids[k], 0.0f);
        lit17 += litAt(mids[k], 0.17f);
        lit50 += litAt(mids[k], 0.5f);
        lit100 += litAt(mids[k], 1.0f);
    }
    check(lit0 == 0, "0 percent lights no segment");
    check(lit17 == 1 && litAt(30.0f, 0.17f), "17 percent lights exactly the first segment");
    check(lit50 == 3 && litAt(150.0f, 0.5f) && !litAt(210.0f, 0.5f), "50 percent lights three segments clockwise from the top");
    check(lit100 == 6, "100 percent lights all six segments");
    check(litAt(50.0f, 1.0f / 6.0f + 0.0001f) && !litAt(90.0f, 1.0f / 6.0f + 0.0001f), "one sixth fills the first segment and nothing more");
    check(litAt(75.0f, 0.25f) && !litAt(100.0f, 0.25f) && litAt(30.0f, 0.25f) && !litAt(150.0f, 0.25f),
        "a partly covered segment is filled clockwise up to its covered part");
    check(litAt(65.0f, 0.17f) == false && litAt(91.0f, 0.34f) && !litAt(135.0f, 0.34f), "partial segments at 17 and 34 percent");
    bool spokes = true;
    for(int k = 0; k < 6; ++k)
    {
        spokes = spokes && !litAt(k * 60.0f, 1.0f) && !litAt(k * 60.0f + 2.0f, 1.0f) && !litAt(k * 60.0f - 2.0f, 1.0f);
        spokes = spokes && HeartHealthRing::isSpoke(std::sin(k * 1.0471976f), -std::cos(k * 1.0471976f));
    }
    check(spokes, "a spoke separates every two segments and is never lit");
    check(!HeartHealthRing::isSpoke(std::sin(0.5236f), -std::cos(0.5236f)), "the middle of a segment is no spoke");
    check(lit0 == 0 && !litAt(30.0f, -0.5f) && !litAt(30.0f, std::numeric_limits<float>::quiet_NaN()), "negative fraction and NaN show nothing");
    check(litAt(330.0f, 7.0f) && litAt(30.0f, 7.0f), "fraction above 1 clamps to a full ring");
    bool monotone = true;
    for(int i = 0; i < 100; ++i)
        for(int degrees = 0; degrees < 360; ++degrees)
            monotone = monotone && (!litAt(static_cast<float>(degrees), i / 100.0f) || litAt(static_cast<float>(degrees), (i + 1) / 100.0f));
    check(monotone, "a lit pixel never goes dark when the health rises");
    check(HeartHealthRing::healthPercent(1700.0, 10000.0) == 17, "1700 of 10000 is 17 percent");
    check(HeartHealthRing::healthPercent(0.0, 10000.0) == 0 && HeartHealthRing::healthPercent(10000.0, 10000.0) == 100, "0 and 100 percent");
    check(HeartHealthRing::healthPercent(5000.0, 10000.0) == 50, "5000 of 10000 is 50 percent");
    check(HeartHealthRing::healthPercent(1649.0, 10000.0) == 16 && HeartHealthRing::healthPercent(1650.0, 10000.0) == 17,
        "the percentage is rounded to the nearest");
    check(HeartHealthRing::healthPercent(20000.0, 10000.0) == 100 && HeartHealthRing::healthPercent(5.0, 0.0) == 0, "percentage is clamped");
}

void checkNotifyRule()
{
    check(HeartHealthRing::shouldNotify(-1.0f, 1.0f), "first message always sent");
    check(HeartHealthRing::shouldNotify(-1.0f, 0.0f), "first message sent even for a destroyed heart");
    check(!HeartHealthRing::shouldNotify(1.0f, 1.0f), "no change is not sent");
    check(!HeartHealthRing::shouldNotify(1.0f, 0.992f), "less than one point is not sent");
    check(HeartHealthRing::shouldNotify(1.0f, 0.99f), "exactly one point is sent");
    check(HeartHealthRing::shouldNotify(0.5f, 0.489f), "more than one point is sent");
    check(!HeartHealthRing::isHpMessageDue(0, 1.4) && !HeartHealthRing::isHpMessageDue(1, 1.4), "an HP-only message is not due within a second");
    check(HeartHealthRing::isHpMessageDue(2, 1.4) && HeartHealthRing::isHpMessageDue(1, 1.0), "an HP-only message is due after a second");
    check(HeartHealthRing::isHpMessageDue(-5, 1.4), "a turn counter that went backwards counts as due");
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
            && near(n->mPacket.values[2], 10000.0) && near(n->mPacket.values[3], 10000.0),
            "start message is a full heart of 10000 HP that is not under attack");
    }
    queue.mQueue.clear();
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "nothing is sent while the health does not change");

    // Whole HP changes without a percentage point: at most one message per second (1.4 turns)
    map.mTurn = 1;
    ownHeart.mHeartHP = 9920.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "a change below one point within a second of the last message is held back");
    map.mTurn = 2;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[2], 9920.0),
        "a change below one point is sent once a second has passed, so that the tooltip shows the exact HP");
    queue.mQueue.clear();
    ownHeart.mHeartHP = 9920.4;
    map.mTurn = 4;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "a change below one whole HP is not sent");
    ownHeart.mHeartHP = 9880.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1, "a change of one point is sent at once, even within a second of the last message");
    if(queue.mQueue.size() == 1)
        check(near(queue.mQueue[0]->mPacket.values[0], 0.988) && queue.mQueue[0]->mPacket.values[1] == 1.0,
            "a hit is sent with its fraction and underAttack");
    queue.mQueue.clear();

    ownHeart.mHeartHP = 1000.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[0], 0.1), "10 percent heart is sent");
    queue.mQueue.clear();
    ownHeart.mHeartHP = 960.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "0.4 point below the last message is held back within the second");
    map.mTurn = 5;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.empty(), "and still held back one turn (0.7 s) after the last message");
    map.mTurn = 6;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1 && near(queue.mQueue[0]->mPacket.values[2], 960.0),
        "0.4 point below the last message is sent for the tooltip after a second");
    queue.mQueue.clear();

    // Healing: the whole HP grows every turn, so the tooltip is refreshed every second turn only
    ownHeart.mHeartHP = 5000.0;
    map.mTurn = 7;
    notifyHeartHealth(&map, &socket, &human);
    queue.mQueue.clear();
    int healMessages = 0;
    bool healNeverAttacked = true;
    for(int turn = 8; turn <= 17; ++turn)
    {
        map.mTurn = turn;
        ownHeart.mHeartHP += 2.5 / 1.4;
        queue.mQueue.clear();
        notifyHeartHealth(&map, &socket, &human);
        healMessages += static_cast<int>(queue.mQueue.size());
        if(!queue.mQueue.empty())
            healNeverAttacked = healNeverAttacked && queue.mQueue[0]->mPacket.values[1] == 0.0;
    }
    check(healMessages == 5, "healing sends one message every second turn, not one per turn");
    check(healNeverAttacked, "healing is never reported as an attack");
    queue.mQueue.clear();

    ownHeart.mHeartHP = 18.0;
    map.mTurn = 18;
    notifyHeartHealth(&map, &socket, &human);
    queue.mQueue.clear();
    ownHeart.mHeartHP = 0.0;
    notifyHeartHealth(&map, &socket, &human);
    check(queue.mQueue.size() == 1, "a destroyed heart is sent at once, even within a second of the last message");
    if(queue.mQueue.size() == 1)
        check(near(queue.mQueue[0]->mPacket.values[0], 0.0) && queue.mQueue[0]->mPacket.values[1] == 1.0,
            "destroyed heart message has fraction 0");
    queue.mQueue.clear();
    map.mTurn = 30;
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
    saved.mHeartHP = 1000.0;
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
    half.mTiles = 25;
    half.mHeartHP = 5000.0;
    check(near(half.getHeartMaxHP(), 10000.0) && near(half.getHeartHealthFraction(), 0.5),
        "the ring follows the heart's own fixed 10000 health, not the floor durability or the number of tiles");
}

void checkDrawing()
{
    std::vector<unsigned char> full;
    std::vector<unsigned char> half;
    std::vector<unsigned char> empty;
    std::vector<unsigned char> attacked;
    std::vector<unsigned char> seventeen;
    std::vector<unsigned char> quarter;
    drawBadgePixels(full, 0, 1.0f, false);
    drawBadgePixels(seventeen, 0, 0.17f, false);
    drawBadgePixels(quarter, 0, 0.25f, false);
    drawBadgePixels(half, 0, 0.5f, false);
    drawBadgePixels(empty, 0, 0.0f, false);
    drawBadgePixels(attacked, 0, 0.5f, true);
    check(full.size() == static_cast<size_t>(BADGE_SIZE * BADGE_SIZE * 4), "badge has its full size");
    const float mids[6] = {30.0f, 90.0f, 150.0f, 210.0f, 270.0f, 330.0f};
    bool allLit = true;
    bool noneLit = true;
    bool allSpokes = true;
    for(int k = 0; k < 6; ++k)
    {
        allLit = allLit && ringLit(full, mids[k]);
        noneLit = noneLit && !ringLit(empty, mids[k]);
        allSpokes = allSpokes && spokeAt(full, k * 60.0f) && spokeAt(empty, k * 60.0f) && spokeAt(half, k * 60.0f);
    }
    check(allLit, "full ring is green in all six segments");
    check(noneLit, "empty ring shows no green");
    check(allSpokes, "the six spokes are bronze bars at 100, 50 and 0 percent");
    check(!ringLit(full, 0.0f) && !ringLit(full, 60.0f) && !ringLit(full, 180.0f), "the spokes are not green");
    check(ringLit(seventeen, 30.0f) && ringLit(seventeen, 50.0f) && !ringLit(seventeen, 90.0f) && !ringLit(seventeen, 200.0f),
        "17 percent shows exactly one green segment");
    check(ringLit(half, 30.0f) && ringLit(half, 90.0f) && ringLit(half, 150.0f)
        && !ringLit(half, 210.0f) && !ringLit(half, 270.0f) && !ringLit(half, 330.0f), "50 percent shows three green segments from the top");
    check(ringLit(quarter, 30.0f) && ringLit(quarter, 70.0f) && !ringLit(quarter, 110.0f) && !ringLit(quarter, 150.0f),
        "a partly covered segment is drawn partly filled");
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

    // The symbols: a red heart muscle in the middle of the well, a gold coin rim with a skull inside
    int r, g, b;
    pixelAt(full, 0.0f, 3.0f, r, g, b);
    check(r > 100 && r > 3 * g && r > 3 * b, "the heart body is red in the middle of the well");
    pixelAt(full, 90.0f, 17.0f, r, g, b);
    check(r < 60 && g < 60, "outside the heart the well is dark stone");
    pixelAt(goldFull, 315.0f, 17.2f, r, g, b);
    check(r > 130 && r > g && g > b, "the coin rim is warm gold");
    pixelAt(goldFull, 0.0f, 9.0f, r, g, b);
    check(r > 90 && r > g && g > b, "the coin field is warm gold");
    pixelAt(goldFull, 294.6f, 3.85f, r, g, b);
    check(r < 70, "the eye socket of the skull is dark");
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

temple_methods = temple[temple.index('const double RoomDungeonTemple::HEART_MAX_HP'):temple.index('RoomDungeonTemple::RoomDungeonTemple(')]
temple_methods += '\n'.join(function(temple, signature) for signature in (
    'double RoomDungeonTemple::getHP(', 'double RoomDungeonTemple::getHeartMaxHP(', 'double RoomDungeonTemple::getHeartHealthFraction(',
    'void RoomDungeonTemple::exportToStream(', 'bool RoomDungeonTemple::importFromStream('))
badge_start = gui.index('const int BADGE_SIZE = 128;')
badge_helpers = ('float badgeClamp(', 'void badgeMix(', 'void badgeSet(', 'float badgeNoise(', 'float badgeSmoothstep(', 'float badgeSmoothMin(',
                 'float badgeHash(', 'float badgeValueNoise(', 'float badgeFbm(', 'float badgeCircle(', 'float badgeTaper(', 'float badgeBox(',
                 'float badgeRound(', 'float badgeHeartDistance(', 'float badgeHeartVeins(', 'float badgeHeartHeight(', 'float badgeSkullDistance(',
                 'float badgeCoinHeight(', 'void badgeNormal(', 'float badgeCavity(', 'void badgeFrame(', 'void badgeGemRing(', 'void badgeBeadRing(',
                 'void badgeHeartWell(', 'void badgeCoinWell(', 'void drawBadgePixels(')
badge_code = 'const int BADGE_SIZE = 128;\n' + '\n'.join(function(gui, signature) for signature in badge_helpers) + '\n'
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
assert 'float healthFraction;' in case and 'bool underAttack;' in case
accepted = client[client.index('case ServerNotificationType::clientAccepted:'):]
accepted = accepted[:accepted.index('break;')]
assert 'mHeartBadge = HeartHealthRing::BadgeState();' in accepted
assert 'HeartHealthRing::BadgeState mHeartBadge;' in client_header
frame = function(game_mode, 'void GameMode::onFrameStarted(')
assert 'ODClient::getSingleton().getHeartBadge()' in frame
assert 'heartBadge.update(evt.timeSinceLastFrame);' in frame
assert 'heartBadge.takeDirty()' in frame
assert 'getGui().updateHeartBadge(heartBadge.mFraction, heartBadge.mGlow);' in frame
assert 'HeartHealthRing::healthPercent(heartBadge.mHP, heartBadge.mMaxHP)' in frame
assert '"Dungeon heart at "' in frame and '" %. Right-click moves the view to the heart."' in frame
assert 'setTooltipText' not in frame and 'Dungeon Heart:' not in frame
click = function(game_mode, 'bool GameMode::clickHeartBadge(')
assert 'CEGUI::RightButton' in click and 'focusRoom(RoomType::dungeonTemple);' in click and 'cameraInputBlocked()' in click
assert 'getChild(Gui::DISPLAY_MANA)->getChild("Icon")->subscribeEvent(' in game_mode and '&GameMode::clickHeartBadge' in game_mode
assert 'void updateHeartBadge(float healthFraction, bool underAttack);' in gui_header
assert 'ODServer::getSingleton().queueServerNotification(serverNotification);\n\n        notifyHeartHealth(gameMap, sock, player);' in server
# Badge geometry and the resource strip are untouched; the heart badge says what it shows
assert layout.count('OpenDungeonsIcons/ManaBadge') == 1 and layout.count('OpenDungeonsIcons/GoldBadge') == 1
assert layout.count('<Property name="TooltipText" value="Your Mana" />') == 1
assert layout.count('<Property name="TooltipText" value="Dungeon heart health bar" />') == 1
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
