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

#include <string>

//! \brief Marks a creature made invisible by the Invisible skill. While the effect is active,
//! enemy creatures do not target it (see Creature::isInvisible).
class CreatureEffectInvisible : public CreatureEffect
{
public:
    //! \brief Creates the effect with its parameters.
    CreatureEffectInvisible(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "")
    {}

    //! \brief Creates an empty effect that is filled by importFromStream.
    CreatureEffectInvisible() :
        CreatureEffect()
    {}

    //! \brief Destroys the effect.
    virtual ~CreatureEffectInvisible()
    {}

    //! \brief Returns the name under which the effect is registered.
    virtual const std::string& getEffectName() const override;

    //! \brief Creates the effect from the stream (used when loading a saved game).
    static CreatureEffectInvisible* load(std::istream& is);

protected:
    //! \brief Applies the effect to the creature; called every turn while it lasts.
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTINVISIBLE_H
