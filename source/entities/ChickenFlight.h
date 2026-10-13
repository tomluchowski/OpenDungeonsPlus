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

#ifndef CHICKENFLIGHT_H
#define CHICKENFLIGHT_H

#include <cstdint>

//! \brief Decision whether a chicken of a hatchery hops away from a hungry creature that comes to eat it.
//! The hop is short (a free point at most HatcheryFleeReach tiles away inside the room), rare (cooldown) and limited (after a few hops in a row the
//! chicken stays put for a long time), so the chicken can always be caught. Pure numbers, no game state.
namespace ChickenFlight
{
//! The chicken only notices a creature closer than this (tiles)...
const double TRIGGER_DISTANCE_MAX = 3.0;
//! ...and does not hop when the creature is this close or closer (tiles): it is caught then
const double TRIGGER_DISTANCE_MIN = 1.6;
//! Seconds between two hops
const double COOLDOWN_SECONDS = 4.0;
//! Hops in a row before the chicken holds still
const int32_t MAX_HOPS_IN_ROW = 2;
//! Seconds without a hop after the last of the hops in a row
const double HOLD_STILL_SECONDS = 12.0;

struct State
{
    State() :
        mCooldownTurns(0),
        mHopsInRow(0)
    {}

    int32_t mCooldownTurns;
    int32_t mHopsInRow;
};

//! \brief Call once per turn
inline void tick(State& state)
{
    if(state.mCooldownTurns > 0)
        --state.mCooldownTurns;
}

//! \brief True if the chicken should hop now. distance is the distance to the approaching hungry creature
//! in tiles, canMove tells that the chicken is free, standing still and has a place to hop to.
inline bool shouldFlee(const State& state, double distance, bool canMove)
{
    if(!canMove)
        return false;

    if((distance > TRIGGER_DISTANCE_MAX) || (distance <= TRIGGER_DISTANCE_MIN))
        return false;

    return state.mCooldownTurns <= 0;
}

//! \brief Call when the chicken hopped
inline void registerFlight(State& state, double turnsPerSecond)
{
    ++state.mHopsInRow;
    if(state.mHopsInRow >= MAX_HOPS_IN_ROW)
    {
        state.mCooldownTurns = static_cast<int32_t>(HOLD_STILL_SECONDS * turnsPerSecond);
        state.mHopsInRow = 0;
    }
    else
        state.mCooldownTurns = static_cast<int32_t>(COOLDOWN_SECONDS * turnsPerSecond);
}
}

#endif // CHICKENFLIGHT_H
