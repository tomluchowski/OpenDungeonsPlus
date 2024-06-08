/*!
 * \file   GameEntity.h
 * \date:  16 September 2011
 * \author StefanP.MUC
 * \brief  Provides the GameEntity class, the base class for all ingame objects
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

#ifndef ROCKLAVA_H
#define ROCKLAVA_H

#include "entities/RenderedMovableEntity.h"

class RockLava : public RenderedMovableEntity{
    friend class TileLava;
public:
 
    RockLava(GameMap*);
    ~RockLava() = default ;
    void update(Ogre::Real) override;
    virtual void restoreEntityState() override ;
    virtual void exportToPacket(ODPacket& os, const Seat* seat) const override;
    virtual void importFromPacket(ODPacket& is) override;
    virtual void setPosition(const Ogre::Vector3& v, GameMap *gameMap = nullptr ) override;
    void restartPosition();
    GameEntityType getObjectType() const override;
    // virtual void fireAddEntity(Seat* seat, bool async, NodeType nt) override;
    static RockLava* getLavaRockEntityFromPacket(GameMap* gameMap, ODPacket& is) ;
    inline void setInitialX(int initialX){ mInitialX = initialX;}
    inline void setInitialY(int initialY){ mInitialY = initialY;}
    
    virtual void clientUpkeep() override;
   
private:
    int mInitialX, mInitialY;
    bool dormant;
    Ogre::Vector3 velocity ;
};








#endif //ROCKLAVA_H
