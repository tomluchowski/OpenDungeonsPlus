/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BOOST_TEST_MODULE DungeonbookQuirks
#include "BoostTestTargetConfig.h"

#include "game/CreatureAppearance.h"
#include "render/DungeonbookQuirks.h"
#include "render/PortraitManifest.h"

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* const KNIGHT_ID = "Knight.mesh-male";
const char* const BRAID_TEXT = "Braid remark";
const char* const HORNED_TEXT = "Horned remark";
const char* const CHEEK_TEXT = "Cheek remark";

std::string getTestsDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}

//! Knight fixture: hair 1-2 (braid, bald), helmet 1-2 (plain, horned), scar 1 (cheek)
bool loadKnight(PortraitManifest& manifest)
{
    return manifest.loadFromFile(getTestsDirectory() + "/fixtures/portraits/variants/" + KNIGHT_ID + "/manifest.cfg");
}

void addChoice(CreatureAppearance& appearance, const std::string& slot, uint32_t number)
{
    CreatureAppearance::Choice choice;
    choice.mSlot = slot;
    choice.mNumber = number;
    appearance.getChoices().push_back(choice);
}

//! hair braid, helmet horned, scar cheek
CreatureAppearance makeKnightLook()
{
    CreatureAppearance appearance;
    appearance.setCatalogId(KNIGHT_ID);
    addChoice(appearance, "hair", 1);
    addChoice(appearance, "helmet", 2);
    addChoice(appearance, "scar", 1);
    return appearance;
}

//! Remarks for the three parts of makeKnightLook() and for two parts that the look does not use
void loadTestQuirks(DungeonbookQuirks& quirks)
{
    std::istringstream is(
        "# test remarks\n"
        "hair\tbraid\tBraid remark\n"
        "helmet\thorned\tHorned remark\n"
        "scar\tcheek\tCheek remark\n"
        "hair\tbald\tBald remark\n"
        "helmet\tplain\tPlain remark\n");
    quirks.loadFromStream(is, "test");
}

std::vector<std::string> makeVector(const std::string& a, const std::string& b)
{
    std::vector<std::string> result;
    result.push_back(a);
    result.push_back(b);
    return result;
}
}

BOOST_AUTO_TEST_CASE(test_ParseValid)
{
    DungeonbookQuirks quirks;
    loadTestQuirks(quirks);
    BOOST_CHECK(quirks.getWarnings().empty());
    BOOST_CHECK_EQUAL(quirks.size(), 5u);
    const std::string* text = quirks.find("hair", "braid");
    BOOST_REQUIRE(text != nullptr);
    BOOST_CHECK_EQUAL(*text, BRAID_TEXT);
    BOOST_CHECK(quirks.find("hair", "unknown") == nullptr);
    BOOST_CHECK(quirks.find("eyes", "braid") == nullptr);
}

BOOST_AUTO_TEST_CASE(test_ParseBadLines)
{
    DungeonbookQuirks quirks;
    std::istringstream is(
        "hair\tbraid\tFirst\n"
        "hair\tbraid\tSecond\n"
        "hair\tonlytwo\n"
        "\tbald\tNo slot\n"
        "hair\tbald\t\n"
        "hair\tcrest\tToo\tmany columns\n"
        "eyes\teyepatch\tFine # trailing comment\n");
    quirks.loadFromStream(is, "bad");

    // The first of two equal keys wins, the broken lines are dropped, nothing else is lost
    BOOST_CHECK_EQUAL(quirks.size(), 2u);
    BOOST_REQUIRE(quirks.find("hair", "braid") != nullptr);
    BOOST_CHECK_EQUAL(*quirks.find("hair", "braid"), "First");
    BOOST_REQUIRE(quirks.find("eyes", "eyepatch") != nullptr);
    BOOST_CHECK_EQUAL(*quirks.find("eyes", "eyepatch"), "Fine");
    BOOST_CHECK_EQUAL(quirks.getWarnings().size(), 5u);
}

