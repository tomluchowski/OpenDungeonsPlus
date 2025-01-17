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

#include "entities/Tile.h"

#include "camera/CullingManager.h"
#include "entities/Building.h"
#include "entities/Creature.h"
#include "entities/GameEntityType.h"
#include "entities/TreasuryObject.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "network/ODClient.h"
#include "network/ODPacket.h"
#include "network/ClientNotification.h"
#include "modes/InputManager.h"
#include "render/RenderManager.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "sound/SoundEffectsManager.h"
#include "traps/Trap.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/System.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/Window.h>

#include <cstddef>
#include <bitset>
#include <istream>
#include <ostream>




const uint32_t Tile::NO_FLOODFILL = 0;
const std::string Tile::TILE_PREFIX = "Tile_";
const std::string Tile::TILE_SCANF = TILE_PREFIX + "%i_%i";

Tile::Tile(GameMap* gameMap, int x, int y, TileType type, double fullness) :
    GameEntity(gameMap, "", "", nullptr),
    mX                  (x),
    mY                  (y),
    mZ                  (0.0f),
    mType               (type),
    mTileVisual         (TileVisual::nullTileVisual),
    mSelected           (false),
    mFullness           (fullness),
    mRefundPriceRoom    (0),
    mRefundPriceTrap    (0),
    mCoveringBuilding   (nullptr),
    mClaimedPercentage  (0.0),
    mIsRoom             (false),
    mIsTrap             (false),
    mDisplayTileMesh    (true),
    mColorCustomMesh    (true),
    mHasBridge          (false),
    mLocalPlayerHasVision   (false),
    mTileCulling        (CullingType::HIDE),
    mNbWorkersClaiming(0),
    mStatsWindow             (nullptr),
    fogPresent          (false)
{
    computeTileVisual();
}

Tile::~Tile()
{
    if(!mEntitiesInTile.empty())
    {
        OD_LOG_ERR(getGameMap()->serverStr() + "tile=" + Tile::displayAsString(this) + ", size=" + Helper::toString(mEntitiesInTile.size()));
    }
}

void Tile::transfer(Tile* tt)
{
    GameEntity::operator=(*tt);

    mType = tt->mType;              
    mTileVisual  = tt->mTileVisual;       
    mSelected  = tt->mSelected;         
    mFullness = tt->mFullness;         
    mRefundPriceRoom = tt->mRefundPriceRoom;   
    mRefundPriceTrap = tt->mRefundPriceTrap; 
    mClaimedPercentage = tt->mClaimedPercentage; 
    mIsRoom = tt->mIsRoom;            
    mIsTrap = tt->mIsTrap;            
    mDisplayTileMesh = tt->mDisplayTileMesh;    
    mColorCustomMesh = tt->mColorCustomMesh;  
    mHasBridge = tt->mHasBridge;         
    mLocalPlayerHasVision = tt->mLocalPlayerHasVision; 
    mTileCulling = tt->mTileCulling;       
    
}


GameEntityType Tile::getObjectType() const
{
    return GameEntityType::tile;
}


bool Tile::isDiggable(const Seat* seat) const
{
    // Handle non claimed
    switch(mTileVisual)
    {
        case TileVisual::dungeonTempleRoom:
        case TileVisual::dormitoryRoom:
        case TileVisual::treasuryRoom:
        case TileVisual::portalRoom:
        case TileVisual::workshopRoom:
        case TileVisual::trainingHallRoom:
        case TileVisual::libraryRoom:
        case TileVisual::hatcheryRoom:
        case TileVisual::cryptRoom:
        case TileVisual::portalWaveRoom:
        case TileVisual::prisonRoom:
        case TileVisual::arenaRoom:
        case TileVisual::casinoRoom:
        case TileVisual::tortureRoom:        
        case TileVisual::claimedGround:
        case TileVisual::dirtGround:
        case TileVisual::goldGround:
        case TileVisual::lavaGround:
        case TileVisual::waterGround:
        case TileVisual::rockGround:
        case TileVisual::gemGround:
        case TileVisual::rockFull:
            return false;
        case TileVisual::goldFull:
        case TileVisual::dirtFull:
        case TileVisual::gemFull:
            return true;
        default:
            break;
    }

    // Should be claimed tile
    if(mTileVisual != TileVisual::claimedFull)
    {
        OD_LOG_ERR("tile=" + Tile::displayAsString(this) + ", mTileVisual=" + tileVisualToString(mTileVisual));
        return false;
    }

    // If the wall is not claimed, it can be dug
    if(!isClaimed())
        return true;

    // It is claimed. If it is by the given seat, it can be dug
    if(isClaimedForSeat(seat))
        return true;

    return false;
}

bool Tile::isWallClaimable(Seat* seat)
{
    if (getFullness() <= 0.0)
        return false;

    if (mType == TileType::lava || mType == TileType::water || mType == TileType::rock || mType == TileType::gold)
        return false;

    // Check whether at least one neighbor is a claimed ground tile of the given seat
    // which is a condition to permit claiming the given wall tile.
    bool foundClaimedGroundTile = false;
    for (Tile* tile : mNeighbors)
    {
        if (tile->getFullness() > 0.0)
            continue;

        if (!tile->isClaimedForSeat(seat))
            continue;

        foundClaimedGroundTile = true;
        break;
    }

    if (foundClaimedGroundTile == false)
        return false;

    // If the tile is not claimed, it is claimable
    if (!isClaimed())
        return true;

    // We check if the tile is already claimed for our seat
    if (isClaimedForSeat(seat))
        return false;

    // The tile is claimed by another team. We check if there is a claimed ground tile
    // claimed by the same team. If not, we can claim. If yes, we cannot
    Seat* tileSeat = getSeat();
    if (tileSeat == nullptr)
        return true;

    foundClaimedGroundTile = false;
    for (Tile* tile : mNeighbors)
    {
        if (tile->getFullness() > 0.0)
            continue;

        if (!tile->isClaimedForSeat(tileSeat))
            continue;

        foundClaimedGroundTile = true;
        break;
    }

    if (foundClaimedGroundTile)
        return false;

    return true;
}

bool Tile::isWallClaimedForSeat(Seat* seat)
{
    if (getFullness() == 0.0)
        return false;

    if (mClaimedPercentage < 1.0)
        return false;

    Seat* tileSeat = getSeat();
    if(tileSeat == nullptr)
        return false;

    if (tileSeat->canOwnedTileBeClaimedBy(seat))
        return false;

    return true;
}

std::string Tile::getFormat()
{
    return "posX\tposY\ttype\tfullness\tseatId(optional)";
}

void Tile::exportToStream(std::ostream& os) const
{
    os << getX() << "\t" << getY() << "\t";
    os << getType() << "\t" << getFullness();
    if(getSeat() == nullptr)
        return;

    os << "\t" << getSeat()->getId();
}

ODPacket& operator<<(ODPacket& os, const TileType& type)
{
    uint32_t intType = static_cast<uint32_t>(type);
    os << intType;
    return os;
}

ODPacket& operator>>(ODPacket& is, TileType& type)
{
    uint32_t intType;
    is >> intType;
    type = static_cast<TileType>(intType);
    return is;
}

std::ostream& operator<<(std::ostream& os, const TileType& type)
{
    uint32_t intType = static_cast<uint32_t>(type);
    os << intType;
    return os;
}

std::istream& operator>>(std::istream& is, TileType& type)
{
    uint32_t intType;
    is >> intType;
    type = static_cast<TileType>(intType);
    return is;
}

ODPacket& operator<<(ODPacket& os, const TileVisual& type)
{
    uint32_t intType = static_cast<uint32_t>(type);
    os << intType;
    return os;
}

ODPacket& operator>>(ODPacket& is, TileVisual& type)
{
    uint32_t intType;
    is >> intType;
    type = static_cast<TileVisual>(intType);
    return is;
}

std::ostream& operator<<(std::ostream& os, const TileVisual& type)
{
    uint32_t intType = static_cast<uint32_t>(type);
    os << intType;
    return os;
}

