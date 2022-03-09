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

#ifndef DRAGGABLETILECONTAINER_H
#define DRAGGABLETILECONTAINER_H

#include "GameMap.h"
#include "TileMarker.h"


class ODClient;
class ODServer;

class DraggableTileContainer : public GameMap
{
    friend class ODClient;
    friend class ODServer;
    
public:
    DraggableTileContainer(bool isServerGameMap):GameMap(isServerGameMap, NodeType::MDTC_NODE){}
    virtual Ogre::Vector2 getPosition() const override;
    Ogre::AxisAlignedBox getAABB();
    void moveDelta(Ogre::Vector2 delta);
    void setRoundedPosition(Ogre::Vector2 position, Ogre::Vector2 offset );    
    Ogre::SceneNode* getParentSceneNode() const;
    TileMarker mTileMarker;
    void clearVisionForGameMap();
    void clearAll();
    virtual ~DraggableTileContainer();
private:


    void setPosition(Ogre::Vector2 position);
    void setPosition(Ogre::Vector2 position, Ogre::Vector2 offset); 
    Ogre::Vector2 mPosition;
};

#endif // DRAGGABLETILECONTAINER_H


