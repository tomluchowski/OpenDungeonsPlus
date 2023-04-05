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

#include "network/ODClient.h"

#include "camera/CullingManager.h"
#include "entities/Building.h"
#include "entities/Creature.h"
#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/EntityLoading.h"
#include "entities/GameEntityType.h"
#include "entities/MapLight.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "entities/Weapon.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "game/Skill.h"
#include "game/SkillType.h"
#include "gamemap/GameMap.h"
#include "gamemap/DraggableTileContainer.h"
#include "modes/EditorMode.h"
#include "modes/GameMode.h"
#include "modes/InputCommand.h"
#include "modes/MenuModeConfigureSeats.h"
#include "modes/ModeManager.h"
#include "network/ChatEventMessage.h"
#include "network/ODPacket.h"
#include "network/ServerMode.h"
#include "network/ServerNotification.h"
#include "render/ODFrameListener.h"
#include "render/RenderManager.h"
#include "sound/MusicPlayer.h"
#include "sound/SoundEffectsManager.h"
#include "spells/SpellType.h"
#include "traps/Trap.h"
#include "traps/TrapType.h"
#include "traps/TrapManager.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "ODApplication.h"

#include <boost/lexical_cast.hpp>

#include <string>

template<> ODClient* Ogre::Singleton<ODClient>::msSingleton = nullptr;

ODClient::ODClient() :
    ODSocketClient(),
    mIsPlayerConfig(false)
{
}

ODClient::~ODClient()
{
}

void ODClient::playerDisconnected()
{
    std::string message = getPlayer() ? getPlayer()->getNick() + " disconnected from the server." : "A player disconnected.";
    // Place an event in the queue to inform the user about the disconnection.
    addEventMessage(new EventMessage(message));
    // TODO : try to reconnect to the server
}