std::istream& operator>>(std::istream& is, TileVisual& type)
{
    uint32_t intType;
    is >> intType;
    type = static_cast<TileVisual>(intType);
    return is;
}

std::string Tile::tileTypeToString(TileType t)
{
    switch (t)
    {
        case TileType::dirt:
            return "Dirt";

        case TileType::rock:
            return "Rock";

        case TileType::gold:
            return "Gold";

        case TileType::water:
            return "Water";

        case TileType::lava:
            return "Lava";

        case TileType::gem:
            return "Gem";
        default:
            return "Unknown tile type=" + Helper::toString(static_cast<uint32_t>(t));
    }
}

std::string Tile::tileVisualToString(TileVisual tileVisual)
{
    switch (tileVisual)
    {
        case TileVisual::nullTileVisual:
            return "nullTileVisual";

        case TileVisual::dirtGround:
            return "dirtGround";

        case TileVisual::dirtFull:
            return "dirtFull";

        case TileVisual::rockGround:
            return "rockGround";

        case TileVisual::rockFull:
            return "rockFull";

        case TileVisual::goldGround:
            return "goldGround";

        case TileVisual::goldFull:
            return "goldFull";

        case TileVisual::waterGround:
            return "waterGround";

        case TileVisual::lavaGround:
            return "lavaGround";

        case TileVisual::claimedGround:
            return "claimedGround";

        case TileVisual::gemGround:
            return "gemGround";

        case TileVisual::gemFull:
            return "gemFull";

        case TileVisual::claimedFull:
            return "claimedFull";

        case TileVisual::dungeonTempleRoom:
            return "dungeonTempleRoom";

        case TileVisual::dormitoryRoom:
            return "dormitoryRoom";

        case TileVisual::treasuryRoom:
            return "treasuryRoom";

        case TileVisual::portalRoom:
            return "portalRoom";

        case TileVisual::workshopRoom:
            return "workshopRoom";

        case TileVisual::trainingHallRoom:
            return "trainingHallRoom";

        case TileVisual::libraryRoom:
            return "libraryRoom";

        case TileVisual::hatcheryRoom:
            return "hatcheryRoom";

        case TileVisual::cryptRoom:
            return "cryptRoom";

        case TileVisual::portalWaveRoom:
            return "portalWaveRoom";

        case TileVisual::prisonRoom:
            return "prisonRoom";

        case TileVisual::arenaRoom:
            return "arenaRoom";

        case TileVisual::casinoRoom:
            return "casinoRoom";

        case TileVisual::tortureRoom:
            return "tortureRoom";
  
            
        default:
            return "Unknown tile type=" + Helper::toString(static_cast<uint32_t>(tileVisual));
    }
}

TileVisual Tile::tileVisualFromString(const std::string& strTileVisual)
{
    uint32_t nb = static_cast<uint32_t>(TileVisual::countTileVisual);
    for(uint32_t k = 0; k < nb; ++k)
    {
        TileVisual tileVisual = static_cast<TileVisual>(k);
        if(strTileVisual.compare(tileVisualToString(tileVisual)) == 0)
            return tileVisual;
    }

    return TileVisual::nullTileVisual;
}

int Tile::nextTileFullness(int f)
{

    // Cycle the tile's fullness through the possible values
    switch (f)
    {
        case 0:
            return 100;

        case 100:
            return 0;

        default:
            return 0;
    }
}

void Tile::setMarkedForDigging(bool ss, const Player *pp)
{
    /* If we are trying to mark a tile that is not dirt or gold
     * or is already dug out, ignore the request.
     */
    if (ss && !isDiggable(pp->getSeat()))
        return;

    // If the tile was already in the given state, we can return
    if (getMarkedForDigging(pp) == ss)
        return;

    if (ss)
        addPlayerMarkingTile(pp);
    else
        removePlayerMarkingTile(pp);
}

bool Tile::getMarkedForDigging(const Player *p) const
{
    if(std::find(mPlayersMarkingTile.begin(), mPlayersMarkingTile.end(), p) != mPlayersMarkingTile.end())
        return true;

    return false;
}

bool Tile::isMarkedForDiggingByAnySeat()
{
    return !mPlayersMarkingTile.empty();
}

void Tile::addPlayerMarkingTile(const Player *p)
{
    mPlayersMarkingTile.push_back(p);
}

void Tile::removePlayerMarkingTile(const Player *p)
{
    auto it = std::find(mPlayersMarkingTile.begin(), mPlayersMarkingTile.end(), p);
    if(it == mPlayersMarkingTile.end())
        return;

    mPlayersMarkingTile.erase(it);
}

void Tile::addNeighbor(Tile *n)
{
    mNeighbors.push_back(n);
    mNbWorkersDigging.push_back(0);
}

Tile* Tile::getNeighbor(unsigned int index)
{
    if(index >= mNeighbors.size())
    {
        OD_LOG_ERR("tile=" + displayAsString(this) + ", index=" + Helper::toString(index) + ", size=" + Helper::toString(mNeighbors.size()));
        return nullptr;
    }

    return mNeighbors[index];
}

std::string Tile::buildName(int x, int y)
{
    std::stringstream ss;
    ss << TILE_PREFIX;
    ss << x;
    ss << "_";
    ss << y;
    return ss.str();
}

bool Tile::checkTileName(const std::string& tileName, int& x, int& y)
{
    if (tileName.compare(0, TILE_PREFIX.length(), TILE_PREFIX) != 0)
        return false;

    if(sscanf(tileName.c_str(), TILE_SCANF.c_str(), &x, &y) != 2)
        return false;

    return true;
}

std::string Tile::toString(FloodFillType type)
{
    switch (type)
    {
        case FloodFillType::ground:
            return "ground";

        case FloodFillType::groundWater:
            return "groundWater";

        case FloodFillType::groundLava:
            return "groundLava";

        case FloodFillType::groundWaterLava:
            return "groundWaterLava";


        default:
            return "Unknown FloodFillType type=" + Helper::toString(static_cast<uint32_t>(type));
    }
}

bool Tile::isFloodFillFilled(Seat* seat) const
{
    uint32_t nbFloodFill = static_cast<uint32_t>(FloodFillType::nbValues);
    for(uint32_t i = 0; i < nbFloodFill; ++i)
    {
        FloodFillType type = static_cast<FloodFillType>(i);
        if(!isFloodFillPossible(seat, type))
            continue;

        if(getFloodFillValue(seat, type) == NO_FLOODFILL)
            return false;
    }

    // No mandatory value is missing, the tile is floodfilled
    return true;
}

bool Tile::isFloodFillPossible(Seat* seat, FloodFillType type) const
{
    // No floodfill can be set on full tiles
    if(getFullness() > 0.0)
        return false;

    switch(getType())
    {
        case TileType::dirt:
        case TileType::gold:
        case TileType::rock:
        {
            switch(type)
            {
                case FloodFillType::ground:
                case FloodFillType::groundWater:
                case FloodFillType::groundLava:
                case FloodFillType::groundWaterLava:
                {
                    return true;
                }
                default:
                    return false;
            }
        }
        case TileType::water:
        {
            switch(type)
            {
                case FloodFillType::groundWater:
                case FloodFillType::groundWaterLava:
                {
                    return true;
                }
                default:
                    return false;
            }
        }
        case TileType::lava:
        {
            switch(type)
            {
                case FloodFillType::groundLava:
                case FloodFillType::groundWaterLava:
                {
                    return true;
                }
                default:
                    return false;
            }
        }
        default:
            return false;
    }

    return false;
}

bool Tile::isSameFloodFill(Seat* seat, FloodFillType type, Tile* tile) const
{
    return getFloodFillValue(seat, type) == tile->getFloodFillValue(seat, type);
}

void Tile::resetFloodFill()
{
    for(std::vector<uint32_t>& values : mFloodFillColor)
    {
        for(uint32_t& floodFillValue : values)
        {
            floodFillValue = NO_FLOODFILL;
        }
    }
}

