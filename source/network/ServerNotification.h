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

#ifndef SERVERNOTIFICATION_H
#define SERVERNOTIFICATION_H

#include "network/ODPacket.h"

#include <string>
#include <Ogre.h>

class Tile;
class Creature;
class MovableGameEntity;
class Player;

enum class ServerNotificationType
{
    // Negotiation for multiplayer
    loadLevel, // Tells the client to load the level: + string LevelFilename
    pickNick,
    addPlayers,
    removePlayers,
    startGameMode,
    newMap,
    addClass,
    clientAccepted,
    clientRejected,
    seatConfigurationRefresh,

    
    playerConfigChange,

    chat,
    chatServer,

    turnStarted,

    animatedObjectSetWalkPath,
    setObjectAnimationState,
    entityPickedUp,
    entityDropped,
    entityTeleported,
    entitySlapped,

    playerFighting, // Tells the player he is under attack or attacking
    playerNoMoreFighting, // Tells the player he is no longer under attack or attacking

    addEntity,
    removeEntity,
    entitiesRefresh,
    orientEntity,
    refreshPlayerSeat,
    setEntityOpacity,
    notifyCreatureInfo,
    notifyTileInfo,
    refreshCreatureVisDebug,
    restoreEverVisitedTiles,

    refreshSeatVisDebug,

    playSpatialSound, // Makes the client play a sound at tile coordinates.
    playRelativeSound, // Makes the client play a sound.

    pingCreateAllEntities,
    pingCreateDraggableTileContainer,
    pingDeleteDraggableTileContainer,
    pingEditorAskSetRoundedPositionDraggableTileContainer,
    markTiles,
    refreshTiles,
    refreshVisibleTiles,
    revealTiles,
    revealTraps,
    carryEntity,
    releaseCarriedEntity,

    skillTree,
    skillsDone,

    setPlayerSettings,

    setSpellCooldown,

    playerEvents,
    
    displayText,

    //! \brief Answer to the editor asking what the waves of a wave portal are
    editorPortalWaveData,

    exit,

    //! The team of a seat changed (an alliance of a level script): + int32_t seatId, int32_t teamId
    seatTeam,

    //! Answer to editorRegionEdit, all the region markers of the level script:
    //! + uint32_t count, then per region: string name and 4 int32_t (the corners).
    editorRegionData,
    //! The time left until the level is lost: + int32_t seconds (-1: there is no time limit; a countdown that
    //! ends without a defeat has GameMap::TIME_LIMIT_COUNTDOWN_FLAG added), then an int32_t with the seconds
    //! left until the next hero wave of a level script (-1: none is shown)
    timeLimit
};

ODPacket& operator<<(ODPacket& os, const ServerNotificationType& nt);
ODPacket& operator>>(ODPacket& is, ServerNotificationType& nt);

//! \brief A data structure used to send messages to the clients
class ServerNotification
{
    friend class ODServer;

    public:
        /*! \brief Creates a message to be sent to concernedPlayer. If concernedPlayer is null, the message will be sent to
         *         every connected player.
         */
        ServerNotification(ServerNotificationType type, Player* concernedPlayer);
        virtual ~ServerNotification()
        {}

        ODPacket mPacket;

        static std::string typeString(ServerNotificationType type);

    private:
        ServerNotificationType mType;
        Player *mConcernedPlayer;
};

#endif // SERVERNOTIFICATION_H
