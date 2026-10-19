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

#ifndef WORKEREXTRAS_H
#define WORKEREXTRAS_H

#include <string>

class Creature;
class GameMap;

/*! \brief Small cosmetic extras of the worker reactions, client side only: carried prisoners that struggle and the
 * sounds of the workers.
 *
 * Nothing here touches the game: no animation state is set, no activity is changed and no message is sent. A
 * struggling prisoner moves the scene node of the carried body a little (bobbing, swaying, tilting, with a strength
 * and a beat chosen by chance) for as long as it is carried and puts the node back the way it was at the end.
 */
class WorkerExtras
{
public:
    //! \brief The body is picked up by the carrier. A creature that is still alive starts to struggle.
    static void startStruggle(GameMap* gameMap, Creature* carrier, Creature* body);

    //! \brief The body is put down: the node is put back the way it was
    static void endStruggle(GameMap* gameMap, const std::string& bodyName);

    //! \brief Moves the struggling bodies. With \c enabled false they are put back and forgotten.
    static void update(GameMap* gameMap, double timeSinceLastFrame, bool enabled);

    //! \brief Puts all bodies back and forgets them
    static void stopAll(GameMap* gameMap);

    //! \brief Plays the sound that belongs to the reaction event (if it has one) at the place of the creature
    static void playSound(const std::string& eventName, const Creature* creature, double now);

    //! \brief The sound family of the event, empty if the event has no sound
    static std::string getSoundFamily(const std::string& eventName);
};

#endif // WORKEREXTRAS_H
