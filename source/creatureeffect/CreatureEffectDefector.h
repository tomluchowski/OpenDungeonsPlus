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

#ifndef CREATUREEFFECTDEFECTOR_H
#define CREATUREEFFECTDEFECTOR_H

#include "creatureeffect/CreatureEffect.h"

//! \brief Marks a creature that was temporarily converted to another seat by the Defector spell.
//! When the effect ends, the creature returns to its original seat (unless its seat was changed
//! by something else in the meantime).
class CreatureEffectDefector : public CreatureEffect
{
public:
    //! \brief Creates the effect.
    //! \param nbTurnsEffect How many turns the creature stays converted.
    //! \param originalSeatId Id of the seat the creature belongs to before the conversion.
    //! \param newSeatId Id of the seat the creature was converted to.
    CreatureEffectDefector(int32_t nbTurnsEffect, int originalSeatId, int newSeatId) :
        CreatureEffect(nbTurnsEffect, "SpellCreatureDefector"),
        mOriginalSeatId(originalSeatId),
        mNewSeatId(newSeatId)
    {}

    //! \brief Creates an empty effect, to be filled from a stream.
    CreatureEffectDefector() :
        CreatureEffect(),
        mOriginalSeatId(-1),
        mNewSeatId(-1)
    {}

    virtual ~CreatureEffectDefector()
    {}

    //! \brief Returns the name under which this effect is registered and saved.
    virtual const std::string& getEffectName() const override;

    //! \brief Writes the effect, including both seat ids, to the stream.
    virtual void exportToStream(std::ostream& os) const override;

    //! \brief Reads the effect, including both seat ids, from the stream.
    //! \return true if everything could be read.
    virtual bool importFromStream(std::istream& is) override;

    //! \brief Creates the effect from the stream (used when loading a saved game).
    static CreatureEffectDefector* load(std::istream& is);

protected:
    //! \brief When the effect has ended, gives the creature back to its original seat.
    virtual void applyEffect(Creature& creature) override;

private:
    //! Id of the seat the creature belongs to before the conversion.
    int mOriginalSeatId;

    //! Id of the seat the creature was converted to.
    int mNewSeatId;
};

#endif // CREATUREEFFECTDEFECTOR_H