BOOST_AUTO_TEST_CASE(test_MissingFile)
{
    DungeonbookQuirks quirks;
    BOOST_CHECK(!quirks.loadFromFile(getTestsDirectory() + "/fixtures/does-not-exist.cfg"));
    BOOST_CHECK_EQUAL(quirks.size(), 0u);
    BOOST_CHECK_EQUAL(quirks.getWarnings().size(), 1u);
}

BOOST_AUTO_TEST_CASE(test_CollectInSlotOrder)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    DungeonbookQuirks quirks;
    loadTestQuirks(quirks);

    std::vector<std::string> candidates = DungeonbookQuirkLogic::collectRemarks(quirks, manifest, makeKnightLook());
    BOOST_REQUIRE_EQUAL(candidates.size(), 3u);
    BOOST_CHECK_EQUAL(candidates[0], BRAID_TEXT);
    BOOST_CHECK_EQUAL(candidates[1], HORNED_TEXT);
    BOOST_CHECK_EQUAL(candidates[2], CHEEK_TEXT);
}

BOOST_AUTO_TEST_CASE(test_CollectSkipsPartsWithoutRemarkOrManifestEntry)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    DungeonbookQuirks quirks;
    loadTestQuirks(quirks);

    // Option 9 of the helmet does not exist in the manifest, the scar has no remark in this config
    CreatureAppearance appearance;
    appearance.setCatalogId(KNIGHT_ID);
    addChoice(appearance, "hair", 2);
    addChoice(appearance, "helmet", 9);
    addChoice(appearance, "scar", 1);
    std::istringstream is("hair\tbald\tBald remark\nhelmet\thorned\tHorned remark\n");
    DungeonbookQuirks reduced;
    reduced.loadFromStream(is, "reduced");

    std::vector<std::string> candidates = DungeonbookQuirkLogic::collectRemarks(reduced, manifest, appearance);
    BOOST_REQUIRE_EQUAL(candidates.size(), 1u);
    BOOST_CHECK_EQUAL(candidates[0], "Bald remark");

    // An empty appearance has no remarks
    CreatureAppearance none;
    BOOST_CHECK(DungeonbookQuirkLogic::collectRemarks(quirks, manifest, none).empty());
}

BOOST_AUTO_TEST_CASE(test_FewCandidatesAreAllShown)
{
    CreatureAppearance appearance = makeKnightLook();
    std::vector<std::string> two = makeVector(BRAID_TEXT, CHEEK_TEXT);
    BOOST_CHECK(DungeonbookQuirkLogic::pickRemarks(two, "Anyone", appearance, DungeonbookQuirkLogic::MAX_REMARKS) == two);
    std::vector<std::string> none;
    BOOST_CHECK(DungeonbookQuirkLogic::pickRemarks(none, "Anyone", appearance, DungeonbookQuirkLogic::MAX_REMARKS).empty());
}

BOOST_AUTO_TEST_CASE(test_PickGoldenValues)
{
    // The values were computed with an independent implementation of the fixed hash (FNV-1a 32 of
    // "<name>|<token>|<text>", the two lowest scores win, shown in candidate order)
    BOOST_REQUIRE_EQUAL(DungeonbookQuirkLogic::MAX_REMARKS, 2u);
    CreatureAppearance appearance = makeKnightLook();
    std::vector<std::string> candidates;
    candidates.push_back(BRAID_TEXT);
    candidates.push_back(HORNED_TEXT);
    candidates.push_back(CHEEK_TEXT);

    BOOST_CHECK(DungeonbookQuirkLogic::pickRemarks(candidates, "Grimbold", appearance, 2) ==
        makeVector(BRAID_TEXT, HORNED_TEXT));
    BOOST_CHECK(DungeonbookQuirkLogic::pickRemarks(candidates, "Ulf", appearance, 2) ==
        makeVector(BRAID_TEXT, CHEEK_TEXT));
    BOOST_CHECK(DungeonbookQuirkLogic::pickRemarks(candidates, "Dag", appearance, 2) ==
        makeVector(HORNED_TEXT, CHEEK_TEXT));
}

