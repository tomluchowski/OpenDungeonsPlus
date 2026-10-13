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

#ifndef SPELLSUMMONCHAMPION_H
#define SPELLSUMMONCHAMPION_H

#include "spells/Spell.h"
#include "spells/SpellType.h"

class GameMap;
class InputCommand;
class InputManager;
class Seat;

//! \brief Summons the champion, a unique creature that cannot be hurt and charges at the enemies.
//! The cast price covers the first seconds, after that the owner pays a mana drain per second
//! (see Creature::handleChampionUpkeep). The spell is unlocked by the campaign Heartstone.
class SpellSummonChampion : public Spell
{
public:
    //! \brief Shows the cast preview on the hovered tile and queues the cast request when the player clicks.
    //! \param gameMap The map the spell is cast on.
    //! \param inputManager The current mouse position and buttons.
    //! \param inputCommand Used to show the price text and to select the target tile.
    static void checkSpellCast(GameMap* gameMap, const InputManager& inputManager, InputCommand& inputCommand);
    //! \brief Server side cast: pays the price and spawns the champion on the requested tile.
    //! \param gameMap The map the spell is cast on.
    //! \param player The player that casts the spell.
    //! \param packet The cast request holding the target tile.
    //! \return True if the champion was summoned.
    static bool castSpell(GameMap* gameMap, Player* player, ODPacket& packet);

    //! \brief True if the given seat already has a champion on the map. Only one can exist at a time
    static bool hasChampion(GameMap* gameMap, const Seat* seat);

    //! \brief The champion is a creature, not a saved spell, so reading it from a file is not supported and returns nullptr.
    static Spell* getSpellFromStream(GameMap* gameMap, std::istream &is);
    //! \brief The champion is a creature, not a sent spell, so reading it from a packet is not supported and returns nullptr.
    static Spell* getSpellFromPacket(GameMap* gameMap, ODPacket &is);

    static const SpellType mSpellType;
};

#endif // SPELLSUMMONCHAMPION_H