bool Tile::updateFloodFillFromTile(Seat* seat, FloodFillType type, Tile* tile)
{
    if(seat->getTeamIndex() >= mFloodFillColor.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", seatIndex=" + Helper::toString(seat->getTeamIndex()) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(mFloodFillColor.size()))
                + ", fullness=" + Helper::toString(getFullness()));
        }
        return false;
    }

    std::vector<uint32_t>& values = mFloodFillColor[seat->getTeamIndex()];
    uint32_t intType = static_cast<uint32_t>(type);
    if(intType >= values.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", intType=" + Helper::toString(intType) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(values.size())));
        }
        return false;
    }

    if((values[intType] != NO_FLOODFILL) ||
       (tile->getFloodFillValue(seat, type) == NO_FLOODFILL))
    {
        return false;
    }

    values[intType] = tile->getFloodFillValue(seat, type);
    return true;
}

void Tile::replaceFloodFill(Seat* seat, FloodFillType type, uint32_t newValue)
{
    if(seat->getTeamIndex() >= mFloodFillColor.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", seatIndex=" + Helper::toString(seat->getTeamIndex()) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(mFloodFillColor.size())));
        }
        return;
    }

    std::vector<uint32_t>& values = mFloodFillColor[seat->getTeamIndex()];
    uint32_t intType = static_cast<uint32_t>(type);
    if(intType >= values.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", intType=" + Helper::toString(intType) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(values.size())));
        }
        return;
    }

    values[intType] = newValue;
}

void Tile::copyFloodFillToOtherSeats(Seat* seatToCopy)
{
    if(seatToCopy->getTeamIndex() >= mFloodFillColor.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seatToCopy->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", seatIndex=" + Helper::toString(seatToCopy->getTeamIndex()) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(mFloodFillColor.size())));
        }
        return;
    }

    std::vector<uint32_t>& valuesToCopy = mFloodFillColor[seatToCopy->getTeamIndex()];
    for(uint32_t indexFloodFill = 0; indexFloodFill < mFloodFillColor.size(); ++indexFloodFill)
    {
        if(seatToCopy->getTeamIndex() == indexFloodFill)
            continue;

        std::vector<uint32_t>& values = mFloodFillColor[indexFloodFill];
        for(uint32_t intType = 0; intType < static_cast<uint32_t>(FloodFillType::nbValues); ++intType)
            values[intType] = valuesToCopy[intType];

    }
}

void Tile::logFloodFill() const
{
    std::string str = "Floodfill : " + Tile::displayAsString(this)
        + " - type=" + Tile::tileVisualToString(getTileVisual())
        + " - fullness=" + Helper::toString(getFullness())
        + " - seatId=" + std::string(getSeat() == nullptr ? "-1" : Helper::toString(getSeat()->getId()));
    for(const std::vector<uint32_t>& values : mFloodFillColor)
    {
        int cpt = 0;
        for(const uint32_t& floodFill : values)
        {
            str += ", [" + Helper::toString(cpt) + "]=" + Helper::toString(floodFill);
            ++cpt;
        }
    }
    OD_LOG_INF(str);
}

bool Tile::isClaimedForSeat(const Seat* seat) const
{
    if(!isClaimed())
        return false;

    if(getSeat()->canOwnedTileBeClaimedBy(seat))
        return false;

    return true;
}

bool Tile::isClaimed() const
{
    if(!getIsOnServerMap())
    {
        if(mTileVisual == TileVisual::claimedGround)
            return true;

        if(mTileVisual == TileVisual::claimedFull)
            return true;


        switch(mTileVisual)
        {
            case TileVisual::dungeonTempleRoom:
            case TileVisual::dormitoryRoom:
            case TileVisual::treasuryRoom:
            case TileVisual::portalRoom:
            case TileVisual::workshopRoom:
            case TileVisual::trainingHallRoom:
            case TileVisual::libraryRoom:
            case TileVisual::hatcheryRoom:
            case TileVisual::cryptRoom:
            case TileVisual::portalWaveRoom:
            case TileVisual::prisonRoom:
            case TileVisual::arenaRoom:
            case TileVisual::casinoRoom:
            case TileVisual::tortureRoom:  
                return true;
        }
        
        // For bridges
        if(getHasBridge())
            return true;

        return false;
    }

    if(getSeat() == nullptr)
        return false;

    if(mClaimedPercentage < 1.0)
        return false;

    return true;
}

void Tile::clearVision()
{
    mSeatsWithVision.clear();
}

void Tile::notifyVision(Seat* seat, NodeType nt)
{
    if(std::find(mSeatsWithVision.begin(), mSeatsWithVision.end(), seat) != mSeatsWithVision.end())
        return;

    seat->notifyVisionOnTile(this, nt);
    mSeatsWithVision.push_back(seat);

    // We also notify vision for allied seats
    for(Seat* alliedSeat : seat->getAlliedSeats())
        notifyVision(alliedSeat, nt);
}

void Tile::setSeats(const std::vector<Seat*>& seats)
{
    mTileChangedForSeats.clear();
    for(Seat* seat : seats)
    {
        // Every tile should be notified by default
        std::pair<Seat*, bool> p(seat, true);
        mTileChangedForSeats.push_back(p);
    }
}

bool Tile::hasChangedForSeat(Seat* seat) const
{
    for(const std::pair<Seat*, bool>& seatChanged : mTileChangedForSeats)
    {
        if(seatChanged.first != seat)
            continue;

        return seatChanged.second;
    }
    OD_LOG_ERR("tile=" + Tile::displayAsString(this) + ", unknown seat id=" + Helper::toString(seat->getId()));
    return false;
}

void Tile::changeNotifiedForSeat(Seat* seat)
{
    for(std::pair<Seat*, bool>& seatChanged : mTileChangedForSeats)
    {
        if(seatChanged.first != seat)
            continue;

        seatChanged.second = false;
        break;
    }
}

