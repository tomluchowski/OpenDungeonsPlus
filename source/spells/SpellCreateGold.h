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

#ifndef SPELLCREATEGOLD_H
#define SPELLCREATEGOLD_H

#include "spells/Spell.h"

class GameMap;
class InputCommand;
class InputManager;

//! \brief Turns mana into a pile of gold on a claimed tile of the caster.
//! The pile can then be picked up and brought to a treasury.
class SpellCreateGold : public Spell
{
public:
    //! \brief Shows the price and the target tile while the player aims the spell and sends the cast request
    //! when the player confirms on a valid claimed tile of their own.
    static void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand);

    //! \brief Server side cast: takes the mana and drops a pile of gold on the claimed target tile.
    //! \return true if the spell was cast, false if the tile is invalid or the mana is not enough.
    static bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet);

    //! \brief The spell is instant and has no entity, so it can never be read from a stream (logs an error).
    //! \return Always nullptr.
    static Spell* getSpellFromStream(GameMap* gameMap, std::istream &is);

    //! \brief The spell is instant and has no entity, so it can never be read from a packet (logs an error).
    //! \return Always nullptr.
    static Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is);

    //! The spell type handled by this class.
    static const SpellType mSpellType;
};

#endif // SPELLCREATEGOLD_H
