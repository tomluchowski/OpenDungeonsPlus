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

#include <OgrePrerequisites.h>
#include <OgreSingleton.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

class Creature;
class GameMap;

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
    bool trigger(Creature* creature, const std::string& eventName, bool forced = false);

    //! \brief Lets some of the creatures react to the same event, one after the other with a
    //! short delay (for example the winners of a fight).
    void triggerGroup(const std::string& eventName, const std::vector<Creature*>& creatures,
        bool forced = false);

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
            mMotionOffset(0.0),
            mMotionLastZ(0.0)
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
        double mMotionOffset;
        double mMotionLastZ;
    };

    struct PendingReaction
    {
        std::string mCreatureName;
        std::string mEventName;
        double mDelay;
        bool mForced;
    };

    //! \brief Starts the chosen variant on the creature (clip, emote, particles, motion).
    //! Returns false if nothing could be shown.
    bool startReaction(Creature* creature, const ReactionEvent& event, const ReactionVariant& variant,
        bool forced);
    //! \brief Chooses a variant that fits the creature, randomly weighted. nullptr if none fits.
    const ReactionVariant* chooseVariant(const Creature* creature, const ReactionEvent& event) const;
    //! \brief True if the creature type, group, job and sleep need allow the variant.
    bool isVariantAllowed(const Creature* creature, const ReactionVariant& variant) const;

    //! \brief Updates a running reaction. Returns false if it is over
    bool updateReaction(RunningReaction& reaction, Creature* creature, Ogre::Real timeSinceLastFrame);
    //! \brief Cleans everything a reaction has put on the creature
    void endReaction(RunningReaction& reaction, Creature* creature);

    //! \brief What the creature currently does, deduced from the animation it plays on this client
    ReactionPriority getCreaturePriority(const Creature* creature) const;

    //! \brief Plays the clip of the variant on the creature; returns false if no clip could be used.
    bool startClip(RunningReaction& reaction, Creature* creature, const ReactionVariant& variant);
    //! \brief Stops the clip of the reaction and sets the weight of the creature's own clip back to full.
    void stopClip(RunningReaction& reaction, Creature* creature);
    //! \brief Lifts the creature scene node by the height the procedural motion (hop) has reached now.
    void applyMotion(RunningReaction& reaction, Creature* creature);
    //! \brief Undoes the offset the procedural motion put on the creature scene node.
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
};

#endif // CREATUREREACTIONS_H
