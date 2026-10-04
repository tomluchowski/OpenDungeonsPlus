/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BOOST_TEST_MODULE CreatureAppearance
#include "BoostTestTargetConfig.h"

#include "game/CreatureAppearance.h"
#include "render/PortraitManifest.h"
#include "render/PortraitManifestRegistry.h"

#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
const char* const KNIGHT_ID = "Knight.mesh-male";

//! The fixtures live in fixtures/portraits next to this source file
std::string getVariantsDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    std::string testsDirectory = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
    return testsDirectory + "/fixtures/portraits/variants/";
}

//! Knight fixture: hair 1-2, helmet 1-2, scar 1 (four different looks)
bool loadKnight(PortraitManifest& manifest)
{
    return manifest.loadFromFile(getVariantsDirectory() + KNIGHT_ID + "/manifest.cfg");
}

//! Same slots, but hair option 2 and helmet option 2 were removed from the manifest
bool loadReducedKnight(PortraitManifest& manifest)
{
    std::istringstream is(
        "Base\t../../neutral-bases/Knight.mesh-male.png\n"
        "Slot\thair\t4\t2\t8\t8\n"
        "Slot\thelmet\t2\t0\t12\t10\n"
        "Slot\tscar\t5\t10\t4\t4\n"
        "Option\thair\t1\tbraid\thair-1-braid.png\n"
        "Option\thelmet\t1\tplain\thelmet-1-plain.png\n"
        "Option\tscar\t1\tcheek\tscar-1-cheek.png\n");
    return manifest.loadFromStream(is, getVariantsDirectory() + KNIGHT_ID, "reduced");
}

//! Simple linear congruential generator for the statistical test
class TestRandom
{
public:
    TestRandom() :
        mState(12345u)
    {
    }

    uint32_t next(uint32_t min, uint32_t max)
    {
        mState = mState * 1664525u + 1013904223u;
        uint32_t span = max - min + 1;
        return min + ((mState >> 8) % span);
    }

private:
    uint32_t mState;
};

//! Cycles through the values: the n-th call returns n modulo the span
class CountingRandom
{
public:
    CountingRandom() :
        mCount(0)
    {
    }

    uint32_t next(uint32_t min, uint32_t max)
    {
        uint32_t value = min + (mCount % (max - min + 1));
        ++mCount;
        return value;
    }

private:
    uint32_t mCount;
};

CreatureAppearance makeKnightLook(uint32_t hair, uint32_t helmet)
{
    CreatureAppearance appearance;
    appearance.setCatalogId(KNIGHT_ID);
    CreatureAppearance::Choice choice;
    choice.mSlot = "hair";
    choice.mNumber = hair;
    appearance.getChoices().push_back(choice);
    choice.mSlot = "helmet";
    choice.mNumber = helmet;
    appearance.getChoices().push_back(choice);
    choice.mSlot = "scar";
    choice.mNumber = 1;
    appearance.getChoices().push_back(choice);
    return appearance;
}
}

BOOST_AUTO_TEST_CASE(test_StableHash)
{
    // FNV-1a, 32 bit: fixed values on every platform
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::stableHash(""), 2166136261u);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::stableHash("a"), 0xE40C292Cu);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::stableHash("foobar"), 0xBF9CF968u);
}

