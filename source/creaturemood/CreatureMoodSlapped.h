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

#ifndef CREATUREMOODSLAPPED_H
#define CREATUREMOODSLAPPED_H

#include "creaturemood/CreatureMood.h"

//! \brief Applied for each slap the creature received during the last DurationTurns turns.
class CreatureMoodSlapped : public CreatureMood
{
public:
    //! \brief Creates the modifier with all values set to zero.
    CreatureMoodSlapped() :
        mDurationTurns(0),
        mMaxSlaps(0),
        mMoodModifier(0)
    {}

    //! \brief Destroys the modifier.
    virtual ~CreatureMoodSlapped()
    {}

    //! \brief Returns the name under which this modifier is listed in the mood configuration.
    const std::string& getModifierName() const override;

    //! \brief Returns the mood change per slap received during the last DurationTurns turns (at most MaxSlaps slaps).
    virtual int32_t computeMood(const Creature& creature) const override;

    //! \brief Returns a copy of this modifier.
    inline CreatureMoodSlapped* clone() const override;

    //! \brief Reads the modifier parameters from the configuration stream; returns false if a value is missing.
    virtual bool importFromStream(std::istream& is) override;
    //! \brief Writes the modifier parameters to the stream.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Appends the names of the modifier parameters to the format string.
    virtual void getFormatString(std::string& format) const override;

private:
    int32_t mDurationTurns;
    int32_t mMaxSlaps;
    int32_t mMoodModifier;
};

#endif // CREATUREMOODSLAPPED_H
