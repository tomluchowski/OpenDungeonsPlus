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

#ifndef CREATUREACTIONPOSSESSED_H
#define CREATUREACTIONPOSSESSED_H

#include "creatureaction/CreatureAction.h"

//! \brief Action on the stack while a player controls the creature (Possess spell). The
//! movement itself is driven by the player input (see Creature::possessedMove). Each turn,
//! this action takes the mana the possession costs and ends it if the creature cannot be
//! controlled anymore or the player cannot pay.
class CreatureActionPossessed : public CreatureAction
{
public:
    CreatureActionPossessed(Creature& creature) :
        CreatureAction(creature)
    {}

    virtual ~CreatureActionPossessed()
    {}

    CreatureActionType getType() const override
    { return CreatureActionType::possessed; }

    std::function<bool()> action() override;

    static bool handlePossessed(Creature& creature);
};

#endif // CREATUREACTIONPOSSESSED_H