void Tile::computeTileVisual()
{
    switch(getType())
    {
        case TileType::dirt:
            if(mFullness > 0.0)
            {
                if(isClaimed())
                    mTileVisual = TileVisual::claimedFull;
                else
                    mTileVisual = TileVisual::dirtFull;
            }
            else
            {
                if(isClaimed())
                {
                    mTileVisual = TileVisual::claimedGround;
                    if(getCoveringRoom()!=nullptr)
                        switch(getCoveringRoom()->getType())
                        {
                           case RoomType::arena:
                               mTileVisual = TileVisual::arenaRoom;
                               return;
                           case RoomType::dungeonTemple: 
                               mTileVisual = TileVisual::dungeonTempleRoom;
                               return;
                           case RoomType::dormitory: 
                               mTileVisual = TileVisual::dormitoryRoom;
                               return;
                           case RoomType::treasury: 
                               mTileVisual = TileVisual::treasuryRoom;
                               return;
                           case RoomType::portal: 
                               mTileVisual = TileVisual::portalRoom;
                               return;
                           case RoomType::workshop: 
                               mTileVisual = TileVisual::workshopRoom;
                               return;
                           case RoomType::trainingHall: 
                               mTileVisual = TileVisual::trainingHallRoom;
                               return;
                           case RoomType::library: 
                               mTileVisual = TileVisual::libraryRoom;
                               return;
                           case RoomType::hatchery: 
                               mTileVisual = TileVisual::hatcheryRoom;
                               return;
                           case RoomType::crypt: 
                               mTileVisual = TileVisual::cryptRoom;
                               return;
                           case RoomType::portalWave: 
                               mTileVisual = TileVisual::portalWaveRoom;
                               return;
                           case RoomType::prison: 
                               mTileVisual = TileVisual::prisonRoom;
                               return;
                           case RoomType::casino: 
                               mTileVisual = TileVisual::casinoRoom;
                               return;
                           case RoomType::torture: 
                               mTileVisual = TileVisual::tortureRoom;
                               return;
                           default:
                               OD_LOG_ERR("Computing tile visual for unknown room type tile=" + Tile::displayAsString(this) + ", TileType=" + roomTypeToString(getCoveringRoom()->getType()));
                               mTileVisual = TileVisual::nullTileVisual;
                        }
                    }
                
                else
                    mTileVisual = TileVisual::dirtGround;
            }
            return;

        case TileType::rock:
            if(mFullness > 0.0)
                mTileVisual = TileVisual::rockFull;
            else
                mTileVisual = TileVisual::rockGround;
            return;

        case TileType::gold:
            if(mFullness > 0.0)
            {
                if(isClaimed())
                    mTileVisual = TileVisual::claimedFull;
                else
                    mTileVisual = TileVisual::goldFull;
            }
            else
            {
                if(isClaimed())
                {
                    mTileVisual = TileVisual::claimedGround;
                    if(getCoveringRoom()!=nullptr)
                        switch(getCoveringRoom()->getType())
                        {
                           case RoomType::arena:
                               mTileVisual = TileVisual::arenaRoom;
                               return;
                           case RoomType::dungeonTemple: 
                               mTileVisual = TileVisual::dungeonTempleRoom;
                               return;
                           case RoomType::dormitory: 
                               mTileVisual = TileVisual::dormitoryRoom;
                               return;
                           case RoomType::treasury: 
                               mTileVisual = TileVisual::treasuryRoom;
                               return;
                           case RoomType::portal: 
                               mTileVisual = TileVisual::portalRoom;
                               return;
                           case RoomType::workshop: 
                               mTileVisual = TileVisual::workshopRoom;
                               return;
                           case RoomType::trainingHall: 
                               mTileVisual = TileVisual::trainingHallRoom;
                               return;
                           case RoomType::library: 
                               mTileVisual = TileVisual::libraryRoom;
                               return;
                           case RoomType::hatchery: 
                               mTileVisual = TileVisual::hatcheryRoom;
                               return;
                           case RoomType::crypt: 
                               mTileVisual = TileVisual::cryptRoom;
                               return;
                           case RoomType::portalWave: 
                               mTileVisual = TileVisual::portalWaveRoom;
                               return;
                           case RoomType::prison: 
                               mTileVisual = TileVisual::prisonRoom;
                               return;
                           case RoomType::casino: 
                               mTileVisual = TileVisual::casinoRoom;
                               return;
                           case RoomType::torture: 
                               mTileVisual = TileVisual::tortureRoom;
                               return;
                           default:
                               OD_LOG_ERR("Computing tile visual for unknown room type tile=" + Tile::displayAsString(this) + ", TileType=" + roomTypeToString(getCoveringRoom()->getType()));
                               mTileVisual = TileVisual::nullTileVisual;
                        }
                }
                else                    
                    mTileVisual = TileVisual::goldGround;
            }
            return;

        case TileType::water:
            mTileVisual = TileVisual::waterGround;
            return;

        case TileType::lava:
            mTileVisual = TileVisual::lavaGround;
            return;

        case TileType::gem:
            if(mFullness > 0.0)
                mTileVisual = TileVisual::gemFull;
            else
                mTileVisual = TileVisual::gemGround;
            return;
            
        default:
            OD_LOG_ERR("Computing tile visual for unknown tile type tile=" + Tile::displayAsString(this) + ", TileType=" + tileTypeToString(getType()));
            mTileVisual = TileVisual::nullTileVisual;
            return;
    }
}

uint32_t Tile::getFloodFillValue(Seat* seat, FloodFillType type) const
{
    if(seat->getTeamIndex() >= mFloodFillColor.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", seatIndex=" + Helper::toString(seat->getTeamIndex()) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(mFloodFillColor.size()))
                + ", fullness=" + Helper::toString(getFullness()));
        }
        return NO_FLOODFILL;
    }

    const std::vector<uint32_t>& values = mFloodFillColor[seat->getTeamIndex()];
    uint32_t intType = static_cast<uint32_t>(type);
    if(intType >= values.size())
    {
        static bool logMsg = false;
        if(!logMsg)
        {
            logMsg = true;
            OD_LOG_ERR("Wrong floodfill seat index seatId=" + Helper::toString(seat->getId())
                + ", tile=" + Tile::displayAsString(this)
                + ", intType=" + Helper::toString(intType) + ", floodfillsize=" + Helper::toString(static_cast<uint32_t>(values.size())));
        }
        return NO_FLOODFILL;
    }

    return values.at(intType);
}

void Tile::setTeamsNumber(uint32_t nbTeams)
{
    mFloodFillColor = std::vector<std::vector<uint32_t>>(nbTeams, std::vector<uint32_t>(static_cast<uint32_t>(FloodFillType::nbValues), NO_FLOODFILL));
}

bool Tile::shouldColorTileMesh() const
{
    // We only set color for claimed tiles
    switch(getTileVisual())
    {
        case TileVisual::claimedGround:
        case TileVisual::claimedFull:
        case TileVisual::portalRoom:    
            return true;
        default:
            return false;
    }
}

void Tile::exportToStream(Tile* tile, std::ostream& os)
{
    tile->exportToStream(os);
}

void Tile::setFullness(double f)
{
    double oldFullness = getFullness();

    mFullness = f;

    // If the tile was marked for digging and has been dug out, unmark it and set its fullness to 0.
    if (mFullness == 0.0 && isMarkedForDiggingByAnySeat())
    {
        setMarkedForDiggingForAllPlayersExcept(false, nullptr);
    }

    if ((oldFullness > 0.0) && (mFullness == 0.0))
    {
        fireTileSound(TileSound::Digged);

        if(!getGameMap()->isInEditorMode())
        {
            // Do a flood fill to update the contiguous region touching the tile.
            for(Seat* seat : getGameMap()->getSeats())
                getGameMap()->refreshFloodFill(seat, this);
        }
    }
}

void Tile::createMeshLocal(NodeType nt)
{
    GameEntity::createMeshLocal();

    if(getIsOnServerMap())
        return;

    RenderManager::getSingleton().rrCreateTile(*this, *getGameMap(), *getGameMap()->getLocalPlayer(),nt);
}

void Tile::destroyMeshLocal(NodeType nt)
{
    GameEntity::destroyMeshLocal();

    if(getIsOnServerMap())
        return;

    RenderManager::getSingleton().rrDestroyTile(*this, nt);
}

bool Tile::isBuildableUpon(Seat* seat) const
{
    if(isFullTile())
        return false;
    if(getIsBuilding())
        return false;
    if(!isClaimedForSeat(seat))
        return false;

    return true;
}

void Tile::setCoveringBuilding(Building *building)
{
    if(mCoveringBuilding == building)
        return;

    // We set the tile as dirty for all seats if needed (we have to check because we
    // don't want to refresh tiles for traps for enemy players)
    if(mCoveringBuilding != nullptr)
    {
        for(std::pair<Seat*, bool>& seatChanged : mTileChangedForSeats)
        {
            if(!mCoveringBuilding->shouldSetCoveringTileDirty(seatChanged.first, this))
                continue;

            seatChanged.second = true;
        }
    }
    mCoveringBuilding = building;
    mIsRoom = false;
    if(getCoveringRoom() != nullptr)
    {
        mIsRoom = true;
        fireTileSound(TileSound::BuildRoom);
    }

    mIsTrap = false;
    if(getCoveringTrap() != nullptr)
    {
        mIsTrap = true;
        fireTileSound(TileSound::BuildTrap);
    }

    if(mCoveringBuilding != nullptr)
    {
        for(std::pair<Seat*, bool>& seatChanged : mTileChangedForSeats)
        {
            if(!mCoveringBuilding->shouldSetCoveringTileDirty(seatChanged.first, this))
                continue;

            seatChanged.second = true;
        }

        // Set the tile as claimed and of the team color of the building
        setSeat(mCoveringBuilding->getSeat());
        mClaimedPercentage = 1.0;
    }
}

bool Tile::isGroundClaimable(Seat* seat) const
{
    if(getFullness() > 0.0)
        return false;

    if(getCoveringBuilding() != nullptr)
        return getCoveringBuilding()->isClaimable(seat);

    if(mType != TileType::dirt && mType != TileType::gold)
        return false;

    if(isClaimedForSeat(seat))
        return false;

    return true;
}

void Tile::exportToPacketForUpdate(ODPacket& os, Seat* seat) 
{
    exportToPacketForUpdate(os, seat, false);
}

