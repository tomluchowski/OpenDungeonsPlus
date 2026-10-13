/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LEVELSCRIPT_H
#define LEVELSCRIPT_H

#include <cstdint>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

//! \brief Level scripting: triggers made of conditions and actions, read from the
//! [Triggers] section of a level file. This file only holds the data and the text
//! format. The evaluation on the server is done in LevelScriptRunner.cpp.
//!
//! Section format (one item per line, fields separated by tabs):
//!
//!   [Triggers]
//!   Flag    <name>  <value>                   # initial value of a flag (unset flags are 0)
//!   Timer   <name> <running 0|1> <turns>      # written by the game: a timer started by an action, in game turns
//!   PortalOff <seatId>                        # written by the game: a seat whose portals are switched off
//!   Block   <seatId> <class>                  # written by the game: a creature class that cannot come to the seat
//!   Event   <tag> <eventName> <count>         # written by the game: events of creatures and parties counted so far
//!   Member  <creatureName> <partyTag>         # written by the game: a creature that a spawn action created for a party
//!   Countdown <seconds>                       # written by the game: the HUD countdown set by an action, in level seconds
//!   WaveCountdown <seconds>                   # written by the game: the HUD wave countdown set by an action, in level seconds
//!   TimeLimit <seconds>                       # written by the game: the time limit set by an action, in level seconds (-2: removed)
//!   SlapLimit <count>                         # written by the game: the number of slaps a player may do (action slaplimit)
//!   Slaps   <seatId> <count>                  # written by the game: slaps a player has tried so far
//!   Order   <creatureName> <job> <seatId> <index> <x,y;x,y;...|->   # written by the game: the standing order of a creature (action order), index is the waypoint it walks to
//!   Region  <name> <x1> <y1> <x2> <y2>        # a named rectangle of tiles (placed in the level editor)
//!   [Trigger]
//!   Name    <name>
//!   Mode    once | repeat <cooldownSeconds>
//!   Cond    time <seconds>                    # at least that many seconds since the level start
//!   Cond    region <seatId> <x1> <y1> <x2> <y2>   # a creature of the seat (-1: any) is in the rectangle
//!   Cond    region <seatId> <regionName>          # the same, with the rectangle of a named region
//!   Cond    region <seatId> <regionName> <op> <count> [<class>]   # number of living creatures (of that class) of the seat in the region
//!   Cond    creatures <seatId> <op> <count> [<class>]   # living creatures of the seat, optionally of one creature class
//!   Cond    event <tag> <eventName> [<op> <count>]   # a creature (its name) or a party (its tag) had that event that many times, default at least once.
//!           # events: killed (dies), incapacitated (dies or is knocked out), attacked (takes damage), slapped, pickedup,
//!           # steals (takes gold), claimed (a neutral creature joins a keeper), created (the creature or a member of the party exists; counts them).
//!           # Two more tags: Seat<id> with the event cast:<spellName> (the player cast that spell, spell type names such as callToWar) and the tag Level with the event payday.
//!   Cond    slabs <seatId> <regionName> <kind> <op> <count>   # tiles of the region of that kind: rock (earth wall), path (dug floor), gold, gems, claimed (claimed floor), wall (reinforced wall), water, lava, impenetrable, manawell or a room name; seat -1 any owner, 0 unclaimed
//!   Cond    tagged <seatId> <regionName> <op> <count>   # tiles of the region marked for digging by the seat
//!   Cond    tagged <seatId> <regionName> all            # every diggable tile of the region is marked for digging by the seat (and there is one)
//!   Cond    defeated <seatId>                 # the seat has lost its dungeon (its player is out of the game)
//!   Cond    slaps <seatId> <op> <count>       # slaps the player of the seat has tried to do (a slap that the slap limit refuses counts too)
//!   Cond    furniture <seatId> <roomName> <op> <count>   # furniture objects (bookcases, beds, ...) in the rooms of that type of the seat
//!   Cond    breached <seatId>                 # a creature of a seat that is not allied with the seat stands on tiles that the seat has claimed (inside the dungeon)
//!   Cond    owns <seatId> <creatureName>      # the creature with that name is alive and belongs to the seat
//!   Cond    portal <seatId> on | off          # the portals of the seat attract creatures (on) or are switched off (off) by an action portal
//!   Cond    alive <creatureName> [1 | 0]      # the creature with that name is alive, whatever its seat (0: it is dead or not there)
//!   Cond    reached <creatureName> region <regionName>   # the living creature with that name stands inside the region
//!   Cond    reached <creatureName> heart <seatId>        # the living creature with that name stands on the dungeon heart room of the seat
//!   Cond    health <creatureName> <op> <percent>   # health of that living creature in percent of its maximum
//!   Cond    spell <seatId> <skillName>        # the seat has the spell (or any other skill) researched
//!   Cond    built <seatId> <trapName> <op> <count>   # tiles of traps or doors of that type (Cannon, DoorSteel, ...) of the seat; trapName any counts all
//!   Cond    roomtiles <seatId> <roomName> <op> <count>   # tiles of all rooms of that type of the seat
//!   Cond    roomsize <seatId> <roomName> <op> <count>    # tiles of the largest room of that type of the seat
//!   Cond    room <seatId> <roomName> <count>  # seat owns at least count rooms of that type
//!   Cond    gold <seatId> >= | <= <amount>    # gold of the seat
//!   Cond    mana <seatId> >= | <= <amount>    # mana of the seat
//!   Cond    mined <seatId> >= | <= <amount>   # gold the seat has mined
//!   Cond    claimed <seatId> <regionName> <count> | all   # tiles of the region claimed by the seat
//!   Cond    goal <seatId> <goalName>          # seat completed a goal with that name
//!   Cond    flag <name> <value>               # flag has exactly that value
//!   Cond    flag <name> <op> <value>          # op is == != < <= > >=; value may be @<otherFlag>
//!   Cond    timer <name> <op> <seconds>       # a timer started by "Action timer" has run that long (a timer that was never started counts 0)
//!
//! Every comparison of a number accepts the operators >= <= == != > < (the older conditions
//! only had >= and <=).
//!   Action  message <seatId> <text>           # seat -1: every human player
//!   Action  objective <seatId> <text>
//!   Action  spawn <seatId> <x> <y> <targetSeatId> [party=<tag>] <class:level>[@<name>] [...]   # the tag names the group for event conditions, a name is the creature name (if no creature has it)
//!   Action  gold <seatId> <amount>
//!   Action  setflag <name> <value>
//!   Action  addflag <name> <delta>            # adds to the flag (a negative value subtracts)
//!   Action  terrain <x1> <y1> <x2> <y2> <kind> [<seatId>]   # changes the tiles of the rectangle: rock, path, gold, gems, claimed, wall (claimed by the seat), water, lava, impenetrable, manawell (open ground that gives its claiming seat mana; with a seat it starts claimed, path removes it)
//!   Action  portal <seatId> on | off          # the portals of the seat do (not) attract creatures
//!   Action  available <seatId> <class> 1 | 0  # a creature class can (not) come to the dungeon of the seat
//!   Action  remove <creatureName>             # the creature leaves the map
//!   Action  alliance <seatA> <seatB> make | break   # the seats become allies (they join one team) or the seat leaves its team
//!   Action  generate <seatId> <class:level>   # a creature comes through a portal of the seat now
//!   Action  timer <name> [<seconds>]          # starts (or restarts) the timer at that value, 0 if it is left out
//!   Action  order <job> <seatId> <x,y;x,y;...|-> <name> [<name> ...]   # standing order for creatures (names or party tags): job killplayer (go to the dungeon of the seat and fight), goto (walk the waypoints, then wait), killcreatures (hunt the creatures of the seat, -1: of any other seat), wait (stand still), stealgold (go to the treasury of the seat and take its gold)
//!   Action  speed <run|walk> <name> [<name> ...]   # the creatures (names or party tags) run or walk again
//!   Action  roomowner <x> <y> <seatId>        # the room that covers the tile belongs to the seat from now on
//!   Action  slaplimit <count>                 # the players may only slap that many times (a slap beyond the limit does nothing)
//!   Action  win <seatId>                      # seat -1: every human player
//!   Action  lose <seatId>
//!   Action  reveal <seatId> <regionName>      # the tiles of the region stay visible to the seat
//!   Action  make <seatId> <skillName>         # room, trap, door or spell becomes available (skill type name such as roomHatchery); seat -1: every human player
//!   Action  countdown <seconds>               # shows a countdown on the HUD that ends without a defeat (0 removes it); a running time limit is shown instead of it
//!   Action  wavecountdown <seconds>           # shows "Next wave in mm:ss" on the HUD, beside the other countdown; it has no effect when it ends (0 removes it)
//!   Action  timelimit <seconds>               # the level is lost for every keeper when that many seconds have passed from now (0: removes any time limit); the remaining time is shown on the HUD
//!   Action  terrain <x1> <y1> <x2> <y2> bridge <seatId>   # also: a wooden bridge of the seat over lava or water
//!   State   <timesFired> <lastFiredTurn>      # written by the game, only needed in savegames
//!   [/Trigger]
//!   [/Triggers]
//!
//! All the conditions of a trigger must be true at the same time. A trigger that is
//! not repeatable fires once. A repeatable one fires again once the cooldown elapsed
//! and the conditions are (still) true.

