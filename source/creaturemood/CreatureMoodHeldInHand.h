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

#ifndef CREATUREMOODHELDINHAND_H
#define CREATUREMOODHELDINHAND_H

#include "creaturemood/CreatureMood.h"

//! \brief Applied when the creature is held in the hand for too long.
class CreatureMoodHeldInHand : public CreatureMood
{
public:
    //! \brief Creates the modifier with all values set to zero.
    CreatureMoodHeldInHand() :
        mTurnsMin(0),
        mTurnsMax(0),
        mMoodModifier(0)
    {}

    //! \brief Destroys the modifier.
    virtual ~CreatureMoodHeldInHand()
    {}

    //! \brief Returns the name under which this modifier is listed in the mood configuration.
    const std::string& getModifierName() const override;

    //! \brief Returns the mood change for the turns the creature stayed in the hand beyond TurnsMin (counted up to TurnsMax turns).
    virtual int32_t computeMood(const Creature& creature) const override;

    //! \brief Returns a copy of this modifier.
    inline CreatureMoodHeldInHand* clone() const override;

    //! \brief Reads the modifier parameters from the configuration stream; returns false if a value is missing.
    virtual bool importFromStream(std::istream& is) override;
    //! \brief Writes the modifier parameters to the stream.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Appends the names of the modifier parameters to the format string.
    virtual void getFormatString(std::string& format) const override;

private:
    int32_t mTurnsMin;
    int32_t mTurnsMax;
    int32_t mMoodModifier;
};

#endif // CREATUREMOODHELDINHAND_H
