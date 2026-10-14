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


#include "creatureskill/CreatureSkillTeleport.h"

#include "creatureaction/CreatureAction.h"
#include "creatureskill/CreatureSkillManager.h"
#include "entities/Creature.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <istream>

const std::string CreatureSkillTeleportName = "Teleport";

namespace
{
class CreatureSkillTeleportFactory : public CreatureSkillFactory
{
    CreatureSkill* createCreatureSkill() const override
    { return new CreatureSkillTeleport; }

    const std::string& getCreatureSkillName() const override
    { return CreatureSkillTeleportName; }
};

// Register the factory
static CreatureSkillRegister reg(new CreatureSkillTeleportFactory);

//! \brief Returns a tile of the dungeon heart of the creature seat, nullptr if there is none
Tile* getHeartTile(GameMap& gameMap, Creature* creature)
{
    std::vector<Room*> hearts = gameMap.getRoomsByTypeAndSeat(RoomType::dungeonTemple, creature->getSeat());
    if(hearts.empty())
        return nullptr;

    return hearts[0]->getCoveredTile(0);
}

double squaredDistance(const Tile* tile1, const Tile* tile2)
{
    double dx = static_cast<double>(tile1->getX() - tile2->getX());
    double dy = static_cast<double>(tile1->getY() - tile2->getY());
    return (dx * dx) + (dy * dy);
}
}

const std::string& CreatureSkillTeleport::getSkillName() const
{
    return CreatureSkillTeleportName;
}

bool CreatureSkillTeleport::canBeUsedBy(const Creature* creature) const
{
    if(creature->getLevel() < mCreatureLevelMin)
        return false;

    return true;
}

bool CreatureSkillTeleport::tryUseSupport(GameMap& gameMap, Creature* creature) const
{
    if(!creature->isAlive() || creature->isFrozen())
        return false;

    Tile* myTile = creature->getPositionTile();
    if(myTile == nullptr)
        return false;

    const double minDistanceSquared = mMinDistance * mMinDistance;

    // A possessed creature can only go back to the dungeon heart
    if(creature->isPossessed())
    {
        Tile* heartTile = getHeartTile(gameMap, creature);
        if((heartTile == nullptr) || (squaredDistance(myTile, heartTile) <= minDistanceSquared))
            return false;

        creature->teleportTo(heartTile);
        return true;
    }

    // In danger (low health and enemies in sight), the creature jumps to the dungeon heart
    // (the const version returns the enemies seen during this turn upkeep)
    const Creature* constCreature = creature;
    bool isInDanger = (creature->getHP() <= (creature->getMaxHp() * mDangerPercent / 100.0)) &&
        !constCreature->getVisibleEnemyObjects().empty();
    if(isInDanger)
    {
        Tile* heartTile = getHeartTile(gameMap, creature);
        if((heartTile == nullptr) || (squaredDistance(myTile, heartTile) <= minDistanceSquared))
            return false;

        creature->clearActionQueue();
        creature->teleportTo(heartTile);
        return true;
    }

    // On its way to a faraway place of its dungeon, the creature jumps there
    if(!creature->isMoving() || creature->getActions().empty() ||
       (creature->getActions().back()->getType() != CreatureActionType::walkToTile))
    {
        return false;
    }

    Tile* destination = creature->getWalkDestinationTile();
    if((destination == nullptr) || destination->isFullTile() || !destination->isClaimedForSeat(creature->getSeat()))
        return false;

    if(squaredDistance(myTile, destination) <= minDistanceSquared)
        return false;

    // The walk is over once the creature is there
    creature->teleportTo(destination);
    creature->popAction();
    return true;
}

CreatureSkillTeleport* CreatureSkillTeleport::clone() const
{
    return new CreatureSkillTeleport(*this);
}

void CreatureSkillTeleport::getFormatString(std::string& format) const
{
    CreatureSkill::getFormatString(format);
    if(!format.empty())
        format += "\t";

    format += "LevelMin\tMinDistance\tDangerPercent";
}

void CreatureSkillTeleport::exportToStream(std::ostream& os) const
{
    CreatureSkill::exportToStream(os);
    os << "\t" << mCreatureLevelMin;
    os << "\t" << mMinDistance;
    os << "\t" << mDangerPercent;
}

bool CreatureSkillTeleport::importFromStream(std::istream& is)
{
    if(!CreatureSkill::importFromStream(is))
        return false;

    if(!(is >> mCreatureLevelMin))
        return false;
    if(!(is >> mMinDistance))
        return false;
    if(!(is >> mDangerPercent))
        return false;

    return true;
}

bool CreatureSkillTeleport::isEqual(const CreatureSkill& creatureSkill) const
{
    if(!CreatureSkill::isEqual(creatureSkill))
        return false;

    const CreatureSkillTeleport* skill = dynamic_cast<const CreatureSkillTeleport*>(&creatureSkill);
    if(skill == nullptr)
        return false;

    if(mCreatureLevelMin != skill->mCreatureLevelMin)
        return false;
    if(mMinDistance != skill->mMinDistance)
        return false;
    if(mDangerPercent != skill->mDangerPercent)
        return false;

    return true;
}
