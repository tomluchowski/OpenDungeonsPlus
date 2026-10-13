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

#ifndef CREATUREREACTIONS_H
#define CREATUREREACTIONS_H

#include "render/CreatureReactionConfig.h"
#include "rooms/RoomType.h"

#include <OgrePrerequisites.h>
#include <OgreSingleton.h>
#include <OgreVector3.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

struct CosmeticEvent;
class Creature;
class CreatureCombatReactions;
class GameEntity;
class GameMap;
class MovableGameEntity;
class Seat;
class Tile;
enum class GameEntityType;

/*! \brief Cosmetic one-shot reactions of creatures (cheering, surprise, ...), client side only.
 *
 * A reaction is an overlay on what the creature does: it never changes the animation state the
 * server decided on, the creature activity or anything that is sent over the network. Each client
 * triggers the reactions on its own from what it already knows, and chooses the variants on its own.
 *
 * Every variant is shown in the highest tier available: a clip made for the reaction, else a clip
 * of the mesh, always together with an emote icon, particles and a short procedural motion. A
 * missing clip, icon or particle effect only leads to the next tier (and one log line).
 */
class CreatureReactions : public Ogre::Singleton<CreatureReactions>
{
public:
    //! \brief How much of the reactions a client shows (the client option "Creature reactions").
    enum class Mode
    {
        //! Everything is shown
        full,
        //! Only emotes, no clips, particles or motions
        reduced,
        //! Nothing is shown
        off
    };

    //! \brief Creates the reactions of a client and reads creatureReactions.cfg from the config path.
    //! \param gameMap The client game map whose creatures react.
    //! \param configPath The system config path with the trailing separator.
    CreatureReactions(GameMap* gameMap, const std::string& configPath);
    //! \brief Destroys the reactions.
    ~CreatureReactions();

    //! \brief Converts the option text ("full", "reduced", "off") to a mode; unknown text gives full.
    static Mode modeFromString(const std::string& text);
    //! \brief Converts a mode to the option text ("full", "reduced", "off").
    static std::string modeToString(Mode mode);

    //! \brief Returns the current mode.
    inline Mode getMode() const
    { return mMode; }

    //! \brief Changes the mode. Running reactions are stopped when the mode gets more restrictive
    void setMode(Mode mode);

    //! \brief Reads the configuration file again
    bool reloadConfig();

    //! \brief Advances the running reactions. To be called once per frame with the time the game
    //! was running (0 or no call while paused).
    void update(Ogre::Real timeSinceLastFrame);

    //! \brief Tries to show a reaction of the given kind on the creature. Returns true if one
    //! was started. The call can refuse: reactions are off, the creature is not seen, busy with
    //! something more important, the cooldown is running, too many reactions are running or the
    //! dice say no. forced skips all of these except the check that the creature is on the map.
    //! If variantName is not empty, that variant of the event is shown (if the event has it)
    //! instead of a random one.
    bool trigger(Creature* creature, const std::string& eventName, bool forced = false,
        const std::string& variantName = std::string());

    //! \brief Lets some of the creatures react to the same event, one after the other with a
    //! short delay (for example the winners of a fight). The first one starts after initialDelay
    //! seconds.
    void triggerGroup(const std::string& eventName, const std::vector<Creature*>& creatures,
        bool forced = false, double initialDelay = 0.0);

    //! \brief Client hook: the entity starts to play the clip. Used to see who fights and who
    //! goes down, which lets the winners of a fight cheer.
    void noteAnimation(MovableGameEntity* entity, const std::string& clip);

    //! \brief Client hook: a creature was updated by the server. oldLevel, oldMood, oldHealth, oldSeat and
    //! oldSeatPrison are the values before the update. Shows the level up, the payday, the healing, the decision to
    //! leave and the freed prisoner reactions.
    void noteCreatureUpdate(Creature* creature, uint32_t oldLevel, uint32_t oldMood, uint32_t oldHealth, Seat* oldSeat,
        Seat* oldSeatPrison);

