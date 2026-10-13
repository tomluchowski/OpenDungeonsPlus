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

#ifndef CHICKENENTITY_H
#define CHICKENENTITY_H

#include "entities/RenderedMovableEntity.h"
#include "rooms/HatcheryRooster.h"

#include <cstdint>
#include <string>
#include <iosfwd>

class Creature;
class Room;
class GameMap;
class Tile;
class ODPacket;

//! \brief Stage of the life of a hatchery animal. Only a hen can be eaten.
enum class ChickenKind : uint32_t
{
    hen,
    chick,
    rooster,
    egg
};

class ChickenEntity: public RenderedMovableEntity
{
public:
    ChickenEntity(GameMap* gameMap, const std::string& hatcheryName, ChickenKind kind = ChickenKind::hen);
    ChickenEntity(GameMap* gameMap);

    virtual void doUpkeep() override;

    virtual double getMoveSpeed() const override
    { return 0.4; }

    virtual GameEntityType getObjectType() const override;

    virtual bool tryPickup(Seat* seat) override;
    virtual void pickup() override;
    virtual bool tryDrop(Seat* seat, Tile* tile) override;

    virtual void correctEntityMovePosition(Ogre::Vector2& position) override;

    bool eatChicken(Creature* creature);

    //! \brief An enemy creature tramples the egg: it is gone at once. Only free eggs can be trampled.
    bool trample(Creature* creature);

    //! \brief The stage of life of this animal (hen, chick, rooster or egg).
    inline ChickenKind getKind() const
    { return mKind; }

    //! \brief The rooster lost a fight with another rooster: he dies like a chicken that is slapped. Only a free
    //! rooster can lose. Server side.
    bool loseFight();

    //! \brief True while the rooster fights another one. The hatchery then moves him itself.
    inline bool isFighting() const
    { return mFighting; }

    //! \brief Marks the rooster as fighting (or no longer fighting) another rooster.
    inline void setFighting(bool fighting)
    { mFighting = fighting; }

    //! \brief Server side: tells the human players that see the animal how the fight with the partner goes
    //! (phase as in ServerNotificationType::chickenFight, this animal is the first one named).
    void notifyFight(const std::string& partnerName, uint32_t phase);

    //! \brief Changes the kind (egg hatches, chick grows). On the server, the clients are told.
    void setKind(ChickenKind kind);

    //! \brief Client side: the server told that the kind changed.
    void setKindFromServer(ChickenKind kind);

    //! \brief True if the animal is alive and on the map (not eaten, not dying).
    inline bool isFree() const
    { return mChickenState == ChickenState::free; }

    //! \brief Only a free hen can be eaten. Eggs, chicks and the rooster never are.
    inline bool isEdible() const
    { return isFree() && (mKind == ChickenKind::hen); }

    //! \brief Turns the egg or chick lived since it was laid or hatched.
    inline uint32_t getAge() const
    { return mAge; }

    //! \brief Adds one turn to the age and returns the new age.
    inline uint32_t incrementAge()
    { return ++mAge; }

    //! \brief Sets the turns until a hen lays her next egg.
    inline void setLayTimer(uint32_t turns)
    { mNbTurnLay = turns; }

    //! \brief Counts down the turns to the next egg. Returns true when the hen has to lay now.
    bool countDownLay();

    //! \brief Plays a pose (see ChickenPose.h) and holds the animal still for the number of turns.
    void playPose(const std::string& pose, uint32_t turns);

    //! \brief True while the animal holds a pose and stays where it is.
    inline bool isBusy() const
    { return mBusyTurns > 0; }

    //! \brief The hatchery tells a chick which animal to follow (the one in front of it in the line).
    void setFollowTarget(const Ogre::Vector2& target, double gap);
    //! \brief The chick no longer follows anybody.
    void clearFollowTarget();

    //! \brief Hens and chicks sit still (night, full hatchery).
    inline void setCalm(bool calm)
    { mCalm = calm; }

