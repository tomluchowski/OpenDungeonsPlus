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

#ifndef ROOMHATCHERY_H
#define ROOMHATCHERY_H

#include "entities/ChickenEntity.h"
#include "rooms/HatcheryCycle.h"
#include "gamemap/RoomObjectPath.h"
#include "rooms/HatcheryNestField.h"
#include "rooms/HatcheryRooster.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"

class Creature;
class Player;
enum class TileVisual;

class RoomHatchery: public Room
{
public:
    RoomHatchery(GameMap* gameMap);

    ~RoomHatchery()
    {}

    RoomType getType() const override
    { return mRoomType; }

    void doUpkeep() override;
    void removeFromGameMap(GameMap* gameMap = nullptr) override;
    bool hasOpenCreatureSpot(Creature* c) override;
    bool shouldStopUseIfHungrySleepy(Creature& creature, bool forced) override
    { return false; }
    bool shouldNotUseIfBadMood(Creature& creature, bool forced) override
    { return false; }

    bool useRoom(Creature& creature, bool forced) override;
    void handleCreatureUsingAbsorbedRoom(Creature& creature) override;

    void creatureDropped(Creature& creature) override;

    //! \brief The settings of the nest places from the config (only the server reads them, the clients draw the
    //! places they get).
    static HatcheryNestField::Settings getNestFieldSettings();

    //! \brief Server side: plans the next walk of a roaming animal that is at from: a free point anywhere in the room
    //! (any position on the room tiles, not bound to the tiles, at least HatcheryWanderEdge from every wall, away from
    //! the coops and the nests, at most HatcheryWanderReach from the animal) and one soft bend on the way. False when no
    //! free point was found in a few tries.
    bool planWanderPath(const Ogre::Vector2& from, std::vector<Ogre::Vector2>& path) const;

    //! \brief Server side: a free point anywhere in the room (rules as for planWanderPath, without the reach).
    bool pickFreePoint(Ogre::Vector2& point) const;

    //! \brief Server side: sends the nest places of this hatchery to the human player (see
    //! ServerNotificationType::hatcheryNests).
    void sendNestPlaces(Player* player) const;

    //! \brief Saves the number of the last day the rooster crowed for.
    void exportToStream(std::ostream& os) const override;
    //! \brief Loads what exportToStream wrote. Saves written without it still load.
    bool importFromStream(std::istream& is) override;

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;
    
protected:
    BuildingObject* notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile) override;
    void notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile) override;