void Tile::exportToPacketForUpdate(ODPacket& os, Seat* seat, bool hideSeatId) 
{
    GameEntity::exportToPacketForUpdate(os, seat);

    seat->exportTileToPacket(os, this, hideSeatId);
}

void Tile::updateFromPacket(ODPacket& is)
{
    GameEntity::updateFromPacket(is);

    // This function should read parameters as sent by Tile::exportToPacketForUpdate
    int seatId;
    std::string meshName;
    std::stringstream ss;

    // We set the seat if there is one
    OD_ASSERT_TRUE(is >> mIsRoom);
    OD_ASSERT_TRUE(is >> mIsTrap);
    OD_ASSERT_TRUE(is >> mRefundPriceRoom);
    OD_ASSERT_TRUE(is >> mRefundPriceTrap);

    OD_ASSERT_TRUE(is >> mDisplayTileMesh);
    OD_ASSERT_TRUE(is >> mColorCustomMesh);
    OD_ASSERT_TRUE(is >> mHasBridge);

    OD_ASSERT_TRUE(is >> seatId);

    OD_ASSERT_TRUE(is >> meshName);
    setMeshName(meshName);

    ss.str(std::string());
    ss << TILE_PREFIX;
    ss << getX();
    ss << "_";
    ss << getY();

    setName(ss.str());

    OD_ASSERT_TRUE(is >> mTileVisual);

    if(seatId == -1)
    {
        setSeat(nullptr);
    }
    else
    {
        Seat* seat = getGameMap()->getSeatById(seatId);
        if(seat != nullptr)
            setSeat(seat);

    }

    // We need to check if the tile is unmarked after reading the needed information.
    if(getMarkedForDigging(getGameMap()->getLocalPlayer()) &&
        !isDiggable(getGameMap()->getLocalPlayer()->getSeat()))
    {
        removePlayerMarkingTile(getGameMap()->getLocalPlayer());
    }

    // TODO: It would be nice to check if a noticeable value changed
    // before firing the event as it would avoid to update the minimap for
    // unchanged tiles
    fireTileStateChanged();
}

void Tile::updateFullnessFromPacket(ODPacket& is)
{
    OD_ASSERT_TRUE(is >> mFullness);

}

void Tile::loadFromLine(const std::string& line, Tile *t)
{
    std::vector<std::string> elems = Helper::split(line, '\t');

    int xLocation = Helper::toInt(elems[0]);
    int yLocation = Helper::toInt(elems[1]);

    std::stringstream tileName("");
    tileName << TILE_PREFIX;
    tileName << xLocation;
    tileName << "_";
    tileName << yLocation;

    t->setName(tileName.str());
    t->mX = xLocation;
    t->mY = yLocation;
    t->mPosition = Ogre::Vector3(static_cast<Ogre::Real>(t->mX), static_cast<Ogre::Real>(t->mY), 0.0f);

    TileType tileType = static_cast<TileType>(Helper::toInt(elems[2]));
    t->setType(tileType);

    // If the tile type is lava or water, we ignore fullness
    double fullness;
    switch(tileType)
    {
        case TileType::water:
        case TileType::lava:
            fullness = 0.0;
            break;

        default:
            fullness = Helper::toDouble(elems[3]);
            break;
    }
    t->setFullnessValue(fullness);

    bool shouldSetSeat = false;
    // We allow to set seat if the tile is dirt (full or not) or if it is gold (ground only)
    if(elems.size() >= 5)
    {
        if(tileType == TileType::dirt)
        {
            shouldSetSeat = true;
        }
        else if((tileType == TileType::gold) &&
            (fullness == 0.0))
        {
            shouldSetSeat = true;
        }
    }

    if(!shouldSetSeat)
    {
        t->setSeat(nullptr);
        return;
    }

    int seatId = Helper::toInt(elems[4]);
    Seat* seat = t->getGameMap()->getSeatById(seatId);
    if(seat == nullptr)
        return;
    t->setSeat(seat);
    t->mClaimedPercentage = 1.0;
}


TileType Tile::getTileTypeFromLine(const std::string& line)
{
    std::vector<std::string> elems = Helper::split(line, '\t');
    return static_cast<TileType>(Helper::toInt(elems[2]));
}


void Tile::refreshMesh(NodeType nt,GameMap* gameMap)
{

    if(gameMap  == nullptr)
        gameMap = getGameMap();
    if (!isMeshExisting())
        return;

    if(getIsOnServerMap())
        return;

    RenderManager::getSingleton().rrRefreshTile(*this, *gameMap, *gameMap->getLocalPlayer(),nt);
}

void Tile::setSelected(bool ss, const Player* pp)
{
    if (mSelected != ss)
    {
        mSelected = ss;

        RenderManager::getSingleton().rrTemporalMarkTile(this);
    }
}

void Tile::setMarkedForDiggingForAllPlayersExcept(bool s, Seat* exceptSeat)
{
    for (Player* player : getGameMap()->getPlayers())
    {
        if(exceptSeat == nullptr || (player->getSeat() != nullptr && !exceptSeat->isAlliedSeat(player->getSeat())))
            setMarkedForDigging(s, player);
    }
}


bool Tile::addEntity(GameEntity *entity)
{
    if(std::find(mEntitiesInTile.begin(), mEntitiesInTile.end(), entity) != mEntitiesInTile.end())
    {
        OD_LOG_ERR(getGameMap()->serverStr() + "Trying to insert twice entity=" + entity->getName() + " on tile=" + Tile::displayAsString(this));
        return false;
    }

    mEntitiesInTile.push_back(entity);
    if(!getGameMap()->isServerGameMap())
    {
        // On client side, we cull any movable entity that walks over a
        // culled tile (or show it if it was previously culled and walks
        // over a non culled tile)
        entity->setParentNodeDetachFlags(
            EntityParentNodeAttach::DETACH_CULLING, mTileCulling == CullingType::HIDE);
    }
    fireTileStateChanged();
    return true;
}

void Tile::removeEntity(GameEntity *entity)
{
    std::vector<GameEntity*>::iterator it = std::find(mEntitiesInTile.begin(), mEntitiesInTile.end(), entity);
    if(it == mEntitiesInTile.end())
    {
        OD_LOG_ERR(getGameMap()->serverStr() + "Trying to remove not inserted entity=" + entity->getName() + " from tile=" + Tile::displayAsString(this));
        return;
    }

    mEntitiesInTile.erase(it);
    fireTileStateChanged();
}


void Tile::claimForSeat(Seat* seat, double nDanceRate)
{
    // If there is a claimable building, we claim it
    if((getCoveringBuilding() != nullptr) &&
        (getCoveringBuilding()->isClaimable(seat)))
    {
        getCoveringBuilding()->claimForSeat(seat, this, nDanceRate);
        return;
    }

    // Claiming walls is less efficient than claiming ground
    if(getFullness() > 0)
        nDanceRate *= ConfigManager::getSingleton().getClaimingWallPenalty();

    // If the seat is allied, we add to it. If it is an enemy seat, we subtract from it.
    if (getSeat() != nullptr && getSeat()->isAlliedSeat(seat))
    {
        mClaimedPercentage += nDanceRate;
    }
    else
    {
        mClaimedPercentage -= nDanceRate;
        if (mClaimedPercentage <= 0.0)
        {
            // We notify the old seat that the tile is lost
            if(getSeat() != nullptr)
                getSeat()->notifyTileClaimedByEnemy(this);

            // The tile is not yet claimed, but it is now an allied seat.
            mClaimedPercentage *= -1.0;
            setSeat(seat);
            computeTileVisual();
            setDirtyForAllSeats();
        }
    }

    if ((getSeat() != nullptr) && (mClaimedPercentage >= 1.0) &&
        (getSeat()->isAlliedSeat(seat)))
    {
        claimTile(seat);
    }
}

