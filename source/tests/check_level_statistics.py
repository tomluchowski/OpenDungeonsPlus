"""Exercise the production level statistics counting hooks and the levelStatistics notification without a game."""
from pathlib import Path
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]


def read(relative):
    return (repo / relative).read_text()


seat_source = read('source/game/Seat.cpp')
creature_source = read('source/entities/Creature.cpp')
room_source = read('source/rooms/Room.cpp')
torture_source = read('source/rooms/RoomTorture.cpp')
workshop_source = read('source/rooms/RoomWorkshop.cpp')
temple_source = read('source/rooms/RoomDungeonTemple.cpp')
player_source = read('source/game/Player.cpp')
client_source = read('source/network/ODClient.cpp')
client_header = read('source/network/ODClient.h')
notification_header = read('source/network/ServerNotification.h')
notification_source = read('source/network/ServerNotification.cpp')


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def line_with(text, token):
    """The production line (without indentation) that contains the token, exactly once."""
    lines = [line.strip() for line in text.splitlines() if token in line]
    assert len(lines) == 1, (token, lines)
    return lines[0]


# ---------------------------------------------------------------------------
# Probe 1: kill attribution (real Seat::recordCreatureKill and the real first
# half of Creature::takeDamage) and the increments of the other counters.
# ---------------------------------------------------------------------------
kill_method = function(seat_source, 'void Seat::recordCreatureKill(')

take_damage = function(creature_source, 'double Creature::takeDamage(')
cut = take_damage.index('    if(!getIsOnServerMap())')
take_damage_kill_part = take_damage[:cut] + '    return damageDone;\n}\n'

# Production statements that are one-liners inside larger functions
capture_start = room_source.index('    // Counts as captured when the claimer takes the last tile')
capture_end = room_source.index('    mCoveredTilesDestroyed.push_back(tile);', capture_start)
capture_statement = room_source[capture_start:capture_end]
convert_line = line_with(torture_source, 'mCreaturesConverted++')
craft_line = line_with(workshop_source, 'mItemsMade++')
heart_line = line_with(temple_source, 'mKeepersDefeated++')

