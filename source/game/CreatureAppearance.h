/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CREATUREAPPEARANCE_H
#define CREATUREAPPEARANCE_H

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

class PortraitManifest;

//! \brief Look of one creature in the Dungeonbook: the catalog id of its portrait base plus one chosen
//! option number for every slot of the manifest. Assigned once on the server and never changed afterwards.
//! Pure data and pure functions without any Ogre or engine dependency, so they can be tested.
class CreatureAppearance
{
public:
    struct Choice
    {
        std::string mSlot;
        uint32_t mNumber;
    };

    //! Empty if the creature has no appearance (no catalog id, or no manifest yet)
    const std::string& getCatalogId() const
    { return mCatalogId; }

    void setCatalogId(const std::string& catalogId)
    { mCatalogId = catalogId; }

    //! Chosen options in the slot order of the manifest
    const std::vector<Choice>& getChoices() const
    { return mChoices; }

    std::vector<Choice>& getChoices()
    { return mChoices; }

    bool isEmpty() const
    { return mCatalogId.empty(); }

    //! Returns 0 if the slot has no choice
    uint32_t getChoice(const std::string& slot) const;

    bool operator==(const CreatureAppearance& other) const;

    bool operator!=(const CreatureAppearance& other) const
    { return !(*this == other); }

private:
    std::string mCatalogId;
    std::vector<Choice> mChoices;
};

namespace CreatureAppearanceLogic
{
//! Random function with the signature of Random::Uint (both bounds are inclusive)
typedef std::function<uint32_t(uint32_t, uint32_t)> RandomFunction;

//! Number of tries to find an appearance that no creature of the keeper has before the last one is accepted
extern const uint32_t MAX_DUPLICATE_TRIES;

//! \brief Fixed 32 bit FNV-1a hash of the bytes of the text. Does not depend on the platform or the
//! standard library, so old saves always get the same look.
uint32_t stableHash(const std::string& text);

//! Tells whether a folder for the catalog id exists
typedef std::function<bool(const std::string&)> CatalogExistsFunction;

//! \brief Catalog id of a creature, chosen like the gender portraits (portrait-<mesh>-<gender>.png): the
//! folder "<mesh name>-<lower case gender>" (e.g. "Elf.mesh-male") if it exists, otherwise the folder
//! "<mesh name>" without suffix (the original gender of the mesh, e.g. "Elf.mesh"). Empty if neither
//! folder exists (then the creature has no appearance and the fallback picture is used).
std::string resolveCatalogId(const std::string& meshName, const std::string& gender,
    const CatalogExistsFunction& exists);

//! \brief First spawn: one option per slot, every listed option with the same chance. Rolls again (up to
//! MAX_DUPLICATE_TRIES times) while the result equals one of the appearances in taken; if the space is
//! exhausted the last roll is accepted. Slots without an option are left out.
CreatureAppearance pickRandom(const PortraitManifest& manifest, const std::string& catalogId,
    const RandomFunction& random, const std::vector<CreatureAppearance>& taken);

//! \brief Old saves: one option per slot, derived from the creature name only (stable hash).
CreatureAppearance pickStable(const PortraitManifest& manifest, const std::string& catalogId,
    const std::string& creatureName);

//! \brief Checks a stored appearance against the manifest. Options that still exist are kept; a missing
//! or removed option is replaced by an existing option of the same slot, picked from the creature name in
//! a stable way. If the catalog id differs from catalogId, the appearance is derived completely from the
//! name. Returns true if anything was changed.
bool validate(const PortraitManifest& manifest, const std::string& catalogId,
    const std::string& creatureName, CreatureAppearance& appearance);

//! \brief One token without white space for the save file: "<catalog id>:<slot>=<n>,<slot>=<n>".
//! Empty for an empty appearance.
std::string toToken(const CreatureAppearance& appearance);

//! \brief Reads a token written by toToken(). Returns false (and an empty result) for a malformed token.
bool fromToken(const std::string& token, CreatureAppearance& appearance);
}

#endif // CREATUREAPPEARANCE_H