//! How a number of a condition is compared with the value of the game
enum class LevelScriptCompare
{
    atLeast,
    atMost,
    equal,
    notEqual,
    greater,
    less
};

//! True if value op reference holds
bool levelScriptCompare(int64_t value, LevelScriptCompare op, int64_t reference);

//! The text form of the operator (>=, <=, ==, !=, >, <)
const char* levelScriptCompareToken(LevelScriptCompare op);

//! Reads an operator, false if the text is none of them
bool levelScriptParseCompare(const std::string& token, LevelScriptCompare& op);

//! \brief The kinds of conditions a trigger can test (see the format above).
enum class LevelScriptConditionType
{
    time,
    region,
    creatures,
    room,
    goal,
    flag,
    gold,
    mana,
    goldMined,
    claimed,
    timer,
    seatDefeated,
    ownsCreature,
    creatureHealth,
    spellKnown,
    trapsBuilt,
    roomTiles,
    largestRoom,
    creatureEvent,
    tileKinds,
    tilesTagged,
    playerSlaps,
    roomFurniture,
    dungeonBreached,
    portalActive,
    creatureAlive,
    creatureReached
};

//! \brief The kinds of actions a trigger can run (see the format above).
enum class LevelScriptActionType
{
    message,
    objective,
    spawn,
    gold,
    setFlag,
    addFlag,
    win,
    lose,
    reveal,
    make,
    timeLimit,
    countdown,
    waveCountdown,
    startTimer,
    alterTerrain,
    portalStatus,
    creatureAvailable,
    removeCreature,
    alliance,
    generateCreature,
    creatureOrder,
    creatureSpeed,
    roomOwner,
    slapLimit
};