    //! \brief Client hook: the entity got a particle effect of this script. A creature that gets a healing, haste,
    //! strength or defense spell shows how it takes it.
    void noteParticleEffect(GameEntity* entity, const std::string& script);

    //! \brief Client hook: the keeper put the entity on the tile. Remembered for a while so that
    //! a creature taking it can show that it got a gift.
    void noteHandDrop(GameEntity* entity, Tile* tile);

    //! \brief Client hook: the entity is removed from the map. If it was a gift of the keeper and a
    //! creature stands on its tile, the creature reacts.
    void noteEntityRemoved(GameEntity* entity);

    //! \brief Client hook: the entity was added to the map. A research result of the library or a
    //! crafted item of the workshop lets the creature that just worked there show its success and
    //! the others in the room react.
    void noteEntityAdded(GameEntity* entity);

    //! \brief Client hook: something happened at the position (a fight, a slap, gold falling). Creatures that
    //! stand still close by turn their head to it and show the event. Not more often than every minInterval
    //! seconds for the same kind of event. The creature exclude (if any) is not asked.
    //! If onlyAlliedTo is given, only creatures of that seat or its allies take part.
    void noteNearbyEvent(const std::string& eventName, const Ogre::Vector3& position, const Creature* exclude,
        double minInterval, Seat* onlyAlliedTo = nullptr);

    //! \brief Client hook: the creature picks up the entity to carry it. Carrying gold is shown.
    void noteCarry(Creature* carrier, GameEntity* carried);

    //! \brief Client hook: the creature puts down the entity it carried. Gold put down in a treasury is
    //! a delivery.
    void noteRelease(Creature* carrier, GameEntity* carried);

    //! \brief Client hook: the local keeper asks to slap the entity. The slap that the server confirms next
    //! (it does not tell what was hit) is shown on that creature.
    void noteSlapRequest(GameEntity* entity);

    //! \brief Client hook: the slap of the keeper hit. handPosition is where the hand is.
    void noteSlapped(const Ogre::Vector3& handPosition);

    //! \brief Client hook: the keeper picked the creature up
    void noteHandPicked(Creature* creature);

    //! \brief Client hook: the keeper dropped the creature
    void noteHandDropped(Creature* creature);

    //! \brief Client hook: the hand of the keeper moved over the creature
    void noteHandHover(Creature* creature);

    //! \brief Client hook: the server sent a cosmetic event (only when cosmetic events were agreed on). Mood
    //! changes, fear, waiting for work and the full treasury are shown here. The other kinds (carried gold,
    //! melee result, missile launch, finished digging) are left to the code that shows them.
    void noteCosmeticEvent(const CosmeticEvent& event);

    //! \brief Ends the running reaction of the creature and forgets the waiting ones. Needed before the creature
    //! changes its parent node and size (picked up and dropped).
    void endForCreature(Creature* creature);

    //! \brief Stops all the running and waiting reactions
    void stopAll();

    //! \brief True if the creature is on the screen and not too far from the camera
    bool isCreatureNearCamera(Creature* creature) const;

    //! \brief Returns the loaded reaction configuration.
    inline const CreatureReactionConfig& getConfig() const
    { return mConfig; }

    //! \brief Returns the number of reactions that are running at the moment.
    inline uint32_t getNbRunning() const
    { return static_cast<uint32_t>(mRunning.size()); }

    //! \brief True if a creature of this type gets tired and so needs a bed. Variants that
    //! yawn or doze off are only for these (the value is read from the creature definition).
    static bool creatureNeedsSleep(const Creature* creature);

private:
    //! Weapon and fight reactions (draw, stance, attack styles, hits, dropped weapons) use the internals
    friend class CreatureCombatReactions;

