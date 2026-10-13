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
#include "rooms/HatcheryRooster.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"

class Creature;
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
    bool hasOpenCreatureSpot(Creature* c) override;
    bool shouldStopUseIfHungrySleepy(Creature& creature, bool forced) override
    { return false; }
    bool shouldNotUseIfBadMood(Creature& creature, bool forced) override
    { return false; }

    bool useRoom(Creature& creature, bool forced) override;
    void handleCreatureUsingAbsorbedRoom(Creature& creature) override;

    void creatureDropped(Creature& creature) override;

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
    //! Hens that peck, scratch and flutter now and then; they scatter cackling when a hungry creature comes close.
    void updateFlock(const std::vector<ChickenEntity*>& hens, bool night);
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
    //! A free egg place in the nests of the coops, the coop closest to the hen first. eggPositions are the places
    //! of the eggs that lie in the hatchery. False if all nests are full (or there is no coop): the egg then lies
    //! at the hen.
    bool findNestSpot(const Ogre::Vector3& henPosition, const std::vector<Ogre::Vector2>& eggPositions,
        Ogre::Vector3& spot) const;
    //! A chick that hatched in a nest stands next to the coop (the coop itself is in its way).
    void leaveNest(ChickenEntity* chick);
    //! An egg is trampled: shell pieces, yolk and feathers fly where it lay (the clients show it).
    void fireEggTrample(const ChickenEntity& egg);
    //! Lets a hen or a rooster come out of a coop. Returns false if no coop has a free place.
    bool spawnFromCoop(ChickenKind kind, const HatcheryCycleSettings& settings, uint32_t count = 1);

    //! Settings of the rooster, the day and the chick line from the config.
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
    //! The chicks follow the hen (or the rooster when he leads) in a line, at night they huddle under the hen.
    void updateChickLine(const std::vector<ChickenEntity*>& hens, const std::vector<ChickenEntity*>& chicks,
        ChickenEntity* rooster, bool night);
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
    //! Number of the last day the rooster crowed for (HatcheryRooster::dayNumber), -1 until he is first seen
    int64_t mLastCrowDay;
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
