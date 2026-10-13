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

#include "traps/TrapDoor.h"
#include "game/SkillManager.h"
#include "game/SkillType.h"

#include "creatureaction/CreatureAction.h"
#include "entities/Creature.h"
#include "entities/DoorEntity.h"
#include "entities/GameEntityType.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "entities/TrapEntity.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "modes/InputCommand.h"
#include "modes/InputManager.h"
#include "network/ODClient.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/Random.h"
#include "utils/LogManager.h"

#include <algorithm>

namespace
{
//! \brief Factory shared by all door types. The doors only differ by name, price and health
class TrapDoorFactory : public TrapFactory
{
public:
    TrapDoorFactory(TrapType doorType, const std::string& name, const std::string& nameReadable,
            const std::string& configPrefix) :
        mDoorType(doorType),
        mName(name),
        mNameReadable(nameReadable),
        mConfigPrefix(configPrefix)
    {
    }

private:
    //! \brief Doors need walls on both sides. The barricade can be placed anywhere
    bool canBuildOn(GameMap* gameMap, Tile* tile) const
    {
        if(mDoorType == TrapType::doorBarricade)
            return true;

        return TrapDoor::canDoorBeOnTile(gameMap, tile);
    }

    TrapType mDoorType;
    std::string mName;
    std::string mNameReadable;
    //! \brief Prefix of the doors parameters in traps.cfg
    std::string mConfigPrefix;

    TrapType getTrapType() const override
    { return mDoorType; }

    const std::string& getName() const override
    { return mName; }

    const std::string& getNameReadable() const override
    { return mNameReadable; }

    int getCostPerTile() const override
    { return ConfigManager::getSingleton().getTrapConfigInt32(mConfigPrefix + "DoorCostPerTile"); }

    // No dedicated models exist yet for the stronger doors. They use the wooden door model
    const std::string& getMeshName() const override
    {
        static const std::string meshName = "WoodenDoor";
        return meshName;
    }

    virtual void checkBuildTrap(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        Player* player = gameMap->getLocalPlayer();
        TrapType type = mDoorType;
        // We only allow 1 tile for door trap
        Tile* tile = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
        if(tile == nullptr)
        {
            inputCommand.unselectAllTiles();
            return;
        }

        int32_t pricePerTarget = TrapManager::costPerTile(type);
        int32_t playerGold = static_cast<int32_t>(player->getSeat()->getGold());
        if(inputManager.mCommandState != InputCommandState::validated)
            inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos,
                inputManager.mXPos, inputManager.mYPos);

        if(!tile->isBuildableUpon(player->getSeat()))
        {
            inputCommand.displayTileBuildFailure(tile, player->getSeat());
            inputCommand.displayPointerText(Ogre::ColourValue::Red, Helper::toString(pricePerTarget));
            return;
        }
        if(!canBuildOn(gameMap, tile))
        {
            inputCommand.displayText(Ogre::ColourValue::Red, "A door needs walls on two opposite sides.");
            inputCommand.displayPointerText(Ogre::ColourValue::Red, Helper::toString(pricePerTarget));
            return;
        }
        if(playerGold < pricePerTarget)
        {
            inputCommand.displayText(Ogre::ColourValue::Red,
                "Not enough gold. " + formatBuildTrap(type, pricePerTarget));
            inputCommand.displayPointerText(Ogre::ColourValue::Red, Helper::toString(pricePerTarget));
            return;
        }
        inputCommand.displayText(Ogre::ColourValue::White, formatBuildTrap(type, pricePerTarget));
        inputCommand.displayPointerText(Ogre::ColourValue::Red, Helper::toString(pricePerTarget));
        if(inputManager.mCommandState != InputCommandState::validated)
            return;

        ClientNotification *clientNotification = TrapManager::createTrapClientNotification(type);
        gameMap->tileToPacket(clientNotification->mPacket, tile);

        ODClient::getSingleton().queueClientNotification(clientNotification);
    }

    virtual bool buildTrap(GameMap* gameMap, Player* player, ODPacket& packet) const override
    {
        Tile* tile = gameMap->tileFromPacket(packet);
        if(tile == nullptr)
            return false;

        if(!tile->isBuildableUpon(player->getSeat()))
            return false;

        if(!canBuildOn(gameMap, tile))
            return false;

        // The door tile is ok
        int32_t pricePerTarget = TrapManager::costPerTile(mDoorType);
        if(!gameMap->withdrawFromTreasuries(pricePerTarget, player->getSeat()))
            return false;

        TrapDoor* trap = new TrapDoor(gameMap, mDoorType);
        std::vector<Tile*> tiles;
        tiles.push_back(tile);
        return buildTrapDefault(gameMap, trap, player->getSeat(), tiles);
    }