void Tile::claimTile(Seat* seat)
{
    // Claim the tile.
    OD_LOG_INF(getGameMap()->serverStr() + "Tile=" + displayAsString(this)
        + " claimed by seat=" + Seat::displayAsString(seat));

    // We need this because if we are a client, the tile may be from a non allied seat
    setSeat(seat);
    mClaimedPercentage = 1.0;

    if(isFullTile())
        fireTileSound(TileSound::ClaimWall);
    else
        fireTileSound(TileSound::ClaimGround);

    // If an enemy player had marked this tile to dig, we disable it
    setMarkedForDiggingForAllPlayersExcept(false, seat);

    computeTileVisual();
    setDirtyForAllSeats();

    // Force all the neighbors to recheck their meshes as we have updated this tile.
    for (Tile* tile : mNeighbors)
    {
        // Update potential active spots.
        Building* building = tile->getCoveringBuilding();
        if (building != nullptr)
        {
            building->updateActiveSpots(getGameMap());
            building->createMesh();
        }
    }

    fireTileStateChanged();
}

void Tile::unclaimTile()
{
    // Unclaim the tile.
    OD_LOG_INF(getGameMap()->serverStr() + "Tile=" + displayAsString(this)
        + " unclaimed. Previous seat=" + Seat::displayAsString(getSeat()));

    setSeat(nullptr);
    mClaimedPercentage = 0.0;

    computeTileVisual();
    setDirtyForAllSeats();

    // Force all the neighbors to recheck their meshes as we have updated this tile.
    for (Tile* tile : mNeighbors)
    {
        // Update potential active spots.
        Building* building = tile->getCoveringBuilding();
        if (building != nullptr)
        {
            building->updateActiveSpots();
            building->createMesh();
        }
    }

    fireTileStateChanged();
}

double Tile::digOut(double digRate)
{
    // We scle dig rate depending on the tile type
    double digRateScaled;
    double fullnessLost;
    switch(getTileVisual())
    {
        case TileVisual::claimedFull:
        {
            static double digCoefClaimedWall = ConfigManager::getSingleton().getDigCoefClaimedWall();
            digRateScaled = digRate * digCoefClaimedWall;
            fullnessLost = digRate * digCoefClaimedWall;
            break;
        }
        case TileVisual::dirtFull:
        case TileVisual::goldFull:
            digRateScaled = digRate;
            fullnessLost = digRate;
            break;
        case TileVisual::gemFull:
            digRateScaled = digRate;
            fullnessLost = 0.0;
            break;
        default:
            // Non diggable type!
            OD_LOG_ERR("Wrong tile visual for digging tile=" + Tile::displayAsString(this) + ", visual=" + tileVisualToString(getTileVisual()));
            return 0.0;
    }

    // Nothing to dig
    if(fullnessLost <= 0.0)
        return digRateScaled;

    if(mFullness <= 0.0)
    {
        OD_LOG_ERR("tile=" + Tile::displayAsString(this) + ", mFullness=" + Helper::toString(mFullness));
        return 0.0;
    }

    if(fullnessLost >= mFullness)
    {
        digRateScaled = mFullness;
        setFullness(0.0);

        computeTileVisual();
        setDirtyForAllSeats();

        for (Tile* tile : mNeighbors)
        {
            // Update potential active spots.
            Building* building = tile->getCoveringBuilding();
            if (building != nullptr)
            {
                building->updateActiveSpots();
                building->createMesh();
            }
        }
        return digRateScaled;
    }

    digRateScaled = fullnessLost;
    setFullness(mFullness - fullnessLost);
    return digRateScaled;
}

void Tile::fillWithCarryableEntities(Creature* carrier, std::vector<GameEntity*>& entities)
{
    for(GameEntity* entity : mEntitiesInTile)
    {
        if(entity == nullptr)
        {
            OD_LOG_ERR("unexpected null entity in tile=" + Tile::displayAsString(this));
            continue;
        }

        // We check if the entity is already being handled by another creature
        if(entity->getCarryLock(*carrier))
            continue;

        if(entity->getEntityCarryType(carrier) == EntityCarryType::notCarryable)
            continue;

        if (std::find(entities.begin(), entities.end(), entity) == entities.end())
            entities.push_back(entity);
    }
}

bool Tile::isEntityOnTile(GameEntity* entity) const
{
    if(entity == nullptr)
        return false;

    switch(entity->getObjectType())
    {
        case GameEntityType::room:
            return (entity == getCoveringRoom());
        case GameEntityType::trap:
            return (entity == getCoveringTrap());
        default:
            break;
    }
    for(GameEntity* tmpEntity : mEntitiesInTile)
    {
        if(tmpEntity == entity)
            return true;
    }
    return false;
}

uint32_t Tile::countEntitiesOnTile(GameEntityType entityType) const
{
    uint32_t nbItems = 0;
    for(GameEntity* entity : mEntitiesInTile)
    {
        if(entity == nullptr)
        {
            OD_LOG_ERR("unexpected null entity in tile=" + Tile::displayAsString(this));
            continue;
        }

        if(entity->getObjectType() != entityType)
            continue;

        ++nbItems;
    }

    return nbItems;
}

void Tile::fillWithEntities(std::vector<GameEntity*>& entities, SelectionEntityWanted entityWanted, Player* player)
{
    for(GameEntity* entity : mEntitiesInTile)
    {
        if(entity == nullptr)
        {
            OD_LOG_ERR("unexpected null entity in tile=" + Tile::displayAsString(this));
            continue;
        }

        switch(entityWanted)
        {
            case SelectionEntityWanted::none:
                continue;
            
            case SelectionEntityWanted::any:
            {
                // We accept any entity
                break;
            }
            case SelectionEntityWanted::creatureAliveOwned:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                if(player->getSeat() != entity->getSeat())
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                break;
            }
            case SelectionEntityWanted::chicken:
            {
                if(entity->getObjectType() != GameEntityType::chickenEntity)
                    continue;

                break;
            }
            case SelectionEntityWanted::treasuryObjects:
            {
                if(entity->getObjectType() != GameEntityType::treasuryObject)
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveOwnedHurt:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                if(player->getSeat() != entity->getSeat())
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                if(!creature->isHurt())
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveAllied:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                if(entity->getSeat() == nullptr)
                    continue;

                if(!player->getSeat()->isAlliedSeat(entity->getSeat()))
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveEnemy:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                if(entity->getSeat() == nullptr)
                    continue;

                if(player->getSeat()->isAlliedSeat(entity->getSeat()))
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAlive:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveOrDead:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveInOwnedPrisonHurt:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                if(!creature->isInPrison())
                    continue;

                if(!creature->getSeatPrison()->canOwnedCreatureBePickedUpBy(player->getSeat()))
                    continue;

                break;
            }
            case SelectionEntityWanted::creatureAliveEnemyAttackable:
            {
                if(entity->getObjectType() != GameEntityType::creature)
                    continue;

                if(entity->getSeat() == nullptr)
                    continue;

                if(player->getSeat()->isAlliedSeat(entity->getSeat()))
                    continue;

                Creature* creature = static_cast<Creature*>(entity);
                if(!creature->isAlive())
                    continue;

                if(!creature->isAttackable(this, player->getSeat()))
                    continue;

                break;
            }
            default:
            {
                static bool logMsg = false;
                if(!logMsg)
                {
                    logMsg = true;
                    OD_LOG_ERR("Wrong SelectionEntityWanted int=" + Helper::toString(static_cast<uint32_t>(entityWanted)));
                }
                continue;
            }
        }

        if (std::find(entities.begin(), entities.end(), entity) != entities.end())
            continue;

        entities.push_back(entity);
    }
}

bool Tile::addTreasuryObject(TreasuryObject* obj)
{
    if (std::find(mEntitiesInTile.begin(), mEntitiesInTile.end(), obj) != mEntitiesInTile.end())
    {
        OD_LOG_ERR(getGameMap()->serverStr() + "Trying to insert twice treasury=" + obj->getName() + " on tile=" + Tile::displayAsString(this));
        return false;
    }

    if(!getIsOnServerMap())
    {
        // On client side, we add the entity to tile. Merging is relevant on server side only
        addEntity(obj);
        return true;
    }

    // If there is already a treasury object, we merge it
    bool isMerged = false;
    for(GameEntity* entity : mEntitiesInTile)
    {
        if(entity == nullptr)
        {
            OD_LOG_ERR("unexpected null entity in tile=" + Tile::displayAsString(this));
            continue;
        }

        if(entity->getObjectType() != GameEntityType::treasuryObject)
            continue;

        TreasuryObject* treasury = static_cast<TreasuryObject*>(entity);
        treasury->mergeGold(obj);
        isMerged = true;
        break;
    }

    if(!isMerged)
        addEntity(obj);

    return true;
}

