/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BOOST_TEST_MODULE AppearanceCompose
#include "BoostTestTargetConfig.h"

#include "game/CreatureAppearance.h"
#include "render/AppearanceCompose.h"
#include "render/PortraitManifest.h"
#include "render/PortraitTint.h"

#include <set>
#include <string>
#include <vector>

using namespace AppearanceCompose;

namespace
{
const char* const KNIGHT_ID = "Knight.mesh-male";

std::string getTestsDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}

RgbaImage makeSolid(uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    RgbaImage image(width, height);
    for(size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
    {
        image.mPixels[i * 4] = r;
        image.mPixels[i * 4 + 1] = g;
        image.mPixels[i * 4 + 2] = b;
        image.mPixels[i * 4 + 3] = a;
    }
    return image;
}

Part makePart(const std::string& slot, uint32_t x, uint32_t y, const RgbaImage& image)
{
    Part part;
    part.mSlot = slot;
    part.mX = x;
    part.mY = y;
    part.mImage = image;
    return part;
}

void checkPixel(const RgbaImage& image, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    const uint8_t* pixel = &image.mPixels[(static_cast<size_t>(y) * image.mWidth + x) * 4];
    BOOST_CHECK_EQUAL(static_cast<int>(pixel[0]), static_cast<int>(r));
    BOOST_CHECK_EQUAL(static_cast<int>(pixel[1]), static_cast<int>(g));
    BOOST_CHECK_EQUAL(static_cast<int>(pixel[2]), static_cast<int>(b));
    BOOST_CHECK_EQUAL(static_cast<int>(pixel[3]), static_cast<int>(a));
}

//! Solid colour stand-ins for the parts of the Knight fixture, placed by the slots of its manifest
std::vector<Part> makeKnightParts(const PortraitManifest& manifest, bool withScar)
{
    std::vector<Part> parts;
    const std::vector<PortraitManifest::Slot>& slots = manifest.getSlots();
    for(size_t i = 0; i < slots.size(); ++i)
    {
        const PortraitManifest::Slot& slot = slots[i];
        RgbaImage image;
        if(slot.mName == "hair")
            image = makeSolid(slot.mWidth, slot.mHeight, 200, 40, 40, 255);
        else if(slot.mName == "helmet")
            image = makeSolid(slot.mWidth, slot.mHeight, 160, 150, 150, 255);
        else if(slot.mName == "scar")
        {
            if(!withScar)
                continue;
            image = makeSolid(slot.mWidth, slot.mHeight, 220, 220, 220, 128);
        }
        parts.push_back(makePart(slot.mName, slot.mX, slot.mY, image));
    }
    return parts;
}

RgbaImage makeKnightBase()
{
    return makeSolid(16, 32, 120, 100, 90, 255);
}

bool loadKnightManifest(PortraitManifest& manifest)
{
    return manifest.loadFromFile(getTestsDirectory() + "/fixtures/portraits/variants/" + KNIGHT_ID + "/manifest.cfg");
}
}

BOOST_AUTO_TEST_CASE(test_AlphaBlend)
{
    RgbaImage base = makeSolid(4, 4, 100, 100, 100, 255);
    std::vector<Part> parts;
    parts.push_back(makePart("hair", 1, 1, makeSolid(2, 2, 200, 0, 0, 128)));

    RgbaImage result = compose(base, parts, false);
    BOOST_REQUIRE(result.isValid());
    checkPixel(result, 0, 0, 100, 100, 100, 255);
    checkPixel(result, 1, 1, 150, 50, 50, 255);
    checkPixel(result, 2, 2, 150, 50, 50, 255);
    checkPixel(result, 3, 3, 100, 100, 100, 255);

    // Fully transparent parts change nothing, opaque ones replace
    parts.clear();
    parts.push_back(makePart("hair", 0, 0, makeSolid(4, 4, 1, 2, 3, 0)));
    parts.push_back(makePart("ears", 0, 0, makeSolid(1, 1, 9, 8, 7, 255)));
    result = compose(base, parts, false);
    checkPixel(result, 0, 0, 9, 8, 7, 255);
    checkPixel(result, 1, 0, 100, 100, 100, 255);

    // The base itself is not changed
    checkPixel(base, 1, 1, 100, 100, 100, 255);
}

