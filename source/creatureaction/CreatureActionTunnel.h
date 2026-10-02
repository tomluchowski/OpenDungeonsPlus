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

#ifndef CREATUREACTIONTUNNEL_H
#define CREATUREACTIONTUNNEL_H

#include "creatureaction/CreatureAction.h"

//! \brief Lets a creature with a dig rate dig through the walls that separate it
//! from the nearest enemy dungeon heart. Does nothing as long as the heart can be reached on foot.
class CreatureActionTunnel : public CreatureAction
{
public:
    CreatureActionTunnel(Creature& creature) :
        CreatureAction(creature)
    {}

    virtual ~CreatureActionTunnel()
    {}

    CreatureActionType getType() const override
    { return CreatureActionType::tunnel; }

    std::function<bool()> action() override;

    static bool handleTunnel(Creature& creature);
};

#endif // CREATUREACTIONTUNNEL_H