    virtual void checkBuildTrapEditor(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand) const override
    {
        Seat* seat = gameMap->getSeatById(inputManager.mSeatIdSelected);
        if(seat == nullptr)
        {
            OD_LOG_ERR("seatId=" + Helper::toString(inputManager.mSeatIdSelected));
            return;
        }

        TrapType type = mDoorType;
        // We only allow 1 tile for door trap
        Tile* tile = gameMap->getTile(inputManager.mXPos, inputManager.mYPos);
        if(tile == nullptr)
        {
            inputCommand.unselectAllTiles();
            return;
        }

        if(inputManager.mCommandState == InputCommandState::infoOnly)
        {
            const std::string& txt = TrapManager::getTrapReadableName(type);
            inputCommand.displayText(Ogre::ColourValue::White, txt);
            inputCommand.selectSquaredTiles(inputManager.mXPos, inputManager.mYPos, inputManager.mXPos,
                inputManager.mYPos);
            return;
        }

        if(inputManager.mCommandState == InputCommandState::building)
        {
            std::vector<Tile*> tiles;
            tiles.push_back(tile);
            inputCommand.selectTiles(tiles);
            // We accept any tile if there is no building and there are 2 full surrounding tiles
            if(tile->getIsBuilding() ||
               !canBuildOn(gameMap, tile))
            {
                inputCommand.displayText(Ogre::ColourValue::Red, "Cannot place door on this tile");
            }
            else
            {
                const std::string& txt = TrapManager::getTrapReadableName(type);
                inputCommand.displayText(Ogre::ColourValue::White, txt);
            }
            return;
        }

        if(!canBuildOn(gameMap, tile))
            return;

        ClientNotification *clientNotification = TrapManager::createTrapClientNotificationEditor(type);
        int32_t seatId = inputManager.mSeatIdSelected;
        clientNotification->mPacket << seatId;
        uint32_t nbTiles = 1;
        clientNotification->mPacket << nbTiles;
        gameMap->tileToPacket(clientNotification->mPacket, tile);

        ODClient::getSingleton().queueClientNotification(clientNotification);
    }

    virtual bool buildTrapEditor(GameMap* gameMap, ODPacket& packet) const override
    {
        int32_t seatId;
        OD_ASSERT_TRUE(packet >> seatId);
        Seat* seatTrap = gameMap->getSeatById(seatId);
        if(seatTrap == nullptr)
        {
            OD_LOG_ERR("seatId=" + Helper::toString(seatId));
            return false;
        }
        uint32_t nbTiles;
        OD_ASSERT_TRUE(packet >> nbTiles);
        // oki let;s pressume now on that there's needed only 1 tile to be build
        
        Tile* tile = gameMap->tileFromPacket(packet);
        if(tile == nullptr)
            return false;

        // If the tile is not buildable, we change it
        if(tile->getCoveringBuilding() != nullptr)
        {
            OD_LOG_ERR("tile=" + Tile::displayAsString(tile) + ", seatId=" + Helper::toString(seatId));
            return false;
        }

        if(!canBuildOn(gameMap, tile))
            return false;

        if((tile->getType() != TileType::gold) &&
           (tile->getType() != TileType::dirt))
        {
            tile->setType(TileType::dirt);
        }
        tile->setFullness(0.0);
        tile->claimTile(seatTrap);
        tile->computeTileVisual();

        std::vector<Tile*> tiles;
        tiles.push_back(tile);
        TrapDoor* trap = new TrapDoor(gameMap, mDoorType);
        return buildTrapDefault(gameMap, trap, seatTrap, tiles);
    }

    Trap* getTrapFromStream(GameMap* gameMap, std::istream& is) const override
    {
        TrapDoor* trap = new TrapDoor(gameMap, mDoorType);
        if(!Trap::importTrapFromStream(*trap, is))
        {
            OD_LOG_ERR("Error while building a trap from the stream");
        }
        return trap;
    }

    bool buildTrapOnTiles(GameMap* gameMap, Player* player, const std::vector<Tile*>& tiles, bool noFee = false) const override
    {        
        return buildTrapOnTiles(gameMap, player->getSeat(), tiles, noFee);
    }

    
    bool buildTrapOnTiles(GameMap* gameMap, Seat* seatPtr, const std::vector<Tile*>& tiles, bool noFee=false) const 
    {
        if(tiles.size() != 1)
            return false;

        int32_t pricePerTarget = TrapManager::costPerTile(mDoorType);
        int32_t price = static_cast<int32_t>(tiles.size()) * pricePerTarget;
        if(!noFee)
            if(!gameMap->withdrawFromTreasuries(price, seatPtr))
                return false;

        TrapDoor* trap = new TrapDoor(gameMap, mDoorType);
        return buildTrapDefault(gameMap, trap, seatPtr, tiles);
    }
};

