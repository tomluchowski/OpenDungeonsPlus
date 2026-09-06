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

#include "gamemap/MiniMapDrawnFull.h"

#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "render/ODFrameListener.h"

#include <cmath>

#include <OgrePrerequisites.h>
#include <OgreSceneNode.h>
#include <OgreTextureManager.h>

#include <CEGUI/BasicImage.h>
#include <CEGUI/Image.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/PropertyHelper.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/Size.h>
#include <CEGUI/System.h>
#include <CEGUI/Texture.h>
#include <CEGUI/Window.h>
#include <CEGUI/WindowManager.h>

class MiniMapDrawnFullTileStateListener : public TileStateListener
{
public:
    MiniMapDrawnFullTileStateListener(MiniMapDrawnFull& minimap,
            uint32_t minimapXMin, uint32_t minimapXMax, uint32_t minimapYMin,
            uint32_t minimapYMax, uint32_t tileXMin, uint32_t tileXMax,
            uint32_t tileYMin, uint32_t tileYMax) :
        mMinimapXMin(minimapXMin),
        mMinimapXMax(minimapXMax),
        mMinimapYMin(minimapYMin),
        mMinimapYMax(minimapYMax),
        mTileXMin(tileXMin),
        mTileXMax(tileXMax),
        mTileYMin(tileYMin),
        mTileYMax(tileYMax),
        mMinimap(minimap)
    {}

    virtual ~MiniMapDrawnFullTileStateListener()
    {}

    void tileStateChanged(Tile& tile) override
    {
        fireTileStateChanged();
    }

    void fireTileStateChanged()
    {
        mAnimated = mMinimap.updateTileState(mMinimapXMin, mMinimapXMax, mMinimapYMin,
            mMinimapYMax, mTileXMin, mTileXMax, mTileYMin, mTileYMax);
    }

    bool mAnimated = false;
    const uint32_t mMinimapXMin;
    const uint32_t mMinimapXMax;
    const uint32_t mMinimapYMin;
    const uint32_t mMinimapYMax;
    const uint32_t mTileXMin;
    const uint32_t mTileXMax;
    const uint32_t mTileYMin;
    const uint32_t mTileYMax;

private:
    MiniMapDrawnFull& mMinimap;
};

namespace
{
void fillPixelRegion(const Ogre::ColourValue& colour, Ogre::PixelBox& output,
        uint32_t xMin, uint32_t xMax, uint32_t yMin, uint32_t yMax)
{
    assert(xMax <= output.getWidth());
    assert(yMax <= output.getHeight());
    for(uint32_t x = xMin; x < xMax; ++x)
        for(uint32_t y = yMin; y < yMax; ++y)
            output.setColourAt(colour, x, output.getHeight() - 1 - y, 0);
}
}