BOOST_AUTO_TEST_CASE(test_MakeCatalogId)
{
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::makeCatalogId("Dwarf1.mesh", "Female"), "Dwarf1.mesh-female");
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::makeCatalogId("Knight.mesh", "Male"), "Knight.mesh-male");
    BOOST_CHECK(CreatureAppearanceLogic::makeCatalogId("Knight.mesh", "").empty());
    BOOST_CHECK(CreatureAppearanceLogic::makeCatalogId("", "Male").empty());
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnEveryOptionPickable)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    TestRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&TestRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);

    std::set<std::pair<std::string, uint32_t> > seen;
    std::vector<CreatureAppearance> none;
    for(uint32_t i = 0; i < 400; ++i)
    {
        CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, none);
        BOOST_CHECK_EQUAL(appearance.getCatalogId(), KNIGHT_ID);

        // No slot stays empty and every choice exists in the manifest
        BOOST_REQUIRE_EQUAL(appearance.getChoices().size(), manifest.getSlots().size());
        for(uint32_t slot = 0; slot < manifest.getSlots().size(); ++slot)
        {
            const CreatureAppearance::Choice& choice = appearance.getChoices()[slot];
            BOOST_CHECK_EQUAL(choice.mSlot, manifest.getSlots()[slot].mName);
            BOOST_CHECK(manifest.findOption(choice.mSlot, choice.mNumber) != nullptr);
            seen.insert(std::make_pair(choice.mSlot, choice.mNumber));
        }
    }

    // Every listed option of every slot was chosen at least once
    for(std::vector<PortraitManifest::Option>::const_iterator it = manifest.getOptions().begin();
        it != manifest.getOptions().end(); ++it)
    {
        BOOST_CHECK_MESSAGE(seen.count(std::make_pair(it->mSlot, it->mNumber)) == 1,
            "option never picked: " + it->mSlot + " " + it->mName);
    }
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnAvoidsDuplicates)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    // The counting generator rolls hair 1, helmet 2, scar 1 first and hair 2, helmet 1, scar 1 next
    CountingRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&CountingRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);

    std::vector<CreatureAppearance> taken;
    taken.push_back(makeKnightLook(1, 2));
    CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
    BOOST_CHECK(appearance == makeKnightLook(2, 1));

    // Another catalog id or another look in the list does not matter
    CountingRandom generator2;
    CreatureAppearanceLogic::RandomFunction random2 =
        std::bind(&CountingRandom::next, &generator2, std::placeholders::_1, std::placeholders::_2);
    std::vector<CreatureAppearance> others;
    others.push_back(makeKnightLook(1, 1));
    CreatureAppearance first = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random2, others);
    BOOST_CHECK(first == makeKnightLook(1, 2));
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnSpaceExhausted)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    // Both looks the counting generator can reach are taken: after the limited tries the last roll is
    // accepted instead of looping forever
    CountingRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&CountingRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);

    std::vector<CreatureAppearance> taken;
    taken.push_back(makeKnightLook(1, 2));
    taken.push_back(makeKnightLook(2, 1));
    CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
    BOOST_CHECK_EQUAL(appearance.getChoices().size(), 3u);
    BOOST_CHECK(appearance == makeKnightLook(1, 2) || appearance == makeKnightLook(2, 1));
}

BOOST_AUTO_TEST_CASE(test_OldSaveStable)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    CreatureAppearance a = CreatureAppearanceLogic::pickStable(manifest, KNIGHT_ID, "Knight 3");
    CreatureAppearance b = CreatureAppearanceLogic::pickStable(manifest, KNIGHT_ID, "Knight 3");
    BOOST_CHECK(a == b);
    BOOST_CHECK_EQUAL(a.getChoices().size(), manifest.getSlots().size());
    for(uint32_t i = 0; i < a.getChoices().size(); ++i)
        BOOST_CHECK(manifest.findOption(a.getChoices()[i].mSlot, a.getChoices()[i].mNumber) != nullptr);

    // Different names spread over the options
    std::set<uint32_t> hairs;
    for(uint32_t i = 0; i < 40; ++i)
    {
        std::ostringstream name;
        name << "Knight " << i;
        hairs.insert(CreatureAppearanceLogic::pickStable(manifest, KNIGHT_ID, name.str()).getChoice("hair"));
    }
    BOOST_CHECK_EQUAL(hairs.size(), 2u);
}

BOOST_AUTO_TEST_CASE(test_TokenRoundTrip)
{
    CreatureAppearance appearance = makeKnightLook(2, 1);
    std::string token = CreatureAppearanceLogic::toToken(appearance);
    BOOST_CHECK_EQUAL(token, "Knight.mesh-male:hair=2,helmet=1,scar=1");
    BOOST_CHECK(token.find_first_of(" \t") == std::string::npos);

    CreatureAppearance loaded;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(token, loaded));
    BOOST_CHECK(loaded == appearance);

    CreatureAppearance empty;
    BOOST_CHECK(CreatureAppearanceLogic::toToken(empty).empty());

    // Malformed tokens give an empty result
    const char* const bad[] = { "", "Knight.mesh-male", ":hair=1", "Knight.mesh-male:hair", "Knight.mesh-male:hair=",
        "Knight.mesh-male:hair=x", "Knight.mesh-male:hair=0", "Knight.mesh-male:=1" };
    for(uint32_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        CreatureAppearance result = makeKnightLook(1, 1);
        BOOST_CHECK_MESSAGE(!CreatureAppearanceLogic::fromToken(bad[i], result), std::string("accepted: ") + bad[i]);
        BOOST_CHECK(result.isEmpty());
    }
}