    struct RunningReaction
    {
        RunningReaction() :
            mPriority(ReactionPriority::event),
            mElapsed(0.0),
            mDuration(0.0),
            mClipSpeed(1.0),
            mClipEnd(1.0),
            mEmoteShown(false),
            mMotion(),
            mMotionPosition(Ogre::Vector3::ZERO),
            mMotionScale(Ogre::Vector3::UNIT_SCALE),
            mMotionAngle(0.0),
            mMotionLastPosition(Ogre::Vector3::ZERO),
            mMotionLastScale(Ogre::Vector3::UNIT_SCALE),
            mMotionTurnAngle(0.0),
            mMotionTurnComputed(false),
            mWhileWorking(false),
            mLookTarget(Ogre::Vector3::ZERO),
            mHasLookTarget(false),
            mMotionAxis(Ogre::Vector3::UNIT_Z),
            mMotionAxisComputed(false),
            mPropHeight(1.0),
            mLateEmoteDelay(0.0),
            mLateEmoteTime(2.0),
            mLateEffectDelay(0.0),
            mLateEffectTime(1.0),
            mEndsWhenMoving(false),
            mInHand(false),
            mDying(false)
        {}

        std::string mCreatureName;
        std::string mEventName;
        ReactionPriority mPriority;
        double mElapsed;
        double mDuration;

        std::string mClip;
        double mClipSpeed;
        //! Fraction of the clip length where the clip is stopped
        double mClipEnd;
        //! Name of the clip the creature was playing when the reaction started
        std::string mBaseClip;

        bool mEmoteShown;
        std::vector<std::string> mParticleSystems;

        ReactionMotion mMotion;
        //! What the motion added to the node, to be able to take it away again
        Ogre::Vector3 mMotionPosition;
        Ogre::Vector3 mMotionScale;
        double mMotionAngle;
        //! Where the node position and scale were after the last motion step. If they are different
        //! the next time, someone else moved or scaled the creature and our share is not taken away.
        Ogre::Vector3 mMotionLastPosition;
        Ogre::Vector3 mMotionLastScale;
        //! For the motion 'turn': the angle to the camera
        double mMotionTurnAngle;
        bool mMotionTurnComputed;
        //! The event decorates the work animation of the creature (see ReactionEvent::mWhileWorking)
        bool mWhileWorking;

        //! The point the motion 'lookat' and some props turn to (a wall, a room, a neighbour, the place of an event)
        Ogre::Vector3 mLookTarget;
        bool mHasLookTarget;
        //! For the motion 'lie': the axis the creature tips over
        Ogre::Vector3 mMotionAxis;
        bool mMotionAxisComputed;

        //! Sprite prop: the billboards live in a scene node of their own, found by name
        ReactionProp mProp;
        std::string mPropSetName;
        std::string mPropNodeName;
        double mPropHeight;

        //! Icon that starts later in the reaction. Empty once it is shown.
        std::string mLateEmote;
        double mLateEmoteDelay;
        double mLateEmoteTime;
        //! Particle effect that starts later in the reaction. Empty once it is shown.
        std::string mLateEffect;
        double mLateEffectDelay;
        double mLateEffectTime;

        //! The reaction needs the creature to stand and ends when it sets off
        bool mEndsWhenMoving;

        //! The creature is held in the hand of the keeper (and not on the map) while the reaction runs
        bool mInHand;

        //! The event decorates the death animation of the creature
        bool mDying;
    };

    struct PendingReaction
    {
        std::string mCreatureName;
        std::string mEventName;
        double mDelay;
        //! Time the reaction already waited for the creature to be free
        double mWaited;
        //! Seconds it waits at most
        double mWaitMax;
        bool mForced;
    };

    //! A gift of the keeper, remembered until a creature takes it
    struct HandDrop
    {
        GameEntityType mType;
        int mTileX;
        int mTileY;
        double mTime;
    };

    //! The last work of a creature in the library or workshop
    struct RoomWork
    {
        RoomType mRoomType;
        std::string mRoomName;
        double mTime;
    };

    //! Work that goes on for a while with one animation (a prisoner in its cell, a spectator in the arena):
    //! now and then a reaction of the event is tried on the creature
    struct OngoingWork
    {
        std::string mEventName;
        //! Time the creature started to do it
        double mSince;
        //! Time of the next try
        double mNext;
    };

