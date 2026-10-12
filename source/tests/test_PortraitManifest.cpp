/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BOOST_TEST_MODULE PortraitManifest
#include "BoostTestTargetConfig.h"

#include "render/DungeonbookAppearanceConfig.h"
#include "render/PortraitManifest.h"

#include <sstream>
#include <string>
#include <vector>

namespace
{
//! The fixtures live in fixtures/portraits next to this source file
std::string getFixtureDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    std::string testsDirectory = (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
    return testsDirectory + "/fixtures/portraits/";
}

std::string getManifestPath(const std::string& catalogId)
{
    return getFixtureDirectory() + "variants/" + catalogId + "/manifest.cfg";
}

std::string getManifestDirectory(const std::string& catalogId)
{
    return getFixtureDirectory() + "variants/" + catalogId;
}
}

BOOST_AUTO_TEST_CASE(test_ReadPngSize)
{
    uint32_t width = 0;
    uint32_t height = 0;
    BOOST_CHECK(PortraitManifest::readPngSize(getFixtureDirectory() + "neutral-bases/Knight.mesh-male.png",
        width, height));
    BOOST_CHECK_EQUAL(width, 16u);
    BOOST_CHECK_EQUAL(height, 32u);

    // Not a png and not there
    BOOST_CHECK(!PortraitManifest::readPngSize(getManifestPath("Knight.mesh-male"), width, height));
    BOOST_CHECK(!PortraitManifest::readPngSize(getFixtureDirectory() + "nothing.png", width, height));
}

BOOST_AUTO_TEST_CASE(test_ValidManifest)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(manifest.loadFromFile(getManifestPath("Knight.mesh-male")));
    BOOST_CHECK(manifest.getErrors().empty());
    BOOST_CHECK_EQUAL(manifest.getBaseWidth(), 16u);
    BOOST_CHECK_EQUAL(manifest.getBaseHeight(), 32u);

    // Slots keep the draw order of the file
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    BOOST_REQUIRE_EQUAL(slots.size(), 3u);
    BOOST_CHECK_EQUAL(slots[0].mName, "hair");
    BOOST_CHECK_EQUAL(slots[1].mName, "helmet");
    BOOST_CHECK_EQUAL(slots[2].mName, "scar");
    BOOST_CHECK_EQUAL(slots[1].mX, 2u);
    BOOST_CHECK_EQUAL(slots[1].mWidth, 12u);
    BOOST_CHECK_EQUAL(slots[1].mHeight, 10u);

    BOOST_CHECK_EQUAL(manifest.getOptions().size(), 5u);
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("hair").size(), 2u);
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("helmet").size(), 2u);
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("scar").size(), 1u);
    BOOST_CHECK(manifest.getOptionsOfSlot("eyes").empty());

    const PortraitManifest::Option* option = manifest.findOption("helmet", 2);
    BOOST_REQUIRE(option != nullptr);
    BOOST_CHECK_EQUAL(option->mName, "horned");
    BOOST_CHECK_EQUAL(option->mFile, "helmet-2-horned.png");
    BOOST_CHECK(manifest.findOption("helmet", 3) == nullptr);
    BOOST_CHECK(manifest.findSlot("scar") != nullptr);
    BOOST_CHECK(manifest.findSlot("neck") == nullptr);
}

BOOST_AUTO_TEST_CASE(test_MissingPartFileIsDropped)
{
    PortraitManifest manifest;
    // One of two options is gone, the manifest stays usable with the other one
    BOOST_REQUIRE(manifest.loadFromFile(getManifestPath("Dwarf1.mesh-female")));
    BOOST_CHECK_EQUAL(manifest.getErrors().size(), 1u);
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("hair").size(), 1u);
    BOOST_CHECK(manifest.findOption("hair", 1) != nullptr);
    BOOST_CHECK(manifest.findOption("hair", 2) == nullptr);
}

BOOST_AUTO_TEST_CASE(test_WrongPartSizeIsDropped)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(manifest.loadFromFile(getManifestPath("Orc.mesh-male")));
    BOOST_CHECK_EQUAL(manifest.getErrors().size(), 1u);
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("hair").size(), 1u);
    BOOST_CHECK(manifest.findOption("hair", 1) != nullptr);
    BOOST_CHECK(manifest.findOption("hair", 2) == nullptr);
}

BOOST_AUTO_TEST_CASE(test_InvalidManifestAsAWhole)
{
    // Base file does not exist
    PortraitManifest missingBase;
    BOOST_CHECK(!missingBase.loadFromFile(getManifestPath("Troll.mesh-male")));
    BOOST_CHECK(!missingBase.getErrors().empty());

    // Manifest does not exist
    PortraitManifest missingManifest;
    BOOST_CHECK(!missingManifest.loadFromFile(getManifestPath("Nothing.mesh-male")));
    BOOST_CHECK(!missingManifest.getErrors().empty());

    // Base is fine, but no part is usable
    PortraitManifest noParts;
    std::istringstream text(
        "Base\t../../neutral-bases/Knight.mesh-male.png\n"
        "Slot\thair\t4\t2\t8\t8\n"
        "Option\thair\t1\tgone\tnot-there.png\n");
    BOOST_CHECK(!noParts.loadFromStream(text, getManifestDirectory("Knight.mesh-male"), "inline"));
    BOOST_CHECK(noParts.getSlots().empty());

    // No Base line
    PortraitManifest noBase;
    std::istringstream noBaseText("Slot\thair\t4\t2\t8\t8\n");
    BOOST_CHECK(!noBase.loadFromStream(noBaseText, getManifestDirectory("Knight.mesh-male"), "inline"));
}