//! \brief One condition of a trigger. The fields that are used depend on the type.
struct LevelScriptCondition
{
    //! \brief Creates a time condition with every value zero or "any seat".
    LevelScriptCondition() :
        mType(LevelScriptConditionType::time),
        mSeatId(-1),
        mX1(0),
        mY1(0),
        mX2(0),
        mY2(0),
        mAtLeast(true),
        mCompare(LevelScriptCompare::atLeast),
        mNumber(0)
    {}

    LevelScriptConditionType mType;
    int32_t mSeatId;
    int32_t mX1;
    int32_t mY1;
    int32_t mX2;
    int32_t mY2;
    //! \brief For the conditions that compare a number: true for >=, false for <=
    bool mAtLeast;
    //! The same comparison with all the operators (mAtLeast is true when it is atLeast)
    LevelScriptCompare mCompare;
    //! \brief Seconds (time), count (creatures, room, claimed, -1 for all of the region),
    //! amount (gold, mana, goldMined) or value (flag)
    int64_t mNumber;
    //! \brief Room name (room), goal name (goal), flag name (flag) or, for region, the
    //! name of a region of the script (empty when the rectangle is given by mX1 to mY2)
    std::string mName;
    //! The flag a flag condition compares with (empty when it compares with mNumber), or the creature class
    //! that a creatures or region condition counts (empty: every creature)
    std::string mName2;
};

//! \brief One action of a trigger. The fields that are used depend on the type.
struct LevelScriptAction
{
    //! \brief Creates a message action with every value zero or "no seat".
    LevelScriptAction() :
        mType(LevelScriptActionType::message),
        mSeatId(-1),
        mX(0),
        mY(0),
        mX2(0),
        mY2(0),
        mTargetSeatId(-1),
        mNumber(0)
    {}

