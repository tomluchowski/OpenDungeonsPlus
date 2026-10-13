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

#ifndef CREATUREEFFECTINVISIBLE_H
#define CREATUREEFFECTINVISIBLE_H

#include "creatureeffect/CreatureEffect.h"
//! \brief Marks a creature made invisible by the Invisible skill. While the effect is active,
//! enemy creatures do not target it (see Creature::isInvisible).
//! the creature can neither move nor fight (see Creature::isInvisible).
class CreatureEffectInvisible : public CreatureEffect
{
public:
    CreatureEffectInvisible(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "")
    {}

    CreatureEffectInvisible() :
        CreatureEffect()
    {}

    virtual ~CreatureEffectInvisible()
    {}

    virtual const std::string& getEffectName() const override;

    static CreatureEffectInvisible* load(std::istream& is);

protected:
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTINVISIBLE_H
