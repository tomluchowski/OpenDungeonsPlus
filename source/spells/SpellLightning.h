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

#ifndef SPELLLIGHTNING_H
#define SPELLLIGHTNING_H

#include "spells/Spell.h"
#include "spells/SpellType.h"

class GameMap;
class InputCommand;
class InputManager;

//! \brief Hurls a lightning bolt on an enemy creature standing on the caster's claimed land.
//! The target takes damage and is knocked down for a few turns.
class SpellLightning : public Spell
{
public:
    //! \brief Shows the price and highlights the enemy creature under the cursor while the player aims the spell,
    //! and sends the cast request when the player confirms on an enemy standing on claimed land of their own.
    static void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand);

    //! \brief Server side cast: takes the mana, damages the target creature and knocks it down for a few turns.
    //! \return true if the spell was cast, false if the creature is not a valid target or the mana is not enough.
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

#endif // SPELLLIGHTNING_H
