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
class CircularMiniMapImage : public CEGUI::BasicImage
{
public:
    explicit CircularMiniMapImage(const CEGUI::String& name) : CEGUI::BasicImage(name), mDot(name + "Dot") { createDot(); }
    explicit CircularMiniMapImage(const CEGUI::XMLAttributes& attributes) : CEGUI::BasicImage(attributes), mDot(getName() + "Dot") { createDot(); }
    ~CircularMiniMapImage() override
    {
        CEGUI::System::getSingleton().getRenderer()->destroyTexture(getName() + "DotTexture");
    }

    bool mShowDirection = false;
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
        for(float y = area.top(); y < area.bottom(); y += 1.0f)
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
        if(!mShowDirection)
            return;
        const Ogre::Vector2 start(area.left() + mDirectionStart.x * area.getWidth(),
            area.top() + mDirectionStart.y * area.getHeight());
        const Ogre::Vector2 end(area.left() + mDirectionEnd.x * area.getWidth(),
            area.top() + mDirectionEnd.y * area.getHeight());
        const Ogre::Vector2 direction = end - start;
        const float length = direction.length();
        if(length < 1.0f)
            return;
        for(float distance = 0.0f; distance <= length; distance += 5.0f)
        {
            const Ogre::Vector2 dot = start + direction * (distance / length);
            const float x = (dot.x - centerX) / std::max(1.0f, radiusX - 2.0f);
            const float y = (dot.y - area.top() - radiusY) / std::max(1.0f, radiusY - 2.0f);
            if(x * x + y * y > 1.0f)
                continue;
            const CEGUI::Rectf rect(dot.x - 1.0f, dot.y - 1.0f, dot.x + 1.0f, dot.y + 1.0f);
            mDot.render(buffer, rect, clip, colours);
        }
    }
private:
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

CEGUI::BasicImage& MiniMap::createMiniMapImage(CEGUI::Window* miniMapWindow, const std::string& name)
{
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    const bool circular = miniMapWindow->isUserStringDefined("Circular") &&
        miniMapWindow->getUserString("Circular") == "true";
    if(circular && !images.isImageTypeAvailable("CircularMiniMap"))
        images.addImageType<CircularMiniMapImage>("CircularMiniMap");
    return static_cast<CEGUI::BasicImage&>(images.create(
        circular ? "CircularMiniMap" : "BasicImage", name));
}

void MiniMap::setZoomLevel(int level)
{
    mZoomLevel = std::max(-3, std::min(2, level));
}

Ogre::Real MiniMap::getZoomScale() const
{
    return std::ldexp(1.0f, -mZoomLevel);
}

void MiniMap::updateHeartDirection(CEGUI::Window* window, GameMap& map,
        const Ogre::Vector2& centre, const Ogre::Vector2& span, Ogre::Real rotation)
{
    auto* image = dynamic_cast<CircularMiniMapImage*>(
        &CEGUI::ImageManager::getSingleton().get(window->getProperty("Image")));
    if(image == nullptr)
        return;
    image->mShowDirection = false;
    if(getZoomLevel() <= 0)
    {
        const auto project = [&centre, &span, rotation](const Ogre::Vector2& world)
        {
            const Ogre::Vector2 delta = world - centre;
            const float x = delta.x * std::cos(rotation) + delta.y * std::sin(rotation);
            const float y = -delta.x * std::sin(rotation) + delta.y * std::cos(rotation);
            return Ogre::Vector2(0.5f + x / span.x, 0.5f - y / span.y);
        };
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