MiniMapDrawnFull::MiniMapDrawnFull(CEGUI::Window* miniMapWindow, const std::string& suffix) :
    mMiniMapWindow(miniMapWindow),
    mResourceSuffix(suffix),
    mGameMap(*ODFrameListener::getSingleton().getClientGameMap()),
    mCameraManager(*ODFrameListener::getSingleton().getCameraManager()),
    mTopLeftCornerX(0),
    mTopLeftCornerY(0),
    mWidth(static_cast<unsigned int>(mMiniMapWindow->getPixelSize().d_width)),
    mHeight(static_cast<unsigned int>(mMiniMapWindow->getPixelSize().d_height)),
    mPixels(mWidth * mHeight * 3, 0),
    mPixelBox(mWidth, mHeight, 1, Ogre::PF_R8G8B8, mPixels.data()),
    mMiniMapOgreTexture(Ogre::TextureManager::getSingletonPtr()->createManual(
            "miniMapOgreTexture" + suffix,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D,
            mWidth, mHeight, 0, Ogre::PF_R8G8B8,
            Ogre::TU_DYNAMIC_WRITE_ONLY)),
    mPixelBuffer(mMiniMapOgreTexture->getBuffer())
{
    uint32_t tileXMax = mGameMap.getMapSizeX();
    uint32_t tileYMax = mGameMap.getMapSizeY();
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    uint32_t mapX = 0;
    uint32_t mapY = 0;

    Ogre::Real gainX = static_cast<Ogre::Real>(mWidth) / static_cast<Ogre::Real>(tileXMax);
    Ogre::Real gainY = static_cast<Ogre::Real>(mHeight) / static_cast<Ogre::Real>(tileYMax);

    while((mapX < mWidth) && (mapY < mHeight) && (tileX < tileXMax) && (tileY < tileYMax))
    {
        uint32_t tileXNext = static_cast<uint32_t>(round(static_cast<Ogre::Real>(mapX + 1) / gainX));
        uint32_t tileYNext = static_cast<uint32_t>(round(static_cast<Ogre::Real>(mapY + 1) / gainY));
        uint32_t mapXNext = static_cast<uint32_t>(round(static_cast<Ogre::Real>(tileX + 1) * gainX));
        uint32_t mapYNext = static_cast<uint32_t>(round(static_cast<Ogre::Real>(tileY + 1) * gainY));

        if(tileXNext == tileX)
            ++tileXNext;
        if(tileYNext == tileY)
            ++tileYNext;
        if(mapXNext == mapX)
            ++mapXNext;
        if(mapYNext == mapY)
            ++mapYNext;

        MiniMapDrawnFullTileStateListener* listener = new MiniMapDrawnFullTileStateListener(*this,
            mapX, mapXNext, mapY, mapYNext, tileX, tileXNext, tileY, tileYNext);

        mTileStateListeners.push_back(listener);

        mapX = mapXNext;
        tileX = tileXNext;
        if(tileXNext >= tileXMax)
        {
            mapY = mapYNext;
            tileY = tileYNext;
            mapX = 0;
            tileX = 0;
        }
    }

    // It  start, we fire the tile state changed event to make sure every pixel is
    // correctly initialized. We also set the listeners on the tiles
    for(MiniMapDrawnFullTileStateListener* listener : mTileStateListeners)
    {
        for(uint32_t xxx = listener->mTileXMin; xxx < listener->mTileXMax; ++xxx)
        {
            for(uint32_t yyy = listener->mTileYMin; yyy < listener->mTileYMax; ++yyy)
            {
                Tile* tile = mGameMap.getTile(xxx, yyy);
                if(tile == nullptr)
                    continue;

                tile->addTileStateListener(*listener);
            }
        }

        listener->fireTileStateChanged();
    }

    mPixelBuffer->blitFromMemory(mPixelBox);

    CEGUI::Texture& miniMapTextureGui = static_cast<CEGUI::OgreRenderer*>(CEGUI::System::getSingletonPtr()
                                            ->getRenderer())->createTexture("miniMapTextureGui" + suffix, mMiniMapOgreTexture);

    CEGUI::BasicImage& imageset = MiniMap::createMiniMapImage(mMiniMapWindow, "MiniMapImageset" + suffix, true);
    imageset.setArea(CEGUI::Rectf(CEGUI::Vector2f(0.0, 0.0),
                                      CEGUI::Size<float>(
                                          static_cast<float>(mWidth), static_cast<float>(mHeight)
                                      )
                                  ));

    // Link the image to the minimap
    imageset.setTexture(&miniMapTextureGui);
    mMiniMapWindow->setProperty("Image", CEGUI::PropertyHelper<CEGUI::Image*>::toString(&imageset));

    mMiniMapOgreTexture->load();

    mTopLeftCornerX = mMiniMapWindow->getUnclippedOuterRect().get().getPosition().d_x;
    mTopLeftCornerY = mMiniMapWindow->getUnclippedOuterRect().get().getPosition().d_y;
}

MiniMapDrawnFull::~MiniMapDrawnFull()
{
    for(MiniMapDrawnFullTileStateListener* listener : mTileStateListeners)
    {
        for(uint32_t xxx = listener->mTileXMin; xxx < listener->mTileXMax; ++xxx)
        {
            for(uint32_t yyy = listener->mTileYMin; yyy < listener->mTileYMax; ++yyy)
            {
                Tile* tile = mGameMap.getTile(xxx, yyy);
                if(tile == nullptr)
                    continue;

                tile->removeTileStateListener(*listener);
            }
        }
        delete listener;
    }
    mTileStateListeners.clear();
    mMiniMapWindow->setProperty("Image", "");
    Ogre::String mm("miniMapOgreTexture" + mResourceSuffix);
    Ogre::TextureManager::getSingletonPtr()->remove(mm,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);    
    CEGUI::ImageManager::getSingletonPtr()->destroy("MiniMapImageset" + mResourceSuffix);
    CEGUI::System::getSingletonPtr()->getRenderer()->destroyTexture("miniMapTextureGui" + mResourceSuffix);
}