    //! The gold deliveries of a creature to the treasury within a short time
    struct Delivery
    {
        Delivery() :
            mCount(0),
            mSince(0.0)
        {}

        uint32_t mCount;
        double mSince;
    };

    //! \brief Starts the chosen variant on the creature (clip, emote, particles, motion).
    //! Returns false if nothing could be shown.
    bool startReaction(Creature* creature, const ReactionEvent& event, const ReactionVariant& variant,
        bool forced);
    //! \brief Chooses a variant that fits the creature, randomly weighted. nullptr if none fits.
    //! If variantName is not empty, only the variant of that name can be chosen.
    const ReactionVariant* chooseVariant(const Creature* creature, const ReactionEvent& event,
        const std::string& variantName) const;
    //! \brief The winners cheer after the loser went down or fled
    void celebrateVictory(Creature* loser, bool fled);
    //! \brief The creature works in the library or workshop (its attack animation was just received):
    //! remembers it and shows another way of working after the movement.
    void noteRoomWork(Creature* creature);
    //! \brief The creature starts to dig: if it digs into a gold vein or gems it is pleased
    void noteDigging(Creature* creature);
    //! \brief Lets the creature show the event once it has finished what it does
    //! waitMax is the time the reaction waits at most for the creature to be free (negative: the usual time)
    //! The reaction starts after delay seconds (negative: a short time).
    void queueReaction(Creature* creature, const std::string& eventName, double waitMax = -1.0,
        double delay = -1.0);
    //! \brief The bout in the arena is over because the creature was knocked out: the one that fought it
    //! cheers as the winner and the others in the arena cheer as spectators
    void celebrateBout(Creature* loser);
    //! \brief Name of the room the creature stands in ("Arena", "Dormitory", ...), empty if in none
    std::string getRoomName(const Creature* creature) const;
    //! \brief The event that is shown now and then while the creature goes on with what the animation
    //! shows (empty if the creature does not do such a thing)
    std::string getOngoingEvent(const Creature* creature, const std::string& clip) const;
    //! \brief Remembers that the creature goes on with the work of the event, to try a reaction now and then
    void startOngoing(Creature* creature, const std::string& eventName);
    //! \brief Forgets the ongoing work of the creature
    void stopOngoing(const std::string& creatureName);
    //! \brief The creature starts to do something else than before (newEvent is empty or the event of the
    //! new ongoing work): the work it was doing ends and may have a done moment
    void finishOngoing(Creature* creature, const std::string& newEvent);
    //! \brief Tries the reaction of every ongoing work whose time has come
    void updateOngoing();
    //! \brief True if the creature stands in a room where the work is done with the attack animation
    bool isWorkingInRoom(const Creature* creature) const;
    //! \brief True if the creature type, group, job and sleep need allow the variant.
    bool isVariantAllowed(const Creature* creature, const ReactionVariant& variant) const;
    //! \brief "happy", "neutral" or "unhappy": the mood of the creature as far as the client knows it. A creature
    //! that just arrived through a portal uses the mood the server told for the arrival.
    std::string getMoodClass(const Creature* creature) const;
    //! \brief True if the server sends cosmetic events to this client
    static bool hasServerEvents();
    //! \brief True if the local keeper holds the creature in the hand
    bool isInHand(const Creature* creature) const;
    //! \brief A creature appeared on the client map: one of the keeper arrives through a portal, an enemy is spotted
    void noteCreatureAdded(Creature* creature);
    //! \brief The creature went down: the standing creatures of its side close by pause and mourn it
    void noteAllyDied(Creature* dead);

    //! \brief Looks at the moods of a few creatures at a time (round robin) and lets them show a feeling now and
    //! then. Not every creature every frame.
    void updateMoods(Ogre::Real timeSinceLastFrame);

    //! \brief Picks the most pressing feeling of one creature (mood, hunger, pride, boredom) and shows it
    //! \param creature The creature to look at
    void examineMood(Creature* creature);

