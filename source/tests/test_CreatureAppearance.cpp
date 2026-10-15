/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BOOST_TEST_MODULE CreatureAppearance
#include "BoostTestTargetConfig.h"

#include "game/CreatureAppearance.h"
#include "network/ODPacket.h"
#include "render/PortraitManifest.h"
#include "render/PortraitManifestRegistry.h"

#include <cstdio>
#include <fstream>
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

namespace
{
bool existsInList(const std::set<std::string>* folders, const std::string& id)
{
    return folders->count(id) != 0;
}

std::string resolveWithFolders(const std::set<std::string>& folders, const std::string& mesh,
    const std::string& gender)
{
    CreatureAppearanceLogic::CatalogExistsFunction exists =
        std::bind(&existsInList, &folders, std::placeholders::_1);
    return CreatureAppearanceLogic::resolveCatalogId(mesh, gender, exists);
}
}

BOOST_AUTO_TEST_CASE(test_ResolveCatalogId)
{
    // Folders as they exist: some with a gender suffix, some only for the original gender
    std::set<std::string> folders;
    folders.insert("Dwarf1.mesh-female");
    folders.insert("Kobold.mesh");
    folders.insert("Kobold.mesh-female");
    folders.insert("Elf.mesh");
    folders.insert("Elf.mesh-male");

    // With suffix
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Dwarf1.mesh", "Female"), "Dwarf1.mesh-female");
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Kobold.mesh", "Female"), "Kobold.mesh-female");
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Elf.mesh", "Male"), "Elf.mesh-male");

    // Original gender: no folder with this suffix, the folder without suffix is used
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Kobold.mesh", "Male"), "Kobold.mesh");
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Elf.mesh", "Female"), "Elf.mesh");
    BOOST_CHECK_EQUAL(resolveWithFolders(folders, "Kobold.mesh", ""), "Kobold.mesh");

    // No folder at all: fallback
    BOOST_CHECK(resolveWithFolders(folders, "Dwarf1.mesh", "Male").empty());
    BOOST_CHECK(resolveWithFolders(folders, "Troll.mesh", "Male").empty());
    BOOST_CHECK(resolveWithFolders(folders, "Troll.mesh", "").empty());
    BOOST_CHECK(resolveWithFolders(folders, "", "Male").empty());
}

BOOST_AUTO_TEST_CASE(test_ResolveCatalogIdWithFixtures)
{
    PortraitManifestRegistry registry;
    registry.setAssetRoot(getVariantsDirectory());
    CreatureAppearanceLogic::CatalogExistsFunction exists =
        std::bind(&PortraitManifestRegistry::hasCatalog, &registry, std::placeholders::_1);

    // Knight.mesh-male exists with suffix, Goblin.mesh only without suffix
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Knight.mesh", "Male", exists), "Knight.mesh-male");
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Goblin.mesh", "Male", exists), "Goblin.mesh");
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Goblin.mesh", "Female", exists), "Goblin.mesh");
    BOOST_CHECK(CreatureAppearanceLogic::resolveCatalogId("Knight.mesh", "Female", exists).empty());

    // Elf.mesh and Elf.mesh-male both exist: the suffix folder wins for its gender, the plain folder is
    // the original gender (and the one used without gender)
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Elf.mesh", "Male", exists), "Elf.mesh-male");
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Elf.mesh", "Female", exists), "Elf.mesh");
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::resolveCatalogId("Elf.mesh", "", exists), "Elf.mesh");
    const PortraitManifest* elfMale = registry.getManifest("Elf.mesh-male");
    const PortraitManifest* elfPlain = registry.getManifest("Elf.mesh");
    BOOST_REQUIRE(elfMale != nullptr);
    BOOST_REQUIRE(elfPlain != nullptr);
    BOOST_CHECK_EQUAL(elfMale->getOptionsOfSlot("hair").size(), 2u);
    BOOST_CHECK_EQUAL(elfPlain->getOptionsOfSlot("hair").size(), 1u);
    BOOST_CHECK(CreatureAppearanceLogic::resolveCatalogId("Nothing.mesh", "Male", exists).empty());

    // The folder without suffix has a usable manifest
    const PortraitManifest* goblin = registry.getManifest("Goblin.mesh");
    BOOST_REQUIRE(goblin != nullptr);
    BOOST_CHECK_EQUAL(goblin->getSlots().size(), 1u);
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