BOOST_AUTO_TEST_CASE(test_SlotRules)
{
    // Out of draw order, unknown name, listed twice, outside of the base: all dropped
    PortraitManifest manifest;
    std::istringstream text(
        "# comment\n"
        "Base\t../../neutral-bases/Knight.mesh-male.png\n"
        "Slot\thelmet\t2\t0\t12\t10\n"
        "Slot\thair\t4\t2\t8\t8\n"
        "Slot\twings\t0\t0\t4\t4\n"
        "Slot\tscar\t5\t10\t4\t4\n"
        "Slot\tscar\t5\t10\t4\t4\n"
        "Slot\tneck\t14\t30\t8\t8\n"
        "Option\thelmet\t1\tplain\thelmet-1-plain.png\n"
        "Option\thair\t1\tbraid\thair-1-braid.png\n"
        "Option\tscar\t1\tcheek\tscar-1-cheek.png\n"
        "Option\tscar\t1\tagain\tscar-1-cheek.png\n");
    BOOST_REQUIRE(manifest.loadFromStream(text, getManifestDirectory("Knight.mesh-male"), "inline"));
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    BOOST_REQUIRE_EQUAL(slots.size(), 2u);
    BOOST_CHECK_EQUAL(slots[0].mName, "helmet");
    BOOST_CHECK_EQUAL(slots[1].mName, "scar");
    BOOST_CHECK_EQUAL(manifest.getOptionsOfSlot("scar").size(), 1u);
    // hair came after helmet and is out of order, so its option has no slot
    BOOST_CHECK(manifest.findOption("hair", 1) == nullptr);
    BOOST_CHECK(!manifest.getErrors().empty());
}

BOOST_AUTO_TEST_CASE(test_ConfigDefaultsAndValues)
{
    DungeonbookAppearanceConfig missing;
    BOOST_CHECK(!missing.loadFromFile(getFixtureDirectory() + "no-such-config.cfg"));
    BOOST_CHECK_EQUAL(missing.getMaxCachedPictures(), DungeonbookAppearanceConfig::DEFAULT_MAX_CACHED_PICTURES);
    BOOST_CHECK_EQUAL(missing.getMaxCacheMegabytes(), DungeonbookAppearanceConfig::DEFAULT_MAX_CACHE_MEGABYTES);
    BOOST_CHECK(missing.getAssetRoot().empty());
    BOOST_CHECK_EQUAL(missing.getWarnings().size(), 1u);

    // The shipped config is complete: no warning, own values
    std::string shipped = getFixtureDirectory() + "../../../../config/dungeonbook-appearance.cfg";
    DungeonbookAppearanceConfig config;
    BOOST_REQUIRE(config.loadFromFile(shipped));
    BOOST_CHECK(config.getWarnings().empty());
    BOOST_CHECK_EQUAL(config.getAssetRoot(), "materials/portraits/variants");

    // Entries given and entries left out or broken
    DungeonbookAppearanceConfig partial;
    std::istringstream text(
        "AssetRoot\tsome/folder\n"
        "MaxCachedPictures\t12\n"
        "MaxCacheMegabytes\tlots\n");
    partial.loadFromStream(text, "inline");
    BOOST_CHECK_EQUAL(partial.getAssetRoot(), "some/folder");
    BOOST_CHECK_EQUAL(partial.getMaxCachedPictures(), 12u);
    BOOST_CHECK_EQUAL(partial.getMaxCacheMegabytes(), DungeonbookAppearanceConfig::DEFAULT_MAX_CACHE_MEGABYTES);
    BOOST_CHECK(!partial.getWarnings().empty());
}

BOOST_AUTO_TEST_CASE(test_OptionReflection)
{
    std::string header = "Base\t../../neutral-bases/Knight.mesh-male.png\nSlot\thair\t4\t2\t8\t8\n";
    std::istringstream input(header +
        "Option\thair\t1\tbraid\thair-1-braid.png\tflip-x\n"
        "Option\thair\t2\tbald\thair-2-bald.png\n");
    PortraitManifest manifest;
    BOOST_REQUIRE(manifest.loadFromStream(input, getManifestDirectory("Knight.mesh-male"), "reflection"));
    BOOST_REQUIRE(manifest.findOption("hair", 1) != nullptr);
    BOOST_CHECK(manifest.findOption("hair", 1)->mFlipX);
    BOOST_REQUIRE(manifest.findOption("hair", 2) != nullptr);
    BOOST_CHECK(!manifest.findOption("hair", 2)->mFlipX);
    std::istringstream invalid(header + "Option\thair\t1\tbraid\thair-1-braid.png\trotate\n");
    BOOST_CHECK(!manifest.loadFromStream(invalid, getManifestDirectory("Knight.mesh-male"), "invalid"));
    BOOST_CHECK(!manifest.getErrors().empty());
}
