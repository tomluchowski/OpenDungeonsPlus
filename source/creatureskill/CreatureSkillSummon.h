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


#ifndef CREATURESKILLSUMMON_H
#define CREATURESKILLSUMMON_H

#include <cstdint>
#include <string>

class Creature;
class GameMap;
class Tile;

namespace CreatureSkillSummon
{
//! \brief Creates a creature of the given class for the seat of the caster on the given tile. The creature
//! falls apart after nbTurns turns. Returns nullptr if the class does not exist.
Creature* summonTemporaryCreature(GameMap& gameMap, const Creature& caster, const std::string& creatureClass,
    Tile* tile, int32_t nbTurns);
}

#endif // CREATURESKILLSUMMON_H
