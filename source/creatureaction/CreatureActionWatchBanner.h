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

#ifndef CREATUREACTIONWATCHBANNER_H
#define CREATUREACTIONWATCHBANNER_H

#include "creatureaction/CreatureAction.h"

#include <cstdint>

class Tile;

//! \brief A guard of a guard room patrols to a watch banner: it walks to the post tile and stays there
//! for a while (or less if it has to eat, sleep or get its fee, or if the post is gone). Enemies in sight
//! are fought by the usual behaviour, then the creature returns to the post.
class CreatureActionWatchBanner : public CreatureAction
{
public:
    //! \brief Creates the action for a creature that walks to a watch banner and stays there.
    //! \param postTile The tile of the watch banner to man.
    //! \param stayTurns How many turns the creature stays on the post once it has arrived.
    CreatureActionWatchBanner(Creature& creature, Tile& postTile, int64_t stayTurns) :
        CreatureAction(creature),
        mPostTile(&postTile),
        mStayTurns(stayTurns),
        mArrivalTurn(-1)
    {}

    virtual ~CreatureActionWatchBanner()
    {}

    //! \brief The type of this action.
    CreatureActionType getType() const override
    { return CreatureActionType::watchBanner; }

    //! \brief Returns the function that runs one turn of this action.
    std::function<bool()> action() override;

    //! \brief The tile of the watch banner this action goes to.
    inline Tile* getPostTile() const
    { return mPostTile; }

    //! \brief One turn of the action: walks to the post, stands on it until the stay is over or the creature
    //! has to eat, sleep or get its fee, or until the post is gone, then pops the action.
    //! \return True if the creature can continue with another action in the same turn.
    static bool handleWatchBanner(Creature& creature, Tile* postTile, CreatureActionWatchBanner* watchBannerAction);

    //! \brief Tells whether another creature of the same seat already mans (or walks to) the given post tile
    static bool isPostTaken(const Creature& creature, Tile* postTile);

    //! \brief Looks for a free reachable watch banner of the creature seat and pushes the action to patrol there.
    //! Returns true if the action has been pushed.
    static bool tryPatrol(Creature& creature, int64_t stayTurns);

    //! \brief Sends the creature to the given tile where an intruder has been seen. Returns true if it goes.
    static bool goToIntruder(Creature& creature, Tile* intruderTile);

private:
    //! \brief The tile of the watch banner to man
    Tile* mPostTile;
    //! \brief How many turns the creature stays on the post once it has arrived
    int64_t mStayTurns;
    //! \brief Turn at which the creature reached the post, -1 while it is still on its way
    int64_t mArrivalTurn;
};

#endif // CREATUREACTIONWATCHBANNER_H
