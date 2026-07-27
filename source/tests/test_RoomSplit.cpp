/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mocks/ODClientTest.h"

#include "game/SeatData.h"
#include "network/ClientNotification.h"
#include "rooms/RoomType.h"
#include "utils/LogManager.h"
#include "utils/LogSinkConsole.h"

#define BOOST_TEST_MODULE TestRoomSplit
#include <BoostTestTargetConfig.h>

class ODClientTestRoomSplit : public ODClientTest
{
public:
    ODClientTestRoomSplit(const std::vector<PlayerInfo>& players, uint32_t indexLocalPlayer) :
        ODClientTest(players, indexLocalPlayer)
    {}
};

//! \brief Builds a room in a row of three tiles, fills it with gold and sells the tile in
//! the middle. What is left is two rooms that only look like one, and the point of the test
//! is that they are handed over to a room of their own without losing what they hold: the
//! gold sits in the tile data, which has to move with the tile rather than be made anew.
BOOST_AUTO_TEST_CASE(test_RoomSplit)
{
    LogManager logMgr;
    logMgr.addSink(std::unique_ptr<LogSink>(new LogSinkConsole()));
    std::vector<PlayerInfo> players;

    int seatId = 1;
    int32_t teamId = 1;
    // The first player is the local one
    for(uint32_t i = 0; i < 1; ++i)
    {
        PlayerInfo player;
        player.mNick = "PlayerStub" + Helper::toString(seatId);
        player.mWantedSeatId = seatId;
        player.mWantedTeamId = teamId;
        player.mIsHuman = true;
        player.mPlayerId = -1;
        player.mWantedFactionIndex = 0;
        players.push_back(player);

        ++seatId;
        ++teamId;
    }

    // We add the AI players
    for(uint32_t i = 0; i < 2; ++i)
    {
        PlayerInfo playerAi;
        playerAi.mPlayerId = 0;
        playerAi.mWantedSeatId = seatId;
        playerAi.mWantedTeamId = teamId;
        playerAi.mWantedFactionIndex = 0;
        playerAi.mIsHuman = false;
        players.push_back(playerAi);

        ++seatId;
        ++teamId;
    }

    ODClientTestRoomSplit client(players, 0);
    BOOST_CHECK(client.connect("localhost", 32222, 10, "test_RoomSplitReplay"));
    BOOST_CHECK(client.isConnected());

    client.runFor(5000);
    if(!client.isConnected())
    {
        BOOST_CHECK(false);
        return;
    }

    SeatData& seatLocal = *client.getLocalSeat();

    // Gold is kept in the treasury rather than by the seat, so the room has to come first.
    // The first tile of a seat's first treasury is free, which is the only way to start.
    ODPacket packSend;
    RoomType type = RoomType::treasury;
    uint32_t nb = 1;
    int32_t x = 3;
    int32_t y = 10;
    packSend << ClientNotificationType::askBuildRoom << type << nb << x << y;
    client.send(packSend);
    client.runFor(3000);

    client.sendConsoleCmd("addgold 1 200");
    client.runFor(3000);
    BOOST_CHECK(seatLocal.getGold() == 200);

    // Widen it to a row of five: (1,10) to (5,10) are claimed ground, and the four added
    // tiles cost 25 each
    packSend.clear();
    nb = 4;
    packSend << ClientNotificationType::askBuildRoom << type << nb;
    for(int32_t xBuild : {1, 2, 4, 5})
    {
        y = 10;
        packSend << xBuild << y;
    }
    client.send(packSend);
    client.runFor(3000);
    BOOST_CHECK(seatLocal.getGold() == 100);

    // Fill the five tiles: a treasury tile holds 1000 gold
    client.sendConsoleCmd("addgold 1 4900");
    client.runFor(3000);
    BOOST_CHECK(seatLocal.getGold() == 5000);

    // Selling the tile in the middle leaves (1,10) (2,10) with nothing joining them to
    // (4,10) (5,10). Both keep two tiles, so the second pair is the one moved out.
    packSend.clear();
    nb = 1;
    x = 3;
    y = 10;
    packSend << ClientNotificationType::askSellRoomTiles << nb << x << y;
    client.send(packSend);
    client.runFor(3000);

    // Only the gold of the tile that was sold is gone: 4000 of the 5000 is left. Both ways of
    // getting the hand over wrong show up here. Lose what the moved tiles held and the seat
    // is down to the 2000 that stayed; leave a copy of it behind, in a room that counts the
    // gold of every tile it has data for rather than only the ones it covers, and selling
    // the middle of a treasury makes 6000 out of 5000.
    BOOST_CHECK(client.isConnected());
    int32_t goldLeft = seatLocal.getGold();
    OD_LOG_INF("Gold left after the split: " + Helper::toString(goldLeft));
    BOOST_CHECK(goldLeft == 4000);

    client.disconnect(false);
}