BOOST_AUTO_TEST_CASE(test_StablePerCreature)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    DungeonbookQuirks quirks;
    loadTestQuirks(quirks);
    CreatureAppearance appearance = makeKnightLook();

    std::vector<std::string> first = DungeonbookQuirkLogic::selectRemarks(quirks, manifest, appearance, "Grimbold");
    BOOST_REQUIRE_EQUAL(first.size(), 2u);
    for(int i = 0; i < 20; ++i)
        BOOST_CHECK(DungeonbookQuirkLogic::selectRemarks(quirks, manifest, appearance, "Grimbold") == first);

    // Every remark is shown for some creature, so the choice is not stuck on the same two
    std::set<std::string> seen;
    const char* const names[] = {"Grimbold", "Ulf", "Dag", "Snorri", "Vex", "Bree", "Krag", "Hilda"};
    for(std::size_t i = 0; i < 8; ++i)
    {
        std::vector<std::string> picked = DungeonbookQuirkLogic::selectRemarks(quirks, manifest, appearance, names[i]);
        BOOST_CHECK_EQUAL(picked.size(), 2u);
        seen.insert(picked.begin(), picked.end());
    }
    BOOST_CHECK_EQUAL(seen.size(), 3u);
}

BOOST_AUTO_TEST_CASE(test_SameAfterLoading)
{
    // Nothing is saved or sent for the remarks: the appearance travels as its token (save file and network),
    // and the remarks derived from the restored appearance equal the ones derived from the original
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnight(manifest));
    DungeonbookQuirks quirks;
    loadTestQuirks(quirks);
    CreatureAppearance appearance = makeKnightLook();

    CreatureAppearance restored;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(CreatureAppearanceLogic::toToken(appearance), restored));
    BOOST_CHECK(restored == appearance);

    const char* const names[] = {"Grimbold", "Ulf", "Dag", "Snorri"};
    for(std::size_t i = 0; i < 4; ++i)
    {
        BOOST_CHECK(DungeonbookQuirkLogic::selectRemarks(quirks, manifest, appearance, names[i]) ==
            DungeonbookQuirkLogic::selectRemarks(quirks, manifest, restored, names[i]));
    }
}

BOOST_AUTO_TEST_CASE(test_Format)
{
    BOOST_CHECK_EQUAL(DungeonbookQuirkLogic::formatRemarks(std::vector<std::string>()), "");
    BOOST_CHECK_EQUAL(DungeonbookQuirkLogic::formatRemarks(makeVector("One.", "Two.")), "Quirks:\n- One.\n- Two.");
}

BOOST_AUTO_TEST_CASE(test_ShippedConfig)
{
    // config/dungeonbook-quirks.cfg: loads without warnings, the slots are the ones the manifests know
    DungeonbookQuirks quirks;
    BOOST_REQUIRE(quirks.loadFromFile(getTestsDirectory() + "/../../config/dungeonbook-quirks.cfg"));
    BOOST_CHECK(quirks.getWarnings().empty());
    BOOST_CHECK(quirks.size() >= 40u);

    std::ifstream file((getTestsDirectory() + "/../../config/dungeonbook-quirks.cfg").c_str());
    std::set<std::string> slots;
    const char* const known[] = {"build", "outfit", "hair", "ears", "eyes", "nose", "mouth", "chin", "helmet", "scar",
        "neck"};
    for(std::size_t i = 0; i < 11; ++i)
        slots.insert(known[i]);

    std::string line;
    while(std::getline(file, line))
    {
        if(line.empty() || (line[0] == '#'))
            continue;
        std::string slot = line.substr(0, line.find('\t'));
        BOOST_CHECK_MESSAGE(slots.count(slot) == 1, "unknown slot " + slot);
    }

    // The examples of the design are in
    const std::string* patch = quirks.find("eyes", "eyepatch");
    BOOST_REQUIRE(patch != nullptr);
    BOOST_CHECK_EQUAL(*patch, "Collects eye patches, green ones only.");
    BOOST_CHECK(quirks.find("mouth", "cigar") != nullptr);
    BOOST_CHECK(quirks.find("scar", "diagonal") != nullptr);
}
