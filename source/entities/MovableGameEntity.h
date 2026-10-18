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

#ifndef MOVABLEGAMEENTITY_H
#define MOVABLEGAMEENTITY_H

#include "entities/GameEntity.h"

#include <Ogre.h>

#include <deque>
#include <cstdint>
#include <list>

class Tile;

namespace EntityAnimation
{
    static const std::string idle_anim = "Idle";
    static const std::string flee_anim = "Flee";
    static const std::string die_anim = "Die";
    static const std::string dig_anim = "Dig";
    static const std::string attack_anim = "Attack1";
    static const std::string claim_anim = "Claim";
    static const std::string walk_anim = "Walk";
    static const std::string sleep_anim = "Sleep";
    static const std::string drop_anim = "Drop";
    static const std::string getup_anim = "GetUp";
    static const std::string combat_attack_anim = "CombatAttack";
    static const std::string ranged_attack_anim = "RangedAttack";
    static const std::string eat_chicken_anim = "EatChicken";
};

//! \brief Moves and animates an entity while keeping deferred strikes synchronized with its displayed pose.
class MovableGameEntity : public GameEntity
{
public:
    //! \brief Creates a movable entity with no pending visual animation request.
    MovableGameEntity(GameMap* gameMap);

    virtual ~MovableGameEntity()
    {}

    //! \brief Checks if the destination queue is empty
    bool isMoving();


    /*! \brief Replaces an object's current walk queue with a new path. During the
     * walk, the entity will play walkAnim (looped). When it gets to the wanted position,
     * it will play endAnim (looped or not depending on loopEndAnim).
     */
    void setWalkPath(const std::string& walkAnim, const std::string& endAnim, bool loopEndAnim,
                     bool playIdleWhenAnimationEnds, const std::vector<Ogre::Vector2>& path, bool walkDistortion);

    /*! \brief Converts a tile list to a vector of Ogre::Vector3
     *
     * If skipFirst is true, the first tile in the list will be skipped
     */
    static void tileToVector2(const std::list<Tile*>& tiles, std::vector<Ogre::Vector2>& path, bool skipFirst, Ogre::Real z);
    static void tileToVector3(const std::list<Tile*>& tiles, std::vector<Ogre::Vector3>& path, bool skipFirst, Ogre::Real z);

    //! \brief Clears all future destinations from the walk queue, stops the object where it is, and sets its animation state.
    //! This is a server side function
    void clearDestinations(const std::string& animation, bool loopAnim, bool playIdleWhenAnimationEnds);

    //! \brief Stops the object where it is, activating its queued animation as the same visual request.
    virtual void stopWalking();

    //! \brief Clients side function that corrects entities moves to allow several entities
    //! to be on the same tile
    virtual void correctEntityMovePosition(Ogre::Vector2& position)
    {}

    virtual void correctDropPosition(Ogre::Vector3& position) override;

    virtual double getMoveSpeed() const
    { return 1.0; }

    //! \brief Requests an animation and identifies it so delayed cosmetic hits cannot follow a later action.
    virtual void setAnimationState(const std::string& state, bool loop = true, const Ogre::Vector3& direction = Ogre::Vector3::ZERO, bool playIdleWhenAnimationEnds = true);

    virtual double getAnimationSpeedFactor() const
    { return 1.0; }

    //! \brief True once the current strike reaches the contact fraction, or its animation was interrupted.
    //! A strike queued behind movement or waiting for its facing turn has not reached contact yet.
    //! \param share Fraction of the displayed strike clip at which contact occurs.
    bool hasReachedAnimationContact(double share) const;

    //! \brief Returns the local visual request identity; queued activation retains the same value.
    uint64_t getAnimationSequence() const
    { return mAnimationSequence; }

    //! \brief Updates the entity path, movement, and direction. Note that entities
    //! are not expected to remove themselves or other entities from the gamemap
    //! in the update function. If they do, it might lead to crashes as the gamemap
    //! will iterate the movable entities vector
    //! \param timeSinceLastFrame the elapsed time since last displayed frame in seconds.
    //! Deferred strikes use the same game-time scale as movement and skeletal clips.
    virtual void update(Ogre::Real timeSinceLastFrame);

    void setWalkDirection(const Ogre::Vector3& direction);

    virtual void setPosition(const Ogre::Vector3& v, GameMap *gameMap = nullptr ) override;

    inline void setAnimationState(Ogre::AnimationState* animationState)
    { mAnimationState = animationState; }

    inline Ogre::AnimationState* getAnimationState() const
    { return mAnimationState; }

    //! \brief Name of the animation the entity was last told to play (empty if none yet)
    inline const std::string& getAnimationStateName() const
    { return mPrevAnimationState; }

    virtual void restoreEntityState() override;

    static std::string getMovableGameEntityStreamFormat();

protected:
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;
    virtual void exportToPacket(ODPacket& os, const Seat* seat) const override;
    virtual void importFromPacket(ODPacket& is) override;

    std::deque<Ogre::Vector2> mWalkQueue;
    std::string mPrevAnimationState;
    bool mPrevAnimationStateLoop;

private:
    //! Client: starts the clip of mPrevAnimationState on the model and tells the reactions
    void startAnimationClip();
    void fireObjectAnimationState(const std::string& state, bool loop, const Ogre::Vector3& direction, bool playIdleWhenAnimationEnds);
    Ogre::AnimationState* mAnimationState;
    std::string mDestinationAnimationState;
    bool mDestinationAnimationLoop;
    bool mDestinationPlayIdleWhenAnimationEnds;
    Ogre::Vector3 mDestinationAnimationDirection;
    Ogre::Vector3 mWalkDirection;
    double mAnimationTime;
    //! Client: seconds the strike clip still waits for the turn to the target (0 = nothing waits). Not saved.
    double mBlowStartDelay;
    //! Local-only request identity for pending impacts; never serialized or sent to the server.
    uint64_t mAnimationSequence;
};


#endif // MOVABLEGAMEENTITY_H
