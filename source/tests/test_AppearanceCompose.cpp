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

    RgbaImage result = compose(base, parts);
    BOOST_REQUIRE(result.isValid());
    checkPixel(result, 0, 0, 100, 100, 100, 255);
    checkPixel(result, 1, 1, 150, 50, 50, 255);
    checkPixel(result, 2, 2, 150, 50, 50, 255);
    checkPixel(result, 3, 3, 100, 100, 100, 255);

    // Fully transparent parts change nothing, opaque ones replace
    parts.clear();
    parts.push_back(makePart("hair", 0, 0, makeSolid(4, 4, 1, 2, 3, 0)));
    parts.push_back(makePart("ears", 0, 0, makeSolid(1, 1, 9, 8, 7, 255)));
    result = compose(base, parts);
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
    RgbaImage result = compose(base, parts);
    checkPixel(result, 0, 0, 255, 0, 0, 255);
    checkPixel(result, 1, 1, 0, 255, 0, 255);

    std::vector<Part> reversed;
    reversed.push_back(parts[1]);
    reversed.push_back(parts[0]);
    result = compose(base, reversed);
    checkPixel(result, 1, 1, 255, 0, 0, 255);

    // A part that reaches over the border is cut, one outside of it is ignored
    parts.clear();
    parts.push_back(makePart("hair", 3, 3, makeSolid(4, 4, 0, 0, 255, 255)));
    parts.push_back(makePart("ears", 9, 9, makeSolid(2, 2, 255, 255, 255, 255)));
    result = compose(base, parts);
    BOOST_CHECK_EQUAL(result.mWidth, 4u);
    BOOST_CHECK_EQUAL(result.mHeight, 4u);
    checkPixel(result, 3, 3, 0, 0, 255, 255);
    checkPixel(result, 2, 3, 0, 0, 0, 255);

    // An invalid base gives nothing
    BOOST_CHECK(!compose(RgbaImage(), parts).isValid());
}

BOOST_AUTO_TEST_CASE(test_NoRuntimeHelmetClipping)
{
    // The scar masks and the helmet damage are baked into the part images, the runtime draws plain source over
    // in the order of the parts: a scar below or beside a helmet is drawn as it is
    RgbaImage helmet = makeSolid(4, 4, 150, 150, 160, 255);
    for(uint32_t y = 0; y < 4; ++y)
    {
        for(uint32_t x = 2; x < 4; ++x)
            helmet.mPixels[(static_cast<size_t>(y) * 4 + x) * 4 + 3] = 0;
    }
    RgbaImage base = makeSolid(8, 8, 50, 50, 50, 255);
    std::vector<Part> parts;
    parts.push_back(makePart("helmet", 2, 0, helmet));
    parts.push_back(makePart("scar", 2, 2, makeSolid(4, 4, 255, 255, 255, 255)));

    RgbaImage result = compose(base, parts);
    // the whole scar is drawn, also where the helmet is transparent and below the helmet
    checkPixel(result, 2, 2, 255, 255, 255, 255);
    checkPixel(result, 4, 2, 255, 255, 255, 255);
    checkPixel(result, 5, 3, 255, 255, 255, 255);
    checkPixel(result, 2, 4, 255, 255, 255, 255);
    // the helmet itself is drawn first and stays where the scar does not cover it
    checkPixel(result, 3, 0, 150, 150, 160, 255);
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

    RgbaImage result = compose(makeKnightBase(), parts);
    BOOST_CHECK_EQUAL(hashPixels(result.mPixels), 0x59549e25u);
    // the scar is half transparent on the cheek
    checkPixel(result, 6, 11, 170, 160, 155, 255);

    // Same input, same pixels
    BOOST_CHECK(compose(makeKnightBase(), parts).mPixels == result.mPixels);
}