BOOST_AUTO_TEST_CASE(test_PartOrderAndBounds)
{
    RgbaImage base = makeSolid(4, 4, 0, 0, 0, 255);
    std::vector<Part> parts;
    parts.push_back(makePart("hair", 0, 0, makeSolid(2, 2, 255, 0, 0, 255)));
    parts.push_back(makePart("ears", 1, 1, makeSolid(2, 2, 0, 255, 0, 255)));

    // The later part is on top where they overlap
    RgbaImage result = compose(base, parts, false);
    checkPixel(result, 0, 0, 255, 0, 0, 255);
    checkPixel(result, 1, 1, 0, 255, 0, 255);

    std::vector<Part> reversed;
    reversed.push_back(parts[1]);
    reversed.push_back(parts[0]);
    result = compose(base, reversed, false);
    checkPixel(result, 1, 1, 255, 0, 0, 255);

    // A part that reaches over the border is cut, one outside of it is ignored
    parts.clear();
    parts.push_back(makePart("hair", 3, 3, makeSolid(4, 4, 0, 0, 255, 255)));
    parts.push_back(makePart("ears", 9, 9, makeSolid(2, 2, 255, 255, 255, 255)));
    result = compose(base, parts, false);
    BOOST_CHECK_EQUAL(result.mWidth, 4u);
    BOOST_CHECK_EQUAL(result.mHeight, 4u);
    checkPixel(result, 3, 3, 0, 0, 255, 255);
    checkPixel(result, 2, 3, 0, 0, 0, 255);

    // An invalid base gives nothing
    BOOST_CHECK(!compose(RgbaImage(), parts, false).isValid());
}

BOOST_AUTO_TEST_CASE(test_HelmetClipping)
{
    BOOST_CHECK(isHelmetDamageClipped("Knight.mesh-male"));
    BOOST_CHECK(isHelmetDamageClipped("Knight.mesh-female"));
    BOOST_CHECK(isHelmetDamageClipped("Cultist.mesh"));
    BOOST_CHECK(!isHelmetDamageClipped("Orc.mesh-male"));
    BOOST_CHECK(!isHelmetDamageClipped("KnightLike.mesh"));
    BOOST_CHECK(!isHelmetDamageClipped(""));

    // Helmet 4x4 at (2,0): its left two columns are opaque, the right two columns transparent.
    RgbaImage helmet = makeSolid(4, 4, 150, 150, 160, 255);
    for(uint32_t y = 0; y < 4; ++y)
    {
        for(uint32_t x = 2; x < 4; ++x)
            helmet.mPixels[(static_cast<size_t>(y) * 4 + x) * 4 + 3] = 0;
    }
    RgbaImage base = makeSolid(8, 8, 50, 50, 50, 255);
    std::vector<Part> parts;
    parts.push_back(makePart("helmet", 2, 0, helmet));
    // Scar 4x4 at (2,2): rows 2 and 3 lie in the helmet rectangle, rows 4 and 5 below it
    parts.push_back(makePart("scar", 2, 2, makeSolid(4, 4, 255, 255, 255, 255)));

    RgbaImage clipped = compose(base, parts, true);
    // opaque helmet pixel: the scar stays
    checkPixel(clipped, 2, 2, 255, 255, 255, 255);
    checkPixel(clipped, 3, 3, 255, 255, 255, 255);
    // transparent helmet pixel and below the helmet: the scar is cut away, the base shows
    checkPixel(clipped, 4, 2, 50, 50, 50, 255);
    checkPixel(clipped, 5, 3, 50, 50, 50, 255);
    checkPixel(clipped, 2, 4, 50, 50, 50, 255);
    // the helmet itself is not touched
    checkPixel(clipped, 3, 0, 150, 150, 160, 255);

    RgbaImage unclipped = compose(base, parts, false);
    checkPixel(unclipped, 4, 2, 255, 255, 255, 255);
    checkPixel(unclipped, 2, 4, 255, 255, 255, 255);

    // Without a helmet part there is nothing to clip to
    std::vector<Part> onlyScar;
    onlyScar.push_back(parts[1]);
    RgbaImage bare = compose(base, onlyScar, true);
    checkPixel(bare, 4, 2, 255, 255, 255, 255);

    // Only the scar slot is limited, other slots are drawn as they are
    std::vector<Part> withHair = parts;
    withHair.push_back(makePart("hair", 6, 6, makeSolid(2, 2, 255, 0, 0, 255)));
    RgbaImage hairResult = compose(base, withHair, true);
    checkPixel(hairResult, 7, 7, 255, 0, 0, 255);
}