    //! \brief True if the creature is hurt and an enemy fighter is close
    bool isHurtAndThreatened(const Creature* creature) const;
    //! \brief The point on the wall next to the creature, false if no wall tile is next to it
    bool findWall(const Creature* creature, Ogre::Vector3& point) const;
    //! \brief The position of the closest other creature, false if none is close
    bool findNeighbour(const Creature* creature, Ogre::Vector3& point) const;
    //! \brief The closest tile of a room of the creature's seat, false if it has none
    bool findRoomTile(const Creature* creature, const std::string& roomName, Ogre::Vector3& point) const;
    //! \brief The point the variant turns to (wall, neighbour or room), false if the variant has none or it is not there
    bool findLookTarget(const Creature* creature, const ReactionVariant& variant, Ogre::Vector3& point) const;
    //! \brief A short meeting of two creatures close by (chat, sparring, a wave, a grumble in the corridor). Nothing
    //! is stored: it is a dice throw now and then, over the creatures that are looked at for their moods.
    void examineInteraction(Creature* creature, bool idle, bool moving);
    //! \brief The first creature shows its event and the second one answers after a moment
    bool startMeeting(Creature* first, const std::string& firstEvent, Creature* second, const std::string& secondEvent,
        double secondDelay);
    //! \brief True if the creature stands still and plays its idle animation, and is free for a meeting
    bool isFreeAndIdle(Creature* creature);
    //! \brief True if the creature belongs to the group of the configuration
    bool isInGroup(const Creature* creature, const std::string& group) const;
    //! \brief True if the two creatures walk towards each other
    static bool areFacingEachOther(const Creature* first, const Creature* second);

    //! \brief Another idle creature close by shows the event a moment later (the yawn that spreads)
    //! \param creature The creature that shows the event first
    //! \param eventName The event the neighbours show
    void spreadTo(Creature* creature, const std::string& eventName);

    //! \brief Shows the event on the creature and lets it turn its head to the point
    //! \param creature The creature that shows the event
    //! \param eventName The event to show
    //! \param target The point the creature turns its head to
    //! \return True if the reaction was started
    bool triggerLook(Creature* creature, const std::string& eventName, const Ogre::Vector3& target);

    //! \brief True if the event (a victory or a level up) makes a creature proud for a while
    static bool isProudEvent(const std::string& eventName);

    //! \brief True if the motion needs the creature to stand where it is (it does not move the model away)
    static bool isStandingMotion(ReactionMotion::Type type);

    //! \brief Creates the sprite props (balls, a coin, pebbles, ...) of a bored creature
    //! \param reaction The running reaction that owns the props
    //! \param creature The creature the props belong to
    //! \param variant The variant that names the prop, its sprite, count and size
    //! \return False if the sprite is missing or the creature has no model to attach the props to
    bool createProps(RunningReaction& reaction, Creature* creature, const ReactionVariant& variant);

    //! \brief Moves the sprites of the props of a reaction to the place of the current moment
    void updateProps(RunningReaction& reaction, Creature* creature);

    //! \brief Destroys the sprites and the scene node of the props of a reaction, if it has any
    void removeProps(RunningReaction& reaction);

    //! \brief Shows the second icon (and the second effect) of the reaction when it is time
    void updateLate(RunningReaction& reaction, Creature* creature);

    //! \brief Updates a running reaction. Returns false if it is over
    bool updateReaction(RunningReaction& reaction, Creature* creature, Ogre::Real timeSinceLastFrame);
    //! \brief Cleans everything a reaction has put on the creature
    void endReaction(RunningReaction& reaction, Creature* creature);

    //! \brief What the creature currently does, deduced from the animation it plays on this client
    //! If the event is given and decorates the work animation, that animation does not count as busy.
    ReactionPriority getCreaturePriority(const Creature* creature, const ReactionEvent* event = nullptr) const;

