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

#ifndef CREATUREACTIONFLEE_H
#define CREATUREACTIONFLEE_H

#include "creatureaction/CreatureAction.h"

class Tile;

class CreatureActionFlee : public CreatureAction
{
public:
    CreatureActionFlee(Creature& creature) :
        CreatureAction(creature),
        mFearTile(nullptr),
        mFearTurns(0)
    {}

    //! \brief Flee from a scary tile (fear trap). The creature runs away from the tile
    //! for nbTurns turns, even if no enemy is visible.
    CreatureActionFlee(Creature& creature, Tile* fearTile, int32_t nbTurns) :
        CreatureAction(creature),
        mFearTile(fearTile),
        mFearTurns(nbTurns)
    {}

    virtual ~CreatureActionFlee()
    {}

    CreatureActionType getType() const override
    { return CreatureActionType::flee; }

    //! \brief Returns the function that makes the creature run away, from the fear tile if there is one
    std::function<bool()> action() override;

    //! \brief Runs the creature away from visible enemies for nbTurns turns
    //! \return true while the creature is still fleeing
    static bool handleFlee(Creature& creature, int32_t nbTurns);

    //! \brief Runs the creature away from the fear tile for fearTurns turns, even if no enemy is visible
    //! \return true while the creature is still fleeing
    static bool handleFear(Creature& creature, int32_t nbTurns, int32_t fearTurns, Tile* fearTile);

private:
    Tile* mFearTile;
    int32_t mFearTurns;
};

#endif // CREATUREACTIONFLEE_H
