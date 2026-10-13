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

class Creature;
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

    //! \brief Client hook: a creature was updated by the server. oldLevel, oldMood, oldSeat and oldSeatPrison
    //! are the values before the update. Shows the level up, the payday and the freed prisoner reactions.
    void noteCreatureUpdate(Creature* creature, uint32_t oldLevel, uint32_t oldMood, Seat* oldSeat,
        Seat* oldSeatPrison);

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

    //! \brief Client hook: the creature picks up the entity to carry it. Carrying gold is shown.
    void noteCarry(Creature* carrier, GameEntity* carried);

    //! \brief Client hook: the creature puts down the entity it carried. Gold put down in a treasury is
    //! a delivery.
    void noteRelease(Creature* carrier, GameEntity* carried);

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
            mWhileWorking(false)
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
};

#endif // CREATUREREACTIONS_H