BOOST_AUTO_TEST_CASE(test_GoldenComposeAndTint)
{
    // A scene whose every step can be derived exactly (see check_dungeonbook_golden.py, which recomputes these
    // values and checks that they are the ones written here): solid parts on block borders, a palette with
    // one colour and a region that selects only the hair
    const uint32_t GOLDEN_COMPOSE_HASH = 0x84120a45u;
    const uint32_t GOLDEN_TINT_HASH = 0x20e4aa70u;

    RgbaImage base = makeSolid(16, 32, 120, 100, 90, 255);
    std::vector<Part> parts;
    parts.push_back(makePart("hair", 4, 4, makeSolid(8, 8, 200, 40, 40, 255)));
    parts.push_back(makePart("scar", 8, 16, makeSolid(4, 4, 255, 255, 255, 128)));
    RgbaImage composed = compose(base, parts);
    BOOST_CHECK_EQUAL(hashPixels(composed.mPixels), GOLDEN_COMPOSE_HASH);
    checkPixel(composed, 5, 5, 200, 40, 40, 255);
    checkPixel(composed, 9, 17, 188, 178, 173, 255);
    checkPixel(composed, 0, 0, 120, 100, 90, 255);

    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/fixtures/portraits/golden-tint.cfg"));
    BOOST_CHECK(tint.getErrors().empty());
    BOOST_CHECK(tint.hasMesh("Golden.mesh"));

    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> result = flattenAndTint(composed, 4, &tint, "Golden.mesh", "Anyone", width, height);
    BOOST_REQUIRE_EQUAL(width, 4u);
    BOOST_REQUIRE_EQUAL(height, 8u);
    BOOST_REQUIRE_EQUAL(result.size(), 4u * 8u * 4u);
    BOOST_CHECK_EQUAL(hashPixels(result), GOLDEN_TINT_HASH);

    // The hair blocks (1..2, 1..2) became the palette colour, everything else keeps its colour; the colour does
    // not depend on the creature (one colour in the palette)
    for(uint32_t y = 0; y < 8; ++y)
    {
        for(uint32_t x = 0; x < 4; ++x)
        {
            const uint8_t* pixel = &result[(static_cast<size_t>(y) * 4 + x) * 4];
            bool hair = (x == 1 || x == 2) && (y == 1 || y == 2);
            bool scar = (x == 2) && (y == 4);
            BOOST_CHECK_EQUAL(static_cast<int>(pixel[0]), hair ? 0 : (scar ? 188 : 120));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel[1]), hair ? 128 : (scar ? 178 : 100));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel[2]), hair ? 0 : (scar ? 173 : 90));
            BOOST_CHECK_EQUAL(static_cast<int>(pixel[3]), 255);
        }
    }
    BOOST_CHECK(flattenAndTint(composed, 4, &tint, "Golden.mesh", "Somebody else", width, height) == result);
}

BOOST_AUTO_TEST_CASE(test_FlattenAndTint)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnightManifest(manifest));
    RgbaImage composed = compose(makeKnightBase(), makeKnightParts(manifest, true));

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

    // The base tint file of the fixtures: it has regions for the fixture bases and none for unknown ids
    // (the shipped file belongs to the shipped bases and is not read here)
    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/fixtures/portraits/base-tints.cfg"));
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

namespace
{
//! One opaque part pixel and one transparent pixel in a 2x1 part
Part makeTwoPixelPart(const std::string& slot)
{
    RgbaImage image(2, 1);
    image.mPixels[0] = 200;
    image.mPixels[1] = 150;
    image.mPixels[2] = 100;
    image.mPixels[3] = 255;
    image.mPixels[4] = 10;
    image.mPixels[5] = 20;
    image.mPixels[6] = 30;
    image.mPixels[7] = 0;
    return makePart(slot, 0, 0, image);
}

//! True if the first pixel of the part is one of the three pure colours of the fixture palette
bool isPrimaryColour(const Part& part)
{
    const uint8_t* pixel = &part.mImage.mPixels[0];
    return ((pixel[0] == 255) && (pixel[1] == 0) && (pixel[2] == 0)) ||
        ((pixel[0] == 0) && (pixel[1] == 255) && (pixel[2] == 0)) ||
        ((pixel[0] == 0) && (pixel[1] == 0) && (pixel[2] == 255));
}
}

