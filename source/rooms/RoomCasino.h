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

#ifndef ROOMCASINO_H
#define ROOMCASINO_H

#include "rooms/Room.h"
#include "rooms/RoomType.h"

class Creature;
class Tile;
enum class TileVisual;

//! \brief What the casino aims at. The keeper sets it per room. There are two settings:
//! Money keeps a share of the bets and annoys the gamblers, Smiles keeps little and cheers them up.
//! The setting is saved as a number: 0 is Money, 1 and 2 are read as Smiles.
enum class CasinoPayout
{
    //! \brief Keeper keeps a large share of the bets and the gamblers get annoyed ($)
    money,
    //! \brief Default level, keeper keeps little or nothing and the gamblers get cheered up (smiley)
    smiles,
    nbValues
};

class RoomCasinoGameCreatureInfo
{
public:
    RoomCasinoGameCreatureInfo(Creature* creature, bool isReadyCreature) :
        mCreature(nullptr),
        mIsReady(false)
    {}

    Creature* mCreature;
    bool mIsReady;
};

class RoomCasinoGame
{
public:
    RoomCasinoGame() :
        mCreature1(nullptr, false),
        mCreature2(nullptr, false),
        mCooldown(0)
    {}

    RoomCasinoGameCreatureInfo mCreature1;
    RoomCasinoGameCreatureInfo mCreature2;
    uint32_t mCooldown;
};

class RoomCasino: public Room
{
public:
    RoomCasino(GameMap* gameMap);

    RoomType getType() const override
    { return mRoomType; }

    void doUpkeep() override;
    bool hasOpenCreatureSpot(Creature* creature) override;
    bool addCreatureUsingRoom(Creature* creature) override;
    void removeCreatureUsingRoom(Creature* creature) override;
    void absorbRoom(Room* room) override;
    bool useRoom(Creature& creature, bool forced) override;
    //! \brief Gives the room that takes over tiles of this casino the payout set for this one
    void splitRoom(Room& newRoom, const std::vector<Tile*>& tiles) override;
    //! \brief Saves the room and the payout level
    void exportToStream(std::ostream& os) const override;
    //! \brief Loads the room and the payout level (kept at the default if the stream has none)
    //! \return false if the stream is invalid
    bool importFromStream(std::istream& is) override;

    //! \brief The payout setting of this casino
    inline CasinoPayout getPayout() const
    { return mPayout; }

    //! \brief Sets the payout setting of this casino
    void setPayout(CasinoPayout payout)
    { mPayout = payout; }

    //! \brief The share of the bets the keeper keeps for the given payout level (0.0 to 1.0)
    static double getFeeForPayout(CasinoPayout payout);

    static const RoomType mRoomType;
    static const TileVisual mRoomVisual;

protected:
    BuildingObject* notifyActiveSpotCreated(ActiveSpotPlace place, Tile* tile) override;
    void notifyActiveSpotRemoved(ActiveSpotPlace place, Tile* tile) override;

private:
    void setCreatureWinning(Creature& creature, const Ogre::Vector3& gamePosition);
    void setCreatureLoosing(Creature& creature, const Ogre::Vector3& gamePosition);
    std::map<Tile*,RoomCasinoGame> mCreaturesSpots;
    CasinoPayout mPayout;
};

#endif // ROOMCASINO_H
