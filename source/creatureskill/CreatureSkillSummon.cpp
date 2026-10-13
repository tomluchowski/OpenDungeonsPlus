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


#include "creatureskill/CreatureSkillSummon.h"

#include "creatureeffect/CreatureEffectTemporary.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

namespace CreatureSkillSummon
{
Creature* summonTemporaryCreature(GameMap& gameMap, const Creature& caster, const std::string& creatureClass,
    Tile* tile, int32_t nbTurns)
{
    if(tile == nullptr)
        return nullptr;

    const CreatureDefinition* definition = ConfigManager::getSingleton().getCreatureDefinition(creatureClass);
    if(definition == nullptr)
    {
        OD_LOG_ERR("No creature definition for the summoned creature, class=" + creatureClass);
        return nullptr;
    }

    Creature* summoned = new Creature(&gameMap, definition, caster.getSeat());
    summoned->addToGameMap();
    Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(tile->getX()),
                                static_cast<Ogre::Real>(tile->getY()),
                                static_cast<Ogre::Real>(0.0));
    // There is no dedicated summon effect yet, the one of the Summon worker spell is used as a placeholder
    summoned->addParticleEffect("SummonWorker", 3);
    summoned->createMesh();
    summoned->setPosition(spawnPosition);
    summoned->addCreatureEffect(new CreatureEffectTemporary(nbTurns));

    return summoned;
}
}
