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

#ifndef SOCIALGENERATOR_H
#define SOCIALGENERATOR_H

#include "social/CreatureProfile.h"
#include "social/SocialData.h"

#include <map>
#include <stdint.h>
#include <string>

namespace social
{

//! \brief Deterministic generator of creature social profiles. It reads only the creature
//! name, its class, its job and the data tables: never time, never Random, never the level.
//! Client side only, nothing produced here is sent over the network or saved.
class SocialGenerator
{
public:
    //! Maximum length of a bio after the slots are expanded
    static const std::size_t MAX_BIO_LENGTH = 160;

    //! Highest name variant that is drawn from the name tables, see makeProfile
    static const uint32_t MAX_NAME_VARIANT = 12;

    //! \brief Gender of a creature ("Female", "Male" or empty), the same value makeProfile puts in
    //! CreatureProfile::mGender. Depends only on the creature name, the class and the data, so the server
    //! and the client get the same result.
    static std::string makeGender(const SocialData& data, const std::string& creatureName,
        const std::string& className);

    //! \brief Builds the profile of a creature from its name (for example "Orc17"). Variant 0 is the
    //! normal name; a higher variant draws another first name and surname or title (everything else stays
    //! the same), so a caller can resolve two creatures of a dungeon with the same name. Variants above
    //! MAX_NAME_VARIANT use the creature name as surname, which is unique in a game.
    static CreatureProfile makeProfile(const SocialData& data, const std::string& creatureName,
        const std::string& className, bool isWorker, uint32_t nameVariant = 0);

    //! \brief The readable name of a creature class ("CaveHornet" -> "Cave Hornet"), the class name
    //! itself if the data has no entry for it.
    static std::string displayClassName(const SocialData& data, const std::string& className);

    //! \brief Replaces {slot} entries of the template with the given values. Unknown or empty
    //! slots are removed, then double spaces and spaces before punctuation are cleaned up.
    static std::string renderText(const std::string& text, const std::map<std::string, std::string>& slots);

    //! \brief Symmetric affinity in [0, 1000[ of two creatures (same value for (a, b) and (b, a)).
    static uint32_t affinity(const std::string& creatureNameA, const std::string& creatureNameB);

    //! \brief Mood line for a state ("Hungry", "Tired", "GetFee", "LeaveDungeon", "KoTemp",
    //! "InJail", "Happy", "Neutral", "Upset", "Angry", "Furious", "Unknown"). The variant is
    //! chosen from the creature name, so a creature always phrases a state the same way.
    static std::string moodLine(const SocialData& data, const std::string& creatureName,
        const std::string& className, bool isWorker, const std::string& state);

    //! \brief Post template (with unexpanded slots) of a category ("eat", "sleep", "levelup", ...)
    //! chosen from creature name and variant. Empty string if the category has no template.
    static std::string postTemplate(const SocialData& data, const std::string& creatureName,
        const std::string& className, bool isWorker, const std::string& category, uint32_t variant);

    //! \brief Text of a feed post: the template of the category expanded with the profile and the
    //! given level and room ("library", ...). Empty if the category has no template.
    static std::string renderPost(const SocialData& data, const CreatureProfile& profile, bool isWorker,
        const std::string& category, uint32_t variant, int32_t level, const std::string& room);

    //! \brief One line with all profile fields, used to compare profiles byte for byte.
    static std::string serialize(const CreatureProfile& profile);
};

}

#endif // SOCIALGENERATOR_H
