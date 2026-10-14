/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

// Two clients and a late joiner fed with the packets of one server. This is not a game run: the server side
// is the appearance logic of the first spawn, the wire is ODPacket with the same string the creature
// packets carry (Creature::exportToPacket / importFromPacket, ServerNotificationType::creatureAppearance),
// and each client turns what it received into the picture data (picture key, composed and flattened pixels)
// and the profile remarks with the same free functions the game uses.

#define BOOST_TEST_MODULE DungeonbookMultiplayer
#include "BoostTestTargetConfig.h"

#include "game/CreatureAppearance.h"
#include "network/ODPacket.h"
#include "render/AppearanceCompose.h"
#include "render/DungeonbookQuirks.h"
#include "render/PortraitManifest.h"

#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* const KNIGHT_ID = "Knight.mesh-male";
const uint32_t NB_CREATURES = 12;

std::string getTestsDirectory()
{
    std::string path = __FILE__;
    std::string::size_type slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}

class TestRandom
{
public:
    TestRandom() :
        mState(2024u)
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

//! What a client keeps of the creatures it was told about
typedef std::map<std::string, CreatureAppearance> ClientAppearances;

std::string makeName(uint32_t index)
{
    std::ostringstream name;
    name << "Knight" << index;
    return name.str();
}

//! The server writes name and token of every creature that has an appearance, as one join or load snapshot
void writeSnapshot(ODPacket& packet, const std::map<std::string, CreatureAppearance>& server)
{
    packet << static_cast<uint32_t>(server.size());
    for(std::map<std::string, CreatureAppearance>::const_iterator it = server.begin(); it != server.end(); ++it)
        packet << it->first << CreatureAppearanceLogic::toToken(it->second);
}

//! Client side: the same reading steps as the creature import, an empty token means no appearance
void readSnapshot(ODPacket& packet, ClientAppearances& client)
{
    uint32_t count = 0;
    BOOST_REQUIRE(packet >> count);
    for(uint32_t i = 0; i < count; ++i)
    {
        std::string name;
        std::string token;
        BOOST_REQUIRE(packet >> name >> token);
        CreatureAppearance appearance;
        if(!token.empty())
            BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(token, appearance));
        client[name] = appearance;
    }
}

//! Solid stand-in for a part: the colour is a function of slot and number, so equal choices give equal pixels
AppearanceCompose::RgbaImage makeStandIn(const PortraitManifest::Slot& slot, uint32_t number)
{
    uint32_t colour = CreatureAppearanceLogic::stableHash(slot.mName + "|" + std::string(1, static_cast<char>('0' + number)));
    AppearanceCompose::RgbaImage image(slot.mWidth, slot.mHeight);
    for(size_t i = 0; i < static_cast<size_t>(slot.mWidth) * slot.mHeight; ++i)
    {
        image.mPixels[i * 4] = static_cast<uint8_t>(colour & 0xff);
        image.mPixels[i * 4 + 1] = static_cast<uint8_t>((colour >> 8) & 0xff);
        image.mPixels[i * 4 + 2] = static_cast<uint8_t>((colour >> 16) & 0xff);
        image.mPixels[i * 4 + 3] = 255;
    }
    return image;
}

//! The pixels a client would show for the creature (compose in slot order, flatten, hash)
uint32_t pictureHash(const PortraitManifest& manifest, const CreatureAppearance& appearance, const std::string& name)
{
    AppearanceCompose::RgbaImage base(16, 32);
    for(size_t i = 0; i < base.mPixels.size(); i += 4)
    {
        base.mPixels[i] = 120;
        base.mPixels[i + 1] = 100;
        base.mPixels[i + 2] = 90;
        base.mPixels[i + 3] = 255;
    }

    std::vector<AppearanceCompose::Part> parts;
    const std::vector<CreatureAppearance::Choice>& choices = appearance.getChoices();
    for(size_t i = 0; i < choices.size(); ++i)
    {
        const PortraitManifest::Slot* slot = manifest.findSlot(choices[i].mSlot);
        BOOST_REQUIRE(slot != nullptr);
        AppearanceCompose::Part part;
        part.mSlot = slot->mName;
        part.mX = slot->mX;
        part.mY = slot->mY;
        part.mImage = makeStandIn(*slot, choices[i].mNumber);
        parts.push_back(part);
    }

    AppearanceCompose::RgbaImage composed = AppearanceCompose::compose(base, parts);
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels = AppearanceCompose::flattenAndTint(composed, 4, nullptr, appearance.getCatalogId(), name,
        width, height);
    BOOST_REQUIRE(!pixels.empty());
    return AppearanceCompose::hashPixels(pixels);
}

