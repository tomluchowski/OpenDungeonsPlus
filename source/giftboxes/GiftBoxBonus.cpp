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

#include "giftboxes/GiftBoxBonus.h"

#include "entities/Creature.h"
#include "entities/Tile.h"
#include "entities/TreasuryObject.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <iostream>

// Proposed values, they need to be approved
static const uint32_t DEFAULT_AMOUNT_MANA = 50000;
static const uint32_t DEFAULT_AMOUNT_GOLD = 10000;
static const uint32_t DEFAULT_AMOUNT_LEVELS = 1;

//! \brief Creates a bonus box of the given type and amount with the mystery box mesh
GiftBoxBonus::GiftBoxBonus(GameMap* gameMap, const std::string& baseName, GiftBoxType type, uint32_t amount) :
    GiftBoxEntity(gameMap, baseName, "MysteryBox", type),
    mAmount(amount)
{
    mPrevAnimationState = "Loop";
    mPrevAnimationStateLoop = true;
}

//! \brief Creates an empty bonus box of the given type, filled afterwards when it is loaded from a stream
GiftBoxBonus::GiftBoxBonus(GameMap* gameMap, GiftBoxType type) :
    GiftBoxEntity(gameMap, type),
    mAmount(0)
{
}

//! \brief Returns the name of the bonus of the given type
std::string GiftBoxBonus::getDisplayName(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return "Mana Boost";
        case GiftBoxType::gold:
            return "Increase Gold";
        case GiftBoxType::revealMap:
            return "Reveal Map";
        case GiftBoxType::levelUp:
            return "Increase Level";
        default:
            return "Special";
    }
}

//! \brief Returns a short text saying what the bonus of the given type does
std::string GiftBoxBonus::getDescription(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return "Gives a large amount of mana.";
        case GiftBoxType::gold:
            return "Gives a large amount of gold.";
        case GiftBoxType::revealMap:
            return "Reveals the whole map.";
        case GiftBoxType::levelUp:
            return "All your creatures gain a level.";
        default:
            return "";
    }
}

//! \brief Returns the amount a box of the given type carries by default
uint32_t GiftBoxBonus::getDefaultAmount(GiftBoxType type)
{
    switch(type)
    {
        case GiftBoxType::mana:
            return DEFAULT_AMOUNT_MANA;
        case GiftBoxType::gold:
            return DEFAULT_AMOUNT_GOLD;
        case GiftBoxType::levelUp:
            return DEFAULT_AMOUNT_LEVELS;
        case GiftBoxType::revealMap:
            return 0;
        default:
            OD_LOG_ERR("Unexpected GiftBoxType=" + Helper::toString(static_cast<uint32_t>(type)));
            return 0;
    }
}

//! \brief Writes the box and its amount to the stream
void GiftBoxBonus::exportToStream(std::ostream& os) const
{
    GiftBoxEntity::exportToStream(os);
    os << mAmount << "\t";
}

//! \brief Reads the box and its amount from the stream. Returns false if the data is incomplete
bool GiftBoxBonus::importFromStream(std::istream& is)
{
    if(!GiftBoxEntity::importFromStream(is))
        return false;
    if(!(is >> mAmount))
        return false;

    return true;
}

//! \brief Server side. Gives the bonus of this box to the seat that delivered it
void GiftBoxBonus::applyEffect()
{
    Seat* seat = getSeat();
    if(seat == nullptr)
    {
        OD_LOG_ERR("null Seat for giftbox=" + getName() + " pos=" + Helper::toString(getPosition()));
        return;
    }

    applyBonus(getGameMap(), seat, getGiftBoxType(), mAmount, getPositionTile());
}

//! \brief Server side. Applies the bonus of the given type and amount to the seat. Gold the
//! treasuries cannot hold is left on the given tile
void GiftBoxBonus::applyBonus(GameMap* gameMap, Seat* seat, GiftBoxType type, uint32_t amount, Tile* positionTile)
{
    switch(type)
    {
        case GiftBoxType::mana:
        {
            gameMap->addManaToSeat(static_cast<int>(amount), seat->getId());
            break;
        }
        case GiftBoxType::gold:
        {
            // What the treasuries cannot hold is left on the ground where the box was
            int notStored = gameMap->addGoldToSeat(static_cast<int>(amount), seat->getId());
            Tile* tile = positionTile;
            if((notStored > 0) && (tile != nullptr))
            {
                TreasuryObject* obj = new TreasuryObject(gameMap, notStored);
                obj->addToGameMap();
                Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                            static_cast<Ogre::Real>(tile->getY()), 0.0f);
                obj->createMesh();
                obj->setPosition(spawnPosition);
            }
            break;
        }
        case GiftBoxType::revealMap:
        {
            seat->revealMapPermanently();
            break;
        }
        case GiftBoxType::levelUp:
        {
            std::vector<Creature*> creatures = gameMap->getCreaturesBySeat(seat);
            for(Creature* creature : creatures)
            {
                if(!creature->isAlive())
                    continue;

                creature->setLevel(creature->getLevel() + amount);
            }
            break;
        }
        default:
            OD_LOG_ERR("Unexpected GiftBoxType=" + Helper::toString(static_cast<uint32_t>(type)));
            break;
    }
}
