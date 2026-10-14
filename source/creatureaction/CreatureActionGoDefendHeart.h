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

#ifndef CREATUREACTIONGODEFENDHEART_H
#define CREATUREACTIONGODEFENDHEART_H

#include "creatureaction/CreatureAction.h"

class Creature;

//! \brief While the seat's heart defence is on, the runner moves to the defence
//! point: the seat's nearest fighter, or the heart while it is damaged. When the
//! defence is over, the action pops and the creature returns to normal work.
class CreatureActionGoDefendHeart : public CreatureAction
{
public:
    CreatureActionGoDefendHeart(Creature& creature) :
        CreatureAction(creature)
    {}

    virtual ~CreatureActionGoDefendHeart()
    {}

    CreatureActionType getType() const override
    { return CreatureActionType::goDefendHeart; }

    std::function<bool()> action() override;

    static bool handleDefendHeart(Creature& creature);
};

#endif // CREATUREACTIONGODEFENDHEART_H