    LevelScriptActionType mType;
    int32_t mSeatId;
    int32_t mX;
    int32_t mY;
    //! \brief terrain: second corner of the rectangle
    int32_t mX2;
    int32_t mY2;
    //! \brief spawn: seat whose dungeon the spawned group attacks (-1: none)
    int32_t mTargetSeatId;
    //! \brief Gold amount (gold), flag value (setflag), amount added to a flag (addflag) or
    //! seconds (timeLimit)
    int64_t mNumber;
    //! \brief Message text (message, objective), flag name (setflag), timer name (timer), region name (reveal) or skill type name (make)
    std::string mText;
    //! \brief spawn: creature class name and level
    std::vector<std::pair<std::string, uint32_t> > mCreatures;
    //! \brief spawn: name of each creature of mCreatures (empty: the game chooses one)
    std::vector<std::string> mCreatureNames;
    //! \brief spawn: tag of the party the created creatures belong to (empty: none)
    std::string mParty;
    //! \brief order: the waypoints (tile coordinates) of a goto order
    std::vector<std::pair<int32_t, int32_t> > mWaypoints;
};

//! \brief A named rectangle of tiles, both corners included. The level editor places them
//! and the conditions and actions of the triggers refer to them by name.
struct LevelScriptRegion
{
    //! \brief Creates a region with all corners at tile 0,0.
    LevelScriptRegion() :
        mX1(0),
        mY1(0),
        mX2(0),
        mY2(0)
    {}

    //! \brief Creates a region with that name between the two corners (any order).
    LevelScriptRegion(const std::string& name, int32_t x1, int32_t y1, int32_t x2, int32_t y2) :
        mName(name),
        mX1(x1),
        mY1(y1),
        mX2(x2),
        mY2(y2)
    {}

    //! \brief True if the tile is inside the rectangle, whatever the order of the corners
    bool contains(int32_t x, int32_t y) const;

    std::string mName;
    int32_t mX1;
    int32_t mY1;
    int32_t mX2;
    int32_t mY2;
};

//! \brief A trigger: conditions that must all hold, the actions to run then and how often it may fire.
struct LevelScriptTrigger
{
    //! \brief Creates a trigger that fires once and has not fired yet.
    LevelScriptTrigger() :
        mRepeat(false),
        mCooldownSeconds(0),
        mTimesFired(0),
        mLastFiredTurn(-1)
    {}

    std::string mName;
    bool mRepeat;
    int64_t mCooldownSeconds;
    std::vector<LevelScriptCondition> mConditions;
    std::vector<LevelScriptAction> mActions;
    //! \brief Runtime state, saved with the game
    uint32_t mTimesFired;
    int64_t mLastFiredTurn;
};

//! \brief The standing order of a creature, given by an order action (the AI of the creature carries it out)
struct LevelScriptOrder
{
    LevelScriptOrder() :
        mSeatId(-1),
        mIndex(0)
    {}

    //! killplayer, goto, killcreatures, wait or stealgold
    std::string mJob;
    //! The seat that is attacked, hunted or robbed (-1: none)
    int32_t mSeatId;
    //! goto: the tiles to walk to, one after the other
    std::vector<std::pair<int32_t, int32_t> > mWaypoints;
    //! goto: the waypoint the creature walks to now
    uint32_t mIndex;
};

//! A timer that an action started: it counts game turns while it runs
struct LevelScriptTimer
{
    //! \brief Creates a timer that is stopped and at turn 0.
    LevelScriptTimer() :
        mRunning(false),
        mTurns(0)
    {}

    bool mRunning;
    int64_t mTurns;
};

//! \brief The level script of a game map: the triggers, regions, flags and timers of a level.
class LevelScript
{
public:
    LevelScript() :
        mWatchedValid(false),
        mTimeLimitSeconds(TIME_LIMIT_NOT_SET),
        mCountdownSeconds(COUNTDOWN_NOT_SET),
        mWaveCountdownSeconds(COUNTDOWN_NOT_SET),
        mSlapLimit(-1)
    {}