bool ODClient::processMessage(ServerNotificationType cmd, ODPacket& packetReceived)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();

    GameMap* gameMap = frameListener->getClientGameMap();

    if (!gameMap)
        return false;

    OD_LOG_INF("processMessage type=" + ServerNotification::typeString(cmd));
    switch(cmd)
    {
        case ServerNotificationType::loadLevel:
        {
            std::string odVersion;
            OD_ASSERT_TRUE(packetReceived >> odVersion);

            // Map
            int32_t mapSizeX;
            int32_t mapSizeY;
            OD_ASSERT_TRUE(packetReceived >> mapSizeX);
            OD_ASSERT_TRUE(packetReceived >> mapSizeY);
            if (!gameMap->createNewMap(mapSizeX, mapSizeY))
                return false;

            // Map infos
            std::string str;
            OD_ASSERT_TRUE(packetReceived >> mLevelFilename);
            gameMap->setLevelName(mLevelFilename);
            OD_ASSERT_TRUE(packetReceived >> str);
            gameMap->setLevelDescription(str);
            OD_ASSERT_TRUE(packetReceived >> str);
            gameMap->setLevelMusicFile(str);
            OD_ASSERT_TRUE(packetReceived >> str);
            gameMap->setLevelFightMusicFile(str);

            OD_ASSERT_TRUE(packetReceived >> str);
            if(str.empty())
                str = ConfigManager::DEFAULT_TILESET_NAME;

            gameMap->setTileSetName(str);

            int32_t nb;
            // Seats
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Seat* seat = new Seat(gameMap);
                OD_ASSERT_TRUE(seat->importFromPacket(packetReceived));
                if(!gameMap->addSeat(seat))
                {
                    OD_LOG_ERR("Couldn't add seat id=" + Helper::toString(seat->getId()));
                    delete seat;
                }
            }

            // Creature definitions
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                CreatureDefinition* def = new CreatureDefinition();
                OD_ASSERT_TRUE(packetReceived >> def);
                gameMap->addClassDescription(def);
            }

            // Weapons
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Weapon* def = new Weapon();
                OD_ASSERT_TRUE(packetReceived >> def);
                gameMap->addWeapon(def);
            }

            // Tiles
            // Gold
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                tile->setType(TileType::gold);
                tile->setTileVisualIfArgNotNull(TileVisual::goldFull);
            }
            // Rock
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                tile->setType(TileType::rock);
                tile->setTileVisualIfArgNotNull(TileVisual::rockFull);
            }
            // Gem
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                tile->setType(TileType::gem);
                tile->setTileVisualIfArgNotNull(TileVisual::gemFull);
            }
            // Water
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                tile->setType(TileType::water);
                tile->setTileVisualIfArgNotNull(TileVisual::waterGround);
            }
            // Lava
            OD_ASSERT_TRUE(packetReceived >> nb);
            while(nb > 0)
            {
                --nb;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                tile->setType(TileType::lava);
                tile->setTileVisualIfArgNotNull(TileVisual::lavaGround);
            }

            
            gameMap->setAllFullnessAndNeighbors();

            ODPacket packSend;
            packSend << ClientNotificationType::levelOK;
            send(packSend);
            break;
        }

        case ServerNotificationType::pickNick:
        {
            ServerMode serverMode;
            OD_ASSERT_TRUE(packetReceived >> serverMode);

            ODPacket packSend;
            const std::string& nick = gameMap->getLocalPlayerNick();
            packSend << ClientNotificationType::setNick << nick;
            send(packSend);

            // We can proceed to configure seat level
            switch(serverMode)
            {
                case ServerMode::ModeGameSinglePlayer:
                case ServerMode::ModeGameMultiPlayer:
                case ServerMode::ModeGameLoaded:
                    frameListener->getModeManager()->requestMode(AbstractModeManager::MENU_CONFIGURE_SEATS);
                    break;
                case ServerMode::ModeEditor:
                    break;
                default:
                    OD_LOG_ERR("Unknown server mode=" + Helper::toString(static_cast<int32_t>(serverMode)));
                    break;
            }
            // If we are watching a replay, we force stopping the processing loop to
            // allow changing mode (because there is no synchronization as there is no server)
            if(getSource() == ODSource::file)
                return false;
            break;
        }

        case ServerNotificationType::playerConfigChange:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::MENU_CONFIGURE_SEATS)
                break;

            MenuModeConfigureSeats* mode = static_cast<MenuModeConfigureSeats*>(frameListener->getModeManager()->getCurrentMode());
            mode->activatePlayerConfig();
            break;
        }

        case ServerNotificationType::seatConfigurationRefresh:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::MENU_CONFIGURE_SEATS)
                break;

            MenuModeConfigureSeats* mode = static_cast<MenuModeConfigureSeats*>(frameListener->getModeManager()->getCurrentMode());
            mode->refreshSeatConfiguration(packetReceived);
            break;
        }

        case ServerNotificationType::addPlayers:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::MENU_CONFIGURE_SEATS)
            {
                OD_LOG_ERR("Wrong mode " + Helper::toString(frameListener->getModeManager()->getCurrentModeType()));
                break;
            }

            MenuModeConfigureSeats* mode = static_cast<MenuModeConfigureSeats*>(frameListener->getModeManager()->getCurrentMode());
            uint32_t nbPlayers;
            OD_ASSERT_TRUE(packetReceived >> nbPlayers);
            for(uint32_t i = 0; i < nbPlayers; ++i)
            {
                std::string nick;
                int32_t id;
                OD_ASSERT_TRUE(packetReceived >> nick >> id);
                mode->addPlayer(nick, id);
                addEventMessage(new EventMessage(nick + " is now connected."));
            }
            break;
        }

        case ServerNotificationType::removePlayers:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::MENU_CONFIGURE_SEATS)
            {
                OD_LOG_ERR("Wrong mode " + Helper::toString(frameListener->getModeManager()->getCurrentModeType()));
                break;
            }

            MenuModeConfigureSeats* mode = static_cast<MenuModeConfigureSeats*>(frameListener->getModeManager()->getCurrentMode());
            uint32_t nbPlayers;
            OD_ASSERT_TRUE(packetReceived >> nbPlayers);
            for(uint32_t i = 0; i < nbPlayers; ++i)
            {
                int32_t id;
                OD_ASSERT_TRUE(packetReceived >> id);
                mode->removePlayer(id);
            }
            break;
        }

        case ServerNotificationType::clientAccepted:
        {
            int32_t nbPlayers;
            OD_ASSERT_TRUE(packetReceived >> ODApplication::turnsPerSecond);

            OD_ASSERT_TRUE(packetReceived >> nbPlayers);
            for(int i = 0; i < nbPlayers; ++i)
            {
                std::string nick;
                int32_t playerId;
                int32_t seatId;
                int32_t teamId;
                OD_ASSERT_TRUE(packetReceived >> nick >> playerId >> seatId >> teamId);
                Player *tempPlayer = new Player(gameMap, playerId);
                tempPlayer->setNick(nick);
                gameMap->addPlayer(tempPlayer);

                Seat* seat = gameMap->getSeatById(seatId);
                if(seat == nullptr)
                {
                    OD_LOG_ERR("unexpected null seat id=" + Helper::toString(seatId));
                    break;
                }
                seat->setPlayer(tempPlayer);
                seat->setTeamId(teamId);
            }
            break;
        }

        case ServerNotificationType::clientRejected:
        {
            // If should be in seat configuration. If we are rejected, we regress mode
            ModeManager::ModeType modeType = frameListener->getModeManager()->getCurrentModeType();
            if(modeType != ModeManager::ModeType::MENU_CONFIGURE_SEATS)
            {
                OD_LOG_ERR("Wrong mode type=" + Helper::toString(static_cast<int>(modeType)));
                break;
            }

            MenuModeConfigureSeats* mode = static_cast<MenuModeConfigureSeats*>(frameListener->getModeManager()->getCurrentMode());
            mode->goBack();
            break;
        }

        case ServerNotificationType::startGameMode:
        {
            int seatId;
            OD_ASSERT_TRUE(packetReceived >> seatId);
            const std::vector<Player*>& players = gameMap->getPlayers();
            for(Player* player : players)
            {
                if(player->getSeat()->getId() == seatId)
                {
                    setPlayer(player);
                    gameMap->setLocalPlayer(player);
                }
            }

            ServerMode serverMode;
            OD_ASSERT_TRUE(packetReceived >> serverMode);

            // Now that the we have received all needed information, we can launch the requested mode
            OD_LOG_INF("Starting game map");
            gameMap->setGamePaused(false);
            // Create ogre entities for the tiles, rooms, and creatures
            gameMap->createAllEntities();

            switch(serverMode)
            {
                case ServerMode::ModeGameSinglePlayer:
                case ServerMode::ModeGameMultiPlayer:
                case ServerMode::ModeGameLoaded:
                    frameListener->getModeManager()->requestMode(AbstractModeManager::GAME);
                    break;
                case ServerMode::ModeEditor:
                    frameListener->getModeManager()->requestMode(AbstractModeManager::EDITOR);
                    break;
                default:
                    OD_LOG_ERR("Unknown server mode=" + Helper::toString(static_cast<int32_t>(serverMode)));
            }

            Seat* tempSeat = gameMap->getSeatById(seatId);
            if(tempSeat == nullptr)
            {
                OD_LOG_ERR("seatId=" + Helper::toString(seatId));
                return true;
            }

            // We reset the renderer
            ODFrameListener::getSingleton().initGameRenderer();

            // Move camera to starting position
            Ogre::Real startX = static_cast<Ogre::Real>(tempSeat->mStartingX);
            Ogre::Real startY = static_cast<Ogre::Real>(tempSeat->mStartingY);
            // We make the temple appear in the center of the game view
            startY = startY - 7.0f;
            // Bound check
            if (startY <= 0.0)
                startY = 0.0;

            frameListener->resetCamera(Ogre::Vector3(startX, startY, MAX_CAMERA_Z));
            // If we are watching a replay, we force stopping the processing loop to
            // allow changing mode (because there is no synchronization as there is no server)
            if(getSource() == ODSource::file)
                return false;
            break;
        }

        case ServerNotificationType::chat:
        {
            int32_t seatId;
            std::string playerNick;
            std::string chatMsg;
            OD_ASSERT_TRUE(packetReceived >> playerNick >> chatMsg >> seatId);
            Seat* seat = gameMap->getSeatById(seatId);
            ChatMessage chat(playerNick, chatMsg, seat);
            frameListener->getModeManager()->getCurrentMode()->receiveChat(chat);
            break;
        }

        case ServerNotificationType::chatServer:
        {
            std::string evtMsg;
            EventShortNoticeType type;
            OD_ASSERT_TRUE(packetReceived >> evtMsg >> type);
            addEventMessage(new EventMessage(evtMsg, type));
            break;
        }

        case ServerNotificationType::newMap:
        {
            gameMap->clearAll();
            break;
        }

        case ServerNotificationType::addClass:
        {
            CreatureDefinition *tempClass = new CreatureDefinition;
            OD_ASSERT_TRUE(packetReceived >> tempClass);
            gameMap->addClassDescription(tempClass);
            break;
        }

        case ServerNotificationType::addEntity:
        {

            NodeType nt;
            GameMap* gameMapPointer;
            packetReceived >> nt;
            if(nt == NodeType::MTILES_NODE)
                gameMapPointer = gameMap;
            else
                gameMapPointer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);

            GameEntity* entity = Entities::getGameEntityFromPacket(gameMapPointer, packetReceived);
            if(entity == nullptr)
            {
                OD_LOG_ERR("addEntity: Entity is NULL");
                break;
            }
            entity->addToGameMap(gameMapPointer);
            entity->createMesh(nt);
            entity->restoreEntityState();
            entity->setPosition( entity->getPosition(), gameMapPointer);
            
            break;
        }

        case ServerNotificationType::removeEntity:
        {
            GameEntityType entityType;
            std::string entityName;
            NodeType nt;
            OD_ASSERT_TRUE(packetReceived >> entityType >> entityName);
            packetReceived >> nt;
            GameMap* gameMapPointer;
            if(nt == NodeType::MTILES_NODE)
                gameMapPointer = gameMap;
            else
                gameMapPointer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
            
            GameEntity* entity = gameMapPointer->getEntityFromTypeAndName(entityType, entityName);
            if(entity == nullptr)
            {
                OD_LOG_ERR("entityType=" + Helper::toString(static_cast<int32_t>(entityType)) + ", entityName=" + entityName);
                break;
            }

            entity->removeEntityFromPositionTile(gameMapPointer);
            entity->removeFromGameMap(gameMapPointer);
            entity->deleteYourself(gameMapPointer,nt);
            break;
        }

        case ServerNotificationType::turnStarted:
        {
            int64_t turnNum;
            OD_ASSERT_TRUE(packetReceived >> turnNum);
            OD_LOG_INF("Client (" + getPlayer()->getNick() + ") received turnStarted="
                + boost::lexical_cast<std::string>(turnNum));

            gameMap->clientUpKeep(turnNum);
            // TODO maybe we should call clientUpKeep also for draggableTileContainer
            
            // We acknowledge the new turn to the server so that he knows we are
            // ready for next one
            ODPacket packSend;
            packSend << ClientNotificationType::ackNewTurn << turnNum;
            send(packSend);

            // For the first turn, we stop processing events because we want the gamemap to
            // be initialized
            if(turnNum == 0)
                return false;

            break;
        }

        case ServerNotificationType::animatedObjectSetWalkPath:
        {
            std::string objName;
            std::string walkAnim;
            std::string endAnim;
            bool walkDistortion;
            bool loopEndAnim;
            bool playIdleWhenAnimationEnds;
            uint32_t nbDest;
            OD_ASSERT_TRUE(packetReceived >> walkDistortion >> objName >> walkAnim >> endAnim);
            OD_ASSERT_TRUE(packetReceived >> loopEndAnim >> playIdleWhenAnimationEnds >> nbDest);

            MovableGameEntity *tempAnimatedObject = gameMap->getAnimatedObject(objName);
            if(tempAnimatedObject == nullptr)
            {
                OD_LOG_ERR("objName=" + objName);
                break;
            }

            std::vector<Ogre::Vector2> path;
                        
            while(nbDest > 0)
            {
                --nbDest;
                Ogre::Vector2 dest;
                OD_ASSERT_TRUE(packetReceived >> dest);
                if(walkDistortion)
                    tempAnimatedObject->correctEntityMovePosition(dest);
                path.push_back(dest);
            }
            
            tempAnimatedObject->setWalkPath(walkAnim, endAnim, loopEndAnim, playIdleWhenAnimationEnds, path,walkDistortion);
            break;
        }

        case ServerNotificationType::entityPickedUp:
        {
            int seatId;
            GameEntityType entityType;
            std::string entityName;
            OD_ASSERT_TRUE(packetReceived >> seatId >> entityType >> entityName);
            Player *tempPlayer = gameMap->getPlayerBySeatId(seatId);
            if(tempPlayer == nullptr)
            {
                OD_LOG_ERR("seatId=" + Helper::toString(seatId));
                break;
            }

            GameEntity* entity = gameMap->getEntityFromTypeAndName(entityType, entityName);
            if(entity == nullptr)
            {
                OD_LOG_ERR("entityType=" + Helper::toString(static_cast<int32_t>(entityType)) + ", entityName=" + entityName);
                break;
            }

            tempPlayer->pickUpEntity(entity);
            break;
        }

        case ServerNotificationType::entityDropped:
        {
            int seatId;
            OD_ASSERT_TRUE(packetReceived >> seatId);
            if (gameMap->getLocalPlayer()->getSeat()->getId() != seatId)
            {
                OD_LOG_ERR("seatId=" + Helper::toString(seatId));
                break;
            }

            Tile* tile = gameMap->tileFromPacket(packetReceived);
            if(tile == nullptr)
                break;

            gameMap->getLocalPlayer()->dropHand(tile);
            break;
        }
        case ServerNotificationType::entityTeleported:
        {
            std::string objName;
            Ogre::Vector3 dest;
            
            OD_ASSERT_TRUE(packetReceived >> objName >> dest);
            MovableGameEntity *obj = gameMap->getAnimatedObject(objName);
            obj->setPosition(dest);
            
            break;
        }
        case ServerNotificationType::entitySlapped:
        {
            RenderManager::getSingleton().entitySlapped();
            break;
        }

        case ServerNotificationType::setObjectAnimationState:
        {
            std::string objName;
            std::string animState;
            bool loop;
            bool playIdleWhenAnimationEnds;
            bool shouldSetWalkDirection;
            OD_ASSERT_TRUE(packetReceived >> objName >> animState
                >> loop >> playIdleWhenAnimationEnds >> shouldSetWalkDirection);
            MovableGameEntity *obj = gameMap->getAnimatedObject(objName);
            if (obj == nullptr)
            {
                OD_LOG_ERR("objName=" + objName + ", state=" + animState);
                break;
            }

            if(shouldSetWalkDirection)
            {
                Ogre::Vector3 walkDirection;
                OD_ASSERT_TRUE(packetReceived >> walkDirection);
                obj->setWalkDirection(walkDirection);
            }

            obj->setAnimationState(animState, loop, Ogre::Vector3::ZERO, playIdleWhenAnimationEnds);
            break;
        }

        case ServerNotificationType::refreshPlayerSeat:
        {
            std::string goalsString;
            OD_ASSERT_TRUE(getPlayer()->getSeat()->importFromPacketForUpdate(packetReceived));
            OD_ASSERT_TRUE(packetReceived >> goalsString);

            refreshMainUI(goalsString);
            break;
        }

        case ServerNotificationType::entitiesRefresh:
        {
            uint32_t nbEntities;
            GameEntityType entityType;
            std::string entityName;
            OD_ASSERT_TRUE(packetReceived >> nbEntities);
            while(nbEntities > 0)
            {
                --nbEntities;
                OD_ASSERT_TRUE(packetReceived >> entityType);
                OD_ASSERT_TRUE(packetReceived >> entityName);
                GameEntity* entity = gameMap->getEntityFromTypeAndName(entityType, entityName);
                if(entity == nullptr)
                {
                    OD_LOG_ERR("entityType=" + Helper::toString(static_cast<int32_t>(entityType)) + ", entityName=" + entityName);
                    break;
                }

                entity->updateFromPacket(packetReceived);
            }
            break;
        }

        case ServerNotificationType::orientEntity:
        {            
            std::string entityName;
            Ogre::Vector3 vv;

            
            OD_ASSERT_TRUE(packetReceived >> entityName);
            OD_ASSERT_TRUE(packetReceived >> vv);            

            MovableGameEntity* entity = gameMap->getRenderedMovableEntity(entityName);
            if(entity == nullptr)
            {
                OD_LOG_ERR("MovableGameEntity pointer equal to nullptr: entityName=" + entityName);
                break;
            }

            
            RenderManager::getSingleton().rrOrientEntityToward(entity,vv);
            

            break;            
        }
        
        case ServerNotificationType::playerFighting:
        {
            int32_t playerFightingId;
            OD_ASSERT_TRUE(packetReceived >> playerFightingId);
            if(getPlayer()->getId() == playerFightingId)
            {
                addEventMessage(new EventMessage("You are under attack!", EventShortNoticeType::majorGameEvent));
            }
            else
            {
                addEventMessage(new EventMessage("An ally is under attack!", EventShortNoticeType::majorGameEvent));
            }

            std::string fightMusic = gameMap->getLevelFightMusicFile();
            if (fightMusic.empty())
                break;
            MusicPlayer::getSingleton().play(fightMusic);
            break;
        }

        case ServerNotificationType::playerNoMoreFighting:
        {
            MusicPlayer::getSingleton().play(gameMap->getLevelMusicFile());
            break;
        }

        case ServerNotificationType::setEntityOpacity:
        {
            std::string entityName;
            float opacity;
            OD_ASSERT_TRUE(packetReceived >> entityName >> opacity);

            RenderedMovableEntity* entity = gameMap->getRenderedMovableEntity(entityName);
            if(entity == nullptr)
            {
                OD_LOG_ERR("entityName=" + entityName);
                break;
            }

            entity->setMeshOpacity(opacity);
            break;
        }

        case ServerNotificationType::playSpatialSound:
        {
            std::string family;
            int xPos;
            int yPos;
            OD_ASSERT_TRUE(packetReceived >> family >> xPos >> yPos);
            SoundEffectsManager::getSingleton().playSpatialSound(family, xPos, yPos);
            break;
        }

        case ServerNotificationType::playRelativeSound:
        {
            std::string family;
            OD_ASSERT_TRUE(packetReceived >> family);
            SoundEffectsManager::getSingleton().playRelativeSound(family);
            break;
        }

        case ServerNotificationType::notifyCreatureInfo:
        {
            std::string name;
            std::string infos;
            OD_ASSERT_TRUE(packetReceived >> name >> infos);
            Creature* creature = gameMap->getCreature(name);
            if(creature == nullptr)
            {
                OD_LOG_ERR("name=" + name);
                break;
            }

            creature->updateStatsWindow(infos);
            break;
        }


        case ServerNotificationType::notifyTileInfo:
        {
            int xx, yy;
            std::string infos;
            OD_ASSERT_TRUE(packetReceived >> xx >> yy >> infos);
            Tile* tile = gameMap->getTile(xx,yy);
            if(tile == nullptr)
            {
                OD_LOG_ERR("name=" + Helper::toString(xx) + " " + Helper::toString(yy));
                break;
            }

            tile->updateStatsWindow(infos);
            break;
       } 
        
        case ServerNotificationType::refreshCreatureVisDebug:
        {
            std::string name;
            bool isDebugVisibleTilesActive;
            OD_ASSERT_TRUE(packetReceived >> name >> isDebugVisibleTilesActive);
            Creature* creature = gameMap->getCreature(name);
            if(creature == nullptr)
            {
                OD_LOG_ERR("name=" + name);
                break;
            }

            if(!isDebugVisibleTilesActive)
            {
                creature->destroyVisualDebugEntities();
                break;
            }

            uint32_t nbTiles;
            OD_ASSERT_TRUE(packetReceived >> nbTiles);
            std::vector<Tile*> tiles;
            while(nbTiles > 0)
            {
                --nbTiles;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                if(tile == nullptr)
                    continue;

                tiles.push_back(tile);
            }
            creature->refreshVisualDebugEntities(tiles);
            break;
        }

        case ServerNotificationType::refreshSeatVisDebug:
        {
            int seatId;
            bool isDebugVisibleTilesActive;
            OD_ASSERT_TRUE(packetReceived >> seatId >> isDebugVisibleTilesActive);
            Seat* seat = gameMap->getSeatById(seatId);
            if(seat == nullptr)
            {
                OD_LOG_ERR("seatId=" + Helper::toString(seatId));
                break;
            }

            if(!isDebugVisibleTilesActive)
            {
                seat->stopVisualDebugEntities();
                break;
            }

            uint32_t nbTiles;
            OD_ASSERT_TRUE(packetReceived >> nbTiles);
            std::vector<Tile*> tiles;
            while(nbTiles > 0)
            {
                --nbTiles;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                if(tile == nullptr)
                    continue;

                tiles.push_back(tile);
            }
            seat->refreshVisualDebugEntities(tiles);
            break;
        }

        case ServerNotificationType::refreshVisibleTiles:
        {
            uint32_t nbTiles;
            NodeType nt;
            OD_ASSERT_TRUE(packetReceived >> nt);
            GameMap* gameMapPointer;
            if(nt == NodeType::MTILES_NODE)
                gameMapPointer = gameMap;
            else if( frameListener->getModeManager()->getCurrentModeType() == ModeManager::ModeType::EDITOR)
                gameMapPointer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer) ;
            else
                gameMapPointer = nullptr;

            if(gameMapPointer !=nullptr)
            {
                // Tiles we gained vision
                OD_ASSERT_TRUE(packetReceived >> nbTiles);
                while(nbTiles > 0)
                {
                    --nbTiles;
                    Tile* tile = gameMapPointer->tileFromPacket(packetReceived);
                    if(tile == nullptr)
                        continue;

                    tile->setLocalPlayerHasVision(true);
                    tile->refreshMesh(nt,gameMapPointer);
                }
                // Tiles we lost vision
                OD_ASSERT_TRUE(packetReceived >> nbTiles);
                while(nbTiles > 0)
                {
                    --nbTiles;
                    Tile* tile = gameMapPointer->tileFromPacket(packetReceived);
                    if(tile == nullptr)
                        continue;

                    tile->setLocalPlayerHasVision(false);
                    tile->refreshMesh(nt,gameMapPointer);
                }
            }
            break;
        }

        case ServerNotificationType::refreshTiles:
        {
            uint32_t nbTiles;
            NodeType nt;
            OD_ASSERT_TRUE(packetReceived >> nbTiles);
            OD_ASSERT_TRUE(packetReceived >> nt);
            GameMap* gameMapPointer = (nt == NodeType::MTILES_NODE) ? gameMap : (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer) ;
            std::vector<Tile*> tiles;
            while(nbTiles > 0)
            {
                --nbTiles;
                Tile* gameTile = gameMapPointer->tileFromPacket(packetReceived);
                if(gameTile == nullptr)
                    continue;

                gameTile->updateFromPacket(packetReceived);
                tiles.push_back(gameTile);
            }
            gameMapPointer->refreshBorderingTilesOf(tiles, nt);
            break;
        }
        
        case ServerNotificationType::revealTiles:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::EDITOR)
            {
                OD_LOG_ERR("Wrong mode " + Helper::toString(frameListener->getModeManager()->getCurrentModeType()));
                break;
            }
            DraggableTileContainer* draggableTileContainer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
        
            if(draggableTileContainer!=nullptr)
            {
                uint32_t nbTiles;
                OD_ASSERT_TRUE(packetReceived >> nbTiles);
                std::vector<Tile*> tiles;
                TileType tileType;
                TileVisual tileVisual;
                double tileFullness;
                int seatId;
                Seat* seatPtr;
                while(nbTiles > 0)
                {
                
                    --nbTiles;
                    Tile* gameTile = draggableTileContainer->tileFromPacket(packetReceived);
                    OD_ASSERT_TRUE( packetReceived >> tileType >> tileFullness >> seatId >> tileVisual  );
                    if(gameTile == nullptr)
                        continue;                    
                    if(seatId != -1)
                        seatPtr = gameMap->getSeatById(seatId);
                    else
                        seatPtr = nullptr;
                
                    // xx = xx - draggableTileContainer->getPosition().x;
                    // yy = yy - draggableTileContainer->getPosition().y;
                    gameTile->setType(tileType);
                    gameTile->setFullness(tileFullness);
                    gameTile->setSeat(seatPtr);
                    gameTile->setTileVisualIfArgNotNull(tileVisual);
                }
                draggableTileContainer->refreshTilesBlock(0, 0, draggableTileContainer->getMapSizeX(), draggableTileContainer->getMapSizeY() , NodeType::MDTC_NODE);
            }
            
            break;
        }
        
        case ServerNotificationType::revealTraps:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::EDITOR)
            {
                OD_LOG_ERR("Wrong mode " + Helper::toString(frameListener->getModeManager()->getCurrentModeType()));
                break;
            }
            DraggableTileContainer* draggableTileContainer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
            if(draggableTileContainer!=nullptr)
            {
                uint32_t nbTraps;
                OD_ASSERT_TRUE(packetReceived >> nbTraps);
                std::vector<Tile*> tiles;
                TrapType trapType;
                std::string name;
                int seatId;
                Seat* seatPtr;
                uint32_t xx, yy;
                uint32_t nbCoveredTiles;
                bool isActivated;
                while(nbTraps > 0)
                {             
                    --nbTraps;
                    OD_ASSERT_TRUE(packetReceived >> trapType );
                    OD_ASSERT_TRUE(packetReceived >> name );
                    OD_ASSERT_TRUE(packetReceived >> seatId );
                    OD_ASSERT_TRUE(packetReceived >> nbCoveredTiles );
                    std::vector<Tile*> affectedTiles;
                    std::vector<bool> isActivatedVector;
                    while(nbCoveredTiles > 0)
                    {
                        --nbCoveredTiles;
                        OD_ASSERT_TRUE(packetReceived >> xx );
                        OD_ASSERT_TRUE(packetReceived >> yy );                             
                        OD_ASSERT_TRUE(packetReceived >> isActivated );
                        xx = xx - draggableTileContainer->getPosition().x;
                        yy = yy - draggableTileContainer->getPosition().y;
                        affectedTiles.push_back(draggableTileContainer->getTile(xx,yy));
                        isActivatedVector.push_back(isActivated);
                    }
                    if(seatId != -1)
                        seatPtr = gameMap->getSeatById(seatId);
                    else
                        seatPtr = nullptr;
                    OD_ASSERT_TRUE(seatPtr != nullptr);
                    TrapManager::buildTrapOnTiles(draggableTileContainer, trapType, seatPtr, affectedTiles , true);
                    // deactivate traps which 'mirror' trap was also deactivated 
                    for(long unsigned int ii = 0 ; ii < affectedTiles.size() ; ++ii)
                        if(!isActivatedVector[ii])
                            affectedTiles[ii]->getCoveringTrap()->deactivate(affectedTiles[ii]);
                }                                
            }
            break;
        }
        case ServerNotificationType::pingCreateAllEntities:
        {
            DraggableTileContainer* draggableTileContainer = (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
            draggableTileContainer->createAllEntities();
            break;
        }
        case ServerNotificationType::pingCreateDraggableTileContainer:
        {
            int sizeX, sizeY, posX, posY, TileMarkerMinX, TileMarkerMinY, TileMarkerMaxX,TileMarkerMaxY;
            OD_ASSERT_TRUE(packetReceived >> sizeX );
            OD_ASSERT_TRUE(packetReceived >> sizeY );
            OD_ASSERT_TRUE(packetReceived >> posX );
            OD_ASSERT_TRUE(packetReceived >> posY );
            OD_ASSERT_TRUE(packetReceived >> TileMarkerMinX);
            OD_ASSERT_TRUE(packetReceived >> TileMarkerMinY);
            OD_ASSERT_TRUE(packetReceived >> TileMarkerMaxX);
            OD_ASSERT_TRUE(packetReceived >> TileMarkerMaxY);
                        
                        


            
            DraggableTileContainer*& draggableTileContainer =  (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
            if(draggableTileContainer==nullptr)
            {
                // we create another Gamemap which will be visible and hover over the orignal
                draggableTileContainer = new DraggableTileContainer(false);

                draggableTileContainer->allocateMapMemory(sizeX,sizeY);

                draggableTileContainer->setTileSetName("");
                for (int jj = 0; jj < draggableTileContainer->getMapSizeY(); ++jj)
                {
                    for (int ii = 0; ii < draggableTileContainer->getMapSizeX(); ++ii)
                    {
                        Tile* tile = new Tile(draggableTileContainer, ii, jj);
                        tile->setParentSceneNode( dynamic_cast<Ogre::SceneNode*>(RenderManager::getSingletonPtr()->getSceneManager()->getRootSceneNode()->getChild("Draggable_scene_node")));
                        std::stringstream tileName("");
                        tileName << Tile::TILE_PREFIX << tile->getX() << "_" << tile->getY();
                        tile->setName(tileName.str());
                        // Maybe we should also setup the tile's position, however this
                        // doesn't have a smallest influence on how draggableTileContainer work
                        draggableTileContainer->addTile(tile);
                    }
                }

                draggableTileContainer->setPosition(Ogre::Vector2(posX,posY));
                for(Seat* ss : gameMap->getSeats())
                    draggableTileContainer->addSeat(ss);
                
                //TODO: make this hack less ugly -- allow somehow on draggableTileContainer show up,
                // despite the enabled culling ....
                
                dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->mMainCullingManager->showAllTiles(draggableTileContainer) ;
                
                // query server what is the content of tiles we hover over in orginal map
                // only server side has the relevant info 
                ClientNotification* notif2 = new ClientNotification(ClientNotificationType::editorAskRevealTiles);
                notif2->mPacket << static_cast<unsigned int> ( TileMarkerMinX)
                                << static_cast<unsigned int> ( TileMarkerMinY);

                ODClient::getSingleton().queueClientNotification(notif2);

                // the same way we query for traps in the orginal map
                // again, only server side has relevant info
                ClientNotification* notif3 = new ClientNotification(ClientNotificationType::editorAskRevealTraps);
                notif3->mPacket << static_cast<unsigned int> ( TileMarkerMinX)
                                << static_cast<unsigned int> ( TileMarkerMinY)
                                << static_cast<unsigned int> ( TileMarkerMaxX)
                                << static_cast<unsigned int> ( TileMarkerMaxY);
                ODClient::getSingleton().queueClientNotification(notif3);
        
                ClientNotification* notif4 = new ClientNotification(ClientNotificationType::editorAskRevealRooms);
                notif4->mPacket << static_cast<unsigned int> ( TileMarkerMinX)
                                << static_cast<unsigned int> ( TileMarkerMinY)
                                << static_cast<unsigned int> ( TileMarkerMaxX)
                                << static_cast<unsigned int> ( TileMarkerMaxY);
                ODClient::getSingleton().queueClientNotification(notif4);

                ClientNotification* notif5 = new ClientNotification(ClientNotificationType::createAllEntities);
                ODClient::getSingleton().queueClientNotification(notif5);  
                
                
            }
            break;
        }

        case ServerNotificationType::pingDeleteDraggableTileContainer:
        {
            DraggableTileContainer*& draggableTileContainer =  (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);
            if(draggableTileContainer!=nullptr)
            {
                delete draggableTileContainer;
                draggableTileContainer=nullptr;
            }            
            break;
        }

        
        case ServerNotificationType::pingEditorAskSetRoundedPositionDraggableTileContainer:
        {
            Ogre::Real posX, posY, offsetX, offsetY;
            OD_ASSERT_TRUE(packetReceived >> posX );
            OD_ASSERT_TRUE(packetReceived >> posY );
            OD_ASSERT_TRUE(packetReceived >> offsetX );
            OD_ASSERT_TRUE(packetReceived >> offsetY );
            DraggableTileContainer*& draggableTileContainer =  (dynamic_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode())->draggableTileContainer);            
            draggableTileContainer->setRoundedPosition(Ogre::Vector2(posX, posY), Ogre::Vector2(offsetX, offsetY));

            break;
        }
        case ServerNotificationType::markTiles:
        {
            bool digSet;
            uint32_t nbTiles;
            OD_ASSERT_TRUE(packetReceived >> digSet >> nbTiles);

            SoundEffectsManager::getSingleton().playRelativeSound(SoundRelativeInterface::PickSelector);

            Player* player = getPlayer();
            while(nbTiles > 0)
            {
                --nbTiles;
                Tile* tile = gameMap->tileFromPacket(packetReceived);
                if(tile == nullptr)
                    continue;

                tile->setMarkedForDigging(digSet, player);
                tile->refreshMesh();
            }
            break;
        }

        case ServerNotificationType::carryEntity:
        {
            std::string carrierName;
            GameEntityType entityType;
            std::string carriedName;
            OD_ASSERT_TRUE(packetReceived >> carrierName >> entityType >> carriedName);
            Creature* carrier = gameMap->getCreature(carrierName);
            if(carrier == nullptr)
            {
                OD_LOG_ERR("carrierName=" + carrierName);
                break;
            }

            GameEntity* carried = gameMap->getEntityFromTypeAndName(entityType, carriedName);
            if(carried == nullptr)
            {
                OD_LOG_ERR("entityType=" + Helper::toString(static_cast<int32_t>(entityType)) + ", carriedName=" + carriedName);
                break;
            }

            carried->removeEntityFromPositionTile();

            RenderManager::getSingleton().rrCarryEntity(carrier, carried);
            break;
        }

        case ServerNotificationType::releaseCarriedEntity:
        {
            std::string carrierName;
            GameEntityType entityType;
            std::string carriedName;
            Ogre::Vector3 pos;
            OD_ASSERT_TRUE(packetReceived >> carrierName >> entityType >> carriedName >> pos);
            Creature* carrier = gameMap->getCreature(carrierName);
            if(carrier == nullptr)
            {
                OD_LOG_ERR("carrierName=" + carrierName);
                break;
            }

            GameEntity* carried = gameMap->getEntityFromTypeAndName(entityType, carriedName);
            if(carried == nullptr)
            {
                OD_LOG_ERR("entityType=" + Helper::toString(static_cast<int32_t>(entityType)) + ", carriedName=" + carriedName);
                break;
            }

            RenderManager::getSingleton().rrReleaseCarriedEntity(carrier, carried);
            carried->setPosition(pos);
            break;
        }

        case ServerNotificationType::skillTree:
        {
            uint32_t nbItems;
            OD_ASSERT_TRUE(packetReceived >> nbItems);
            std::vector<SkillType> skills;
            while(nbItems > 0)
            {
                nbItems--;
                SkillType skill;
                OD_ASSERT_TRUE(packetReceived >> skill);
                skills.push_back(skill);
            }

            getPlayer()->getSeat()->setSkillTree(skills);
            break;
        }

        case ServerNotificationType::skillsDone:
        {
            uint32_t nbItems;
            OD_ASSERT_TRUE(packetReceived >> nbItems);
            std::vector<SkillType> skills;
            while(nbItems > 0)
            {
                nbItems--;
                SkillType skill;
                OD_ASSERT_TRUE(packetReceived >> skill);
                skills.push_back(skill);
            }

            getPlayer()->getSeat()->setSkillsDone(skills);
            break;
        }

        case ServerNotificationType::playerEvents:
        {
            uint32_t nbItems;
            OD_ASSERT_TRUE(packetReceived >> nbItems);
            std::vector<PlayerEvent*> events;
            while(nbItems > 0)
            {
                nbItems--;
                PlayerEvent* event = PlayerEvent::getPlayerEventFromPacket(gameMap, packetReceived);
                if(event == nullptr)
                {
                    OD_LOG_ERR("Error while reading PlayerEvent from packet");
                    continue;
                }
                events.push_back(event);
            }

            getPlayer()->updateEvents(events);
            break;
        }
        case ServerNotificationType::setSpellCooldown:
        {
            SpellType spellType;
            uint32_t cooldown;
            OD_ASSERT_TRUE(packetReceived >> spellType >> cooldown);
            getPlayer()->setSpellCooldownTurns(spellType, cooldown);
            break;
        }
        case ServerNotificationType::setPlayerSettings:
        {
            bool koCreatures;
            OD_ASSERT_TRUE(packetReceived >> koCreatures);
            getPlayer()->getSeat()->setPlayerSettings(koCreatures);
            break;
        }
        case ServerNotificationType::displayText:
        {
            if(frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::EDITOR
                && frameListener->getModeManager()->getCurrentModeType() != ModeManager::ModeType::GAME)
            {
                OD_LOG_ERR("Wrong mode " + Helper::toString(frameListener->getModeManager()->getCurrentModeType()));
                break;
            }
            std::string text;
            OD_ASSERT_TRUE(packetReceived >> text);            
            if(frameListener->getModeManager()->getCurrentModeType() == ModeManager::ModeType::GAME )
            {
                GameMode* gm = static_cast<GameMode*>(frameListener->getModeManager()->getCurrentMode());
                gm->displayText(Ogre::ColourValue::Red,text);
            }
            else if(frameListener->getModeManager()->getCurrentModeType() == ModeManager::ModeType::EDITOR)
            {
                EditorMode* em = static_cast<EditorMode*>(frameListener->getModeManager()->getCurrentMode());
                em->displayText(Ogre::ColourValue::Red,text);

            }
 
            break;

        }

        default:
        {
            OD_LOG_ERR("Unknown server command:"
                + Helper::toString(static_cast<int>(cmd)));
            break;
        }
    }

    return true;
}