kills_probe = r'''
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_INF(x)
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
struct SeatStatistics
{
    uint32_t mKeepersDefeated = 0;
    uint32_t mCreaturesKilled = 0;
    uint32_t mHeroesDestroyed = 0;
    uint32_t mRoomsCaptured = 0;
    uint32_t mItemsMade = 0;
    uint32_t mCreaturesConverted = 0;
};
struct Seat
{
    Seat(int id, int team, const std::string& faction) : mId(id), mTeamId(team), mFaction(faction) {}
    int getTeamId() const { return mTeamId; }
    const std::string& getFaction() const { return mFaction; }
    bool isAlliedSeat(const Seat* seat) const { return getTeamId() == seat->getTeamId(); }
    SeatStatistics& getStatistics() { return mStatistics; }
    void recordCreatureKill(const Seat* victimSeat);
    int mId;
    int mTeamId;
    std::string mFaction;
    SeatStatistics mStatistics;
};
KILL_METHOD
struct Tile {};
struct GameEntity
{
    GameEntity(Seat* seat) : mSeat(seat) {}
    Seat* getSeat() { return mSeat; }
    std::string getName() { return "entity"; }
    Seat* mSeat;
};
struct CreatureDefinition
{
    bool isWorker() const { return mWorker; }
    bool mWorker = false;
};
struct ConfigManager
{
    static ConfigManager& getSingleton() { static ConfigManager config; return config; }
    int getNbTurnsKoCreatureAttacked() { return 5; }
};
struct Creature : public GameEntity
{
    Creature(Seat* seat, double hp) : GameEntity(seat), mHp(hp), mNbTurnsWithoutBattle(3), mKoTurnCounter(0), mDeaths(0) {}
    bool isAlive() const { return mHp > 0; }
    double getPhysicalDefense() { return 0.0; }
    double getMagicalDefense() { return 0.0; }
    double getElementDefense() { return 0.0; }
    const CreatureDefinition* getDefinition() { return &mDefinition; }
    void dropCarriedEquipment() {}
    void computeCreatureOverlayHealthValue() {}
    void computeCreatureOverlayMoodValue() {}
    void fireEntityDead() { ++mDeaths; }
    double takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage, double magicalDamage,
        double elementDamage, Tile* tileTakingDamage, bool ko);
    double mHp;
    int mNbTurnsWithoutBattle;
    int mKoTurnCounter;
    int mDeaths;
    CreatureDefinition mDefinition;
};
TAKE_DAMAGE
struct MockRoom
{
    MockRoom(Seat* seat) : mSeat(seat) {}
    Seat* getSeat() { return mSeat; }
    void capture(Seat* seat);
    void convert(Seat* seat);
    void craft();
    std::vector<int> mCoveredTiles;
    Seat* mSeat;
};
void MockRoom::capture(Seat* seat)
{
CAPTURE_STATEMENT}
void MockRoom::convert(Seat* seat)
{
    CONVERT_LINE
}
void MockRoom::craft()
{
    CRAFT_LINE
}
struct HeartMock
{
    void kill(Seat* attackerSeat)
    {
        HEART_LINE
    }
};
GameEntity* noEntity() { return nullptr; }

void testKills()
{
    Seat keeperA(1, 1, "Keeper");
    Seat keeperAAlly(2, 1, "Keeper");
    Seat keeperB(3, 2, "Keeper");
    Seat heroSeat(4, 3, "Hero");
    Seat rogue(0, 0, "Keeper");
    GameEntity attackerA(&keeperA);

    {   // An enemy keeper creature dies from the attacker's blow
        Creature victim(&keeperB, 10.0);
        victim.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mCreaturesKilled == 1, "killing an enemy creature counts as creature killed");
        check(keeperA.mStatistics.mHeroesDestroyed == 0, "a keeper creature is not a hero");
        check(victim.mDeaths == 1, "death listeners still notified");
        check(keeperB.mStatistics.mCreaturesKilled == 0, "the victim seat gets nothing");
    }
    {   // A creature of a Hero faction seat counts only as hero
        Creature hero(&heroSeat, 10.0);
        hero.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mHeroesDestroyed == 1, "killing a hero creature counts as hero destroyed");
        check(keeperA.mStatistics.mCreaturesKilled == 1, "hero kill does not touch the creature row");
    }
    {   // Allied creature (same team) is not counted
        Creature ally(&keeperAAlly, 10.0);
        ally.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mCreaturesKilled == 1 && keeperA.mStatistics.mHeroesDestroyed == 1, "allied kill is not counted");
        check(ally.mDeaths == 1, "allied death is still processed");
    }
    {   // Rogue seat victims count as creatures killed by their killer
        Creature rogueCreature(&rogue, 10.0);
        rogueCreature.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mCreaturesKilled == 2, "rogue seat creature counts as creature killed");
    }
    {   // A second hit on the corpse does not count again
        Creature victim(&keeperB, 5.0);
        victim.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        victim.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mCreaturesKilled == 3, "a dead creature is counted only once");
    }
    {   // A creature that survives, and a KO, are not kills
        Creature victim(&keeperB, 100.0);
        victim.takeDamage(&attackerA, 5.0, 0.0, 0.0, 0.0, nullptr, false);
        check(keeperA.mStatistics.mCreaturesKilled == 3, "non lethal damage counts nothing");
        Creature knockedOut(&keeperB, 5.0);
        knockedOut.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, true);
        check(knockedOut.isAlive() && knockedOut.mDeaths == 0 && keeperA.mStatistics.mCreaturesKilled == 3, "a KO is not a kill");
        Creature worker(&keeperB, 5.0);
        worker.mDefinition.mWorker = true;
        worker.takeDamage(&attackerA, 999.0, 0.0, 0.0, 0.0, nullptr, true);
        check(!worker.isAlive() && keeperA.mStatistics.mCreaturesKilled == 4, "a worker is never KO'd and counts as creature killed");
    }
    {   // Attackers without a seat or without an entity do not crash and count nothing
        GameEntity seatless(nullptr);
        Creature victim(&keeperB, 5.0);
        victim.takeDamage(&seatless, 999.0, 0.0, 0.0, 0.0, nullptr, false);
        Creature victim2(&keeperB, 5.0);
        victim2.takeDamage(noEntity(), 999.0, 0.0, 0.0, 0.0, nullptr, false);
        check(victim.mDeaths == 1 && victim2.mDeaths == 1 && keeperA.mStatistics.mCreaturesKilled == 4, "unknown attacker counts nothing");
    }
    {   // The helper on its own
        Seat killer(7, 5, "Keeper");
        killer.recordCreatureKill(nullptr);
        killer.recordCreatureKill(&killer);
        check(killer.mStatistics.mCreaturesKilled == 0 && killer.mStatistics.mHeroesDestroyed == 0, "no victim seat and own seat count nothing");
    }
}

void testOtherCounters()
{
    Seat owner(1, 1, "Keeper");
    Seat ally(2, 1, "Keeper");
    Seat enemy(3, 2, "Keeper");
    Seat rogue(0, 0, "Keeper");
    {   // Room capture: only when the last tile of a non-allied room is taken
        MockRoom room(&enemy);
        room.mCoveredTiles.push_back(1);
        room.capture(&owner);
        check(owner.mStatistics.mRoomsCaptured == 0, "room with tiles left is not captured yet");
        room.mCoveredTiles.clear();
        room.capture(&owner);
        check(owner.mStatistics.mRoomsCaptured == 1, "taking the last tile of an enemy room captures it");
        MockRoom rogueRoom(&rogue);
        rogueRoom.capture(&owner);
        check(owner.mStatistics.mRoomsCaptured == 2, "a room of the rogue seat counts as captured");
        MockRoom allyRoom(&ally);
        allyRoom.capture(&owner);
        check(owner.mStatistics.mRoomsCaptured == 2, "an allied room is not counted");
        MockRoom ownerless(nullptr);
        ownerless.capture(&owner);
        check(owner.mStatistics.mRoomsCaptured == 2, "a room without seat is not counted");
    }
    {   // Conversion and crafting count for the room's seat
        MockRoom torture(&owner);
        torture.convert(&enemy);
        torture.convert(&enemy);
        check(owner.mStatistics.mCreaturesConverted == 2 && enemy.mStatistics.mCreaturesConverted == 0, "each conversion counts for the torturing seat");
        MockRoom workshop(&owner);
        workshop.craft();
        check(owner.mStatistics.mItemsMade == 1 && enemy.mStatistics.mItemsMade == 0, "each crafted trap counts for the workshop seat");
    }
    {   // Heart kill counts for the attacker seat
        HeartMock heart;
        heart.kill(&enemy);
        check(enemy.mStatistics.mKeepersDefeated == 1 && owner.mStatistics.mKeepersDefeated == 0, "heart kill counts for the attacker seat");
    }
}

int main()
{
    testKills();
    testOtherCounters();
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
kills_probe = (kills_probe.replace('KILL_METHOD', kill_method)
    .replace('TAKE_DAMAGE', take_damage_kill_part)
    .replace('CAPTURE_STATEMENT', capture_statement)
    .replace('CONVERT_LINE', convert_line)
    .replace('CRAFT_LINE', craft_line)
    .replace('HEART_LINE', heart_line.replace('attacker->getSeat()->', 'attackerSeat->')))

# ---------------------------------------------------------------------------
# Probe 2: the real Player::notifyNoMoreDungeonTemple packet content and order.
# ---------------------------------------------------------------------------
notify_method = function(player_source, 'void Player::notifyNoMoreDungeonTemple(')

notify_probe = r'''
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#define OD_LOG_INF(x)
namespace Helper {template<typename T> std::string toString(T v){return std::to_string(v);}}
enum class ServerNotificationType {chatServer, playerDefeated, levelStatistics};
enum class EventShortNoticeType {majorGameEvent};
enum class RoomType {dungeonTemple};
enum class SoundRelativeKeeperStatements {Lost, Defeat, AllyDefeated};
struct ODPacket
{
    std::vector<std::string> tokens;
    ODPacket& operator<<(const char*) { tokens.push_back("text"); return *this; }
    ODPacket& operator<<(EventShortNoticeType) { return *this; }
    ODPacket& operator<<(int32_t v) { tokens.push_back("i" + std::to_string(v)); return *this; }
    ODPacket& operator<<(uint32_t v) { tokens.push_back("u" + std::to_string(v)); return *this; }
    ODPacket& operator<<(bool v) { tokens.push_back(v ? "btrue" : "bfalse"); return *this; }
};
struct ODApplication { static double turnsPerSecond; };
double ODApplication::turnsPerSecond = 4.0;
struct SeatStatistics
{
    uint32_t mKeepersDefeated = 0;
    uint32_t mCreaturesKilled = 0;
    uint32_t mHeroesDestroyed = 0;
    uint32_t mRoomsCaptured = 0;
    uint32_t mItemsMade = 0;
    uint32_t mCreaturesConverted = 0;
};
struct Player;
struct ServerNotification
{
    ServerNotification(ServerNotificationType t, Player* p) : type(t), player(p) {}
    ServerNotificationType type;
    Player* player;
    ODPacket mPacket;
};
struct ODServer
{
    static ODServer& getSingleton() { static ODServer server; return server; }
    void queueServerNotification(ServerNotification* n) { queue.push_back(n); }
    std::vector<ServerNotification*> queue;
};
struct Seat;
struct Room
{
    Room(Seat* s) : seat(s) {}
    Seat* getSeat() { return seat; }
    Seat* seat;
};
struct GameMapMock;
struct Player
{
    Player(Seat* s, bool h) : mSeat(s), mIsHuman(h), mHasLost(false), mGameMap(nullptr),
        mConquerorSeatId(-1), mDefeatHeartTileX(-1), mDefeatHeartTileY(-1) {}
    Seat* getSeat() { return mSeat; }
    bool getIsHuman() const { return mIsHuman; }
    void notifyNoMoreDungeonTemple();
    Seat* mSeat;
    bool mIsHuman;
    bool mHasLost;
    GameMapMock* mGameMap;
    int32_t mConquerorSeatId;
    int32_t mDefeatHeartTileX;
    int32_t mDefeatHeartTileY;
};
struct Seat
{
    Seat(int i, int t) : id(i), team(t), player(nullptr) {}
    int getId() const { return id; }
    Player* getPlayer() { return player; }
    bool isAlliedSeat(Seat* s) { return s && team == s->team; }
    bool isRogueSeat() const { return id == 0; }
    const SeatStatistics& getStatistics() const { return stats; }
    int id;
    int team;
    Player* player;
    SeatStatistics stats;
};
struct GameMapMock
{
    GameMapMock() : turn(0) {}
    int64_t getTurnNumber() const { return turn; }
    std::vector<Room*> getRoomsByType(RoomType) { return temples; }
    std::vector<Seat*>& getSeats() { return seats; }
    void fireRelativeSound(std::vector<Seat*>&, SoundRelativeKeeperStatements) {}
    int64_t turn;
    std::vector<Seat*> seats;
    std::vector<Room*> temples;
};
NOTIFY_METHOD
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
ServerNotification* findLast(Player* p, ServerNotificationType t)
{
    ServerNotification* found = nullptr;
    for(ServerNotification* n : ODServer::getSingleton().queue)
    {
        if(n->player == p && n->type == t)
            found = n;
    }
    return found;
}
int countOf(Player* p, ServerNotificationType t)
{
    int count = 0;
    for(ServerNotification* n : ODServer::getSingleton().queue)
    {
        if(n->player == p && n->type == t)
            ++count;
    }
    return count;
}
std::string join(const ServerNotification* n)
{
    std::string text;
    if(n == nullptr)
        return text;
    for(const std::string& token : n->mPacket.tokens)
        text += token + " ";
    return text;
}

