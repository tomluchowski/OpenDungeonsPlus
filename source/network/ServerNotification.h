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

    //! A short cosmetic note that something happened (a mood change, a full treasury, a blow, a missile
    //! launch): + a CosmeticEvent (see network/CosmeticEvent.h): int32_t kind, string subject, string object,
    //! string text, int32_t value, int32_t value2, Vector3 position. Only sent to clients that negotiated
    //! cosmetic events; an older client never gets it. Appended last so that no existing numeric value
    //! changes.
    cosmeticEvent,

    //! Owner-only tier of a creature pair that changed (or the current tier, sent once when a
    //! client joins or a game is loaded): + string creatureA, string creatureB, int32_t tier
    //! (RelationshipTier), bool replay (true: replay of the current tier, no Dungeonbook post or emote).
    //! Only sent when the creature relationships option is on.
    //! Appended last so that no existing numeric value changes.
    relationshipTier
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
