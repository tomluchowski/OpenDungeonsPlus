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

#ifndef SPELLBREAKWALL_H
#define SPELLBREAKWALL_H

#include "spells/Spell.h"
#include "spells/SpellType.h"

class GameMap;
class InputCommand;
class InputManager;
class Seat;
class Tile;

//! \brief Unclaims walls reinforced by an enemy team. Workers cannot dig such a wall, nor claim it
//! while enemy ground lies behind it; once unclaimed it is a plain earth wall that anybody can dig
//! or claim. Only walls the caster can see can be broken.
class SpellBreakWall : public Spell
{
public:
    static void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand);
    static bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet);

    static Spell* getSpellFromStream(GameMap* gameMap, std::istream &is);
    static Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is);

    //! \brief Whether the given seat can break the given tile. Works on the client map (from
    //! what the tiles look like) as well as on the server map.
    static bool canBreakTile(Tile* tile, Seat* seat);

    //! \brief Why the given seat cannot break the given tile, to tell the player, or an empty
    //! string if it can.
    static std::string getRefusalReason(Tile* tile, Seat* seat);

    static const SpellType mSpellType;
};

#endif // SPELLBREAKWALL_H