    //! \brief Reads the lines after the [Triggers] tag, up to and including [/Triggers].
    //! Returns false on a malformed section. Existing content is replaced.
    bool importFromStream(std::istream& is);

    //! \brief Writes the whole section, including the [Triggers] and [/Triggers] tags.
    void exportToStream(std::ostream& os) const;

    //! \brief Removes everything the script holds and resets the runtime values.
    void clear();

    //! \brief True if the script holds nothing that a level file has to store.
    inline bool isEmpty() const
    { return mTriggers.empty() && mFlags.empty() && mTimers.empty() && mEvents.empty() && mPortalOff.empty() && mBlocked.empty() && mRegions.empty() && (mTimeLimitSeconds == TIME_LIMIT_NOT_SET) && (mCountdownSeconds == COUNTDOWN_NOT_SET) && (mWaveCountdownSeconds == COUNTDOWN_NOT_SET) && mOrders.empty() && mSlaps.empty() && (mSlapLimit < 0); }

    //! \brief The triggers of the script. The caller may change them.
    inline std::vector<LevelScriptTrigger>& getTriggers()
    {
        mWatchedValid = false;
        return mTriggers;
    }

    //! \brief The triggers of the script.
    inline const std::vector<LevelScriptTrigger>& getTriggers() const
    { return mTriggers; }

    //! \brief The value of the flag, 0 if it was never set.
    int64_t getFlag(const std::string& name) const;
    //! \brief Sets the flag to the value.
    void setFlag(const std::string& name, int64_t value);

    //! Timers: a timer that was never started does not exist and counts 0 turns
    int64_t getTimerTurns(const std::string& name) const;
    //! \brief Starts the timer with the given name (or restarts it) and sets it to the given number of turns.
    void startTimer(const std::string& name, int64_t turns);

    //! Adds one turn to every running timer
    void advanceTimers();

    //! \brief All timers that were started.
    inline const std::map<std::string, LevelScriptTimer>& getTimers() const
    { return mTimers; }

    //! \brief Switches the portals of a seat off or on again (the portal action)
    //! \param seatId The seat whose portals change
    //! \param off true to stop the portals from attracting creatures
    void setPortalOff(int32_t seatId, bool off);

    //! \brief True if the portals of the seat are switched off by the script
    bool isPortalOff(int32_t seatId) const;

    //! \brief Keeps a creature class away from the dungeon of a seat or lets it come again (the available action)
    //! \param seatId The seat whose portals attract the class
    //! \param className The creature class
    //! \param blocked true to keep the class away
    void setCreatureBlocked(int32_t seatId, const std::string& className, bool blocked);

    //! \brief True if the script keeps the creature class away from the dungeon of the seat
    bool isCreatureBlocked(int32_t seatId, const std::string& className) const;

    //! Counts an event of a creature (and of the party it belongs to). Only the creatures and parties
    //! that a condition of the script names are counted.
    void recordEvent(const std::string& creatureName, const std::string& eventName);
    //! \brief Gets how often the event was counted for a creature name or a party tag, 0 if never.
    int64_t getEventCount(const std::string& tag, const std::string& eventName) const;

    //! Puts a creature into a party
    void addPartyMember(const std::string& party, const std::string& creatureName);

    //! \brief All counted events, by (creature name or party tag, event name), with their counts.
    inline const std::map<std::pair<std::string, std::string>, int64_t>& getEvents() const
    { return mEvents; }

    //! \brief All party memberships, by creature name, with the name of the party of the creature.
    inline const std::map<std::string, std::string>& getPartyMembers() const
    { return mPartyMembers; }

    //! Standing orders of the creatures (by creature name)
    void setOrder(const std::string& creatureName, const LevelScriptOrder& order);
    //! \brief Removes the standing order of the creature, if it has one.
    void clearOrder(const std::string& creatureName);
    //! Returns nullptr if the creature has no order
    const LevelScriptOrder* getOrder(const std::string& creatureName) const;
    //! Moves a goto order to the next waypoint, the order is a wait order after the last one
    void advanceOrder(const std::string& creatureName);

    //! Names of the creatures of a party (tag) that were created by a spawn action
    std::vector<std::string> getPartyMemberNames(const std::string& party) const;