void ODClient::processClientNotifications()
{
    while (isConnected())
    {
        // Wait until a message is place in the queue

        // Test whether there is something left to deal with
        if (mClientNotificationQueue.empty())
            break;

        ClientNotification* event = mClientNotificationQueue.front();
        mClientNotificationQueue.pop_front();

        if (!event)
            continue;

        switch (event->mType)
        {
            // If there are specific things to do before sending, it can be done here
            default:
                send(event->mPacket);
                break;
        }
        delete event;
    }
}

void ODClient::addEventMessage(EventMessage* event)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    switch(frameListener->getModeManager()->getCurrentModeType())
    {
        case AbstractModeManager::GAME:
        case AbstractModeManager::EDITOR:
        {
            GameEditorModeBase* gm = static_cast<GameEditorModeBase*>(frameListener->getModeManager()->getCurrentMode());
            gm->receiveEventShortNotice(event);
            break;
        }
        // Note: Later, we can handle other modes here.
        default:
            delete event;
            break;
    }
}

void ODClient::refreshMainUI(const std::string& goalsString)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if (frameListener->getModeManager()->getCurrentModeType() == AbstractModeManager::GAME)
    {
        GameMode* gm = static_cast<GameMode*>(frameListener->getModeManager()->getCurrentMode());
        gm->refreshPlayerGoals(goalsString);
        gm->refreshMainUI();
    }
    // Note: Later, we can handle other modes here if necessary.
}