namespace
{
//! Always rolls the lowest value: it can only ever produce the first combination
uint32_t lowestValue(uint32_t min, uint32_t)
{
    return min;
}

bool isLookOfKnight(const CreatureAppearance& appearance)
{
    return appearance == makeKnightLook(1, 1) || appearance == makeKnightLook(1, 2) ||
        appearance == makeKnightLook(2, 1) || appearance == makeKnightLook(2, 2);
}
}

BOOST_AUTO_TEST_CASE(test_CombinationCount)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    // hair 2 x helmet 2 x scar 1
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::countCombinations(manifest), 4u);

    PortraitManifest reduced;
    BOOST_REQUIRE(loadReducedKnight(reduced));
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::countCombinations(reduced), 1u);

    // Big spaces saturate instead of overflowing
    const uint64_t biggest = 0xFFFFFFFFFFFFFFFFull;
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(3, 4), 12u);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(0, biggest), 0u);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(biggest, 1), biggest);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(biggest, 2), biggest);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(0x100000000ull, 0x100000000ull), biggest);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(0xFFFFFFFFull, 0x100000001ull), biggest);
    BOOST_CHECK_EQUAL(CreatureAppearanceLogic::multiplySaturating(0x100000000ull, 0xFFFFFFFFull), 0xFFFFFFFF00000000ull);
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnSpaceExhausted)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    // All four combinations are in use: only now a duplicate is accepted, and the call ends
    CountingRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&CountingRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);

    std::vector<CreatureAppearance> taken;
    taken.push_back(makeKnightLook(1, 1));
    taken.push_back(makeKnightLook(1, 2));
    taken.push_back(makeKnightLook(2, 1));
    taken.push_back(makeKnightLook(2, 2));
    CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
    BOOST_CHECK_EQUAL(appearance.getChoices().size(), 3u);
    BOOST_CHECK(isLookOfKnight(appearance));

    // The same look listed twice does not count twice: three different looks leave one free
    std::vector<CreatureAppearance> repeated;
    repeated.push_back(makeKnightLook(1, 1));
    repeated.push_back(makeKnightLook(1, 1));
    repeated.push_back(makeKnightLook(1, 2));
    repeated.push_back(makeKnightLook(1, 2));
    CreatureAppearance free = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, repeated);
    BOOST_CHECK(free == makeKnightLook(2, 1) || free == makeKnightLook(2, 2));
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnNearlyFull)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    std::vector<CreatureAppearance> taken;
    taken.push_back(makeKnightLook(1, 1));
    taken.push_back(makeKnightLook(1, 2));
    taken.push_back(makeKnightLook(2, 1));

    // A usual generator finds the only free combination
    TestRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&TestRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);
    BOOST_CHECK(CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken) == makeKnightLook(2, 2));

    // A generator that only produces a used combination does not stop the call: after the roll budget the
    // free combination is chosen directly
    CreatureAppearanceLogic::RandomFunction stuck = lowestValue;
    BOOST_CHECK(CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, stuck, taken) == makeKnightLook(2, 2));

    // Every hole is found, wherever it is
    for(uint32_t hair = 1; hair <= 2; ++hair)
    {
        for(uint32_t helmet = 1; helmet <= 2; ++helmet)
        {
            std::vector<CreatureAppearance> others;
            for(uint32_t h = 1; h <= 2; ++h)
            {
                for(uint32_t m = 1; m <= 2; ++m)
                {
                    if(h != hair || m != helmet)
                        others.push_back(makeKnightLook(h, m));
                }
            }
            TestRandom again;
            CreatureAppearanceLogic::RandomFunction rollAgain =
                std::bind(&TestRandom::next, &again, std::placeholders::_1, std::placeholders::_2);
            BOOST_CHECK(CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, rollAgain, others) ==
                makeKnightLook(hair, helmet));
            BOOST_CHECK(CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, stuck, others) ==
                makeKnightLook(hair, helmet));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_FirstSpawnOtherCatalogOrLooksDoNotCount)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));

    // Many looks of other catalog ids, and looks that use an option the manifest no longer has, never use up
    // the space of this one
    std::vector<CreatureAppearance> taken;
    for(uint32_t i = 0; i < 1000; ++i)
    {
        CreatureAppearance other = makeKnightLook(1 + i % 2, 1 + (i / 2) % 2);
        other.setCatalogId("Orc.mesh-male");
        taken.push_back(other);
    }
    taken.push_back(makeKnightLook(3, 1));
    taken.push_back(makeKnightLook(1, 9));
    taken.push_back(makeKnightLook(1, 1));
    taken.push_back(makeKnightLook(1, 2));
    taken.push_back(makeKnightLook(2, 1));

    CreatureAppearanceLogic::RandomFunction stuck = lowestValue;
    BOOST_CHECK(CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, stuck, taken) == makeKnightLook(2, 2));

    // A one combination manifest is exhausted by that combination
    PortraitManifest reduced;
    BOOST_REQUIRE(loadReducedKnight(reduced));
    std::vector<CreatureAppearance> none;
    CreatureAppearance only = CreatureAppearanceLogic::pickRandom(reduced, KNIGHT_ID, stuck, none);
    BOOST_CHECK(only == makeKnightLook(1, 1));
    std::vector<CreatureAppearance> one;
    one.push_back(only);
    BOOST_CHECK(CreatureAppearanceLogic::pickRandom(reduced, KNIGHT_ID, stuck, one) == only);
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