int main()
{
    ODServer& server = ODServer::getSingleton();
    {   // Human loses; rogue seat, a seat without player and a bot are around
        server.queue.clear();
        GameMapMock map;
        map.turn = 130;
        Seat rogue(0, 0), noPlayer(9, 9), a(1, 1), bot(2, 2), other(3, 3);
        Player rogueBot(&rogue, false), pa(&a, true), pbot(&bot, false), pother(&other, true);
        rogue.player = &rogueBot;
        a.player = &pa;
        bot.player = &pbot;
        other.player = &pother;
        pa.mGameMap = &map;
        map.seats = {&rogue, &noPlayer, &a, &bot, &other};
        a.stats.mKeepersDefeated = 1; a.stats.mCreaturesKilled = 2; a.stats.mHeroesDestroyed = 3;
        a.stats.mRoomsCaptured = 4; a.stats.mItemsMade = 5; a.stats.mCreaturesConverted = 6;
        bot.stats.mKeepersDefeated = 10; bot.stats.mCreaturesKilled = 20; bot.stats.mHeroesDestroyed = 30;
        bot.stats.mRoomsCaptured = 40; bot.stats.mItemsMade = 50; bot.stats.mCreaturesConverted = 60;
        rogue.stats.mCreaturesKilled = 99;
        pa.notifyNoMoreDungeonTemple();
        pa.notifyNoMoreDungeonTemple();

        std::vector<ServerNotificationType> order;
        for(ServerNotification* n : server.queue)
        {
            if(n->player == &pa)
                order.push_back(n->type);
        }
        check(order.size() == 3 && order[0] == ServerNotificationType::chatServer
            && order[1] == ServerNotificationType::playerDefeated
            && order[2] == ServerNotificationType::levelStatistics, "statistics are sent right after playerDefeated");
        check(countOf(&pa, ServerNotificationType::levelStatistics) == 1, "statistics sent exactly once");
        check(countOf(&pbot, ServerNotificationType::levelStatistics) == 0
            && countOf(&pother, ServerNotificationType::levelStatistics) == 0, "only the defeated human gets the statistics");
        ServerNotification* n = findLast(&pa, ServerNotificationType::levelStatistics);
        std::string expected = "i32 bfalse i3 "
            "i1 u1 u2 u3 u4 u5 u6 "
            "i2 u10 u20 u30 u40 u50 u60 "
            "i3 u0 u0 u0 u0 u0 u0 ";
        check(join(n) == expected, "packet: elapsed seconds, levelWon=false, seat count, then id and six counters per seat");
        if(join(n) != expected)
            std::cout << "  got: " << join(n) << "\n  exp: " << expected << '\n';
        check(join(n).find(" i0 ") == std::string::npos && join(n).find("u99") == std::string::npos, "rogue seat 0 is not sent");
        check(join(n).find("i9 ") == std::string::npos, "a seat without player is not sent");
        ServerNotification* defeated = findLast(&pa, ServerNotificationType::playerDefeated);
        check(join(defeated) == "i-1 i-1 i-1 ", "playerDefeated packet is unchanged");
    }
    {   // A bot loses: nothing is sent
        server.queue.clear();
        GameMapMock map;
        Seat a(1, 1);
        Player pa(&a, false);
        a.player = &pa;
        pa.mGameMap = &map;
        map.seats = {&a};
        pa.notifyNoMoreDungeonTemple();
        check(countOf(&pa, ServerNotificationType::levelStatistics) == 0, "no statistics for a bot");
    }
    {   // Team game: the ally of a defeated player does not receive the statistics
        server.queue.clear();
        GameMapMock map;
        Seat a(1, 1), ally(2, 1), e(3, 2);
        Player pa(&a, true), pally(&ally, true), pe(&e, true);
        a.player = &pa;
        ally.player = &pally;
        e.player = &pe;
        pa.mGameMap = &map;
        map.seats = {&a, &ally, &e};
        Room allyHeart(&ally);
        map.temples.push_back(&allyHeart);
        pa.notifyNoMoreDungeonTemple();
        check(countOf(&pa, ServerNotificationType::levelStatistics) == 1, "the defeated player gets the statistics when only he lost");
        check(countOf(&pally, ServerNotificationType::levelStatistics) == 0 && countOf(&pe, ServerNotificationType::levelStatistics) == 0, "allies and enemies get no statistics");
        ServerNotification* n = findLast(&pa, ServerNotificationType::levelStatistics);
        check(n != nullptr && n->mPacket.tokens.size() == 3 + 3 * 7 && n->mPacket.tokens[2] == "i3", "all three player seats are listed, allies included");
    }
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
notify_probe = notify_probe.replace('NOTIFY_METHOD', notify_method)

# ---------------------------------------------------------------------------
# Static wiring checks on the production sources
# ---------------------------------------------------------------------------
wiring_checks = 0

enum_body = notification_header[notification_header.index('enum class ServerNotificationType'):]
enum_body = enum_body[:enum_body.index('};')]
assert 'levelStatistics,' in enum_body and enum_body.rstrip().endswith('heartHealth')
assert enum_body.index('playerDefeated') < enum_body.index('levelStatistics')
assert 'return "levelStatistics";' in notification_source
wiring_checks += 3

# The one-line hooks sit in the intended place of their functions
handover = function(room_source, 'Room* Room::handTileOverToSeat(')
assert 'mRoomsCaptured++' in handover
assert handover.index('mCoveredTiles.erase(itTile)') < handover.index('mRoomsCaptured++') < handover.index('newRoom->mCoveredTiles.push_back(tile)')
assert 'handTileOverToSeat(seat, tile);' in function(room_source, 'void Room::claimForSeat(')
wiring_checks += 2

change_seat = torture_source.index('creature.changeSeat(getSeat());')
assert change_seat < torture_source.index('mCreaturesConverted++') < torture_source.index('creature.clearActionQueue();', change_seat)
wiring_checks += 1

craft = workshop_source[workshop_source.index('CraftedTrap* craftedTrap = new CraftedTrap('):]
assert craft.index('craftedTrap->setSeat(getSeat());') < craft.index('mItemsMade++') < craft.index('craftedTrap->addToGameMap();')
wiring_checks += 1

take_heart = function(temple_source, 'double RoomDungeonTemple::takeHeartDamage(')
assert take_heart.index('if(mHeartHP <= 0.0)') < take_heart.index('mKeepersDefeated++') < take_heart.index('recordHeartDestroyed(')
wiring_checks += 1

# Creature hook: counted in the branch that fires the death, using the alive state from before the hit
assert 'bool wasAlive = isAlive();' in take_damage
assert take_damage.index('recordCreatureKill(getSeat())') < take_damage.index('fireEntityDead();')
wiring_checks += 2

# Server sends the statistics after playerDefeated
assert notify_method.index('ServerNotificationType::playerDefeated') < notify_method.index('ServerNotificationType::levelStatistics')
wiring_checks += 1

# Client: reads in the server's order, stores only in game mode, exposes accessors
handler = client_source[client_source.index('case ServerNotificationType::levelStatistics:'):]
handler = handler[:handler.index('break;')]
assert 'packetReceived >> statistics.mElapsedSeconds >> statistics.mLevelWon >> seatCount' in handler
order = [handler.index(name) for name in ('mSeatId', 'mKeepersDefeated', 'mCreaturesKilled', 'mHeroesDestroyed',
                                          'mRoomsCaptured', 'mItemsMade', 'mCreaturesConverted')]
assert order == sorted(order)
assert 'ModeManager::GAME' in handler and 'mHasLevelStatistics = true;' in handler
assert 'const LevelStatistics& getLevelStatistics() const' in client_header
assert 'bool hasLevelStatistics() const' in client_header
wiring_checks += 5
print('WIRING OK (%d assertions): enum value last, hooks in place, client reads in server order and stores only in game mode' % wiring_checks)


def run_probe(name, code):
    with tempfile.TemporaryDirectory(prefix='odp-level-statistics-') as directory:
        work = Path(directory)
        (work / 'check.cpp').write_text(code)
        subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', 'check.cpp', '/Fecheck.exe'],
                       cwd=work, check=True, stdout=subprocess.DEVNULL)
        for attempt in range(4):
            try:
                subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
                return
            except OSError as error:
                # Windows may block a freshly compiled fixture (WinError 4551) for a moment
                if getattr(error, 'winerror', None) != 4551 or attempt == 3:
                    raise
                print('%s: WinError 4551, retry %d' % (name, attempt + 1))
                time.sleep(2)


run_probe('kills', kills_probe)
run_probe('notification', notify_probe)