    //! \brief The hatchery controls the rooster itself (perching, guarding, chasing). Otherwise he struts around.
    inline void setRoomDriven(bool driven)
    { mRoomDriven = driven; }

    //! \brief The current mood of the rooster.
    inline RoosterMood getMood() const
    { return mMood; }

    //! \brief Turns left of the current mood.
    inline uint32_t getMoodTurns() const
    { return mMoodTurns; }

    //! \brief Sets the mood of the rooster for the given number of turns.
    inline void setMood(RoosterMood mood, uint32_t turns)
    {
        mMood = mood;
        mMoodTurns = turns;
    }

    //! \brief Counts down the turns of the mood.
    inline void countDownMood()
    {
        if(mMoodTurns > 0)
            --mMoodTurns;
    }

    //! \brief Turns since the rooster crowed last.
    inline uint32_t getSinceCrow() const
    { return mSinceCrow; }

    //! \brief Counts one more turn since the last crow.
    inline void incrementSinceCrow()
    { ++mSinceCrow; }

    //! \brief The rooster crowed just now.
    inline void resetSinceCrow()
    { mSinceCrow = 0; }

    //! \brief True while the rooster sits on a coop roof.
    inline bool isOnRoof() const
    { return mOnRoof; }

    //! \brief Jumps onto a coop roof.
    void hopToRoof(const Ogre::Vector3& position);

    //! \brief Jumps down from a coop roof.
    void hopDown(const Ogre::Vector2& position);

    //! \brief The seat of the hatchery the animal lives in. A rooster that is dropped elsewhere runs back to
    //! the nearest hatchery of this seat.
    inline void setHomeSeat(Seat* seat)
    { mHomeSeat = seat; }

    //! \brief Walks toward a point and stops stopDistance before it. False if there is no way.
    bool walkToward(const Ogre::Vector2& target, double stopDistance, const std::string& walkAnim);

    bool canSlap(Seat* seat) override;

    void slap() override
    { mIsSlapped = true; }

    inline bool getLockEat(const Creature& worker) const
    { return mLockedEat; }

    inline void setLockEat(const Creature& worker, bool lock)
    { mLockedEat = lock; }

    //! \brief Writes the entity and its kind to a packet for the clients.
    virtual void exportToPacket(ODPacket& os, const Seat* seat) const override;
    //! \brief Reads the entity and its kind from a packet sent by the server.
    virtual void importFromPacket(ODPacket& is) override;

    static ChickenEntity* getChickenEntityFromStream(GameMap* gameMap, std::istream& is);
    static ChickenEntity* getChickenEntityFromPacket(GameMap* gameMap, ODPacket& is);
    static std::string getChickenEntityStreamFormat();
protected:
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:
    enum ChickenState
    {
        free,
        eaten,
        dying
    };
    ChickenState mChickenState;
    ChickenKind mKind;
    uint32_t mNbTurnLay;
    uint32_t mAge;
    uint32_t mBusyTurns;
    bool mCalm;
    bool mRoomDriven;
    bool mFighting;
    bool mOnRoof;
    bool mFollowing;
    Ogre::Vector2 mFollowTarget;
    double mFollowGap;
    RoosterMood mMood;
    uint32_t mMoodTurns;
    uint32_t mSinceCrow;
    Seat* mHomeSeat;
    bool mReturningHome;
    int32_t mNbTurnOutsideHatchery;
    int32_t mNbTurnDie;
    bool mIsSlapped;
    bool mLockedEat;

    //! \brief Server side: the rooster is outside of any hatchery. Walks to the nearest hatchery of its seat.
    //! Returns true if he is on his way.
    bool runBackToHatchery(Tile* tile);

    //! \brief Server side: puts the animal somewhere else at once and tells the clients.
    void teleport(const Ogre::Vector3& position);

    //! \brief Server side: one random step inside the hatchery (or around if outside).
    void wander(Tile* tile, Room* currentHatchery);

    void addTileToListIfPossible(int x, int y, Room* currentHatchery, std::vector<Tile*>& possibleTileMove);
};

#endif // CHICKENENTITY_H
