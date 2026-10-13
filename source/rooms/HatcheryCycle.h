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

//! \brief Timings and limits of the chicken life cycle of a hatchery, in game turns.
struct HatcheryCycleSettings
{
    HatcheryCycleSettings() :
        mLayMin(2),
        mLayMax(4),
        mHatchTurns(2),
        mGrowTurns(3),
        mCoopWait(15),
        mRoosterWait(15),
        mTilesPerChicken(1),
        mCareLayPercent(25),
        mTramplePercent(30)
    {}

    //! Turns between two eggs of one hen (random value in [mLayMin, mLayMax]).
    uint32_t mLayMin;
    uint32_t mLayMax;
    //! Turns an egg needs to hatch, once the hatchery has a rooster.
    uint32_t mHatchTurns;
    //! Turns a chick needs to grow into a hen.
    uint32_t mGrowTurns;
    //! Turns an empty hatchery waits until a hen comes out of a coop.
    uint32_t mCoopWait;
    //! Turns a hatchery without rooster waits until a rooster comes out of a coop.
    uint32_t mRoosterWait;
    //! Number of hatchery tiles needed for one hen, chick or egg.
    uint32_t mTilesPerChicken;
    //! Percent by which the laying times are shorter while the hatchery is well cared for.
    uint32_t mCareLayPercent;
    //! Chance (percent per turn) that an enemy creature next to an egg tramples it.
    uint32_t mTramplePercent;
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

    //! A rooster comes out of a coop only when the hatchery has a coop and no rooster.
    static bool needCoopRooster(const HatcheryCounts& counts, uint32_t nbCoops);

    //! Eggs only hatch while the hatchery has a rooster and no enemy stands in it.
    static bool canHatch(const HatcheryCounts& counts, bool enemiesPresent);

    //! Breeding needs care: claimed, lit and without enemies.
    static bool wellCared(const HatcheryCare& care);

    //! The settings with the laying times shortened by mCareLayPercent (at most 90) when the hatchery is
    //! well cared for, otherwise unchanged.
    static HatcheryCycleSettings withCare(const HatcheryCycleSettings& settings, const HatcheryCare& care);

    //! Enemy creatures and heroes trample eggs. Creatures of the keeper never harm eggs, and nothing
    //! tramples hens, chicks or the rooster this way. roll is a random number in [0, 99].
    static bool tramples(const HatcheryCycleSettings& settings, bool enemyCreature, bool isEgg, uint32_t roll);

    //! Turns until the next egg of a hen. random is any random number.
    static uint32_t layInterval(const HatcheryCycleSettings& settings, uint32_t random);

    //! Copy of the settings where the laying times are multiplied by factor (research).
    static HatcheryCycleSettings scaled(const HatcheryCycleSettings& settings, double factor);
};

#endif // HATCHERYCYCLE_H
