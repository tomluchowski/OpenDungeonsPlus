/*!
 * \file   DraggableTileContainer.cpp
 * \brief  The object which would hover over the orginal map in editor mode 
 *
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

#ifdef _MSC_VER
#define snprintf_is_banned_in_OD_code _snprintf
#endif


#include "gamemap/DraggableTileContainer.h"

#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "gamemap/TileMarker.h"
#include "render/RenderManager.h"
#include "utils/LogManager.h"
#include "game/Seat.h"

#include <OgreVector.h>

void DraggableTileContainer::moveDelta(Ogre::Vector2 delta)
{
    if(!mIsServerGameMap)
    {   
        Ogre::Vector3 vv = getParentSceneNode()->getPosition();
        vv.x += delta.x;
        vv.y += delta.y;
        getParentSceneNode()->setPosition(vv);
    }
    else
    {
        mPosition += delta;
    }
}

void DraggableTileContainer::setPosition(Ogre::Vector2 position)
{
    if(!mIsServerGameMap)
    {    
        getParentSceneNode()->setPosition(position.x, position.y, getParentSceneNode()->getPosition().z);
    }
    else
    {
        mPosition = position;
    }
}

void DraggableTileContainer::setPosition(Ogre::Vector2 position, Ogre::Vector2 offset )
{
    if(!mIsServerGameMap)
    { 
        getParentSceneNode()->setPosition(position.x + offset.x, position.y + offset.y, getParentSceneNode()->getPosition().z);
    }
    else
    {
        mPosition = position + offset;
    }
}

void DraggableTileContainer::setRoundedPosition(Ogre::Vector2 position, Ogre::Vector2 offset )
{
    if(!mIsServerGameMap)
    {    
        getParentSceneNode()->setPosition(std::round(position.x + offset.x), std::round( position.y + offset.y),  getParentSceneNode()->getPosition().z);
    }
    else
    {
        mPosition = Ogre::Vector2(std::round(position.x + offset.x), std::round( position.y + offset.y));
    }
}

Ogre::Vector2 DraggableTileContainer::getPosition() const
{
    if(!mIsServerGameMap)
    {
        Ogre::Vector3 vv = getParentSceneNode()->getPosition();
        return Ogre::Vector2( vv.x, vv.y);
    }
    else
        return mPosition;

}

Ogre::SceneNode* DraggableTileContainer::getParentSceneNode() const
{
    return mTiles[0][0]->getParentSceneNode();

}

Ogre::AxisAlignedBox DraggableTileContainer::getAABB()
{
    Ogre::Real draggableSceneNodePosX = RenderManager::getSingletonPtr()->getSceneManager()->getRootSceneNode()->getChild("Draggable_scene_node")->getPosition().x;
    Ogre::Real draggableSceneNodePosY = RenderManager::getSingletonPtr()->getSceneManager()->getRootSceneNode()->getChild("Draggable_scene_node")->getPosition().y;    
    
    Ogre::Real draggableSceneNodeHight =  RenderManager::getSingletonPtr()->getSceneManager()->getRootSceneNode()->getChild("Draggable_scene_node")->getPosition().z; 
    Ogre::AxisAlignedBox aabb(Ogre::Vector3(draggableSceneNodePosX -0.5, draggableSceneNodePosY -0.5,draggableSceneNodeHight) , Ogre::Vector3(draggableSceneNodePosX + getMapSizeX()+0.5, draggableSceneNodePosY +  getMapSizeY() + 0.5, TileMarker::TILE_MARKER_HIGHT + draggableSceneNodeHight));

    

    return aabb;
}

void DraggableTileContainer::clearVisionForGameMap()
{

    for(Seat* ss : mSeats)
        ss->clearVisionForGameMap(this);

}


void DraggableTileContainer::clearAll()
{
    clearVisionForGameMap();
    clearTraps(NodeType::MDTC_NODE);
    clearRooms(NodeType::MDTC_NODE);

    // NOTE : clearRenderedMovableEntities should be called after clearRooms because clearRooms will try to remove the objects from the room
    clearRenderedMovableEntities(NodeType::MDTC_NODE);

    processDeletionQueues();

    clearTiles(NodeType::MDTC_NODE);
    processDeletionQueues();


    // We check if the different vectors are empty
    if(!mActiveObjects.empty())
    {
        OD_LOG_ERR("mActiveObjects not empty size=" + Helper::toString(static_cast<uint32_t>(mActiveObjects.size())));
        for(GameEntity* entity : mActiveObjects)
        {
            OD_LOG_ERR("entity not removed=" + entity->getName());
        }
        mActiveObjects.clear();
    }
    if(!mAnimatedObjects.empty())
    {
        OD_LOG_ERR("mAnimatedObjects not empty size=" + Helper::toString(static_cast<uint32_t>(mAnimatedObjects.size())));
        for(GameEntity* entity : mAnimatedObjects)
        {
            OD_LOG_ERR("entity not removed=" + entity->getName());
        }
        mAnimatedObjects.clear();
    }
    if(!mEntitiesToDelete.empty())
    {
        OD_LOG_ERR("mEntitiesToDelete not empty size=" + Helper::toString(static_cast<uint32_t>(mEntitiesToDelete.size())));
        for(GameEntity* entity : mEntitiesToDelete)
        {
            OD_LOG_ERR("entity not removed=" + entity->getName());
        }
        mEntitiesToDelete.clear();
    }
    if(!mGameEntityClientUpkeep.empty())
    {
        OD_LOG_ERR("mGameEntityClientUpkeep not empty size=" + Helper::toString(static_cast<uint32_t>(mGameEntityClientUpkeep.size())));
        for(GameEntity* entity : mGameEntityClientUpkeep)
        {
            OD_LOG_ERR("entity not removed=" + entity->getName());
        }
        mGameEntityClientUpkeep.clear();
    }
}

DraggableTileContainer::~DraggableTileContainer()
{
    clearAll();
}