BOOST_AUTO_TEST_CASE(test_TintParts)
{
    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/fixtures/portraits/part-tints.cfg"));
    BOOST_CHECK(tint.getErrors().empty());
    BOOST_CHECK(tint.hasMesh("hair"));
    BOOST_CHECK(tint.hasMesh("chin:forked"));
    BOOST_CHECK(!tint.hasMesh("chin:square"));

    // A solid part becomes exactly one colour of the palette; alpha and transparent pixels stay as they are
    Part hair = makeTwoPixelPart("hair");
    tintPart(hair, KNIGHT_ID, "swept", &tint, "Brak");
    BOOST_CHECK(isPrimaryColour(hair));
    BOOST_CHECK_EQUAL(static_cast<int>(hair.mImage.mPixels[3]), 255);
    BOOST_CHECK_EQUAL(static_cast<int>(hair.mImage.mPixels[4]), 10);
    BOOST_CHECK_EQUAL(static_cast<int>(hair.mImage.mPixels[5]), 20);
    BOOST_CHECK_EQUAL(static_cast<int>(hair.mImage.mPixels[6]), 30);
    BOOST_CHECK_EQUAL(static_cast<int>(hair.mImage.mPixels[7]), 0);

    // Same creature, same colour, every time
    Part again = makeTwoPixelPart("hair");
    tintPart(again, KNIGHT_ID, "waves", &tint, "Brak");
    BOOST_CHECK(again.mImage.mPixels == hair.mImage.mPixels);

    // The beard has the colour of the hair (same region name), also by slot and option key
    Part beard = makeTwoPixelPart("chin");
    tintPart(beard, KNIGHT_ID, "forked", &tint, "Brak");
    BOOST_CHECK(beard.mImage.mPixels == hair.mImage.mPixels);

    // Creatures differ
    const char* const names[] = {"Brak", "Zog", "Mira", "Ulf", "Hesta", "Grim", "Tilda", "Rurik"};
    std::set<uint32_t> colours;
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        Part part = makeTwoPixelPart("hair");
        tintPart(part, KNIGHT_ID, "swept", &tint, names[i]);
        BOOST_CHECK(isPrimaryColour(part));
        colours.insert(hashPixels(part.mImage.mPixels));
    }
    BOOST_CHECK_GT(colours.size(), 1u);

    // Parts without an entry and a missing tint are left alone
    Part skinChin = makeTwoPixelPart("chin");
    tintPart(skinChin, KNIGHT_ID, "square", &tint, "Brak");
    BOOST_CHECK(skinChin.mImage.mPixels == makeTwoPixelPart("chin").mImage.mPixels);
    Part noTint = makeTwoPixelPart("hair");
    tintPart(noTint, KNIGHT_ID, "swept", nullptr, "Brak");
    BOOST_CHECK(noTint.mImage.mPixels == makeTwoPixelPart("hair").mImage.mPixels);
    Part empty;
    empty.mSlot = "hair";
    tintPart(empty, KNIGHT_ID, "swept", &tint, "Brak");
    BOOST_CHECK(!empty.mImage.isValid());

    // An entry for slot and option applies to that option only, and the eyes slot works like any other
    Part eyes = makeTwoPixelPart("eyes");
    tintPart(eyes, KNIGHT_ID, "round", &tint, "Brak");
    BOOST_CHECK(isPrimaryColour(eyes));
    Part otherEyes = makeTwoPixelPart("eyes");
    tintPart(otherEyes, KNIGHT_ID, "narrow", &tint, "Brak");
    BOOST_CHECK(otherEyes.mImage.mPixels == makeTwoPixelPart("eyes").mImage.mPixels);

    // The coloured part ends up in the composed picture
    RgbaImage base = makeSolid(2, 1, 0, 0, 0, 255);
    std::vector<Part> parts;
    parts.push_back(hair);
    RgbaImage composed = compose(base, parts);
    BOOST_CHECK_EQUAL(static_cast<int>(composed.mPixels[0]), static_cast<int>(hair.mImage.mPixels[0]));
    BOOST_CHECK_EQUAL(static_cast<int>(composed.mPixels[4]), 0);
}

BOOST_AUTO_TEST_CASE(test_EyesByCatalogId)
{
    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/fixtures/portraits/part-tints.cfg"));
    BOOST_CHECK(tint.getErrors().empty());
    BOOST_CHECK(tint.hasMesh("Knight.mesh-male:eyes:narrow"));

    // Only the left half of an 8x1 part lies in the ellipse of the eyes entry (centre 2, radius 1.6 pixels)
    Part eyes;
    eyes.mSlot = "eyes";
    eyes.mX = 0;
    eyes.mY = 0;
    eyes.mImage = makeSolid(8, 1, 200, 150, 100, 255);
    Part before = eyes;
    tintPart(eyes, KNIGHT_ID, "narrow", &tint, "Brak");
    BOOST_CHECK(isPrimaryColour(eyes));
    // the last pixel is far outside and keeps its colour, the alpha is never changed
    for(size_t c = 0; c < 4; ++c)
        BOOST_CHECK_EQUAL(static_cast<int>(eyes.mImage.mPixels[7 * 4 + c]), static_cast<int>(before.mImage.mPixels[7 * 4 + c]));

    // Another catalog id or option has no entry
    Part other = before;
    tintPart(other, "Orc.mesh-male", "narrow", &tint, "Brak");
    BOOST_CHECK(other.mImage.mPixels == before.mImage.mPixels);
    tintPart(other, KNIGHT_ID, "round", &tint, "Brak");
    BOOST_CHECK(other.mImage.mPixels == before.mImage.mPixels);
}