BOOST_AUTO_TEST_CASE(test_PacketRoundTrip)
{
    // Creature::exportToPacket appends the token as a string after the progress data,
    // Creature::importFromPacket reads it back; an empty string means no appearance
    CreatureAppearance appearance = makeKnightLook(2, 1);
    ODPacket packet;
    packet << std::string("other data") << CreatureAppearanceLogic::toToken(appearance)
        << CreatureAppearanceLogic::toToken(CreatureAppearance());

    std::string before;
    std::string token;
    std::string emptyToken;
    BOOST_REQUIRE(packet >> before >> token >> emptyToken);
    BOOST_CHECK_EQUAL(before, "other data");

    CreatureAppearance received;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(token, received));
    BOOST_CHECK(received == appearance);

    // The creature without appearance arrives as an empty string and stays empty
    BOOST_CHECK(emptyToken.empty());
    CreatureAppearance none;
    BOOST_CHECK(!CreatureAppearanceLogic::fromToken(emptyToken, none));
    BOOST_CHECK(none.isEmpty());
}

BOOST_AUTO_TEST_CASE(test_AppearanceNotificationRoundTrip)
{
    // ServerNotificationType::creatureAppearance: creature name, then the token
    CreatureAppearance appearance = makeKnightLook(1, 2);
    ODPacket packet;
    packet << std::string("Knight 3") << CreatureAppearanceLogic::toToken(appearance);

    std::string name;
    std::string token;
    BOOST_REQUIRE(packet >> name >> token);
    BOOST_CHECK_EQUAL(name, "Knight 3");

    CreatureAppearance received;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(token, received));
    BOOST_CHECK(received == appearance);
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

namespace
{
std::string getRetryManifestPath()
{
    return getVariantsDirectory() + "../retry/manifest.cfg";
}

void writeRetryManifest(const std::string& text)
{
    std::ofstream file(getRetryManifestPath().c_str(), std::ios::out | std::ios::trunc);
    file << text;
}
}

