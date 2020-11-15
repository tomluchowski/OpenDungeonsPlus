#ifndef TILEMARKER_H
#define TILEMARKER_H



#include <OgreAxisAlignedBox.h>

class TileMarker
{


    Ogre::AxisAlignedBox aab;
    void normalize()
    {
        aab.setMinimumX(getMinX() - 0.5);
        aab.setMinimumY(getMinY() - 0.5);        
        aab.setMaximumX(getMaxX() + 0.5);
        aab.setMaximumY(getMaxY() + 0.5); 
    }
    
public:
    Ogre::Vector2 mark;
    Ogre::Vector2 point;
    bool isVisible;
    bool isExtensible;
    Ogre::AxisAlignedBox getAABB(){
        return aab;
    }
    void setMark( Ogre::Vector2 vv){ point = vv; normalize(); }
    void setPoint( Ogre::Vector2 vv){ mark = vv; normalize();}
    
    Ogre::Real getMinX(){return std::min(point.x,mark.x);}
    Ogre::Real getMaxX(){return std::max(point.x,mark.x);}
    Ogre::Real getMinY(){return std::min(point.y,mark.y);}
    Ogre::Real getMaxY(){return std::max(point.y,mark.y);}
    
    static constexpr Ogre::Real TILE_MARKER_HIGHT = 2.0;
    TileMarker():aab(Ogre::Vector3(-0.5,-0.5,0),Ogre::Vector3(0.5,0.5,TILE_MARKER_HIGHT)),mark(Ogre::Vector2::ZERO),point(Ogre::Vector2::ZERO),isVisible(false),isExtensible(false){};
};

#endif // TILEMARKER_H
