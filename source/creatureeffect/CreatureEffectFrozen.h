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

#ifndef CREATUREEFFECTFROZEN_H
#define CREATUREEFFECTFROZEN_H

#include "creatureeffect/CreatureEffect.h"

#include <cstdint>
#include <string>

//! \brief Marks a creature that is paralysed by the Freeze trap. While the effect is active,
//! the creature can neither move nor fight (see Creature::isFrozen).
class CreatureEffectFrozen : public CreatureEffect
{
public:
    //! \brief Creates the effect for the given number of turns.
    CreatureEffectFrozen(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "")
    {}

    //! \brief Creates an empty effect to be filled from a stream.
    CreatureEffectFrozen() :
        CreatureEffect()
    {}

    virtual ~CreatureEffectFrozen()
    {}

    //! \brief The name of this effect ("Frozen").
    virtual const std::string& getEffectName() const override;

    //! \brief Reads the effect from the given stream.
    static CreatureEffectFrozen* load(std::istream& is);

protected:
    //! \brief Nothing to update each turn: the frozen state is deduced from the effect (Creature::isFrozen).
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTFROZEN_H