Room* Tile::getCoveringRoom() const
{
    if(mCoveringBuilding == nullptr)
        return nullptr;

    if(mCoveringBuilding->getObjectType() != GameEntityType::room)
        return nullptr;

    return static_cast<Room*>(mCoveringBuilding);
}

unsigned int Tile::getEightNeighbouringRoomTypeCount(RoomType type) const
{
    unsigned int count = 0;
    for(int ii = -1; ii <= 1; ++ii  )
        for(int jj = -1; jj <= 1; ++jj  )
        {
            if( ii == 0 && jj == 0)
                continue;
            Tile* tt = getGameMap()->getTile(getX()+ ii,getY() + jj);
            if ( tt != nullptr)
                if(tt->getCoveringRoom()!= nullptr && tt->getCoveringRoom()->getSeat() == getCoveringRoom()->getSeat() && tt->getCoveringRoom()->getType() == type)
                    count++;
        }
    return count;
}

bool Tile::checkCoveringRoomType(RoomType type) const
{
    Room* coveringRoom = getCoveringRoom();
    if(coveringRoom == nullptr)
        return false;

    return (coveringRoom->getType() == type);
}

Trap* Tile::getCoveringTrap() const
{
    if(mCoveringBuilding == nullptr)
        return nullptr;

    if(mCoveringBuilding->getObjectType() != GameEntityType::trap)
        return nullptr;

    return static_cast<Trap*>(mCoveringBuilding);
}

bool Tile::checkCoveringTrapType(TrapType type) const
{
    Trap* coveringTrap = getCoveringTrap();
    if(coveringTrap == nullptr)
        return false;

    return (coveringTrap->getType() == type);
}

void Tile::computeVisibleTiles()
{
    if(!getGameMap()->getIsFOWActivated())
    {
        // If the FOW is deactivated, we allow vision for every seat
        for(Seat* seat : getGameMap()->getSeats())
            notifyVision(seat);

        return;
    }

    if(!isClaimed())
        return;

    // A claimed tile can see it self and its neighboors
    notifyVision(getSeat());
    for(Tile* tile : mNeighbors)
    {
        tile->notifyVision(getSeat());
    }
}

void Tile::setDirtyForAllSeats()
{
    if(!getIsOnServerMap())
        return;

    for(std::pair<Seat*, bool>& seatChanged : mTileChangedForSeats)
        seatChanged.second = true;
}

void Tile::notifyEntitiesSeatsWithVision(NodeType nt)
{
    for(GameEntity* entity : mEntitiesInTile)
    {
        entity->notifySeatsWithVision(mSeatsWithVision , nt);
    }
}


bool Tile::isFullTile() const
{
    if(getIsOnServerMap())
    {
        return getFullness() > 0.0;
    }
    else
    {
        switch(mTileVisual)
        {
            case TileVisual::claimedFull:
            case TileVisual::dirtFull:
            case TileVisual::goldFull:
            case TileVisual::rockFull:
                return true;
            default:
                return false;
        }
    }
}

bool Tile::permitsVision()
{
    if(isFullTile())
        return false;

    if((getCoveringBuilding() != nullptr) &&
       (!getCoveringBuilding()->permitsVision(this)))
    {
        return false;
    }

    return true;
}

void Tile::fireTileSound(TileSound sound)
{
    std::string soundFamily;
    switch(sound)
    {
        case TileSound::ClaimGround:
            soundFamily = "ClaimTile";
            break;
        case TileSound::ClaimWall:
            soundFamily = "ClaimTile";
            break;
        case TileSound::Digged:
            soundFamily = "RocksFalling";
            break;
        case TileSound::BuildRoom:
            soundFamily = "BuildRoom";
            break;
        case TileSound::BuildTrap:
            soundFamily = "BuildTrap";
            break;
        default:
            OD_LOG_ERR("Wrong TileSound value=" + Helper::toString(static_cast<uint32_t>(sound)));
            return;
    }

    getGameMap()->fireGameSound(*this, soundFamily);
}

double Tile::getCreatureSpeedDefault(const Creature* creature) const
{
    // If we are on a full tile, we set the speed to ground speed. That can happen
    // on client side if there is a desynchro between server and client and the
    // creature is not exactly on the same tile
    if(!getIsOnServerMap() && isFullTile())
        return creature->getMoveSpeedGround();

    switch(getTileVisual())
    {
        case TileVisual::dungeonTempleRoom:
        case TileVisual::dormitoryRoom:
        case TileVisual::treasuryRoom:
        case TileVisual::portalRoom:
        case TileVisual::workshopRoom:
        case TileVisual::trainingHallRoom:
        case TileVisual::libraryRoom:
        case TileVisual::hatcheryRoom:
        case TileVisual::cryptRoom:
        case TileVisual::portalWaveRoom:
        case TileVisual::prisonRoom:
        case TileVisual::arenaRoom:
        case TileVisual::casinoRoom:
        case TileVisual::tortureRoom:        
        case TileVisual::dirtGround:
        case TileVisual::goldGround:
        case TileVisual::rockGround:
        case TileVisual::claimedGround:
            return creature->getMoveSpeedGround();
        case TileVisual::waterGround:
            return creature->getMoveSpeedWater();
        case TileVisual::lavaGround:
            return creature->getMoveSpeedLava();
        default:
            return 0.0;
    }
}

bool Tile::canWorkerClaim(const Creature& worker)
{
    if(mNbWorkersClaiming < ConfigManager::getSingleton().getNbWorkersClaimSameTile())
        return true;

    return false;
}

bool Tile::addWorkerClaiming(const Creature& worker)
{
    if(!canWorkerClaim(worker))
        return false;

    ++mNbWorkersClaiming;
    return true;
}

bool Tile::removeWorkerClaiming(const Creature& worker)
{
    // Sanity check
    if(mNbWorkersClaiming <= 0)
    {
        OD_LOG_ERR("Cannot remove worker=" + worker.getName() + ", tile=" + Tile::displayAsString(this));
        return false;
    }

    --mNbWorkersClaiming;
    return true;
}

void Tile::canWorkerDig(const Creature& worker, std::vector<Tile*>& tiles)
{
    Tile* myTile = worker.getPositionTile();
    if (myTile == nullptr)
    {
        OD_LOG_ERR("worker=" + worker.getName() + ", pos=" + Helper::toString(worker.getPosition()));
        return;
    }

    for(uint32_t i = 0; i < mNeighbors.size(); ++i)
    {
        Tile* neigh = mNeighbors[i];
        if(neigh->isFullTile())
            continue;

        if(!getGameMap()->pathExists(&worker, myTile, neigh))
            continue;

        if(i >= mNbWorkersDigging.size())
        {
            static bool log = true;
            if(log)
            {
                log = false;
                OD_LOG_ERR("worker=" + worker.getName() + ", myTile=" + Tile::displayAsString(myTile)
                    + ", neigh=" + Tile::displayAsString(neigh) + ", i=" + Helper::toString(i)
                    + ", size=" + Helper::toString(mNbWorkersDigging.size()));
            }
            continue;
        }

        if(mNbWorkersDigging[i] >= ConfigManager::getSingleton().getNbWorkersDigSameFaceTile())
            continue;

        tiles.push_back(neigh);
    }
}

bool Tile::addWorkerDigging(const Creature& worker, Tile& tile)
{
    for(uint32_t i = 0; i < mNeighbors.size(); ++i)
    {
        Tile* neigh = mNeighbors[i];
        if(neigh != &tile)
            continue;

        if(i >= mNbWorkersDigging.size())
        {
            static bool log = true;
            if(log)
            {
                log = false;
                OD_LOG_ERR("worker=" + worker.getName() + ", tile=" + Tile::displayAsString(&tile)
                    + ", neigh=" + Tile::displayAsString(neigh) + ", i=" + Helper::toString(i)
                    + ", size=" + Helper::toString(mNbWorkersDigging.size()));
            }
            continue;
        }

        ++mNbWorkersDigging[i];
        return true;
    }

    return false;
}

