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

#ifndef HATCHERYCYCLE_H
#define HATCHERYCYCLE_H

#include <cstdint>
#include <vector>

//! \brief Timings and limits of the chicken life cycle of a hatchery, in game turns.
struct HatcheryCycleSettings
{
    HatcheryCycleSettings() :
        mLayMin(3),
        mLayMax(7),
        mHatchTurns(2),
        mGrowTurns(4),
        mCoopWait(15),
        mTilesPerChicken(1),
        mCareLightPercent(10),
        mCareCalmPercent(15),
        mTramplePercent(30),
        mFightTurns(14),
        mFightApproachTurns(40),
        mLayShowTurns(2),
        mCoopBatch(0)
    {}

    //! Turns between two eggs of one hen (random value in [mLayMin, mLayMax]).
    uint32_t mLayMin;
    uint32_t mLayMax;
    //! Turns an egg needs to hatch, once the hatchery has a rooster.
    uint32_t mHatchTurns;
    //! Turns a chick needs to grow into a hen.
    uint32_t mGrowTurns;
    //! Turns an empty hatchery waits until a hen comes out of a coop. A hatchery without rooster waits just as
    //! long until a rooster comes out of a coop.
    uint32_t mCoopWait;
    //! Number of hatchery tiles needed for one hen, chick or egg.
    uint32_t mTilesPerChicken;
    //! Percent by which the laying times are shorter while a light (map light) is close to the claimed hatchery.
    uint32_t mCareLightPercent;
    //! Percent by which the laying times are shorter while the claimed hatchery has no enemy in it. Adds to mCareLightPercent.
    uint32_t mCareCalmPercent;
    //! Chance (percent per turn) that an enemy creature next to an egg tramples it.
    uint32_t mTramplePercent;
    //! Turns two roosters of one hatchery fight once they stand face to face.
    uint32_t mFightTurns;
    //! Turns two roosters get at most to walk up to each other before the fight starts where they stand.
    uint32_t mFightApproachTurns;
    //! Turns a laying hen shows herself sitting before the egg appears in the nest (0 = the egg appears at once).
    uint32_t mLayShowTurns;
    //! Hens that come out of the coops together once the wait is over (0 = one hen per coop, as many as the capacity allows).
    uint32_t mCoopBatch;
};

//! \brief How well the keeper looks after a hatchery.
struct HatcheryCare
{
    HatcheryCare() :
        mClaimed(false),
        mLit(false),
        mEnemies(false)
    {}

    //! All tiles of the hatchery are claimed by its keeper.
    bool mClaimed;
    //! A light is close to the hatchery.
    bool mLit;
    //! An enemy creature stands in the hatchery.
    bool mEnemies;
};

//! \brief Number of animals of a hatchery per kind.
struct HatcheryCounts
{
    HatcheryCounts() :
        mHens(0),
        mChicks(0),
        mEggs(0),
        mRoosters(0)
    {}

    //! Hens + chicks + eggs. The rooster does not count.
    uint32_t population() const
    { return mHens + mChicks + mEggs; }

    uint32_t mHens;
    uint32_t mChicks;
    uint32_t mEggs;
    uint32_t mRoosters;
};

//! \brief Rules of the chicken life cycle. They have no dependency on the game map so they can be tested alone.
class HatcheryCycle
{
public:
    //! Maximum of hens + chicks + eggs: limited by the number of coops and by the size of the hatchery.
    static uint32_t capacity(uint32_t nbTiles, uint32_t nbCoops, const HatcheryCycleSettings& settings);

    //! A hen lays an egg only while the hatchery is not full.
    static bool canLay(const HatcheryCounts& counts, uint32_t capacity);

    //! Eggs only hatch while the hatchery has a rooster.
    static bool eggsMayHatch(const HatcheryCounts& counts);

    //! A hen comes out of a coop only when there is no hen, chick or egg at all.
    static bool needCoopHen(const HatcheryCounts& counts, uint32_t nbCoops);

    //! Number of hens that come out of the coops when the wait of an empty hatchery is over: one per coop.
    static uint32_t coopHenCount(const HatcheryCycleSettings& settings, uint32_t capacity);

    //! A rooster comes out of a coop only when the hatchery has a coop and no rooster.
    static bool needCoopRooster(const HatcheryCounts& counts, uint32_t nbCoops);

    //! A hatchery has room for one rooster only: with two or more of them two fight each other.
    static bool needFight(const HatcheryCounts& counts);

    //! Which of the two fighters wins: 0 for the first, 1 for the second. random is any random number, the
    //! server draws it so that the result is the same on every client.
    static uint32_t fightWinner(uint32_t random);

    //! A fight goes on while both fighters are still in the hatchery (not picked up, not gone).
    static bool fightContinues(bool firstPresent, bool secondPresent);

    //! Eggs only hatch while the hatchery has a rooster and no enemy stands in it.
    static bool canHatch(const HatcheryCounts& counts, bool enemiesPresent);

    //! Percent by which the laying times are shorter: nothing unless all tiles are claimed, then mCareLightPercent
    //! while a light is close plus mCareCalmPercent while no enemy stands in the hatchery (at most 90).
    static uint32_t carePercent(const HatcheryCycleSettings& settings, const HatcheryCare& care);

    //! The settings with the laying times shortened by carePercent, unchanged without care.
    static HatcheryCycleSettings withCare(const HatcheryCycleSettings& settings, const HatcheryCare& care);

    //! Enemy creatures and heroes trample eggs. Creatures of the keeper never harm eggs, and nothing
    //! tramples hens, chicks or the rooster this way. roll is a random number in [0, 99].
    static bool tramples(const HatcheryCycleSettings& settings, bool enemyCreature, bool isEgg, uint32_t roll);

    //! Place for the egg of a hen in the nests of one coop. occupied has one entry per egg place, nest after nest
    //! (slotsPerNest places each). A nest that is not full comes before a full one, the nest with the fewest eggs
    //! first (the first one on a tie). Returns the index of the free place, or -1 when all places are taken: the
    //! egg then lies on a free tile of the hatchery next to the hen.
    static int32_t pickNestPlace(const std::vector<bool>& occupied, uint32_t slotsPerNest);

    //! Turns until the next egg of a hen. random is any random number.
    static uint32_t layInterval(const HatcheryCycleSettings& settings, uint32_t random);

    //! Copy of the settings where the laying times are multiplied by factor (research).
    static HatcheryCycleSettings scaled(const HatcheryCycleSettings& settings, double factor);
};

#endif // HATCHERYCYCLE_H