BOOST_AUTO_TEST_CASE(test_RegistryRetriesFailedManifests)
{
    const std::string validManifest =
        "Base\t../neutral-bases/Knight.mesh-male.png\n"
        "Slot\thair\t4\t2\t8\t8\n"
        "Slot\thelmet\t2\t0\t12\t10\n"
        "Slot\tscar\t5\t10\t4\t4\n"
        "Option\thair\t1\tbraid\t../variants/Knight.mesh-male/hair-1-braid.png\n"
        "Option\thair\t2\tbald\t../variants/Knight.mesh-male/hair-2-bald.png\n"
        "Option\thelmet\t1\tplain\t../variants/Knight.mesh-male/helmet-1-plain.png\n"
        "Option\thelmet\t2\thorned\t../variants/Knight.mesh-male/helmet-2-horned.png\n"
        "Option\tscar\t1\tcheek\t../variants/Knight.mesh-male/scar-1-cheek.png\n";

    std::remove(getRetryManifestPath().c_str());
    PortraitManifestRegistry registry;
    registry.setAssetRoot(getVariantsDirectory());
    const std::string id = "../retry";

    // Missing: no manifest, reported once
    BOOST_CHECK(registry.getManifest(id) == nullptr);
    BOOST_CHECK(!registry.takeMessages().empty());

    // Invalid (no base): still nothing, and the file is not read again before the retry
    writeRetryManifest("Slot\thair\t4\t2\t8\t8\n");
    BOOST_CHECK(registry.getManifest(id) == nullptr);
    BOOST_CHECK(registry.takeMessages().empty());
    BOOST_CHECK(registry.retryFailed());
    BOOST_CHECK(registry.getManifest(id) == nullptr);
    BOOST_CHECK(!registry.takeMessages().empty());

    // The same problem again is not reported again
    BOOST_CHECK(registry.retryFailed());
    BOOST_CHECK(registry.getManifest(id) == nullptr);
    BOOST_CHECK(registry.takeMessages().empty());

    // Repaired: found after the retry, without a restart
    writeRetryManifest(validManifest);
    BOOST_CHECK(registry.getManifest(id) == nullptr);
    BOOST_CHECK(registry.retryFailed());
    const PortraitManifest* manifest = registry.getManifest(id);
    BOOST_REQUIRE(manifest != nullptr);
    BOOST_CHECK_EQUAL(manifest->getSlots().size(), 3u);

    // Loaded manifests are kept and a retry without failures does nothing
    BOOST_CHECK(!registry.retryFailed());
    BOOST_CHECK(registry.getManifest(id) == manifest);

    // The creature gets its look like an old save: the stable pick from the name, the same every time
    CreatureAppearance first = CreatureAppearanceLogic::pickStable(*manifest, id, "Late Knight");
    CreatureAppearance second = CreatureAppearanceLogic::pickStable(*manifest, id, "Late Knight");
    BOOST_CHECK(first == second);
    BOOST_CHECK_EQUAL(first.getChoices().size(), 3u);

    std::remove(getRetryManifestPath().c_str());
}

