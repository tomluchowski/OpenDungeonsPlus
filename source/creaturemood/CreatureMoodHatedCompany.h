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

#ifndef CREATUREMOODHATEDCOMPANY_H
#define CREATUREMOODHATEDCOMPANY_H

#include "creaturemood/CreatureMood.h"

//! \brief Annoyance of a creature that stays near a creature of the opposite alignment. Fades after leaving it.
class CreatureMoodHatedCompany : public CreatureMood
{
public:
    //! \brief Creates the modifier with empty parameters (they are read from the configuration).
    CreatureMoodHatedCompany() :
        mTurnsMax(0),
        mMoodModifier(0)
    {}

    //! \brief Nothing to release.
    virtual ~CreatureMoodHatedCompany()
    {}

    //! \brief The name of this modifier ("HatedCompany").
    const std::string& getModifierName() const override;

    //! \brief Mood change per turn spent near a hated creature, counted up to TurnsMax turns.
    virtual int32_t computeMood(const Creature& creature) const override;

    //! \brief Returns a copy of this modifier.
    inline CreatureMoodHatedCompany* clone() const override;

    //! \brief Reads TurnsMax and MoodModifier, returns false if one is missing.
    virtual bool importFromStream(std::istream& is) override;
    //! \brief Writes TurnsMax and MoodModifier.
    virtual void exportToStream(std::ostream& os) const override;
    //! \brief Appends the column names TurnsMax and MoodModifier.
    virtual void getFormatString(std::string& format) const override;

private:
    //! \brief Number of turns near a hated creature after which the mood change stops growing
    int32_t mTurnsMax;
    //! \brief Mood change for each of these turns
    int32_t mMoodModifier;
};

#endif // CREATUREMOODHATEDCOMPANY_H
