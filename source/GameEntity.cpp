/*!
 * \file   GameEntity.cpp
 * \date:  28 March 2012
 * \author StefanP.MUC
 * \brief  Provides the GameEntity class, the base class for all ingame objects
 */

#include "Creature.h"
#include "GameMap.h"
#include "RenderManager.h"
#include "RenderRequest.h"
#include "Room.h"
#include "RoomObject.h"
#include "Weapon.h"

#include "GameEntity.h"

void GameEntity::destroyMesh()
{
    if(!meshExists)
    {
        return;
    }

    meshExists = false;

    RenderRequest* request = new RenderRequest;

    switch(objectType)
    {
        case creature:
        {
            Creature* tempCreature = static_cast<Creature*>(this);
            tempCreature->destroyStatsWindow();
            tempCreature->getWeaponL()->destroyMesh();
            tempCreature->getWeaponR()->destroyMesh();
            request->type = RenderRequest::destroyCreature;
            break;
        }

        case room:
        {
            Room* tempRoom = static_cast<Room*>(this);
            tempRoom->destroyRoomObjectMeshes();

            for (unsigned int i = 0; i < getCoveredTiles().size(); ++i)
            {
                RenderRequest* r = new RenderRequest;
                r->type = RenderRequest::destroyRoom;
                r->p    = static_cast<void*>(this);
                r->p2   = getCoveredTiles()[i];

                RenderManager::queueRenderRequest(r);
            }

            //delete original request and return because rooms delete evert tile separately
            delete request;
            return;
        }

        case roomobject:
        {
            Room* parent = static_cast<RoomObject*>(this)->getParentRoom();

            request->type   = RenderRequest::destroyRoomObject;
            request->p2     = parent;

            parent->getGameMap()->removeAnimatedObject(static_cast<MovableGameEntity*>(this));
            break;
        }

        case missileobject:
            request->type = RenderRequest::destroyMissileObject;
            break;

        case trap:
        {
            for (unsigned int i = 0; i < getCoveredTiles().size(); ++i)
            {
                RenderRequest *r = new RenderRequest;
                r->type = RenderRequest::destroyTrap;
                r->p    = static_cast<void*>(this);
                r->p2   = getCoveredTiles()[i];

                // Add the request to the queue of rendering operations to be performed before the next frame.
                RenderManager::queueRenderRequest(r);
            }

            //delete original request and return because traps delete every tile separately
            delete request;
            return;
        }

        case tile:
            request->type = RenderRequest::destroyTile;
            break;

        case weapon:
            request->type   = RenderRequest::destroyWeapon;
            request->p2     = static_cast<Weapon*>(this)->getParentCreature();
            break;

        default:
            request->type = RenderRequest::noRequest;
            break;
    }

    request->p = static_cast<void*>(this);

    RenderManager::queueRenderRequest(request);
}

void GameEntity::deleteYourself()
{
    RenderRequest* request = new RenderRequest;

    if(meshExists)
    {
        destroyMesh();
    }

    switch(objectType)
    {
        case creature:
        {
            Creature* tempCreature = static_cast<Creature*>(this);
            tempCreature->getWeaponL()->deleteYourself();
            tempCreature->getWeaponR()->deleteYourself();

            // If standing on a valid tile, notify that tile we are no longer there.
            if (tempCreature->positionTile() != 0)
                tempCreature->positionTile()->removeCreature(tempCreature);

            request->type = RenderRequest::deleteCreature;

            break;
        }

        case room:
            request->type = RenderRequest::deleteRoom;
            break;

        case roomobject:
            request->type = RenderRequest::deleteRoomObject;
            break;

        case missileobject:
            request->type = RenderRequest::deleteMissileObject;
            break;

        case trap:
            request->type = RenderRequest::deleteTrap;
            break;

        case tile:
        {
            request->type = RenderRequest::destroyTile;

            RenderRequest* request2 = new RenderRequest;
            request2->type = RenderRequest::deleteTile;
            request2->p = static_cast<void*>(this);
            RenderManager::queueRenderRequest(request2);

            break;
        }

        case weapon:
            request->type = RenderRequest::deleteWeapon;
            break;

        default:
            request->type = RenderRequest::noRequest;
            break;
    }

    request->p = static_cast<void*>(this);

    RenderManager::queueRenderRequest(request);
}