BOOST_AUTO_TEST_CASE(test_StoredLookCheckedWhenManifestBecomesValid)
{
    const std::string validManifest =
        "Base	../neutral-bases/Knight.mesh-male.png\n"
        "Slot	hair	4	2	8	8\n"
        "Slot	helmet	2	0	12	10\n"
        "Slot	scar	5	10	4	4\n"
        "Option	hair	1	braid	../variants/Knight.mesh-male/hair-1-braid.png\n"
        "Option	hair	2	bald	../variants/Knight.mesh-male/hair-2-bald.png\n"
        "Option	helmet	1	plain	../variants/Knight.mesh-male/helmet-1-plain.png\n"
        "Option	helmet	2	horned	../variants/Knight.mesh-male/helmet-2-horned.png\n"
        "Option	scar	1	cheek	../variants/Knight.mesh-male/scar-1-cheek.png\n";

    std::remove(getRetryManifestPath().c_str());
    PortraitManifestRegistry registry;
    registry.setAssetRoot(getVariantsDirectory());
    const std::string id = "../retry";

    // A creature loaded while the manifest was invalid keeps a stored look that uses a removed option (hair 3)
    CreatureAppearance stored = makeKnightLook(3, 2);
    stored.setCatalogId(id);
    stored.getChoices()[2].mNumber = 1;
    writeRetryManifest("Slot	hair	4	2	8	8\n");
    BOOST_CHECK(registry.getManifest(id) == nullptr);

    // The manifest is repaired: after the retry the stored look is checked like at load
    writeRetryManifest(validManifest);
    BOOST_CHECK(registry.retryFailed());
    const PortraitManifest* manifest = registry.getManifest(id);
    BOOST_REQUIRE(manifest != nullptr);

    CreatureAppearance replaced = stored;
    BOOST_CHECK(CreatureAppearanceLogic::validate(*manifest, id, "Old Knight", replaced));
    BOOST_CHECK(replaced.getChoice("hair") == 1 || replaced.getChoice("hair") == 2);
    // the options that still exist stay
    BOOST_CHECK_EQUAL(replaced.getChoice("helmet"), 2u);
    BOOST_CHECK_EQUAL(replaced.getChoice("scar"), 1u);
    // the replacement is stable: the same name gives the same hair every time
    CreatureAppearance again = stored;
    BOOST_CHECK(CreatureAppearanceLogic::validate(*manifest, id, "Old Knight", again));
    BOOST_CHECK(again == replaced);

    // A look that is fine stays as it is and nothing is reported as changed
    CreatureAppearance fine = makeKnightLook(2, 1);
    fine.setCatalogId(id);
    CreatureAppearance copy = fine;
    BOOST_CHECK(!CreatureAppearanceLogic::validate(*manifest, id, "Old Knight", copy));
    BOOST_CHECK(copy == fine);

    std::remove(getRetryManifestPath().c_str());
}

BOOST_AUTO_TEST_CASE(test_CreaturesWithoutCatalogAreLeftAlone)
{
    using CreatureAppearanceLogic::needsAppearanceCheck;
    // No catalog id, same generation: never looked at, whatever else is true
    BOOST_CHECK(!needsAppearanceCheck(true, false, true, 3, 3));
    BOOST_CHECK(!needsAppearanceCheck(false, false, true, 3, 3));
    // A new generation (folders may have appeared) lets it be looked at once
    BOOST_CHECK(needsAppearanceCheck(true, false, true, 3, 4));
    // With a catalog id nothing changes: empty or unchecked looks are checked, checked ones are not
    BOOST_CHECK(needsAppearanceCheck(true, false, false, 0, 0));
    BOOST_CHECK(needsAppearanceCheck(false, false, false, 0, 7));
    BOOST_CHECK(!needsAppearanceCheck(false, true, false, 0, 7));

    // The registry answers hasCatalog from memory until the catalogs are invalidated (one file lookup per period)
    std::remove(getRetryManifestPath().c_str());
    PortraitManifestRegistry registry;
    registry.setAssetRoot(getVariantsDirectory());
    const std::string id = "../retry";
    uint32_t generation = registry.getCatalogGeneration();
    BOOST_CHECK(!registry.hasCatalog(id));
    writeRetryManifest("Slot	hair	4	2	8	8\n");
    BOOST_CHECK(!registry.hasCatalog(id));
    registry.invalidateCatalogs();
    BOOST_CHECK(registry.getCatalogGeneration() != generation);
    BOOST_CHECK(registry.hasCatalog(id));
    std::remove(getRetryManifestPath().c_str());
}
