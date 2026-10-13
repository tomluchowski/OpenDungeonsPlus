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

//! \brief Marks a creature that only exists for a while (the skeletons of Raise Dead and Skeleton Army).
//! When the effect runs out, the creature falls apart: it dies.
class CreatureEffectTemporary : public CreatureEffect
{
public:
    CreatureEffectTemporary(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "")
    {}

    CreatureEffectTemporary() :
        CreatureEffect()
    {}

    virtual ~CreatureEffectTemporary()
    {}

    virtual const std::string& getEffectName() const override;

    static CreatureEffectTemporary* load(std::istream& is);

protected:
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTTEMPORARY_H