bool ODClient::connect(const std::string& host, const int port, uint32_t timeout, const std::string& outputReplayFilename)
{
    mIsPlayerConfig = false;
    // Start the server socket listener as well as the server socket thread
    if (ODClient::getSingleton().isConnected())
    {
        OD_LOG_INF("Couldn't try to connect: The client is already connected");
        return false;
    }

    if(!ODSocketClient::connect(host, port, timeout, outputReplayFilename))
        return false;

    // Send a hello request to start the conversation with the server
    ODPacket packSend;
    packSend << ClientNotificationType::hello
        << std::string("OpenDungeons V ") + ODApplication::VERSION;
    send(packSend);

    return true;
}

bool ODClient::replay(const std::string& filename)
{
    mIsPlayerConfig = false;
    // Start the server socket listener as well as the server socket thread
    if (ODClient::getSingleton().isConnected())
    {
        OD_LOG_INF("Couldn't try to launch replay: The client is already connected");
        return false;
    }

    if(!ODSocketClient::replay(filename))
        return false;

    return true;
}

void ODClient::queueClientNotification(ClientNotification* n)
{
    mClientNotificationQueue.push_back(n);
}

void ODClient::disconnect(bool keepReplay)
{
    ODSocketClient::disconnect(keepReplay);
    while(!mClientNotificationQueue.empty())
    {
        delete mClientNotificationQueue.front();
        mClientNotificationQueue.pop_front();
    }

    mIsPlayerConfig = false;
}

void ODClient::notifyExit()
{
    disconnect();
}
