/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "game/CreatureAppearance.h"

#include "render/PortraitManifest.h"

#include <cctype>
#include <cstdlib>

namespace CreatureAppearanceLogic
{
const uint32_t MAX_DUPLICATE_TRIES = 32;
}

uint32_t CreatureAppearance::getChoice(const std::string& slot) const
{
    for(std::vector<Choice>::const_iterator it = mChoices.begin(); it != mChoices.end(); ++it)
    {
        if(it->mSlot == slot)
            return it->mNumber;
    }
    return 0;
}

bool CreatureAppearance::operator==(const CreatureAppearance& other) const
{
    if(mCatalogId != other.mCatalogId || mChoices.size() != other.mChoices.size())
        return false;

    for(uint32_t i = 0; i < mChoices.size(); ++i)
    {
        if(mChoices[i].mSlot != other.mChoices[i].mSlot || mChoices[i].mNumber != other.mChoices[i].mNumber)
            return false;
    }
    return true;
}

namespace CreatureAppearanceLogic
{
namespace
{
//! Option numbers of one slot, ordered by number (the manifest already returns them ordered)
std::vector<uint32_t> getNumbers(const PortraitManifest& manifest, const std::string& slot)
{
    std::vector<uint32_t> numbers;
    std::vector<PortraitManifest::Option> options = manifest.getOptionsOfSlot(slot);
    for(std::vector<PortraitManifest::Option>::const_iterator it = options.begin(); it != options.end(); ++it)
        numbers.push_back(it->mNumber);

    return numbers;
}

uint32_t pickStableNumber(const std::vector<uint32_t>& numbers, const std::string& creatureName,
    const std::string& slot)
{
    uint32_t index = stableHash(creatureName + "|" + slot) % static_cast<uint32_t>(numbers.size());
    return numbers[index];
}

void addChoice(CreatureAppearance& appearance, const std::string& slot, uint32_t number)
{
    CreatureAppearance::Choice choice;
    choice.mSlot = slot;
    choice.mNumber = number;
    appearance.getChoices().push_back(choice);
}
}

uint32_t stableHash(const std::string& text)
{
    uint32_t hash = 2166136261u;
    for(std::string::size_type i = 0; i < text.size(); ++i)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(text[i]));
        hash *= 16777619u;
    }
    return hash;
}

std::string resolveCatalogId(const std::string& meshName, const std::string& gender,
    const CatalogExistsFunction& exists)
{
    if(meshName.empty())
        return std::string();

    if(!gender.empty())
    {
        std::string lowerGender = gender;
        for(std::string::size_type i = 0; i < lowerGender.size(); ++i)
            lowerGender[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerGender[i])));

        std::string withGender = meshName + "-" + lowerGender;
        if(exists(withGender))
            return withGender;
    }

    if(exists(meshName))
        return meshName;

    return std::string();
}

CreatureAppearance pickRandom(const PortraitManifest& manifest, const std::string& catalogId,
    const RandomFunction& random, const std::vector<CreatureAppearance>& taken)
{
    CreatureAppearance appearance;
    for(uint32_t attempt = 0; attempt < MAX_DUPLICATE_TRIES; ++attempt)
    {
        appearance = CreatureAppearance();
        appearance.setCatalogId(catalogId);
        const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
        for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
        {
            std::vector<uint32_t> numbers = getNumbers(manifest, it->mName);
            if(numbers.empty())
                continue;

            uint32_t index = random(0, static_cast<uint32_t>(numbers.size()) - 1);
            if(index >= numbers.size())
                index = static_cast<uint32_t>(numbers.size()) - 1;
            addChoice(appearance, it->mName, numbers[index]);
        }

        bool duplicate = false;
        for(std::vector<CreatureAppearance>::const_iterator other = taken.begin(); other != taken.end(); ++other)
        {
            if(*other == appearance)
            {
                duplicate = true;
                break;
            }
        }
        if(!duplicate)
            return appearance;
    }

    // The space is exhausted (or nearly): accept the last roll
    return appearance;
}

CreatureAppearance pickStable(const PortraitManifest& manifest, const std::string& catalogId,
    const std::string& creatureName)
{
    CreatureAppearance appearance;
    appearance.setCatalogId(catalogId);
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
    {
        std::vector<uint32_t> numbers = getNumbers(manifest, it->mName);
        if(numbers.empty())
            continue;

        addChoice(appearance, it->mName, pickStableNumber(numbers, creatureName, it->mName));
    }
    return appearance;
}

bool validate(const PortraitManifest& manifest, const std::string& catalogId,
    const std::string& creatureName, CreatureAppearance& appearance)
{
    if(appearance.getCatalogId() != catalogId)
    {
        appearance = pickStable(manifest, catalogId, creatureName);
        return true;
    }

    CreatureAppearance result;
    result.setCatalogId(catalogId);
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
    {
        std::vector<uint32_t> numbers = getNumbers(manifest, it->mName);
        if(numbers.empty())
            continue;

        uint32_t number = appearance.getChoice(it->mName);
        if(number == 0 || manifest.findOption(it->mName, number) == nullptr)
            number = pickStableNumber(numbers, creatureName, it->mName);

        addChoice(result, it->mName, number);
    }

    bool changed = (result != appearance);
    appearance = result;
    return changed;
}

std::string toToken(const CreatureAppearance& appearance)
{
    if(appearance.isEmpty())
        return std::string();

    std::string token = appearance.getCatalogId() + ":";
    const std::vector<CreatureAppearance::Choice>& choices = appearance.getChoices();
    for(uint32_t i = 0; i < choices.size(); ++i)
    {
        if(i > 0)
            token += ",";
        token += choices[i].mSlot + "=" + std::to_string(static_cast<unsigned long long>(choices[i].mNumber));
    }
    return token;
}

bool fromToken(const std::string& token, CreatureAppearance& appearance)
{
    appearance = CreatureAppearance();
    std::string::size_type colon = token.find(':');
    if(colon == std::string::npos || colon == 0)
        return false;

    CreatureAppearance result;
    result.setCatalogId(token.substr(0, colon));
    std::string::size_type pos = colon + 1;
    while(pos < token.size())
    {
        std::string::size_type end = token.find(',', pos);
        if(end == std::string::npos)
            end = token.size();

        std::string part = token.substr(pos, end - pos);
        std::string::size_type equals = part.find('=');
        if(equals == std::string::npos || equals == 0 || equals + 1 >= part.size())
            return false;

        std::string numberText = part.substr(equals + 1);
        for(std::string::size_type i = 0; i < numberText.size(); ++i)
        {
            if(!std::isdigit(static_cast<unsigned char>(numberText[i])))
                return false;
        }
        unsigned long number = std::strtoul(numberText.c_str(), nullptr, 10);
        if(number == 0)
            return false;

        addChoice(result, part.substr(0, equals), static_cast<uint32_t>(number));
        pos = end + 1;
    }

    appearance = result;
    return true;
}
}
