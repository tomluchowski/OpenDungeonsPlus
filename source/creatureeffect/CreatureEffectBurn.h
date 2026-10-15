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

#ifndef CREATUREEFFECTBURN_H
#define CREATUREEFFECTBURN_H

#include "creatureeffect/CreatureEffect.h"

//! \brief Sets a creature on fire: it takes damage every turn while the effect lasts.
class CreatureEffectBurn : public CreatureEffect
{
public:
    //! \brief Creates a burn effect.
    //! \param nbTurnsEffect How many turns the creature burns.
    //! \param damagePerTurn How much damage the creature takes each turn.
    //! \param particleEffectName Particle effect shown on the creature; may be empty.
    CreatureEffectBurn(int32_t nbTurnsEffect, double damagePerTurn, const std::string& particleEffectName) :
        CreatureEffect(nbTurnsEffect, particleEffectName),
        mDamagePerTurn(damagePerTurn)
    {}

    //! \brief Creates an empty burn effect, to be filled from a stream.
    CreatureEffectBurn() :
        CreatureEffect(),
        mDamagePerTurn(0.0)
    {}

    virtual ~CreatureEffectBurn()
    {}

    //! \brief Returns the name under which this effect is registered and saved.
    virtual const std::string& getEffectName() const override;

    //! \brief Writes the effect, including the damage per turn, to the stream.
    virtual void exportToStream(std::ostream& os) const override;

    //! \brief Reads the effect, including the damage per turn, from the stream.
    //! \return true if everything could be read.
    virtual bool importFromStream(std::istream& is) override;

    //! \brief Creates a burn effect from the stream (used when loading a saved game).
    static CreatureEffectBurn* load(std::istream& is);

protected:
    //! \brief Applies one turn of fire damage to the creature if it is still alive.
    virtual void applyEffect(Creature& creature) override;

private:
    //! Damage the creature takes each turn while it burns.
    double mDamagePerTurn;
};

#endif // CREATUREEFFECTBURN_H