BOOST_AUTO_TEST_CASE(test_BaseSkinAmplitude)
{
    PortraitTint parts;
    BOOST_REQUIRE(parts.loadFromFile(getTestsDirectory() + "/fixtures/portraits/part-tints.cfg"));
    float skinShift[3] = {0.0f, 0.0f, 0.0f};
    BOOST_REQUIRE(parts.getRegionShift("base-skin", "Skin", skinShift));
    BOOST_CHECK_GT(skinShift[0], 0.0f);
    BOOST_CHECK_GT(skinShift[1], 0.0f);
    BOOST_CHECK_GT(skinShift[2], 0.0f);

    PortraitTint base;
    BOOST_REQUIRE(base.loadFromFile(getTestsDirectory() + "/fixtures/portraits/base-tints-flat.cfg"));
    BOOST_CHECK(base.getErrors().empty());

    PortraitManifest manifest;
    BOOST_REQUIRE(loadKnightManifest(manifest));
    RgbaImage composed = compose(makeKnightBase(), makeKnightParts(manifest, false));
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> plain = flattenAndTint(composed, 4, nullptr, KNIGHT_ID, "Brak", width, height);

    // With amplitude 0 the skin does not vary
    BOOST_CHECK(flattenAndTint(composed, 4, &base, KNIGHT_ID, "Brak", width, height) == plain);

    // After the amplitude is set, names differ and the same name stays the same
    base.setShiftWhereNone("Skin", skinShift);
    std::vector<uint8_t> first = flattenAndTint(composed, 4, &base, KNIGHT_ID, "Brak", width, height);
    BOOST_CHECK(first == flattenAndTint(composed, 4, &base, KNIGHT_ID, "Brak", width, height));
    BOOST_CHECK(first != plain);
    const char* const names[] = {"Zog", "Mira", "Ulf", "Hesta", "Grim", "Tilda", "Rurik"};
    std::set<uint32_t> hashes;
    hashes.insert(hashPixels(first));
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        hashes.insert(hashPixels(flattenAndTint(composed, 4, &base, KNIGHT_ID, names[i], width, height)));
    BOOST_CHECK_GT(hashes.size(), 1u);

    // A region with its own amplitude is not touched by setShiftWhereNone
    PortraitTint own;
    BOOST_REQUIRE(own.loadFromFile(getTestsDirectory() + "/fixtures/portraits/base-tints.cfg"));
    std::vector<uint8_t> ownBefore = flattenAndTint(composed, 4, &own, KNIGHT_ID, "Brak", width, height);
    own.setShiftWhereNone("Skin", skinShift);
    BOOST_CHECK(flattenAndTint(composed, 4, &own, KNIGHT_ID, "Brak", width, height) == ownBefore);
}

BOOST_AUTO_TEST_CASE(test_ShippedPartTintsLoad)
{
    PortraitTint tint;
    BOOST_REQUIRE(tint.loadFromFile(getTestsDirectory() + "/../../config/dungeonbook-part-tints.cfg"));
    BOOST_CHECK(tint.getErrors().empty());
    // Hair of people and beards are coloured; skin like chins and the eyes slot are not
    BOOST_CHECK(tint.hasMesh("hair:swept"));
    BOOST_CHECK(tint.hasMesh("chin:forked"));
    BOOST_CHECK(tint.hasMesh("chin:braided"));
    BOOST_CHECK(!tint.hasMesh("chin:square"));
    BOOST_CHECK(!tint.hasMesh("chin:rounded"));
    BOOST_CHECK(!tint.hasMesh("hair:shortfrill"));
    // The skin amplitude of the bases is above zero in all three values
    float skinShift[3] = {0.0f, 0.0f, 0.0f};
    BOOST_REQUIRE(tint.getRegionShift("base-skin", "Skin", skinShift));
    BOOST_CHECK_GT(skinShift[0], 0.0f);
    BOOST_CHECK_GT(skinShift[1], 0.0f);
    BOOST_CHECK_GT(skinShift[2], 0.0f);
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