// Register the factories
static TrapRegister regWooden(new TrapDoorFactory(TrapType::doorWooden, "DoorWooden", "Wooden door", "Wooden"));
static TrapRegister regIronbound(new TrapDoorFactory(TrapType::doorIronbound, "DoorIronbound", "Ironbound door", "Ironbound"));
static TrapRegister regSteel(new TrapDoorFactory(TrapType::doorSteel, "DoorSteel", "Steel door", "Steel"));
static TrapRegister regBarricade(new TrapDoorFactory(TrapType::doorBarricade, "DoorBarricade", "Barricade", "Barricade"));
static TrapRegister regSecret(new TrapDoorFactory(TrapType::doorSecret, "DoorSecret", "Secret door", "Secret"));
static TrapRegister regRuned(new TrapDoorFactory(TrapType::doorRuned, "DoorRuned", "Runed door", "Runed"));
}

const std::string TrapDoor::ANIMATION_OPEN = "Open";
const std::string TrapDoor::ANIMATION_CLOSE = "Close";

double TrapDoor::getHP(Tile* tile) const
{
    return SkillManager::getResearchValue(getSeat(), SkillType::trapDoorWooden, Building::getHP(tile));
}

double TrapDoor::takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage, double magicalDamage,
    double elementDamage, Tile* tileTakingDamage, bool ko)
{
    // Store health in base units, preserving its fraction across research and ownership changes.
    const double factor = SkillManager::getResearchValue(getSeat(), SkillType::trapDoorWooden, 1.0);
    return Building::takeDamage(attacker, absoluteDamage / factor, physicalDamage / factor,
        magicalDamage / factor, elementDamage / factor, tileTakingDamage, ko) * factor;
}

TrapDoor::TrapDoor(GameMap* gameMap, TrapType doorType) :
    Trap(gameMap),
    mDoorType(doorType),
    mIsLocked(doorType == TrapType::doorBarricade),
    mIsLockedState(false),
    mFireCooldownTurns(0)
{
    mReloadTime = 0;
    mMinDamage = 0;
    mMaxDamage = 0;
    mNbShootsBeforeDeactivation = -1;
    setMeshName("");
}

TrapEntity* TrapDoor::getTrapEntity(Tile* tile)
{
    Ogre::Real rotation = 90.0;
    Tile* tileW = getGameMap()->getTile(tile->getX() - 1, tile->getY());
    Tile* tileE = getGameMap()->getTile(tile->getX() + 1, tile->getY());

    if((tileW != nullptr) &&
       (tileE != nullptr) &&
       tileW->isFullTile() &&
       tileE->isFullTile())
    {
        rotation = 0.0;
    }
    return new DoorEntity(getGameMap(), *this, TrapManager::getMeshFromTrapType(mDoorType), tile, rotation, false, isActivated(tile) ? 1.0f : 0.5f,
        ANIMATION_OPEN, false);
}

double TrapDoor::getDefaultTileHP() const
{
    switch(mDoorType)
    {
        case TrapType::doorIronbound:
            return ConfigManager::getSingleton().getTrapConfigDouble("IronboundDoorHP");
        case TrapType::doorSteel:
            return ConfigManager::getSingleton().getTrapConfigDouble("SteelDoorHP");
        case TrapType::doorBarricade:
            return ConfigManager::getSingleton().getTrapConfigDouble("BarricadeDoorHP");
        case TrapType::doorSecret:
            return ConfigManager::getSingleton().getTrapConfigDouble("SecretDoorHP");
        case TrapType::doorRuned:
            return ConfigManager::getSingleton().getTrapConfigDouble("RunedDoorHP");
        default:
            return ConfigManager::getSingleton().getTrapConfigDouble("WoodenDoorHP");
    }
}