BOOST_AUTO_TEST_CASE(test_ComposePixelHash)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnightManifest(manifest));

    // Fixed look: hair, helmet and the scar of the Knight fixture; the hashes were computed by an
    // independent integer implementation of the blending
    std::vector<Part> parts = makeKnightParts(manifest, true);
    BOOST_REQUIRE_EQUAL(parts.size(), 3u);
    BOOST_CHECK_EQUAL(parts[0].mSlot, "hair");
    BOOST_CHECK_EQUAL(parts[1].mSlot, "helmet");
    BOOST_CHECK_EQUAL(parts[2].mSlot, "scar");

    RgbaImage unclipped = compose(makeKnightBase(), parts, false);
    BOOST_CHECK_EQUAL(hashPixels(unclipped.mPixels), 0x59549e25u);
    // the scar is half transparent on the cheek
    checkPixel(unclipped, 6, 11, 170, 160, 155, 255);

    // The scar of the fixture lies below the helmet, so helmet damage clipping removes it completely
    RgbaImage clipped = compose(makeKnightBase(), parts, true);
    BOOST_CHECK_EQUAL(hashPixels(clipped.mPixels), 0x40678085u);
    RgbaImage noScar = compose(makeKnightBase(), makeKnightParts(manifest, false), false);
    BOOST_CHECK(clipped.mPixels == noScar.mPixels);

    // Same input, same pixels
    BOOST_CHECK(compose(makeKnightBase(), parts, true).mPixels == clipped.mPixels);
}

BOOST_AUTO_TEST_CASE(test_FlattenAndTint)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnightManifest(manifest));
    RgbaImage composed = compose(makeKnightBase(), makeKnightParts(manifest, true), true);

    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> plain = flattenAndTint(composed, 4, nullptr, KNIGHT_ID, "Brak", width, height);
    BOOST_CHECK_EQUAL(width, 4u);
    BOOST_CHECK_EQUAL(height, 8u);
    BOOST_REQUIRE_EQUAL(plain.size(), 4u * 8u * 4u);
    // The bottom left block is bare base
    BOOST_CHECK_EQUAL(static_cast<int>(plain[(7 * 4 + 0) * 4]), 120);
    BOOST_CHECK_EQUAL(static_cast<int>(plain[(7 * 4 + 0) * 4 + 1]), 100);
    BOOST_CHECK_EQUAL(static_cast<int>(plain[(7 * 4 + 0) * 4 + 2]), 90);
    BOOST_CHECK_EQUAL(static_cast<int>(plain[(7 * 4 + 0) * 4 + 3]), 255);

    // Transparent pixels show the dark portrait background
    uint32_t emptyWidth = 0;
    uint32_t emptyHeight = 0;
    std::vector<uint8_t> dark = flattenAndTint(makeSolid(4, 4, 255, 255, 255, 0), 4, nullptr, "", "Brak",
        emptyWidth, emptyHeight);
    BOOST_REQUIRE_EQUAL(dark.size(), 4u);
    BOOST_CHECK_EQUAL(static_cast<int>(dark[0]), 6);
    BOOST_CHECK_EQUAL(static_cast<int>(dark[1]), 5);
    BOOST_CHECK_EQUAL(static_cast<int>(dark[2]), 4);
    BOOST_CHECK_EQUAL(static_cast<int>(dark[3]), 255);

    // Too small or invalid input gives nothing
    BOOST_CHECK(flattenAndTint(makeSolid(3, 3, 0, 0, 0, 255), 4, nullptr, "", "Brak", emptyWidth, emptyHeight).empty());
    BOOST_CHECK(flattenAndTint(RgbaImage(), 4, nullptr, "", "Brak", emptyWidth, emptyHeight).empty());
    BOOST_CHECK(flattenAndTint(composed, 0, nullptr, "", "Brak", emptyWidth, emptyHeight).empty());

    // The base tint file of the game: it has regions for the fixture bases and none for unknown ids
    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/../../config/dungeonbook-base-tints.cfg"));
    BOOST_CHECK(tint.getErrors().empty());
    BOOST_CHECK(tint.hasMesh(KNIGHT_ID));
    BOOST_CHECK(!tint.hasMesh("Troll.mesh-male"));

    // The same creature always gets the same colours
    std::vector<uint8_t> first = flattenAndTint(composed, 4, &tint, KNIGHT_ID, "Brak", width, height);
    std::vector<uint8_t> second = flattenAndTint(composed, 4, &tint, KNIGHT_ID, "Brak", width, height);
    BOOST_CHECK(first == second);
    BOOST_CHECK_EQUAL(hashPixels(first), hashPixels(second));

    // A catalog id without regions is not tinted
    std::vector<uint8_t> unknown = flattenAndTint(composed, 4, &tint, "Troll.mesh-male", "Brak", width, height);
    BOOST_CHECK(unknown == plain);

    // The tint changes the picture, and creatures do not all get the same one
    const char* const names[] = {"Brak", "Zog", "Mira", "Ulf", "Hesta", "Grim", "Tilda", "Rurik"};
    std::set<uint32_t> hashes;
    uint32_t changed = 0;
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        std::vector<uint8_t> tinted = flattenAndTint(composed, 4, &tint, KNIGHT_ID, names[i], width, height);
        BOOST_REQUIRE_EQUAL(tinted.size(), plain.size());
        hashes.insert(hashPixels(tinted));
        if(tinted != plain)
            ++changed;
        for(size_t p = 0; p < tinted.size() / 4; ++p)
            BOOST_CHECK_EQUAL(static_cast<int>(tinted[p * 4 + 3]), 255);
    }
    BOOST_CHECK_GT(changed, 0u);
    BOOST_CHECK_GT(hashes.size(), 1u);
}

