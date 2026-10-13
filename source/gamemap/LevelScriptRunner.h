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

#ifndef LEVELSCRIPTRUNNER_H
#define LEVELSCRIPTRUNNER_H

class Creature;
class GameMap;

namespace LevelScriptRunner
{
    //! \brief Checks the triggers of the game map level script and runs the actions of
    //! the ones whose conditions are met. Server side only, called once per turn.
    void doTurn(GameMap& gameMap);

    //! \brief Carries out the standing order that a script action gave to the creature (go to
    //! the dungeon of a player, walk to a point, hunt creatures, wait, steal gold). Server side only,
    //! called when the creature has nothing to do. Returns true if the order takes the turn of the
    //! creature, false if the creature may do what it does without an order.
    bool doCreatureOrder(Creature& creature);
}

#endif // LEVELSCRIPTRUNNER_H
