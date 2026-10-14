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


#ifndef CREATUREEFFECTTEMPORARY_H
#define CREATUREEFFECTTEMPORARY_H

#include "creatureeffect/CreatureEffect.h"

#include <string>

//! \brief Marks a creature that only exists for a while (the skeletons of Raise Dead and Skeleton Army).
//! When the effect runs out, the creature falls apart: it dies.
class CreatureEffectTemporary : public CreatureEffect
{
public:
    //! \brief Creates the effect that makes the creature fall apart after the given number of turns.
    CreatureEffectTemporary(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "")
    {}

    //! \brief Creates an empty effect to be filled from a stream.
    CreatureEffectTemporary() :
        CreatureEffect()
    {}

    //! \brief Nothing to release.
    virtual ~CreatureEffectTemporary()
    {}

    //! \brief The name of this effect ("Temporary").
    virtual const std::string& getEffectName() const override;

    //! \brief Creates the effect from the stream (used when loading a saved game).
    static CreatureEffectTemporary* load(std::istream& is);

protected:
    //! \brief Kills the creature once the effect has run out.
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTTEMPORARY_H