BOOST_AUTO_TEST_CASE(test_PictureKey)
{
    CreatureAppearance appearance;
    BOOST_CHECK_EQUAL(makePictureKey(appearance, "Brak"), "|Brak");

    appearance.setCatalogId(KNIGHT_ID);
    CreatureAppearance::Choice hair = {"hair", 1};
    CreatureAppearance::Choice helmet = {"helmet", 2};
    appearance.getChoices().push_back(hair);
    appearance.getChoices().push_back(helmet);
    BOOST_CHECK_EQUAL(makePictureKey(appearance, "Brak"), "Knight.mesh-male:hair=1,helmet=2|Brak");
    // other options or another creature give another picture
    BOOST_CHECK(makePictureKey(appearance, "Zog") != makePictureKey(appearance, "Brak"));
    appearance.getChoices()[0].mNumber = 2;
    BOOST_CHECK(makePictureKey(appearance, "Brak") != "Knight.mesh-male:hair=1,helmet=2|Brak");
}

BOOST_AUTO_TEST_CASE(test_CacheLruAndLimits)
{
    std::vector<CacheEntry> entries;
    const char* const keys[] = {"a", "b", "c", "d", "e", "f"};
    for(size_t i = 0; i < 6; ++i)
        cacheAdd(entries, keys[i], 100);
    BOOST_REQUIRE_EQUAL(entries.size(), 6u);

    // Nothing to do while within the limits
    BOOST_CHECK(cacheEvict(entries, 6, 600, 1).empty());
    BOOST_CHECK_EQUAL(entries.size(), 6u);

    // Picture limit: the oldest go first
    std::vector<std::string> removed = cacheEvict(entries, 4, 1000, 1);
    BOOST_REQUIRE_EQUAL(removed.size(), 2u);
    BOOST_CHECK_EQUAL(removed[0], "a");
    BOOST_CHECK_EQUAL(removed[1], "b");
    BOOST_REQUIRE_EQUAL(entries.size(), 4u);
    BOOST_CHECK_EQUAL(entries[0].mKey, "c");

    // Using an entry makes it the newest, unknown keys are not found
    BOOST_CHECK(cacheTouch(entries, "c"));
    BOOST_CHECK(!cacheTouch(entries, "a"));
    BOOST_CHECK_EQUAL(entries[0].mKey, "d");
    BOOST_CHECK_EQUAL(entries[3].mKey, "c");

    // Adding an existing key replaces it and makes it the newest
    cacheAdd(entries, "d", 300);
    BOOST_REQUIRE_EQUAL(entries.size(), 4u);
    BOOST_CHECK_EQUAL(entries[3].mKey, "d");
    BOOST_CHECK_EQUAL(entries[3].mBytes, 300u);

    // Memory limit: entries e, f, c, d = 100 + 100 + 100 + 300 bytes, 450 allowed
    removed = cacheEvict(entries, 10, 450, 1);
    BOOST_REQUIRE_EQUAL(removed.size(), 2u);
    BOOST_CHECK_EQUAL(removed[0], "e");
    BOOST_CHECK_EQUAL(removed[1], "f");
    BOOST_REQUIRE_EQUAL(entries.size(), 2u);
    BOOST_CHECK_EQUAL(entries[0].mKey, "c");

    // The newest entries stay even if they alone are over the limits
    removed = cacheEvict(entries, 0, 0, 2);
    BOOST_CHECK(removed.empty());
    BOOST_CHECK_EQUAL(entries.size(), 2u);
    removed = cacheEvict(entries, 0, 0, 1);
    BOOST_REQUIRE_EQUAL(removed.size(), 1u);
    BOOST_CHECK_EQUAL(removed[0], "c");
    BOOST_CHECK_EQUAL(entries[0].mKey, "d");

    // The game keeps at least a few pictures
    BOOST_CHECK_GT(CACHE_KEEP_NEWEST, 0u);
}
