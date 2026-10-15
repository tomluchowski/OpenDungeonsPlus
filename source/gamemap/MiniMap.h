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
class GameMap;

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
    //! \brief Creates the image the minimap texture is shown with. The image clips the map to a
    //! circle if the window asks for it and draws the camera area outline when showViewport is true
    //! or the map is circular.
    static CEGUI::BasicImage& createMiniMapImage(CEGUI::Window* miniMapWindow,
        const std::string& name = "MiniMapImageset", bool showViewport = false);

    //! \brief Sets the zoom level, limited to the supported range. Positive values zoom in.
    void setZoomLevel(int level);
    int getZoomLevel() const { return mZoomLevel; }
    //! \brief The part of the default map area that is shown at the current zoom level
    //! (1 at level 0, smaller when zoomed in).
    Ogre::Real getZoomScale() const;

    // Returns the list of all possible minimap types
    static const std::vector<std::string>& getMiniMapTypes();
protected:
    //! \brief The colour a tile is drawn with on the map.
    struct TileColour
    {
        Ogre::ColourValue colour = Ogre::ColourValue::Black;
        //! Tiles with a higher priority win when several tiles share one map pixel.
        unsigned int priority = 0;
        //! True if the colour changes with the animation phase.
        bool animated = false;
    };
    //! \brief Computes the colour of a tile as seen by the owner of playerSeat. The phase is
    //! the current animation phase, see getAnimationPhase().
    static TileColour colourFromTile(Tile& tile, Seat& playerSeat, unsigned int phase);
    //! \brief Updates the camera area outline and the direction towards the dungeon heart
    //! drawn above the minimap image of the given window.
    //! \param centre The world position shown in the middle of the image.
    //! \param span The world size shown by the image.
    //! \param rotation The rotation of the image in radians.
    void updateMapOverlay(CEGUI::Window* window, GameMap& map,
        const Ogre::Vector2& centre, const Ogre::Vector2& span, Ogre::Real rotation, const std::vector<Ogre::Vector3>& cornerTiles);
    //! \brief Advances the colour animation by the given time in seconds.
    void advanceAnimation(Ogre::Real elapsed);
    //! \brief The current step of the colour animation.
    unsigned int getAnimationPhase() const;
    //! \brief Time in seconds within the current animation cycle.
    Ogre::Real mAnimationTime = 0.0f;
private:
    int mZoomLevel = 0;
};

#endif // MINIMAP_H
