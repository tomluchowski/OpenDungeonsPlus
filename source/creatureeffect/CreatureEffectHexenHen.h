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

#ifndef CREATUREEFFECTHEXENHEN_H
#define CREATUREEFFECTHEXENHEN_H

#include "creatureeffect/CreatureEffect.h"

//! \brief Marks a creature that is temporarily turned into a chicken by the Hexen Hen spell.
//! While the effect is active, the creature is harmless (see Creature::isHexenHen). When it ends,
//! the creature gets its normal form back.
class CreatureEffectHexenHen : public CreatureEffect
{
public:
    //! \brief Creates the effect.
    //! \param nbTurnsEffect How many turns the creature stays a chicken.
    CreatureEffectHexenHen(int32_t nbTurnsEffect) :
        CreatureEffect(nbTurnsEffect, "SpellCreatureHexenHen")
    {}

    //! \brief Creates an empty effect, to be filled from a stream.
    CreatureEffectHexenHen() :
        CreatureEffect()
    {}

    virtual ~CreatureEffectHexenHen()
    {}

    //! \brief Returns the name under which this effect is registered and saved.
    virtual const std::string& getEffectName() const override;

    //! \brief Creates the effect from the stream (used when loading a saved game).
    static CreatureEffectHexenHen* load(std::istream& is);

protected:
    //! \brief When the effect has ended, asks the clients to show the normal form again.
    virtual void applyEffect(Creature& creature) override;
};

#endif // CREATUREEFFECTHEXENHEN_H
