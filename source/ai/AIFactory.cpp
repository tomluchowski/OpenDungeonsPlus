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
            return new KeeperAI(gameMap, player, 30, 50, 30, 50, 60, 80, 150, 25, 14, 2, 2, 1);
        case KeeperAIType::normal:
            return new KeeperAI(gameMap, player, 0, 5, 0, 5, 30, 50, 100, 20, 11, 4, 5, 3);
        case KeeperAIType::hard:
            return new KeeperAI(gameMap, player, 0, 3, 0, 3, 20, 35, 60, 12, 8, 7, 8, 5);
        default:
            break;
    }
    OD_LOG_ERR("Asked wrong AI type=" + KeeperAITypes::toString(type));
    return nullptr;
}
} // namespace AIFactory