BOOST_AUTO_TEST_CASE(test_SaveLineOptionalToken)
{
    // The creature line of an old save ends after the effects, a new one carries the appearance token
    CreatureAppearance appearance = makeKnightLook(1, 2);
    std::istringstream oldLine("Knight\t1\t0\tmax\t100\t0\t0\tnone\tnone\tnone\tnone\t0");
    std::istringstream newLine("Knight\t1\t0\tmax\t100\t0\t0\tnone\tnone\tnone\tnone\t0\t"
        + CreatureAppearanceLogic::toToken(appearance));

    std::string field;
    for(uint32_t i = 0; i < 12; ++i)
    {
        BOOST_REQUIRE(oldLine >> field);
        BOOST_REQUIRE(newLine >> field);
    }

    std::string token;
    BOOST_CHECK(!(oldLine >> token));
    BOOST_REQUIRE(newLine >> token);
    CreatureAppearance loaded;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(token, loaded));
    BOOST_CHECK(loaded == appearance);
}

BOOST_AUTO_TEST_CASE(test_ValidateKeepsExistingOptions)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    CreatureAppearance appearance = makeKnightLook(2, 1);
    CreatureAppearance before = appearance;
    BOOST_CHECK(!CreatureAppearanceLogic::validate(manifest, KNIGHT_ID, "Knight 3", appearance));
    BOOST_CHECK(appearance == before);
}

BOOST_AUTO_TEST_CASE(test_ValidateReplacesRemovedOption)
{
    PortraitManifest reduced;
    BOOST_REQUIRE(loadReducedKnight(reduced));
    BOOST_REQUIRE_EQUAL(reduced.getOptionsOfSlot("hair").size(), 1u);

    // hair 2 and helmet 2 are gone, scar 1 stays
    CreatureAppearance appearance = makeKnightLook(2, 2);
    BOOST_CHECK(CreatureAppearanceLogic::validate(reduced, KNIGHT_ID, "Knight 3", appearance));
    BOOST_CHECK(appearance == makeKnightLook(1, 1));

    // With several options left the replacement is the stable pick of the name, the same every time
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    CreatureAppearance broken = makeKnightLook(7, 1);
    CreatureAppearance expected = CreatureAppearanceLogic::pickStable(manifest, KNIGHT_ID, "Knight 3");
    CreatureAppearance first = broken;
    CreatureAppearance second = broken;
    BOOST_CHECK(CreatureAppearanceLogic::validate(manifest, KNIGHT_ID, "Knight 3", first));
    BOOST_CHECK(CreatureAppearanceLogic::validate(manifest, KNIGHT_ID, "Knight 3", second));
    BOOST_CHECK(first == second);
    BOOST_CHECK_EQUAL(first.getChoice("hair"), expected.getChoice("hair"));
    BOOST_CHECK_EQUAL(first.getChoice("helmet"), 1u);
}

BOOST_AUTO_TEST_CASE(test_ValidateFillsMissingSlotAndOtherCatalogId)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    // A slot that was added to the manifest later gets an option
    CreatureAppearance partial;
    partial.setCatalogId(KNIGHT_ID);
    CreatureAppearance::Choice choice;
    choice.mSlot = "hair";
    choice.mNumber = 1;
    partial.getChoices().push_back(choice);
    BOOST_CHECK(CreatureAppearanceLogic::validate(manifest, KNIGHT_ID, "Knight 3", partial));
    BOOST_CHECK_EQUAL(partial.getChoices().size(), manifest.getSlots().size());
    BOOST_CHECK_EQUAL(partial.getChoice("hair"), 1u);
    BOOST_CHECK(partial.getChoice("helmet") != 0);

    // Stored for another base: derived completely from the name
    CreatureAppearance other = makeKnightLook(2, 2);
    other.setCatalogId("Orc.mesh-male");
    BOOST_CHECK(CreatureAppearanceLogic::validate(manifest, KNIGHT_ID, "Knight 3", other));
    BOOST_CHECK(other == CreatureAppearanceLogic::pickStable(manifest, KNIGHT_ID, "Knight 3"));
}

BOOST_AUTO_TEST_CASE(test_RegistryLoadsOnce)
{
    PortraitManifestRegistry registry;
    registry.setAssetRoot(getVariantsDirectory());

    const PortraitManifest* knight = registry.getManifest(KNIGHT_ID);
    BOOST_REQUIRE(knight != nullptr);
    BOOST_CHECK_EQUAL(knight->getSlots().size(), 3u);
    BOOST_CHECK(registry.getManifest(KNIGHT_ID) == knight);

    // No base: missing for good, reported once
    BOOST_CHECK(registry.getManifest("Troll.mesh-male") == nullptr);
    std::vector<std::string> messages = registry.takeMessages();
    BOOST_CHECK(!messages.empty());
    BOOST_CHECK(registry.getManifest("Troll.mesh-male") == nullptr);
    BOOST_CHECK(registry.takeMessages().empty());

    // Unknown id and empty id
    BOOST_CHECK(registry.getManifest("Nothing.mesh-male") == nullptr);
    BOOST_CHECK(registry.getManifest("") == nullptr);
}