void checkSameView(const PortraitManifest& manifest, const DungeonbookQuirks& quirks,
    const std::map<std::string, CreatureAppearance>& server, const ClientAppearances& client)
{
    BOOST_CHECK_EQUAL(client.size(), server.size());
    for(std::map<std::string, CreatureAppearance>::const_iterator it = server.begin(); it != server.end(); ++it)
    {
        ClientAppearances::const_iterator found = client.find(it->first);
        BOOST_REQUIRE(found != client.end());
        BOOST_CHECK(found->second == it->second);
        BOOST_CHECK_EQUAL(AppearanceCompose::makePictureKey(found->second, it->first),
            AppearanceCompose::makePictureKey(it->second, it->first));
        BOOST_CHECK_EQUAL(pictureHash(manifest, found->second, it->first), pictureHash(manifest, it->second, it->first));
        BOOST_CHECK(DungeonbookQuirkLogic::selectRemarks(quirks, manifest, found->second, it->first) ==
            DungeonbookQuirkLogic::selectRemarks(quirks, manifest, it->second, it->first));
    }
}
}

BOOST_AUTO_TEST_CASE(test_TwoClientsShowTheSamePicture)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(manifest.loadFromFile(getTestsDirectory() + "/fixtures/portraits/variants/" + KNIGHT_ID +
        "/manifest.cfg"));
    DungeonbookQuirks quirks;
    std::istringstream is("hair\tbraid\tBraid remark\nhair\tbald\tBald remark\nhelmet\thorned\tHorned remark\n"
        "scar\tcheek\tCheek remark\n");
    quirks.loadFromStream(is, "test");

    // The server rolls the looks of the first spawns, nobody else rolls
    TestRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&TestRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);
    std::map<std::string, CreatureAppearance> server;
    std::vector<CreatureAppearance> taken;
    for(uint32_t i = 0; i < NB_CREATURES; ++i)
    {
        CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
        BOOST_REQUIRE(!appearance.isEmpty());
        taken.push_back(appearance);
        server[makeName(i)] = appearance;
    }

    // One snapshot for the two clients that are in the game when the map is sent
    ODPacket snapshot;
    writeSnapshot(snapshot, server);
    ODPacket copyForB = snapshot;
    ClientAppearances clientA;
    ClientAppearances clientB;
    readSnapshot(snapshot, clientA);
    readSnapshot(copyForB, clientB);

    checkSameView(manifest, quirks, server, clientA);
    checkSameView(manifest, quirks, server, clientB);
    BOOST_CHECK(clientA == clientB);
}

BOOST_AUTO_TEST_CASE(test_LateJoinReceivesAllAppearances)
{
    PortraitManifest manifest;
    BOOST_REQUIRE(manifest.loadFromFile(getTestsDirectory() + "/fixtures/portraits/variants/" + KNIGHT_ID +
        "/manifest.cfg"));
    DungeonbookQuirks quirks;
    std::istringstream is("hair\tbraid\tBraid remark\nhelmet\thorned\tHorned remark\nscar\tcheek\tCheek remark\n");
    quirks.loadFromStream(is, "test");

    TestRandom generator;
    CreatureAppearanceLogic::RandomFunction random =
        std::bind(&TestRandom::next, &generator, std::placeholders::_1, std::placeholders::_2);
    std::map<std::string, CreatureAppearance> server;
    std::vector<CreatureAppearance> taken;
    for(uint32_t i = 0; i < NB_CREATURES; ++i)
    {
        CreatureAppearance appearance = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
        taken.push_back(appearance);
        server[makeName(i)] = appearance;
    }

    // A client that is already in the game, then one creature gets its appearance late (notification:
    // name and token) and a creature without any appearance is among the creatures
    ClientAppearances early;
    ODPacket first;
    writeSnapshot(first, server);
    readSnapshot(first, early);

    CreatureAppearance lateLook = CreatureAppearanceLogic::pickRandom(manifest, KNIGHT_ID, random, taken);
    server["Latecomer"] = lateLook;
    server["Plain"] = CreatureAppearance();
    ODPacket notification;
    notification << std::string("Latecomer") << CreatureAppearanceLogic::toToken(lateLook);
    std::string notifiedName;
    std::string notifiedToken;
    BOOST_REQUIRE(notification >> notifiedName >> notifiedToken);
    CreatureAppearance notified;
    BOOST_REQUIRE(CreatureAppearanceLogic::fromToken(notifiedToken, notified));
    early[notifiedName] = notified;
    early["Plain"] = CreatureAppearance();

    // A player that joins now gets one snapshot with everything: the same view as the client that was there
    ODPacket joinPacket;
    writeSnapshot(joinPacket, server);
    ClientAppearances late;
    readSnapshot(joinPacket, late);

    BOOST_CHECK_EQUAL(late.size(), NB_CREATURES + 2);
    BOOST_CHECK(late["Plain"].isEmpty());
    BOOST_CHECK(!late["Latecomer"].isEmpty());
    checkSameView(manifest, quirks, server, late);
    BOOST_CHECK(early == late);
}