bool TrapDoor::shoot(Tile* tile)
{
    if(mDoorType == TrapType::doorSecret)
    {
        // Enemies discover the secret door when they see a creature of the owner (or of
        // an ally) pass through it. The seats with vision on the tile are then notified
        for(GameEntity* entity : tile->getEntitiesInTile())
        {
            if(entity->getObjectType() != GameEntityType::creature)
                continue;

            const Creature* creature = static_cast<const Creature*>(entity);
            if(getSeat()->isAlliedSeat(creature->getSeat()))
                return true;
        }

        return false;
    }

    // The doors return true to make sure every creature with vision on the door tile can see it.
    // The runed door also fires a fireball at the enemies standing on it, then has to recharge
    if(mDoorType != TrapType::doorRuned)
        return true;

    if(mFireCooldownTurns > 0)
        return true;

    std::vector<Tile*> doorTiles;
    doorTiles.push_back(tile);
    std::vector<GameEntity*> enemyCreatures = getGameMap()->getVisibleCreatures(doorTiles, getSeat(), true);
    if(enemyCreatures.empty())
        return true;

    // The runed door does not fire if its owner cannot pay the mana
    double manaToFire = ConfigManager::getSingleton().getTrapConfigDouble("RunedDoorManaToFire");
    if(!getSeat()->takeMana(manaToFire))
        return true;

    double damage = ConfigManager::getSingleton().getTrapConfigDouble("RunedDoorDamage");
    for(GameEntity* target : enemyCreatures)
    {
        Tile* targetTile = target->getCoveredTile(0);
        target->takeDamage(this, 0.0, 0.0, 0.0, damage, targetTile, false);
        target->notifyFightPlayer(targetTile);
    }
    mFireCooldownTurns = ConfigManager::getSingleton().getTrapConfigUInt32("RunedDoorReloadTurns");
    return true;
}

void TrapDoor::doUpkeep()
{
    if(mFireCooldownTurns > 0)
        --mFireCooldownTurns;

    for(Tile* tile : mCoveredTiles)
    {
        if((mDoorType != TrapType::doorBarricade) &&
           !canDoorBeOnTile(getGameMap(), tile))
        {
            std::map<Tile*, TileData*>::iterator it = mTileData.find(tile);
            if(it == mTileData.end())
            {
                OD_LOG_ERR("trap=" + getName() + ", tile=" + Tile::displayAsString(tile));
                return;
            }

            TrapTileData* trapTileData = static_cast<TrapTileData*>(it->second);
            trapTileData->mHP = 0.0;
        }

        // The runed door slowly repairs itself
        if((mDoorType == TrapType::doorRuned) &&
           (mTileData[tile]->mHP > 0.0))
        {
            double regen = ConfigManager::getSingleton().getTrapConfigDouble("RunedDoorRegenPerTurn");
            mTileData[tile]->mHP = std::min(getDefaultTileHP(), mTileData[tile]->mHP + regen);
        }

        // We need to look for destroyed door before calling Trap::doUpkeep otherwise, they will be removed
        // from covered tiles
        if (mTileData[tile]->mHP <= 0.0)
            getGameMap()->doorLock(tile, getSeat(), false);
        else if(mIsLockedState != mIsLocked)
        {
            RenderedMovableEntity* entity = getBuildingObjectFromTile(tile);
            if(entity == nullptr)
            {
                OD_LOG_ERR("nullptr entity trap=" + getName() + ", tile=" + Tile::displayAsString(tile));
                continue;
            }

            if(entity->getObjectType() != GameEntityType::trapEntity)
            {
                OD_LOG_ERR("wrong entity type trap=" + getName() + ", tile=" + Tile::displayAsString(tile) + ", entity=" + entity->getName());
                continue;
            }

            TrapEntity* trapEntity = static_cast<TrapEntity*>(entity);
            if(trapEntity->getTrapEntityType() != TrapEntityType::doorEntity)
            {
                OD_LOG_ERR("wrong entity type trap=" + getName() + ", tile=" + Tile::displayAsString(tile) + ", entity=" + entity->getName());
                continue;
            }

            DoorEntity* doorEntity = static_cast<DoorEntity*>(entity);
            changeDoorState(doorEntity, tile, mIsLocked);
        }
    }
    mIsLockedState = mIsLocked;

    // When a seat discovers a secret door, the tile has to be sent again to the seats
    // that saw it as a wall
    Tile* secretTile = nullptr;
    uint32_t nbSeatsVisionBefore = 0;
    if((mDoorType == TrapType::doorSecret) &&
       !mCoveredTiles.empty())
    {
        secretTile = mCoveredTiles.front();
        std::map<Tile*, TileData*>::iterator itBefore = mTileData.find(secretTile);
        if(itBefore != mTileData.end())
            nbSeatsVisionBefore = itBefore->second->mSeatsVision.size();
    }

    Trap::doUpkeep();

    if(secretTile != nullptr)
    {
        std::map<Tile*, TileData*>::iterator itAfter = mTileData.find(secretTile);
        if((itAfter != mTileData.end()) &&
           (itAfter->second->mSeatsVision.size() != nbSeatsVisionBefore))
        {
            secretTile->setDirtyForAllSeats();
        }
    }
}

