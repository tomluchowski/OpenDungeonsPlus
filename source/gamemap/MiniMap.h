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

#ifndef MINIMAP_H
#define MINIMAP_H

#include <OgrePrerequisites.h>
#include <OgreColourValue.h>

namespace CEGUI
{
class BasicImage;
class Window;
}

class Tile;
class Seat;

class MiniMap
{
public:
    virtual ~MiniMap()
    {}

    virtual Ogre::Vector2 camera_2dPositionFromClick(int xx, int yy) = 0;

    //! \brief This function will be called each frame. cornerTiles corresponds to
    //! the corner tiles currently displayed in the main map
    virtual void update(Ogre::Real timeSinceLastFrame, const std::vector<Ogre::Vector3>& cornerTiles) = 0;

    static const std::string& DEFAULT_MINIMAP;

    //! \brief This function will create the minimap according to user preferences
    static MiniMap* createMiniMap(CEGUI::Window* miniMapWindow);
    static CEGUI::BasicImage& createMiniMapImage(CEGUI::Window* miniMapWindow,
        const std::string& name = "MiniMapImageset");

    void setZoomLevel(int level);
    int getZoomLevel() const { return mZoomLevel; }
    Ogre::Real getZoomScale() const;

    // Returns the list of all possible minimap types
    static const std::vector<std::string>& getMiniMapTypes();
protected:
    struct TileColour
    {
        Ogre::ColourValue colour = Ogre::ColourValue::Black;
        unsigned int priority = 0;
        bool animated = false;
    };
    static TileColour colourFromTile(Tile& tile, Seat& playerSeat, unsigned int phase);
    Ogre::Real mAnimationTime = 0.0f;
private:
    int mZoomLevel = 0;
};

#endif // MINIMAP_H