    //! Counts a slap the player of the seat tries. Returns false if the slap limit refuses it.
    bool registerSlap(int32_t seatId);
    //! \brief Gets how many slaps the player of the seat tried so far, 0 if none.
    int64_t getSlaps(int32_t seatId) const;
    //! \brief Gets the number of slaps each player may do, a negative value means there is no limit.
    inline int64_t getSlapLimit() const
    { return mSlapLimit; }

    //! \brief Sets the number of slaps each player may do, a negative value removes the limit.
    inline void setSlapLimit(int64_t limit)
    { mSlapLimit = limit; }

    //! \brief All flags that were set.
    inline const std::map<std::string, int64_t>& getFlags() const
    { return mFlags; }

    //! \brief All named regions of the level.
    inline const std::vector<LevelScriptRegion>& getRegions() const
    { return mRegions; }

    //! \brief Returns the region with that name, nullptr if there is none
    const LevelScriptRegion* getRegion(const std::string& name) const;

    //! \brief Adds the region or, if the name is already used, moves that region
    void setRegion(const LevelScriptRegion& region);

    //! \brief Returns false if there was no region with that name
    bool removeRegion(const std::string& name);

    //! \brief The name of the first region that holds the tile, empty if there is none
    std::string getRegionNameAt(int32_t x, int32_t y) const;

    //! \brief A name of the form Region<number> that no region uses yet
    std::string getFreeRegionName() const;

    //! \brief Replaces all the regions, used by the editor when the server sends them
    void setRegions(const std::vector<LevelScriptRegion>& regions);

    //! \brief All slap counts, by seat id.
    inline const std::map<int32_t, int64_t>& getSlapCounts() const
    { return mSlaps; }

    static const int64_t TIME_LIMIT_NOT_SET = -1;
    static const int64_t TIME_LIMIT_REMOVED = -2;

    //! \brief The time limit set by a script action, as the level second at which it runs out.
    //! TIME_LIMIT_NOT_SET if no action set one (the game duration setting applies),
    //! TIME_LIMIT_REMOVED if an action removed the limit.
    inline int64_t getTimeLimitSeconds() const
    { return mTimeLimitSeconds; }

    inline void setTimeLimitSeconds(int64_t seconds)
    { mTimeLimitSeconds = seconds; }

    static const int64_t COUNTDOWN_NOT_SET = -1;

    //! \brief The HUD countdown set by a script action, as the level second at which it ends.
    //! COUNTDOWN_NOT_SET if there is none. Unlike a time limit, running out has no effect.
    inline int64_t getCountdownSeconds() const
    { return mCountdownSeconds; }

    inline void setCountdownSeconds(int64_t seconds)
    { mCountdownSeconds = seconds; }

    //! \brief The HUD wave countdown set by a script action, as the level second at which it ends.
    //! COUNTDOWN_NOT_SET if there is none. Running out has no effect.
    inline int64_t getWaveCountdownSeconds() const
    { return mWaveCountdownSeconds; }

    inline void setWaveCountdownSeconds(int64_t seconds)
    { mWaveCountdownSeconds = seconds; }

    //! \brief Moves a time limit and a countdown that are set so that it counts from a new level start, which is
    //! elapsedSeconds later than the current one. Used when a game is saved: the turn counter
    //! starts at 0 again when it is loaded.
    void rebaseTimeLimit(int64_t elapsedSeconds);

private:
    std::vector<LevelScriptTrigger> mTriggers;
    std::map<std::string, int64_t> mFlags;
    std::map<std::string, LevelScriptTimer> mTimers;
    std::set<int32_t> mPortalOff;
    std::set<std::pair<int32_t, std::string> > mBlocked;
    std::map<std::pair<std::string, std::string>, int64_t> mEvents;
    //! creature name -> party tag
    std::map<std::string, std::string> mPartyMembers;
    //! The creature names and party tags that the event conditions of the triggers name
    mutable std::set<std::string> mWatched;
    mutable bool mWatchedValid;
    std::vector<LevelScriptRegion> mRegions;
    int64_t mTimeLimitSeconds;
    int64_t mCountdownSeconds;
    int64_t mWaveCountdownSeconds;
    std::map<std::string, LevelScriptOrder> mOrders;
    std::map<int32_t, int64_t> mSlaps;
    int64_t mSlapLimit;
};

#endif // LEVELSCRIPT_H
