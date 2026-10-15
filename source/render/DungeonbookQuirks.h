/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DUNGEONBOOKQUIRKS_H
#define DUNGEONBOOKQUIRKS_H

#include <stdint.h>
#include <istream>
#include <map>
#include <string>
#include <utility>
#include <vector>

class CreatureAppearance;
class PortraitManifest;

//! \brief Profile remarks that match the parts of a creature's appearance (config/dungeonbook-quirks.cfg).
//! Pure source without any Ogre or engine dependency. Line based, TAB separated, '#' starts a comment:
//!   <slot><TAB><option name><TAB><text>
//! Slot and option name are the ones used in the portrait manifests. Bad lines and repeated keys (the
//! first one wins) are dropped and listed in getWarnings(); the caller logs them once.
class DungeonbookQuirks
{
public:
    //! \brief Loads the config file. Returns false if it cannot be opened (no remarks then).
    bool loadFromFile(const std::string& path);

    //! \brief Same for a stream, source only names the file in the warnings.
    void loadFromStream(std::istream& is, const std::string& source);

    //! Returns nullptr if the part has no remark
    const std::string* find(const std::string& slot, const std::string& optionName) const;

    std::size_t size() const
    { return mTexts.size(); }

    const std::vector<std::string>& getWarnings() const
    { return mWarnings; }

private:
    std::map<std::pair<std::string, std::string>, std::string> mTexts;
    std::vector<std::string> mWarnings;
};

namespace DungeonbookQuirkLogic
{
//! Number of remarks one creature shows at most
extern const uint32_t MAX_REMARKS;

//! \brief The remarks of all parts of the appearance that have one, in the slot order of the appearance.
//! Parts that no longer exist in the manifest are skipped.
std::vector<std::string> collectRemarks(const DungeonbookQuirks& quirks, const PortraitManifest& manifest,
    const CreatureAppearance& appearance);

//! \brief Picks at most maxCount of the candidates, derived only from the creature name, the appearance
//! and the texts with CreatureAppearanceLogic::stableHash. The same input gives the same remarks on every
//! client and after loading, and nothing has to be stored or sent. The result keeps the order of the
//! candidates.
std::vector<std::string> pickRemarks(const std::vector<std::string>& candidates, const std::string& creatureName,
    const CreatureAppearance& appearance, uint32_t maxCount);

//! \brief collectRemarks() followed by pickRemarks() with MAX_REMARKS.
std::vector<std::string> selectRemarks(const DungeonbookQuirks& quirks, const PortraitManifest& manifest,
    const CreatureAppearance& appearance, const std::string& creatureName);

//! \brief The remarks as one text for the profile: the heading "Quirks:" and one line per remark. Empty if
//! there are none.
std::string formatRemarks(const std::vector<std::string>& remarks);
}

#endif // DUNGEONBOOKQUIRKS_H