private:
    //! Creatures of the hatchery that are after a chicken (hungry, on their way to eat).
    void collectHungry(std::vector<Creature*>& hungry) const;
    //! True if the point lies on a tile of the hatchery, at least edge from every tile that is not part of it, on no
    //! obstacle (coop, room object) and at least nestClearance from the middle of every nest.
    bool isFreeWanderPoint(const Ogre::Vector2& point, const std::vector<RoomObjectPath::Obstacle>& obstacles, double edge,
        double nestClearance) const;
    //! True if the straight way from from to to stays on tiles of the hatchery.
    bool isSegmentInRoom(const Ogre::Vector2& from, const Ogre::Vector2& to) const;
    //! Hens that peck, scratch and flutter now and then; they scatter cackling when a hungry creature comes close.
    void updateFlock(const std::vector<ChickenEntity*>& hens);
    //! Creatures of an enemy seat that stand on a tile of the hatchery.
    void collectEnemies(std::vector<Creature*>& enemies) const;
    //! True if an enemy creature stands on the tile of the egg (the egg does not hatch then).
    bool isEnemyOnEggTile(const ChickenEntity& egg, const std::vector<Creature*>& enemies) const;
    //! True if a map light is within HatcheryCareLightRadius tiles of the hatchery.
    bool isLit() const;
    //! Claimed by the keeper, lit and free of enemies (see HatcheryCycle::carePercent).
    HatcheryCare getCare(const std::vector<Creature*>& enemies) const;
    //! Settings of the life cycle from the config, laying times scaled by the research.
    HatcheryCycleSettings getCycleSettings() const;
    //! Creates a hatchery animal at the given position.
    ChickenEntity* spawnAnimal(ChickenKind kind, const Ogre::Vector3& position, const HatcheryCycleSettings& settings);
    //! Server side. The entrances of a hatchery with the given tiles: the tiles that lie next to (not diagonal) a
    //! walkable tile (fullness 0: door, corridor, other room) that is not one of the given tiles. A door of the given
    //! seat or of an allied seat counts as an entrance, also when it is locked; a door of a seat that is not allied
    //! does not.
    static std::vector<HatcheryNestField::TileCoord> collectEntrances(const std::vector<Tile*>& coveredTiles, const Seat* seat);
    //! Server side. The nest places of this hatchery, computed again when its tiles, coops or entrances change (then
    //! they are sent to the clients, see sendNestPlaces). The clients never compute them.
    const std::vector<HatcheryNestField::Place>& getNestPlaces() const;
    //! Server side. The places of the loose feathers that lie scattered over the hatchery while it is empty (computed
    //! together with the nest places, sent with them).
    const std::vector<HatcheryNestField::Place>& getFeatherPlaces() const;
    //! Server side: sends the nest places to every human player when they changed since they were sent last.
    void updateNestSync();
    //! A free egg place in the nests scattered over the hatchery, the nest closest to the hen first. eggPositions
    //! are the places of the eggs that lie in the hatchery. False if all nests are full (or there is none): the egg
    //! waits until a nest becomes available.
    bool findNestSpot(const Ogre::Vector3& henPosition, const std::vector<Ogre::Vector2>& eggPositions,
        Ogre::Vector3& spot) const;
    //! Puts an animal on a free spot next to where it is (the coop is in the way of an animal that sat in it). A chick
    //! that hatched in a nest comes out on the ground there.
    void leaveNest(ChickenEntity* chick);
    //! An egg is trampled: shell pieces, yolk and feathers fly where it lay (the clients show it).
    void fireEggTrample(const ChickenEntity& egg);
    //! Lets the eggs appear whose laying timer has run out and whose hen has shown herself laying (see mPendingEggs).
    //! Incubation starts at actual laying. The new eggs are added to eggs.
    void releasePendingEggs(const HatcheryCycleSettings& settings, std::vector<ChickenEntity*>& eggs);
    //! A free place next to the nest where a hen can stand (she does not stand in the nest).
    bool getNestStandPoint(const Ogre::Vector3& nestSpot, Ogre::Vector2& standing) const;
    //! Turns the hen needs to walk from where she is to the place next to the nest: the real distance at her walking
    //! speed, and a turn for setting off. 0 when she is there already.
    uint32_t nestWalkTurns(ChickenEntity& hen, const Ogre::Vector2& standing) const;
    //! True from the moment the hen sets off for the nest until her egg appears (see PendingEgg::mHen).
    bool isOnNestTrip(const ChickenEntity& hen) const;
    struct PendingEgg;
    //! The planned egg of a hen (set off or not), nullptr if she has none.
    PendingEgg* findPendingEgg(const ChickenEntity& hen);
    //! Hens with a planned egg: one sets off when the turns left until the egg are as many as the walk and the Lay
    //! pose, waits at the nest until the pose has to start, and sits down. A hen that is gone loses an egg that is not
    //! laid yet, an egg whose laying pose started still appears.
    void updateNestTrips(const std::vector<ChickenEntity*>& hens, const HatcheryCycleSettings& settings);
    //! Lets a hen or a rooster come out of a coop. Returns false if no coop has a free place.
    bool spawnFromCoop(ChickenKind kind, const HatcheryCycleSettings& settings, uint32_t count = 1);

    //! Settings of the rooster and the chick line from the config.
    RoosterSettings getRoosterSettings() const;
    //! A hatchery has room for one rooster: when it has two or more, two of them fight until one is dead. The
    //! server draws the winner when the fight starts. Fighters are moved here, not by updateRooster. The loser
    //! is taken out of the roosters list when the fight is over.
    void updateFight(std::vector<ChickenEntity*>& roosters, const HatcheryCycleSettings& settings,
        HatcheryCounts& counts);
    //! Ends the fight: the one that won stays and crows, the other one dies. Without a winner (one of them was
    //! picked up or is gone) the fight is called off and the survivor goes on as before.
    void endFight(ChickenEntity* first, ChickenEntity* second, bool finished, HatcheryCounts& counts);
    //! Moves the rooster: perching, crowing, chasing a hen, guarding the flock, leading the chicks, sleeping.
    void updateRooster(ChickenEntity* rooster, const std::vector<ChickenEntity*>& hens,
        const std::vector<ChickenEntity*>& chicks, const RoosterSettings& settings);
    //! \brief Starts the mood the rules chose: sets it on the rooster and prepares the move (down from the roof, crow timer).
    void beginRoosterMood(ChickenEntity* rooster, const RoosterPlan& plan);
    //! \brief Does one turn of the current mood: walks, chases, guards against the threat position or plays a pose.
    void actRoosterMood(ChickenEntity* rooster, const std::vector<ChickenEntity*>& hens,
        const RoosterSettings& settings, const Ogre::Vector2& threat);
    //! Sits the rooster on the roof of the nearest coop with the pose. Without coop he stays on the ground.
    void roostOnRoof(ChickenEntity* rooster, const std::string& pose, bool hopFromFar);
    //! \brief Lets the rooster jump down from a coop roof to a free place next to the coop.
    void climbDown(ChickenEntity* rooster);
    //! The chicks follow the hen (or the rooster when he leads) in a line.
    void updateChickLine(const std::vector<ChickenEntity*>& hens, const std::vector<ChickenEntity*>& chicks,
        ChickenEntity* rooster);
    //! A creature inside the hatchery that wants to eat chickens or is an enemy, close to the rooster.
    bool findThreat(const ChickenEntity& rooster, double radius, Ogre::Vector2& position) const;
    //! \brief The coop tile of this hatchery closest to the position, null without a coop.
    Tile* getNearestCoop(const Ogre::Vector2& position) const;
    //! \brief The point on the roof of the coop where the rooster sits.
    Ogre::Vector2 getPerchSpot(const Tile& coopTile) const;
    //! A free place next to a coop, where an animal can stand after jumping down.
    bool getGroundSpot(const Tile& coopTile, Ogre::Vector2& spot) const;

    //! The rooster settings of the current upkeep (read from the config once per turn, not saved)
    RoosterSettings mRoosterSettings;
    //! Turns until the rooster crows next
    uint32_t mCrowInterval;
    //! An egg that a hen is going to lay. The egg is laid when the laying timer of the hen runs out (it is due then,
    //! counts as an egg for the capacity; once laying has started it is saved in "HatcheryNestLays"). Before that:
    //! she chose the place in a nest, sets off when the turns left are as many as the walk and the Lay pose, waits at the
    //! nest and shows herself sitting (Lay pose) for HatcheryLayShowTurns turns, so that the egg appears just when the
    //! timer runs out when possible. A late hen waits until actual arrival; incubation starts when the egg appears.
    struct PendingEgg
    {
        PendingEgg(const Ogre::Vector3& spot, uint32_t turns) :
            mSpot(spot),
            mTurns(turns),
            mStand(0.0f, 0.0f),
            mWalk(0),
            mWalked(0),
            mNest(false),
            mStarted(false),
            mPosing(false),
            mDue(true),
            mLate(0)
        {}

        Ogre::Vector3 mSpot;
        //! Turns until the egg may appear (counts down while the hen sits, or at once without a hen).
        uint32_t mTurns;
        //! Not empty while the egg is planned by a hen: her name and the place next to the nest where she stands to lay.
        //! Not saved: a save in between lets a due egg appear after mTurns, a planned one is planned again.
        std::string mHen;
        Ogre::Vector2 mStand;
        //! Turns the walk to mStand takes (real distance and walking speed), turns she has been walking.
        uint32_t mWalk;
        uint32_t mWalked;
        //! The egg lies in a nest (she walks to mStand). Otherwise it lies where she sits down.
        bool mNest;
        //! She has set off / sits and shows herself laying.
        bool mStarted;
        bool mPosing;
        //! The laying timer has run out: the egg exists for the capacity. mLate counts the turns since then.
        bool mDue;
        uint32_t mLate;
    };
    std::vector<PendingEgg> mPendingEggs;
    //! The nest places, computed by the server from the tiles, the coops and the entrances when they change (not
    //! saved: they are derived; sent to the clients when they change and to a client that joins).
    mutable std::vector<HatcheryNestField::Place> mNestPlaces;
    //! The places of the loose feathers of an empty hatchery, computed together with the nests (see computeFeathers)
    mutable std::vector<HatcheryNestField::Place> mFeatherPlaces;
    mutable uint32_t mNestFieldKey;
    mutable bool mNestFieldValid;
    //! The places changed and the clients have not been told yet
    mutable bool mNestSendPending;
    //! Turns the hatchery has been empty (no hen, chick or egg)
    uint32_t mCoopHenWait;
    //! Turns the hatchery has been without rooster
    uint32_t mCoopRoosterWait;

    //! A fight of two roosters is going on (not saved: after loading, two roosters start a new one)
    bool mFightActive;
    //! The two fighters by name and who wins (drawn by the server when the fight starts)
    std::string mFightFirst;
    std::string mFightSecond;
    bool mFightFirstWins;
    //! They walk up to each other first, then they brawl for mFightTurnsLeft turns
    bool mFightBrawling;
    uint32_t mFightApproach;
    uint32_t mFightTurnsLeft;
};

#endif // ROOMHATCHERY_H