bool TrapDoor::appearsAsWallForSeat(Tile* tile, Seat* seat) const
{
    if(mDoorType != TrapType::doorSecret)
        return false;

    if(getGameMap()->isInEditorMode())
        return false;

    if(getSeat()->isAlliedSeat(seat))
        return false;

    std::map<Tile*, TileData*>::const_iterator it = mTileData.find(tile);
    if(it == mTileData.end())
        return false;

    const std::vector<Seat*>& seatsVision = it->second->mSeatsVision;
    return std::find(seatsVision.begin(), seatsVision.end(), seat) == seatsVision.end();
}

void TrapDoor::notifyDoorSlapped(DoorEntity* doorEntity, Tile* tile)
{
    // A barricade cannot be opened
    if(mDoorType == TrapType::doorBarricade)
        return;

    mIsLocked = !mIsLocked;
    changeDoorState(doorEntity, tile, mIsLocked);

    mIsLockedState = mIsLocked;
}

void TrapDoor::changeDoorState(DoorEntity* doorEntity, Tile* tile, bool locked)
{
    if(locked)
        doorEntity->setAnimationState(ANIMATION_CLOSE, false, Ogre::Vector3::ZERO, false);
    else
        doorEntity->setAnimationState(ANIMATION_OPEN, false, Ogre::Vector3::ZERO, false);

    if(!isActivated(tile))
        return;

    getGameMap()->doorLock(tile, getSeat(), locked);
}

bool TrapDoor::canDoorBeOnTile(GameMap* gameMap, Tile* tile)
{
    // We check if the tile is suitable. It can only be built on 2 full tiles
    Tile* tileW = gameMap->getTile(tile->getX() - 1, tile->getY());
    Tile* tileE = gameMap->getTile(tile->getX() + 1, tile->getY());

    if((tileW != nullptr) &&
       (tileE != nullptr) &&
       tileW->isFullTile() &&
       tileE->isFullTile())
    {
        // Ok
        return true;
    }

    Tile* tileS = gameMap->getTile(tile->getX(), tile->getY() - 1);
    Tile* tileN = gameMap->getTile(tile->getX(), tile->getY() + 1);
    if((tileS != nullptr) &&
       (tileN != nullptr) &&
       tileS->isFullTile() &&
       tileN->isFullTile())
    {
        // Ok
        return true;
    }

    return false;
}

bool TrapDoor::permitsVision(Tile* tile)
{
    // A secret door blocks the sight like a wall, otherwise it would give itself away
    if(mDoorType == TrapType::doorSecret)
        return false;

    TrapTileData* trapTileData = static_cast<TrapTileData*>(mTileData.at(tile));
    if (!trapTileData->isActivated())
        return true;

    // A barricade does not hide what is behind it
    if(mDoorType == TrapType::doorBarricade)
        return true;

    return !mIsLockedState;
}

double TrapDoor::getCreatureSpeed(const Creature* creature, Tile* tile) const
{
    // Seats that did not discover a secret door see a wall and cannot walk through it
    if(appearsAsWallForSeat(tile, creature->getSeat()))
        return 0.0;

    const TrapTileData* trapTileData = static_cast<const TrapTileData*>(mTileData.at(tile));
    if (!trapTileData->isActivated())
        return tile->getCreatureSpeedDefault(creature);

    if(!mIsLocked)
        return tile->getCreatureSpeedDefault(creature);

    // No creature can pass a barricade, not even the flying ones. Enemies have to destroy it
    if(mDoorType == TrapType::doorBarricade)
        return 0.0;

    // Enemy units can go through doors. We need that otherwise, they won't be able to
    // get to the door. But in any case, if they are not fighting, we let them go. If
    // they are fighting, we don't
    if(getSeat()->isAlliedSeat(creature->getSeat()))
        return 0.0;

    if(creature->isActionInList(CreatureActionType::fight) ||
       creature->isActionInList(CreatureActionType::flee))
    {
        return 0.0;
    }

    return tile->getCreatureSpeedDefault(creature);
}

void TrapDoor::exportToStream(std::ostream& os) const
{
    Trap::exportToStream(os);

    os << mIsLocked << "\n";
}

bool TrapDoor::importFromStream(std::istream& is)
{
    if(!Trap::importFromStream(is))
        return false;

    if(!(is >> mIsLocked))
        return false;

    return true;
}