bool Tile::removeWorkerDigging(const Creature& worker, Tile& tile)
{
    // Sanity check
    for(uint32_t i = 0; i < mNeighbors.size(); ++i)
    {
        Tile* neigh = mNeighbors[i];
        if(neigh != &tile)
            continue;

        if(i >= mNbWorkersDigging.size())
        {
            static bool log = true;
            if(log)
            {
                log = false;
                OD_LOG_ERR("worker=" + worker.getName() + ", tile=" + Tile::displayAsString(&tile)
                    + ", neigh=" + Tile::displayAsString(neigh) + ", i=" + Helper::toString(i)
                    + ", size=" + Helper::toString(mNbWorkersDigging.size()));
            }
            continue;
        }

        --mNbWorkersDigging[i];
        return true;
    }

    return false;
}

void Tile::setTileCullingFlags(uint32_t mask, bool value)
{
    // We save the current state. If the result is different, we refresh culling
    mTileCulling = (value ? mTileCulling | mask : mTileCulling & ~mask);

    if(mTileCulling == CullingType::HIDE)
    {
        // We cull the tile
        setParentNodeDetachFlags(EntityParentNodeAttach::DETACH_CULLING, true);
        for(GameEntity* entity : mEntitiesInTile)
            entity->setParentNodeDetachFlags(EntityParentNodeAttach::DETACH_CULLING, true);
    }
    else
    {
        // Here, we want to show the tile
        setParentNodeDetachFlags(EntityParentNodeAttach::DETACH_CULLING, false);
        for(GameEntity* entity : mEntitiesInTile)
            entity->setParentNodeDetachFlags(EntityParentNodeAttach::DETACH_CULLING, false);
    }
}

bool Tile::addTileStateListener(TileStateListener& listener)
{
    mStateListeners.push_back(&listener);
    return true;
}

bool Tile::removeTileStateListener(TileStateListener& listener)
{
    auto it = std::find(mStateListeners.begin(), mStateListeners.end(), &listener);
    if(it == mStateListeners.end())
        return false;

    mStateListeners.erase(it);
    return true;
}

void Tile::fireTileStateChanged()
{
    for(TileStateListener* stateListener : mStateListeners)
        stateListener->tileStateChanged(*this);
}

std::string Tile::displayAsString(const Tile* tile)
{
    if(tile == nullptr)
        return "nullptr";

    return "[" + Helper::toString(tile->getX()) + ","
         + Helper::toString(tile->getY())+ "]";
}


bool Tile::CloseStatsWindow(const CEGUI::EventArgs& /*e*/)
{
    destroyStatsWindow();
    return true;
}

void Tile::createStatsWindow()
{
    if (mStatsWindow != nullptr)
        return;

    ClientNotification *clientNotification = new ClientNotification(
        ClientNotificationType::askTileInfos);
    clientNotification->mPacket << getX() << getY() << true;
    ODClient::getSingleton().queueClientNotification(clientNotification);

    CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
    CEGUI::Window* rootWindow = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    mStatsWindow = wmgr->createWindow("OD/FrameWindow", std::string("CreatureStatsWindows_") + getName());
    mStatsWindow->setPosition(CEGUI::UVector2(CEGUI::UDim(0.3, 0), CEGUI::UDim(0.3, 0)));
    mStatsWindow->setSize(CEGUI::USize(CEGUI::UDim(0, 380), CEGUI::UDim(0, 400)));

    CEGUI::Window* textWindow = wmgr->createWindow("OD/StaticText", "TextDisplay");
    textWindow->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05, 0), CEGUI::UDim(0.1, 0)));
    textWindow->setSize(CEGUI::USize(CEGUI::UDim(0.9, 0), CEGUI::UDim(0.85, 0)));
    textWindow->setProperty("FrameEnabled", "False");
    textWindow->setProperty("BackgroundEnabled", "False");

    // We want to close the window when the cross is clicked
    mStatsWindow->subscribeEvent(CEGUI::FrameWindow::EventCloseClicked,
        CEGUI::Event::Subscriber(&Tile::CloseStatsWindow, this));

    // Set the window title
    mStatsWindow->setText(getName() + " ( TILE INFO )");

    mStatsWindow->addChild(textWindow);
    rootWindow->addChild(mStatsWindow);
    mStatsWindow->show();

    updateStatsWindow("Loading...");
}

void Tile::destroyStatsWindow()
{
    if (mStatsWindow != nullptr)
    {
        ClientNotification *clientNotification = new ClientNotification(
            ClientNotificationType::askTileInfos);
        clientNotification->mPacket << getX() << getY() << false;
        ODClient::getSingleton().queueClientNotification(clientNotification);

        mStatsWindow->destroy();
        mStatsWindow = nullptr;
    }
}

void Tile::updateStatsWindow(const std::string& txt)
{
    if (mStatsWindow == nullptr)
        return;

    CEGUI::Window* textWindow = mStatsWindow->getChild("TextDisplay");
    textWindow->setText(txt);
}

std::string Tile::getStatsText()
{
    // The creatures are not refreshed at each turn so this information is relevant in the server
    // GameMap only
    const std::string formatTitleOn = "[font='MedievalSharp-12'][colour='CCBBBBFF']";
    const std::string formatTitleOff = "[font='MedievalSharp-10'][colour='FFFFFFFF']";

    std::stringstream tempSS;
    tempSS << "PosX: " << getX() << " PosY: " << getY() << std::endl; 
    tempSS << "Type : " << tileTypeToString(getType()) << std::endl;
    tempSS << "TileVisual : " << tileVisualToString(getTileVisual()) << std::endl;
    tempSS << "Fulness : " << getFullness() << std::endl;
    tempSS << "SeatID : " << ((getSeat() == nullptr) ? " NULL " : Helper::toString(getSeat()->getId())) << std::endl;
    if ( getSeat()!=nullptr )
        for(FloodFillType ft = static_cast<FloodFillType>(0) ; ft < FloodFillType::nbValues   ; ft = static_cast<FloodFillType>((size_t)ft + 1))
            tempSS << toString(ft) << ": " <<  getFloodFillValue(getSeat(), ft) << " \n"; 
    return tempSS.str();

}


bool Tile::addItselfToContainer(TileContainer* tc)
{

    int x = getX();
    int y = getY();

    if (x < tc->getMapSizeX() && y < tc->getMapSizeY() && x >= 0 && y >= 0)
    {
        if(tc->mTiles[x][y] != nullptr)
        {
            tc->mTiles[x][y]->destroyMesh();
            delete tc->mTiles[x][y];
        }
        tc->mTiles[x][y] = this;
        return true;
    }

    return false;


}

Creature* Tile::getClosestCreature(SelectionEntityWanted se)
{
    InputManager& inputManager = InputManager::getSingleton();
    std::vector<GameEntity*> entities;
    assert( getGameMap()->getLocalPlayer() != nullptr);
    fillWithEntities(entities, se, getGameMap()->getLocalPlayer());
    // We search the closest creature alive
    Creature* closestCreature = nullptr;
    double closestDist = 0;
    for(GameEntity* entity : entities)
    {
        if(entity->getObjectType() != GameEntityType::creature)
        {
            OD_LOG_ERR("entityName=" + entity->getName() + ", entityType=" + Helper::toString(static_cast<uint32_t>(entity->getObjectType())));
            continue;
        }

        const Ogre::Vector3& entityPos = entity->getPosition();
        double dist = Pathfinding::squaredDistance(entityPos.x, inputManager.mKeeperHandPos.x, entityPos.y, inputManager.mKeeperHandPos.y);
        if(closestCreature == nullptr)
        {
            closestDist = dist;
            closestCreature = static_cast<Creature*>(entity);
            continue;
        }

        if(dist >= closestDist)
            continue;
        
        closestDist = dist;
        closestCreature = static_cast<Creature*>(entity);
    }

    return closestCreature;

}