Ogre::Vector2 MiniMapDrawnFull::camera_2dPositionFromClick(int xx, int yy)
{
    mTopLeftCornerX = static_cast<int>(mMiniMapWindow->getPixelPosition().d_x);
    mTopLeftCornerY = static_cast<int>(mMiniMapWindow->getPixelPosition().d_y);
    Ogre::Vector2 v(0, 0);
    const CEGUI::Sizef displaySize = mMiniMapWindow->getPixelSize();
    v.x = std::max(0.0f, std::min(static_cast<Ogre::Real>(mGameMap.getMapSizeX() - 1),
        (mViewOrigin.x + (xx - mTopLeftCornerX) / displaySize.d_width * mViewSize.x) * mGameMap.getMapSizeX()));
    v.y = std::max(0.0f, std::min(static_cast<Ogre::Real>(mGameMap.getMapSizeY() - 1),
        (1.0f - mViewOrigin.y - (yy - mTopLeftCornerY) / displaySize.d_height * mViewSize.y) * mGameMap.getMapSizeY()));

    return v;
}

bool MiniMapDrawnFull::updateTileState(uint32_t minimapXMin, uint32_t minimapXMax,
        uint32_t minimapYMin, uint32_t minimapYMax, uint32_t tileXMin,
        uint32_t tileXMax, uint32_t tileYMin, uint32_t tileYMax)
{
    Seat& localPlayerSeat = *mGameMap.getLocalPlayer()->getSeat();
    TileColour selected;
    bool animated = false;
    for(uint32_t x = tileXMin; x < tileXMax; ++x)
    {
        for(uint32_t y = tileYMin; y < tileYMax; ++y)
        {
            Tile* tile = mGameMap.getTile(x, y);
            if(tile == nullptr)
                continue;
            const TileColour colour = colourFromTile(*tile, localPlayerSeat,
                static_cast<unsigned int>(mAnimationTime * 2.0f));
            animated |= colour.animated;
            if(colour.priority > selected.priority)
                selected = colour;
        }
    }
    fillPixelRegion(selected.colour, mPixelBox, minimapXMin, minimapXMax, minimapYMin, minimapYMax);
    mPixelsDirty = true;
    return animated;
}

void MiniMapDrawnFull::update(Ogre::Real timeSinceLastFrame, const std::vector<Ogre::Vector3>& cornerTiles)
{
    const unsigned int oldPhase = static_cast<unsigned int>(mAnimationTime * 2.0f);
    mAnimationTime = std::fmod(mAnimationTime + timeSinceLastFrame, 2.0f);
    if(oldPhase != static_cast<unsigned int>(mAnimationTime * 2.0f))
        for(MiniMapDrawnFullTileStateListener* listener : mTileStateListeners)
            if(listener->mAnimated)
                listener->fireTileStateChanged();

    const Ogre::Real scale = std::min(1.0f, getZoomScale());
    const Ogre::Vector3 target = mCameraManager.getCameraViewTarget();
    mViewSize = Ogre::Vector2(scale, scale);
    mViewOrigin.x = std::max(0.0f, std::min(1.0f - scale, target.x / mGameMap.getMapSizeX() - scale * 0.5f));
    mViewOrigin.y = std::max(0.0f, std::min(1.0f - scale, 1.0f - target.y / mGameMap.getMapSizeY() - scale * 0.5f));
    auto& image = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().get("MiniMapImageset" + mResourceSuffix));
    image.setArea(CEGUI::Rectf(mViewOrigin.x * mWidth, mViewOrigin.y * mHeight,
        (mViewOrigin.x + scale) * mWidth, (mViewOrigin.y + scale) * mHeight));
    mMiniMapWindow->invalidate();
    updateMapOverlay(mMiniMapWindow, mGameMap,
        Ogre::Vector2((mViewOrigin.x + scale * 0.5f) * mGameMap.getMapSizeX(),
            (1.0f - mViewOrigin.y - scale * 0.5f) * mGameMap.getMapSizeY()),
        Ogre::Vector2(mGameMap.getMapSizeX(), mGameMap.getMapSizeY()) * scale, 0.0f, cornerTiles);

    if(!mPixelsDirty)
        return;
    mPixelBuffer->blitFromMemory(mPixelBox);
    mPixelsDirty = false;
}