    //! \brief Plays the clip of the variant on the creature; returns false if no clip could be used.
    bool startClip(RunningReaction& reaction, Creature* creature, const ReactionVariant& variant);
    //! \brief Stops the clip of the reaction and sets the weight of the creature's own clip back to full.
    void stopClip(RunningReaction& reaction, Creature* creature);
    //! \brief Moves, scales or turns the creature scene node by what the procedural motion (hop, shake,
    //! squash, spin or turn) has reached now.
    void applyMotion(RunningReaction& reaction, Creature* creature);
    //! \brief Undoes the position, scale and turn the procedural motion put on the creature scene node.
    void clearMotion(RunningReaction& reaction, Creature* creature);
    //! \brief Attaches the named particle effect to the creature; returns false if it does not exist.
    bool addParticles(RunningReaction& reaction, Creature* creature, const std::string& effect);
    //! \brief Destroys the particle systems the reaction created.
    void removeParticles(RunningReaction& reaction);

    //! \brief Returns the running reaction of the creature, or nullptr if it has none.
    RunningReaction* findRunning(const std::string& creatureName);
    //! \brief Removes the running reaction of the creature from the list.
    void eraseRunning(const std::string& creatureName);
    //! \brief Returns the render entity of the creature, or nullptr if it has none.
    Ogre::Entity* getCreatureEntity(const Creature* creature) const;
    //! \brief Logs a missing clip, icon or effect once per kind and name.
    void logMissingOnce(const std::string& kind, const std::string& name);
    //! \brief Deletes the cooldown entries that are over.
    void pruneCooldowns();

    GameMap* mGameMap;
    std::string mConfigFileName;
    CreatureReactionConfig mConfig;
    bool mConfigLoaded;
    Mode mMode;

    //! Seconds the game has been running since the reactions started. Used for the cooldowns.
    double mTime;
    double mTimeLastPrune;
    uint32_t mNextParticleId;
    uint32_t mNextPropId;

    //! Time before the next look at the moods of a few creatures, and the creature it goes on with
    double mMoodTimer;
    size_t mMoodIndex;
    //! Time a creature was first seen standing idle ("creature" -> mTime)
    std::map<std::string, double> mIdleSince;
    //! Time until a creature is proud of its victory or level up ("creature" -> mTime)
    std::map<std::string, double> mProudUntil;
    //! Time before which no creature looks at an event of this kind again (event -> mTime)
    std::map<std::string, double> mNextLook;
    //! Where the creature that is triggered now has to look (set around one trigger call only)
    Ogre::Vector3 mNextLookTarget;
    bool mHasNextLookTarget;

    //! The creature the keeper asked to slap last, and when
    std::string mSlapTarget;
    double mSlapTime;
    //! Time a creature was slapped last ("creature" -> mTime)
    std::map<std::string, double> mSlappedAt;
    //! Time before which no new meeting of two creatures starts
    double mNextInteraction;

    //! Time before which a creature may not show a reaction of a kind again ("creature|event")
    std::map<std::string, double> mCooldownEnd;
    std::vector<RunningReaction> mRunning;
    std::vector<PendingReaction> mPending;
    std::set<std::string> mLoggedMissing;

    //! Time of the last attack animation of each creature ("creature" -> mTime)
    std::map<std::string, double> mLastAttack;
    //! Time a creature last went down or fled, to celebrate it once ("creature" -> mTime)
    std::map<std::string, double> mLastCelebration;
    //! Entities the keeper dropped, by name
    std::map<std::string, HandDrop> mHandDrops;
    //! Last work of each creature in the library or workshop ("creature" -> work)
    std::map<std::string, RoomWork> mLastRoomWork;
    //! What the creatures do that goes on for a while ("creature" -> work)
    std::map<std::string, OngoingWork> mOngoing;
    //! The gold the creatures delivered lately ("creature" -> deliveries)
    std::map<std::string, Delivery> mDeliveries;
    //! Time of the last gold delivery to a treasury of each creature ("creature" -> mTime)
    std::map<std::string, double> mLastDelivery;
    //! The mood level the server told for an arrival through a portal ("creature" -> level and time)
    std::map<std::string, std::pair<int32_t, double> > mArrivalMoods;
};

#endif // CREATUREREACTIONS_H
