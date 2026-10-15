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

    // Sent only to clients that negotiated live nickname changes.
    playerNickChanged,

    // Owner-only aggregate counts, sent only after creature-panel negotiation.
    creaturePanel,

    // Presentation-only burst for newly built gameplay room tiles.
    roomConstructionEffect,

    // Presentation-only creature melee impact.
    creatureCombatImpact,

    // Presentation-only consumption of an authoritative chicken.
    creatureChickenFeeding,

    // Owner-only reply to a production query or reorder request.
    trapProductionQueue,

    // Owner-only start of the defeat sequence for a defeated human player:
    // + int32_t conquerorSeatId (-1 if unknown), int32_t heartTileX, int32_t heartTileY (-1/-1 if unknown).
    // Appended last so that no existing numeric value changes.
    playerDefeated,

    // Owner-only debriefing counters, sent right after playerDefeated:
    // + int32_t elapsedSeconds, bool levelWon, int32_t seatCount, then per seat:
    // int32_t seatId and 6 uint32_t (keepers defeated, creatures killed, heroes destroyed,
    // rooms captured, items made, creatures converted).
    // Appended last so that no existing numeric value changes.
    levelStatistics,

    // Owner-only dungeon heart health for the ring of the top-left badge:
    // + float healthFraction (0 to 1, heart health / maximum heart health), bool underAttack.
    // Sent to a human owner when the fraction changed by at least one percentage point, when the
    // heart is destroyed, and once when the game starts or is loaded.
    // Appended last so that no existing numeric value changes.
    heartHealth
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
