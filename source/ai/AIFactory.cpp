/*!
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

#include "ai/AIFactory.h"

#include "ai/KeeperAI.h"
#include "ai/KeeperAIType.h"
#include "utils/LogManager.h"

namespace AIFactory
{
namespace
{
//! Values of the easy AI level, named like the parameters of the KeeperAI constructor
//! (cooldowns in turns, percent values in percent, counts in creatures or tiles).
static const int EASY_COOLDOWN_DEFENSE_MIN = 30;
static const int EASY_COOLDOWN_DEFENSE_MAX = 50;
static const int EASY_COOLDOWN_SAVE_WOUNDED_MIN = 30;
static const int EASY_COOLDOWN_SAVE_WOUNDED_MAX = 50;
static const int EASY_COOLDOWN_LOOKING_FOR_ROOMS_MIN = 60;
static const int EASY_COOLDOWN_LOOKING_FOR_ROOMS_MAX = 80;
static const int EASY_REACTION_PERCENT = 150;
static const int EASY_MIN_HP_PERCENT_TO_FIGHT = 25;
static const int EASY_MIN_FIGHTERS_TO_ATTACK = 14;
static const int EASY_MIN_CREATURE_LEVEL = 2;
static const int EASY_MAX_TRAP_TILES = 2;
static const int EASY_MAX_DOORS = 1;

//! Values of the normal AI level, named like the parameters of the KeeperAI constructor.
static const int NORMAL_COOLDOWN_DEFENSE_MIN = 0;
static const int NORMAL_COOLDOWN_DEFENSE_MAX = 5;
static const int NORMAL_COOLDOWN_SAVE_WOUNDED_MIN = 0;
static const int NORMAL_COOLDOWN_SAVE_WOUNDED_MAX = 5;
static const int NORMAL_COOLDOWN_LOOKING_FOR_ROOMS_MIN = 30;
static const int NORMAL_COOLDOWN_LOOKING_FOR_ROOMS_MAX = 50;
static const int NORMAL_REACTION_PERCENT = 100;
static const int NORMAL_MIN_HP_PERCENT_TO_FIGHT = 20;
static const int NORMAL_MIN_FIGHTERS_TO_ATTACK = 11;
static const int NORMAL_MIN_CREATURE_LEVEL = 4;
static const int NORMAL_MAX_TRAP_TILES = 5;
static const int NORMAL_MAX_DOORS = 3;

//! Values of the hard AI level, named like the parameters of the KeeperAI constructor.
static const int HARD_COOLDOWN_DEFENSE_MIN = 0;
static const int HARD_COOLDOWN_DEFENSE_MAX = 3;
static const int HARD_COOLDOWN_SAVE_WOUNDED_MIN = 0;
static const int HARD_COOLDOWN_SAVE_WOUNDED_MAX = 3;
static const int HARD_COOLDOWN_LOOKING_FOR_ROOMS_MIN = 20;
static const int HARD_COOLDOWN_LOOKING_FOR_ROOMS_MAX = 35;
static const int HARD_REACTION_PERCENT = 60;
static const int HARD_MIN_HP_PERCENT_TO_FIGHT = 12;
static const int HARD_MIN_FIGHTERS_TO_ATTACK = 8;
static const int HARD_MIN_CREATURE_LEVEL = 7;
static const int HARD_MAX_TRAP_TILES = 8;
static const int HARD_MAX_DOORS = 5;
} // namespace

//! \brief Creates the keeper AI of the given level for a player.
//! \param gameMap The map the AI plays on.
//! \param player The player the AI controls.
//! \param type The AI level: easy, normal or hard.
//! \return The new AI, or nullptr (with an error in the log) for any other AI type.
BaseAI* createAI(GameMap& gameMap, Player& player, KeeperAIType type)
{
    // Easy, normal and hard differ in attack threshold (minFightersToAttack), minimum creature
    // level for an attack, retreat health (minHpPercentToFight), reaction scale, cooldowns and
    // trap and door counts. The harder the level, the earlier and the stronger it attacks.
    //   attack with more than N creatures: 14 / 11 / 8;
    //   minimum creature level for an attack: 2 / 4 / 7; retreat at health percent: 25 / 20 / 12.
    switch(type)
    {
        case KeeperAIType::easy:
            return new KeeperAI(gameMap, player, EASY_COOLDOWN_DEFENSE_MIN, EASY_COOLDOWN_DEFENSE_MAX,
                EASY_COOLDOWN_SAVE_WOUNDED_MIN, EASY_COOLDOWN_SAVE_WOUNDED_MAX,
                EASY_COOLDOWN_LOOKING_FOR_ROOMS_MIN, EASY_COOLDOWN_LOOKING_FOR_ROOMS_MAX,
                EASY_REACTION_PERCENT, EASY_MIN_HP_PERCENT_TO_FIGHT, EASY_MIN_FIGHTERS_TO_ATTACK,
                EASY_MIN_CREATURE_LEVEL, EASY_MAX_TRAP_TILES, EASY_MAX_DOORS);
        case KeeperAIType::normal:
            return new KeeperAI(gameMap, player, NORMAL_COOLDOWN_DEFENSE_MIN, NORMAL_COOLDOWN_DEFENSE_MAX,
                NORMAL_COOLDOWN_SAVE_WOUNDED_MIN, NORMAL_COOLDOWN_SAVE_WOUNDED_MAX,
                NORMAL_COOLDOWN_LOOKING_FOR_ROOMS_MIN, NORMAL_COOLDOWN_LOOKING_FOR_ROOMS_MAX,
                NORMAL_REACTION_PERCENT, NORMAL_MIN_HP_PERCENT_TO_FIGHT, NORMAL_MIN_FIGHTERS_TO_ATTACK,
                NORMAL_MIN_CREATURE_LEVEL, NORMAL_MAX_TRAP_TILES, NORMAL_MAX_DOORS);
        case KeeperAIType::hard:
            return new KeeperAI(gameMap, player, HARD_COOLDOWN_DEFENSE_MIN, HARD_COOLDOWN_DEFENSE_MAX,
                HARD_COOLDOWN_SAVE_WOUNDED_MIN, HARD_COOLDOWN_SAVE_WOUNDED_MAX,
                HARD_COOLDOWN_LOOKING_FOR_ROOMS_MIN, HARD_COOLDOWN_LOOKING_FOR_ROOMS_MAX,
                HARD_REACTION_PERCENT, HARD_MIN_HP_PERCENT_TO_FIGHT, HARD_MIN_FIGHTERS_TO_ATTACK,
                HARD_MIN_CREATURE_LEVEL, HARD_MAX_TRAP_TILES, HARD_MAX_DOORS);
        default:
            break;
    }
    OD_LOG_ERR("Asked wrong AI type=" + KeeperAITypes::toString(type));
    return nullptr;
}
} // namespace AIFactory
