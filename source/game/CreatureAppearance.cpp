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
const uint32_t ROLL_BUDGET = 100000;
const uint64_t ENUMERATION_LIMIT = 4000000;
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

uint64_t multiplySaturating(uint64_t a, uint64_t b)
{
    if((a != 0) && (b > UINT64_MAX / a))
        return UINT64_MAX;

    return a * b;
}

uint64_t countCombinations(const PortraitManifest& manifest)
{
    uint64_t total = 1;
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
    {
        uint64_t count = getNumbers(manifest, it->mName).size();
        if(count == 0)
            continue;

        total = multiplySaturating(total, count);
    }
    return total;
}

namespace
{
//! One slot of the appearance space: its name and its option numbers
struct SlotNumbers
{
    std::string mSlot;
    std::vector<uint32_t> mNumbers;
};

std::vector<SlotNumbers> getSlotNumbers(const PortraitManifest& manifest)
{
    std::vector<SlotNumbers> result;
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(std::vector<PortraitManifest::Slot>::const_iterator it = slots.begin(); it != slots.end(); ++it)
    {
        SlotNumbers entry;
        entry.mSlot = it->mName;
        entry.mNumbers = getNumbers(manifest, it->mName);
        if(!entry.mNumbers.empty())
            result.push_back(entry);
    }
    return result;
}

//! True if the appearance is one combination of the space (right catalog id, one existing option per slot)
bool isInSpace(const CreatureAppearance& appearance, const std::string& catalogId, const std::vector<SlotNumbers>& space)
{
    if(appearance.getCatalogId() != catalogId || appearance.getChoices().size() != space.size())
        return false;

    for(uint32_t i = 0; i < space.size(); ++i)
    {
        const CreatureAppearance::Choice& choice = appearance.getChoices()[i];
        if(choice.mSlot != space[i].mSlot)
            return false;

        bool found = false;
        for(uint32_t n = 0; n < space[i].mNumbers.size(); ++n)
            found = found || (space[i].mNumbers[n] == choice.mNumber);

        if(!found)
            return false;
    }
    return true;
}

bool isTaken(const CreatureAppearance& appearance, const std::vector<CreatureAppearance>& taken)
{
    for(std::vector<CreatureAppearance>::const_iterator other = taken.begin(); other != taken.end(); ++other)
    {
        if(*other == appearance)
            return true;
    }
    return false;
}

//! The combination with the given index (mixed radix, the first slot changes slowest)
CreatureAppearance makeCombination(const std::string& catalogId, const std::vector<SlotNumbers>& space, uint64_t index)
{
    std::vector<uint32_t> positions(space.size(), 0);
    for(size_t i = space.size(); i > 0; --i)
    {
        uint64_t count = space[i - 1].mNumbers.size();
        positions[i - 1] = static_cast<uint32_t>(index % count);
        index /= count;
    }

    CreatureAppearance appearance;
    appearance.setCatalogId(catalogId);
    for(size_t i = 0; i < space.size(); ++i)
        addChoice(appearance, space[i].mSlot, space[i].mNumbers[positions[i]]);

    return appearance;
}

CreatureAppearance rollCombination(const std::string& catalogId, const std::vector<SlotNumbers>& space,
    const RandomFunction& random)
{
    CreatureAppearance appearance;
    appearance.setCatalogId(catalogId);
    for(std::vector<SlotNumbers>::const_iterator it = space.begin(); it != space.end(); ++it)
    {
        uint32_t index = random(0, static_cast<uint32_t>(it->mNumbers.size()) - 1);
        if(index >= it->mNumbers.size())
            index = static_cast<uint32_t>(it->mNumbers.size()) - 1;
        addChoice(appearance, it->mSlot, it->mNumbers[index]);
    }
    return appearance;
}
}

CreatureAppearance pickRandom(const PortraitManifest& manifest, const std::string& catalogId,
    const RandomFunction& random, const std::vector<CreatureAppearance>& taken)
{
    const std::vector<SlotNumbers> space = getSlotNumbers(manifest);
    const uint64_t total = countCombinations(manifest);

    // Count the different combinations of this space that are already in use
    std::vector<CreatureAppearance> used;
    for(std::vector<CreatureAppearance>::const_iterator it = taken.begin(); it != taken.end(); ++it)
    {
        if(isInSpace(*it, catalogId, space) && !isTaken(*it, used))
            used.push_back(*it);
    }

    // Every combination is in use: a duplicate cannot be avoided
    if(static_cast<uint64_t>(used.size()) >= total)
        return rollCombination(catalogId, space, random);

    for(uint32_t roll = 0; roll < ROLL_BUDGET; ++roll)
    {
        CreatureAppearance appearance = rollCombination(catalogId, space, random);
        if(!isTaken(appearance, taken))
            return appearance;
    }

    // The generator keeps hitting used combinations: pick among the free ones directly
    if(total <= ENUMERATION_LIMIT)
    {
        uint64_t free = total - static_cast<uint64_t>(used.size());
        uint64_t wanted = static_cast<uint64_t>(random(0, 0xFFFFFFFFu)) % free;
        for(uint64_t index = 0; index < total; ++index)
        {
            CreatureAppearance appearance = makeCombination(catalogId, space, index);
            if(isTaken(appearance, used))
                continue;

            if(wanted == 0)
                return appearance;

            --wanted;
        }
    }

    // Too many combinations to list and still no free one: the space is huge, keep rolling
    while(true)
    {
        CreatureAppearance appearance = rollCombination(catalogId, space, random);
        if(!isTaken(appearance, taken))
            return appearance;
    }
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
