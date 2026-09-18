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

#include "gamemap/MiniMap.h"

#include "gamemap/MiniMapCamera.h"
#include "gamemap/MiniMapDrawn.h"
#include "gamemap/MiniMapDrawnFull.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "render/ODFrameListener.h"
#include "camera/CameraManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <CEGUI/BasicImage.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/Renderer.h>
#include <CEGUI/System.h>
#include <CEGUI/Texture.h>
#include <CEGUI/Window.h>
#include <cmath>

namespace
{
class MiniMapImage : public CEGUI::BasicImage
{
public:
    explicit MiniMapImage(const CEGUI::String& name) : CEGUI::BasicImage(name), mDot(name + "Dot") { createDot(); }
    explicit MiniMapImage(const CEGUI::XMLAttributes& attributes) : CEGUI::BasicImage(attributes), mDot(getName() + "Dot") { createDot(); }
    ~MiniMapImage() override
    {
        CEGUI::System::getSingleton().getRenderer()->destroyTexture(getName() + "DotTexture");
    }

    bool mCircular = true;
    bool mShowDirection = false;
    std::vector<Ogre::Vector2> mViewport;
    Ogre::Vector2 mDirectionStart;
    Ogre::Vector2 mDirectionEnd;

    void render(CEGUI::GeometryBuffer& buffer, const CEGUI::Rectf& area,
        const CEGUI::Rectf* clip, const CEGUI::ColourRect& colours) const override
    {
        if(area.getWidth() <= 0.0f || area.getHeight() <= 0.0f)
            return;
        // Clip the existing texture, preserving its coordinates and all renderer choices.
        const float radiusX = area.getWidth() * 0.5f;
        const float radiusY = area.getHeight() * 0.5f;
        const float centerX = area.left() + radiusX;
        if(!mCircular)
            CEGUI::BasicImage::render(buffer, area, clip, colours);
        else for(float y = area.top(); y < area.bottom(); y += 1.0f)
        {
            const float nextY = std::min(y + 1.0f, area.bottom());
            const float dy = ((y + nextY) * 0.5f - area.top() - radiusY) / radiusY;
            const float halfWidth = radiusX * std::sqrt(std::max(0.0f, 1.0f - dy * dy));
            CEGUI::Rectf strip(centerX - halfWidth, y, centerX + halfWidth, nextY);
            if(clip != nullptr)
                strip = strip.getIntersection(*clip);
            if(strip.getWidth() > 0.0f && strip.getHeight() > 0.0f)
                CEGUI::BasicImage::render(buffer, area, &strip, colours);
        }
        if(mShowDirection)
            drawLine(buffer, area, clip, colours, mDirectionStart, mDirectionEnd, true);
        for(size_t i = 0; i < mViewport.size(); ++i)
            drawLine(buffer, area, clip, colours, mViewport[i], mViewport[(i + 1) % mViewport.size()], false);
    }

private:
    void drawLine(CEGUI::GeometryBuffer& buffer, const CEGUI::Rectf& area,
            const CEGUI::Rectf* clip, const CEGUI::ColourRect& colours,
            const Ogre::Vector2& from, const Ogre::Vector2& to, bool dotted) const
    {
        Ogre::Vector2 start(area.left() + from.x * area.getWidth(), area.top() + from.y * area.getHeight());
        const Ogre::Vector2 direction((to.x - from.x) * area.getWidth(), (to.y - from.y) * area.getHeight());
        const CEGUI::Rectf bounds = clip == nullptr ? area : area.getIntersection(*clip);
        if(bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
            return;
        // Clip before sampling: camera ground intersections may be far outside the map.
        float first = 0.0f, last = 1.0f;
        const float p[] = {-direction.x, direction.x, -direction.y, direction.y};
        const float q[] = {start.x - bounds.left(), bounds.right() - start.x,
            start.y - bounds.top(), bounds.bottom() - start.y};
        for(int edge = 0; edge < 4; ++edge)
        {
            if(p[edge] == 0.0f)
            {
                if(q[edge] < 0.0f)
                    return;
                continue;
            }
            const float distance = q[edge] / p[edge];
            if(p[edge] < 0.0f)
                first = std::max(first, distance);
            else
                last = std::min(last, distance);
            if(first > last)
                return;
        }
        start += direction * first;
        const Ogre::Vector2 segment = direction * (last - first);
        const float length = segment.length();
        if(length < 1.0f)
            return;
        const float halfSize = dotted ? 1.0f : 0.5f;
        for(float distance = 0.0f; distance <= length; distance += dotted ? 5.0f : 0.75f)
        {
            const Ogre::Vector2 dot = start + segment * (distance / length);
            if(mCircular)
            {
                const float radiusX = area.getWidth() * 0.5f, radiusY = area.getHeight() * 0.5f;
                const float x = (dot.x - area.left() - radiusX) / std::max(1.0f, radiusX - halfSize - 1.0f);
                const float y = (dot.y - area.top() - radiusY) / std::max(1.0f, radiusY - halfSize - 1.0f);
                if(x * x + y * y > 1.0f)
                    continue;
            }
            const float left = std::floor(dot.x - halfSize + 0.5f);
            const float top = std::floor(dot.y - halfSize + 0.5f);
            mDot.render(buffer, CEGUI::Rectf(left, top,
                left + halfSize * 2.0f, top + halfSize * 2.0f), &bounds, colours);
        }
    }

    void createDot()
    {
        CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture(getName() + "DotTexture");
        const unsigned char white[] = {255, 255, 255, 255};
        texture.loadFromMemory(white, CEGUI::Sizef(1, 1), CEGUI::Texture::PF_RGBA);
        mDot.setTexture(&texture);
        mDot.setArea(CEGUI::Rectf(0, 0, 1, 1));
    }
    CEGUI::BasicImage mDot;
};
}

CEGUI::BasicImage& MiniMap::createMiniMapImage(CEGUI::Window* miniMapWindow, const std::string& name, bool showViewport)
{
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    const bool circular = miniMapWindow->isUserStringDefined("Circular") &&
        miniMapWindow->getUserString("Circular") == "true";
    if(!circular && !showViewport)
        return static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
    if(!images.isImageTypeAvailable("MiniMap"))
        images.addImageType<MiniMapImage>("MiniMap");
    auto& image = static_cast<MiniMapImage&>(images.create("MiniMap", name));
    image.mCircular = circular;
    return image;
}

void MiniMap::setZoomLevel(int level)
{
    mZoomLevel = std::max(-3, std::min(2, level));
}

Ogre::Real MiniMap::getZoomScale() const
{
    return std::ldexp(1.0f, -mZoomLevel);
}

void MiniMap::updateMapOverlay(CEGUI::Window* window, GameMap& map,
        const Ogre::Vector2& centre, const Ogre::Vector2& span, Ogre::Real rotation, const std::vector<Ogre::Vector3>& cornerTiles)
{
    auto* image = dynamic_cast<MiniMapImage*>(
        &CEGUI::ImageManager::getSingleton().get(window->getProperty("Image")));
    if(image == nullptr)
        return;
    const auto project = [&centre, &span, rotation](const Ogre::Vector2& world)
    {
        const Ogre::Vector2 delta = world - centre;
        const float x = delta.x * std::cos(rotation) + delta.y * std::sin(rotation);
        const float y = -delta.x * std::sin(rotation) + delta.y * std::cos(rotation);
        return Ogre::Vector2(0.5f + x / span.x, 0.5f - y / span.y);
    };
    image->mViewport.clear();
    for(const Ogre::Vector3& corner : cornerTiles)
        image->mViewport.push_back(project(Ogre::Vector2(corner.x, corner.y)));
    image->mShowDirection = false;
    if(image->mCircular && getZoomLevel() <= 0)
    {
        const Ogre::Vector3 target = ODFrameListener::getSingleton().getCameraManager()->getCameraViewTarget();
        image->mDirectionStart = project(Ogre::Vector2(target.x, target.y));
        Seat* owner = map.getLocalPlayer()->getSeat();
        for(int y = 0; y < map.getMapSizeY() && !image->mShowDirection; ++y)
            for(int x = 0; x < map.getMapSizeX(); ++x)
            {
                Tile* tile = map.getTile(x, y);
                if(tile->getEverVisible() && tile->getSeat() == owner && tile->getTileVisual() == TileVisual::dungeonTempleRoom)
                {
                    image->mDirectionEnd = project(Ogre::Vector2(x, y));
                    image->mShowDirection = true;
                    break;
                }
            }
    }
    window->invalidate();
}

MiniMap::TileColour MiniMap::colourFromTile(Tile& tile, Seat& playerSeat, unsigned int phase)
{
    TileColour result;
    if(!tile.getEverVisible())
        return result;

    const TileVisual visual = tile.getTileVisual();
    Seat* owner = tile.getSeat();
    const bool room = visual >= TileVisual::dungeonTempleRoom && visual < TileVisual::countTileVisual;
    result.priority = 1;
    if(room || visual == TileVisual::claimedGround || visual == TileVisual::claimedFull)
    {
        result.priority = 3;
        if(room && (owner == nullptr || owner->isRogueSeat()))
        {
            const Ogre::ColourValue colours[] = {Ogre::ColourValue::Red, Ogre::ColourValue::Green,
                Ogre::ColourValue::Blue, Ogre::ColourValue(1.0f, 1.0f, 0.0f)};
            result.colour = colours[phase % 4];
            result.animated = true;
        }
        else
        {
            result.colour = owner == nullptr ? Ogre::ColourValue(0.36f, 0.18f, 0.05f) : owner->getColorValue();
            if(visual == TileVisual::dungeonTempleRoom)
                result.colour = result.colour * 0.65f + Ogre::ColourValue::White * 0.35f;
            else if(visual == TileVisual::claimedFull)
                result.colour = result.colour * 0.45f;
        }
    }
    else
    {
        switch(visual)
        {
            case TileVisual::dirtGround:
            case TileVisual::goldGround:
            case TileVisual::gemGround:
            case TileVisual::rockGround:
                result.colour = Ogre::ColourValue(0.82f, 0.70f, 0.50f);
                break;
            case TileVisual::dirtFull:
                result.colour = Ogre::ColourValue(0.36f, 0.18f, 0.05f);
                break;
            case TileVisual::rockFull:
                result.colour = Ogre::ColourValue(0.20f, 0.10f, 0.04f);
                break;
            case TileVisual::goldFull:
                result.colour = Ogre::ColourValue(1.0f, 0.90f, 0.0f);
                result.priority = 4;
                break;
            case TileVisual::gemFull:
                result.colour = Ogre::ColourValue(0.63f, 0.0f, 0.82f);
                result.priority = 4;
                break;
            case TileVisual::waterGround:
                result.colour = Ogre::ColourValue(0.13f, 0.21f, 0.48f);
                result.priority = 2;
                break;
            case TileVisual::lavaGround:
                result.colour = Ogre::ColourValue(0.72f, 0.30f, 0.15f);
                result.priority = 2;
                break;
            default:
                break;
        }
    }
    if(tile.getMarkedForDigging(playerSeat.getPlayer()))
    {
        result.colour = Ogre::ColourValue(0.0f, 1.0f, 1.0f);
        result.priority = 5;
    }
    if(tile.getLocalPlayerHasVision())
    {
        for(GameEntity* entity : tile.getEntitiesInTile())
        {
            if(entity->getObjectType() == GameEntityType::creature)
            {
                Seat* creatureOwner = entity->getSeat();
                if(creatureOwner == &playerSeat)
                {
                    result.animated = true;
                    if(phase % 2 == 0 && result.priority < 7)
                    {
                        result.colour = Ogre::ColourValue::Black;
                        result.priority = 7;
                    }
                }
                else
                {
                    result.colour = creatureOwner == nullptr ? Ogre::ColourValue::White : creatureOwner->getColorValue();
                    result.priority = 8;
                }
            }
            else if(result.priority < 6 && entity->tryPickup(&playerSeat))
            {
                result.colour = Ogre::ColourValue(0.87f, 0.87f, 0.07f);
                result.priority = 6;
            }
        }
    }
    return result;
}

namespace MiniMapTypes
{
static const std::string MINIMAP_CAMERA = "MiniMapCamera";
static const std::string MINIMAP_DRAWN = "MiniMapDrawn";
static const std::string MINIMAP_DRAWN_FULL = "MiniMapDrawnFull";

static std::vector<std::string> buildMiniMapTypes()
{
    std::vector<std::string> mapTypes;
    mapTypes.push_back(MINIMAP_CAMERA);
    mapTypes.push_back(MINIMAP_DRAWN);
    mapTypes.push_back(MINIMAP_DRAWN_FULL);
    return mapTypes;
}

};

const std::string& MiniMap::DEFAULT_MINIMAP = MiniMapTypes::MINIMAP_DRAWN;

MiniMap* MiniMap::createMiniMap(CEGUI::Window* miniMapWindow)
{
    ConfigManager& config = ConfigManager::getSingleton();
    std::string minimapType = config.getGameValue(Config::MINIMAP_TYPE, DEFAULT_MINIMAP, false);
    if(minimapType == MiniMapTypes::MINIMAP_CAMERA)
        return new MiniMapCamera(miniMapWindow);
    if(minimapType == MiniMapTypes::MINIMAP_DRAWN)
        return new MiniMapDrawn(miniMapWindow);
    if(minimapType == MiniMapTypes::MINIMAP_DRAWN_FULL)
        return new MiniMapDrawnFull(miniMapWindow);

    OD_LOG_ERR("Couldn't find requested minimap=" + minimapType);
    // Per default, we return the default minimap
    return new MiniMapDrawn(miniMapWindow);
}

const std::vector<std::string>& MiniMap::getMiniMapTypes()
{
    static std::vector<std::string> minimapTypes = MiniMapTypes::buildMiniMapTypes();
    return minimapTypes;
}
