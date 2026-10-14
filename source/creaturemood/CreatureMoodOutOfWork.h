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

#ifndef CREATUREMOODOUTOFWORK_H
#define CREATUREMOODOUTOFWORK_H

#include "creaturemood/CreatureMood.h"

//! \brief Applied when the creature wants to work but could not find a job for some time.
class CreatureMoodOutOfWork : public CreatureMood
{
public:
    CreatureMoodOutOfWork() :
        mTurnsMin(0),
        mTurnsMax(0),
        mMoodModifier(0)
    {}

    virtual ~CreatureMoodOutOfWork()
    {}

    const std::string& getModifierName() const override;

    virtual int32_t computeMood(const Creature& creature) const override;

    inline CreatureMoodOutOfWork* clone() const override;

    virtual bool importFromStream(std::istream& is) override;
    virtual void exportToStream(std::ostream& os) const override;
    virtual void getFormatString(std::string& format) const override;

private:
    int32_t mTurnsMin;
    int32_t mTurnsMax;
    int32_t mMoodModifier;
};

#endif // CREATUREMOODOUTOFWORK_H
