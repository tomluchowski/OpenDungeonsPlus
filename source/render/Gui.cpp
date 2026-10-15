/*
 * \file   Gui.cpp
 * \date   05 April 2011
 * \author StefanP.MUC
 * \brief  Class Gui containing all the stuff for the GUI, including translation.
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

#include "render/Gui.h"

#include "ODApplication.h"
#include "sound/SoundEffectsManager.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

#include <CEGUI/CEGUI.h>
#include <CEGUI/BasicImage.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/RendererModules/Ogre/ResourceProvider.h>
#include <CEGUI/RendererModules/Ogre/ImageCodec.h>
#include <CEGUI/SchemeManager.h>
#include <CEGUI/System.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/TabControl.h>
#include <CEGUI/widgets/TabButton.h>
#include <CEGUI/Event.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const float LAYOUT_DESIGN_WIDTH = 1024.0f;
const float LAYOUT_DESIGN_HEIGHT = 768.0f;

// Layout of the room, trap and spell buttons in unscaled pixels.
const float ACTION_BUTTON_MARGIN_X = 20.0f;
const float ACTION_BUTTON_MARGIN_Y = 6.0f;
const float ACTION_BUTTON_SPACING = 4.0f;
const float LARGE_ACTION_BUTTON_SIDE = 102.0f;
const float SMALL_ACTION_BUTTON_SIDE = 52.0f;
//! Vertical distance between the two rows of small buttons.
const float SMALL_ACTION_BUTTON_ROW_PITCH = 56.0f;
//! Most buttons that are still shown large, in a single row.
const size_t MAX_LARGE_ACTION_BUTTONS = 6;

// Properties and user strings the layouts use to mark the navigation frame.
const std::string NAVIGATION_FRAME_NAME = "NavigationFrame";
const std::string NAVIGATION_COLOUR_NAME = "NavigationColour";
const std::string NAVIGATION_IMAGE_NAME = "NavigationImage";
const float FONT_DESIGN_WIDTH = 800.0f;
const float FONT_DESIGN_HEIGHT = 600.0f;

void createHandFeedbackImage()
{
    // Project artwork: a prohibition shape, without copied assets.
    const int size = 64;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    for(int y = 0; y < size; ++y)
    {
        for(int x = 0; x < size; ++x)
        {
            const float dx = x + 0.5f - size * 0.5f;
            const float dy = y + 0.5f - size * 0.5f;
            const float radius = std::sqrt(dx * dx + dy * dy);
            const float ring = std::min(28.0f - radius, radius - 21.0f);
            const float slash = std::min(24.0f - radius, 3.5f - std::abs(dx - dy) * 0.70710678f);
            const float coverage = std::max(0.0f, std::min(1.0f, std::max(ring, slash) + 0.5f));
            const int i = (y * size + x) * 4;
            pixels[i] = 210;
            pixels[i + 1] = 32;
            pixels[i + 2] = 48;
            pixels[i + 3] = static_cast<unsigned char>(coverage * 255.0f);
        }
    }
    CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture("HandProhibition");
    texture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
        "BasicImage", "OpenDungeonsIcons/Prohibition"));
    image.setTexture(&texture);
    image.setArea(CEGUI::Rectf(0, 0, size, size));
}

//! Round button that fills a corner of the minimap frame: it only reacts outside the round map.
class MiniMapCornerButton : public CEGUI::PushButton
{
public:
    static const CEGUI::String WidgetTypeName;
    MiniMapCornerButton(const CEGUI::String& type, const CEGUI::String& name) : CEGUI::PushButton(type, name) {}

    bool isHit(const CEGUI::Vector2f& position, bool allowDisabled = false) const override
    {
        if(!CEGUI::PushButton::isHit(position, allowDisabled))
            return false;
        if(getParent() == nullptr || !getParent()->isChild("MiniMap"))
            return true;
        // Use the actual map bounds: pixel rounding can differ from this button.
        const CEGUI::Rectf& map = getParent()->getChild("MiniMap")->getUnclippedOuterRect().get();
        const float x = (position.d_x - map.left() - map.getWidth() * 0.5f) / (map.getWidth() * 0.5f);
        const float y = (position.d_y - map.top() - map.getHeight() * 0.5f) / (map.getHeight() * 0.5f);
        return x * x + y * y >= 1.0f;
    }
};
const CEGUI::String MiniMapCornerButton::WidgetTypeName("OD/MiniMapCornerBase");

// Generated navigation artwork, in pixels of the generated textures.
const int CORNER_IMAGE_SIZE = 128;
//! Size of the corner button the corner artwork is designed for.
const float CORNER_DESIGN_SIZE = 44.0f;
//! Radius of the round map the corner artwork is cut out of, in design pixels.
const float CORNER_CURVE_RADIUS = 88.0f;
const int NAVIGATION_ICON_SIZE = 64;
const int BADGE_IMAGE_SIZE = 128;
//! Each icon pixel is sampled on a 4 x 4 grid to get smooth edges.
const int ICON_SUBSAMPLES = 4;
const float ICON_SUBSAMPLE_STEP = 1.0f / ICON_SUBSAMPLES;
const int ICON_SUBSAMPLE_COUNT = ICON_SUBSAMPLES * ICON_SUBSAMPLES;
//! The utility icons are drawn on a narrower grid than the square category icons.
const float UTILITY_ICON_WIDTH = 32.0f;
const float UTILITY_ICON_HEIGHT = 52.0f;
const int CATEGORY_ICON_COUNT = 4;
const int UTILITY_ICON_COUNT = 3;
const int BADGE_COUNT = 2;
const int MANA_BADGE = 0;
const int MESSAGES_UTILITY_ICON = 2;

//! Registers RGBA pixels as the texture \a name and the image "OpenDungeonsIcons/<name>".
void createIconImage(const std::string& name, const std::vector<unsigned char>& pixels, int width, int height)
{
    CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture(name);
    texture.loadFromMemory(pixels.data(), CEGUI::Sizef(width, height), CEGUI::Texture::PF_RGBA);
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
        "BasicImage", "OpenDungeonsIcons/" + name));
    image.setTexture(&texture);
    image.setArea(CEGUI::Rectf(0, 0, width, height));
}

//! Brightness of a corner pixel: a dark outline, a light and a mid rim, then the shaded face.
float getCornerShade(float edge, int y, int size)
{
    if(edge < 1.0f)
        return 12.0f;
    if(edge < 2.0f)
        return 116.0f;
    if(edge < 3.0f)
        return 62.0f;
    return 24.0f - 10.0f * y / size;
}

void createMiniMapCornerImages()
{
    CEGUI::WindowFactoryManager::addFactory<CEGUI::TplWindowFactory<MiniMapCornerButton>>();
    const int size = CORNER_IMAGE_SIZE;
    for(int corner = 0; corner < 4; ++corner)
    {
        std::vector<unsigned char> pixels(size * size * 4, 0);
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                // Bit 0 mirrors the artwork horizontally, bit 1 vertically.
                const float u = ((corner & 1) ? size - x - 0.5f : x + 0.5f) * CORNER_DESIGN_SIZE / size;
                const float v = ((corner & 2) ? size - y - 0.5f : y + 0.5f) * CORNER_DESIGN_SIZE / size;
                const float curve = std::sqrt((CORNER_CURVE_RADIUS - u) * (CORNER_CURVE_RADIUS - u) +
                    (CORNER_CURVE_RADIUS - v) * (CORNER_CURVE_RADIUS - v)) - CORNER_CURVE_RADIUS;
                const float edge = std::min(curve, std::min(std::min(u, v),
                    std::min(CORNER_DESIGN_SIZE - u, CORNER_DESIGN_SIZE - v)));
                if(edge <= 0.0f)
                    continue;
                const float shade = getCornerShade(edge, y, size);
                const int i = (y * size + x) * 4;
                pixels[i] = static_cast<unsigned char>(shade);
                pixels[i + 1] = static_cast<unsigned char>(shade + 3.0f);
                pixels[i + 2] = static_cast<unsigned char>(shade + 5.0f);
                pixels[i + 3] = static_cast<unsigned char>(255.0f * std::min(1.0f, edge * size / CORNER_DESIGN_SIZE));
            }
        }
        createIconImage("MiniMapCorner" + std::to_string(corner), pixels, size, size);
    }
}

//! Whether (px, py) lies within \a radius of the segment from (ax, ay) to (bx, by).
bool isNearSegment(float px, float py, float ax, float ay, float bx, float by, float radius)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    const float t = std::max(0.0f, std::min(1.0f,
        ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
    const float ex = px - ax - t * dx;
    const float ey = py - ay - t * dy;
    return ex * ex + ey * ey <= radius * radius;
}

//! Whether (px, py) is inside the symbol of a category icon (creatures, rooms, spells, workshop).
bool isInsideCategoryIcon(int category, float px, float py)
{
    if(category == 0)
    {
        const float dx = px - 32.0f;
        const float dy = py - 13.0f;
        return dx * dx + dy * dy <= 25.0f
            || (py >= 21 && py <= 40 && std::abs(dx) <= 9 - (py - 21) * 0.2f)
            || isNearSegment(px, py, 24, 24, 16, 40, 2.5f) || isNearSegment(px, py, 40, 24, 48, 40, 2.5f)
            || isNearSegment(px, py, 29, 38, 24, 55, 3) || isNearSegment(px, py, 35, 38, 40, 55, 3);
    }
    if(category == 1)
    {
        if(px >= 27 && px <= 37 && py >= 39)
            return false;
        return (py >= 9 && py <= 30 && std::abs(px - 32) <= (py - 9) * 1.2f)
            || (px >= 13 && px <= 51 && py >= 27 && py <= 55);
    }
    if(category == 2)
    {
        return isNearSegment(px, py, 12, 54, 43, 23, 2.5f)
            || isNearSegment(px, py, 44, 9, 44, 16, 1.5f) || isNearSegment(px, py, 51, 22, 58, 22, 1.5f)
            || isNearSegment(px, py, 31, 11, 35, 15, 1.5f) || isNearSegment(px, py, 51, 14, 55, 10, 1.5f)
            || isNearSegment(px, py, 51, 29, 55, 33, 1.5f) || isNearSegment(px, py, 31, 27, 35, 23, 1.5f);
    }
    return isNearSegment(px, py, 23, 20, 52, 51, 3)
        || isNearSegment(px, py, 8, 27, 17, 14, 2.5f) || isNearSegment(px, py, 17, 14, 30, 9, 2.5f)
        || isNearSegment(px, py, 30, 9, 43, 11, 2.5f) || isNearSegment(px, py, 17, 14, 26, 22, 3);
}

//! Whether (px, py) is inside the symbol of a utility icon (panel, objectives, messages).
bool isInsideUtilityIcon(int utility, float px, float py)
{
    if(utility == 0)
    {
        return isNearSegment(px, py, 7, 14, 16, 6, 1.5f) || isNearSegment(px, py, 16, 6, 25, 14, 1.5f)
            || isNearSegment(px, py, 7, 38, 16, 46, 1.5f) || isNearSegment(px, py, 16, 46, 25, 38, 1.5f);
    }
    if(utility == 1)
    {
        const float dx = (px - 16) / 10;
        const float dy = std::abs(py - 26);
        const float lid = 5 * (1 - dx * dx);
        return (std::abs(dx) <= 1 && std::abs(dy - lid) <= 1.1f)
            || (px - 16) * (px - 16) + dy * dy <= 9;
    }
    return (px - 18) * (px - 18) + (py - 14) * (py - 14) <= 4
        || isNearSegment(px, py, 16, 23, 13, 38, 1.5f)
        || isNearSegment(px, py, 12, 23, 17, 23, 1) || isNearSegment(px, py, 12, 38, 18, 38, 1);
}

//! Counts how many of the sub-samples of pixel (x, y) are inside the icon symbol.
//! \a scaleX and \a scaleY map pixels to the coordinate system the symbol is drawn in.
int sampleIconCoverage(int x, int y, float scaleX, float scaleY,
    bool (*isInside)(int, float, float), int icon)
{
    int coverage = 0;
    for(int sy = 0; sy < ICON_SUBSAMPLES; ++sy)
    {
        for(int sx = 0; sx < ICON_SUBSAMPLES; ++sx)
        {
            const float px = (x + (sx + 0.5f) * ICON_SUBSAMPLE_STEP) * scaleX;
            const float py = (y + (sy + 0.5f) * ICON_SUBSAMPLE_STEP) * scaleY;
            if(isInside(icon, px, py))
                ++coverage;
        }
    }
    return coverage;
}

void createMapZoomImage()
{
    const int size = NAVIGATION_ICON_SIZE;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    // A magnifier: a ring with a diagonal handle.
    const float ringCentre = 25.0f;
    const float ringRadius = 15.0f;
    const float lineHalfWidth = 3.0f;
    const float handleStart = 36.0f;
    const float handleEnd = 53.0f;
    const float handleLengthSquared = 2.0f * (handleEnd - handleStart) * (handleEnd - handleStart);
    for(int y = 0; y < size; ++y)
    {
        for(int x = 0; x < size; ++x)
        {
            const float dx = x + 0.5f - ringCentre;
            const float dy = y + 0.5f - ringCentre;
            const float ringDistance = std::abs(std::sqrt(dx * dx + dy * dy) - ringRadius);
            const float handleX = x + 0.5f - handleStart;
            const float handleY = y + 0.5f - handleStart;
            const float handleT = std::max(0.0f, std::min(1.0f,
                ((handleX + handleY) * (handleEnd - handleStart)) / handleLengthSquared));
            const float nearestX = handleStart + handleT * (handleEnd - handleStart);
            const float nearestY = handleStart + handleT * (handleEnd - handleStart);
            const float segmentX = x + 0.5f - nearestX;
            const float segmentY = y + 0.5f - nearestY;
            const float handleDistance = std::sqrt(segmentX * segmentX + segmentY * segmentY);
            const float coverage = std::max(0.0f, std::min(1.0f,
                std::max(lineHalfWidth - ringDistance, lineHalfWidth - handleDistance)));
            const int i = (y * size + x) * 4;
            pixels[i] = 232;
            pixels[i + 1] = 226;
            pixels[i + 2] = 202;
            pixels[i + 3] = static_cast<unsigned char>(coverage * 255.0f);
        }
    }
    createIconImage("MapZoom", pixels, size, size);
}

void createCategoryImages()
{
    const int size = NAVIGATION_ICON_SIZE;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    const char* categories[] = {"NavigationCreatures", "NavigationRooms", "NavigationSpells", "NavigationWorkshop"};
    for(int category = 0; category < CATEGORY_ICON_COUNT; ++category)
    {
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                const int coverage = sampleIconCoverage(x, y, 1.0f, 1.0f, isInsideCategoryIcon, category);
                const int i = (y * size + x) * 4;
                pixels[i] = pixels[i + 1] = pixels[i + 2] = static_cast<unsigned char>(244 - y);
                pixels[i + 3] = static_cast<unsigned char>(coverage * 255 / ICON_SUBSAMPLE_COUNT);
            }
        }
        createIconImage(categories[category], pixels, size, size);
    }
}

void createUtilityImages()
{
    const int size = NAVIGATION_ICON_SIZE;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    const char* utilities[] = {"NavigationPanel", "NavigationObjectives", "NavigationMessages"};
    for(int utility = 0; utility < UTILITY_ICON_COUNT; ++utility)
    {
        const bool messages = utility == MESSAGES_UTILITY_ICON;
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                const int coverage = sampleIconCoverage(x, y, UTILITY_ICON_WIDTH / size,
                    UTILITY_ICON_HEIGHT / size, isInsideUtilityIcon, utility);
                const int i = (y * size + x) * 4;
                pixels[i] = messages ? 140 : 224;
                pixels[i + 1] = messages ? 235 : 226;
                pixels[i + 2] = messages ? 255 : 218;
                pixels[i + 3] = static_cast<unsigned char>(coverage * 255 / ICON_SUBSAMPLE_COUNT);
            }
        }
        createIconImage(utilities[utility], pixels, size, size);
    }
}

//! Whether (dx, dy), relative to the badge centre, is inside the badge symbol:
//! a heart for the mana badge, an S-shaped sign with a vertical bar for the gold badge.
bool isInsideBadgeSymbol(int badge, float dx, float dy)
{
    if(badge == MANA_BADGE)
    {
        const float hx = dx / 13;
        const float hy = -dy / 13;
        const float heart = hx * hx + hy * hy - 1;
        return heart * heart * heart - hx * hx * hy * hy * hy <= 0;
    }
    const float upper = std::sqrt((dx + 1) * (dx + 1) + (dy + 7) * (dy + 7));
    const float lower = std::sqrt((dx - 1) * (dx - 1) + (dy - 7) * (dy - 7));
    return (std::abs(dx) < 1.5f && std::abs(dy) < 20)
        || (std::abs(upper - 8) < 1.8f && (dx < 0 || dy < -7))
        || (std::abs(lower - 8) < 1.8f && (dx > 0 || dy > 7));
}

void createBadgeImages()
{
    const int badgeSize = BADGE_IMAGE_SIZE;
    std::vector<unsigned char> pixels(badgeSize * badgeSize * 4, 0);
    for(int badge = 0; badge < BADGE_COUNT; ++badge)
    {
        const bool mana = badge == MANA_BADGE;
        for(int y = 0; y < badgeSize; ++y)
        {
            for(int x = 0; x < badgeSize; ++x)
            {
                const float dx = (x + 0.5f) * 64 / badgeSize - 32;
                const float dy = (y + 0.5f) * 64 / badgeSize - 32;
                const float radius = std::sqrt(dx * dx + dy * dy);
                const float light = -(dx + dy) / std::max(1.0f, radius * 1.414214f);
                const int i = (y * badgeSize + x) * 4;
                unsigned char shade = static_cast<unsigned char>(std::max(0.0f, 28 - radius * 0.6f));
                pixels[i] = pixels[i + 1] = pixels[i + 2] = shade;
                if(radius > 25)
                {
                    const float slope = std::max(-1.0f, std::min(1.0f, (radius - 28) / 3));
                    const float face = std::sqrt(std::max(0.0f, 1 - slope * slope));
                    shade = static_cast<unsigned char>(std::max(12.0f,
                        74 + 92 * light * slope + 65 * face));
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = shade;
                }
                else if(radius > 22 && radius < 24)
                {
                    const float relief = 0.65f + 0.35f * light * (radius - 23);
                    pixels[i] = static_cast<unsigned char>((mana ? 24 : 210) * relief);
                    pixels[i + 1] = static_cast<unsigned char>((mana ? 178 : 171) * relief);
                    pixels[i + 2] = static_cast<unsigned char>((mana ? 114 : 35) * relief);
                }
                if(isInsideBadgeSymbol(badge, dx, dy))
                {
                    const float highlight = std::exp(-((dx + 5) * (dx + 5) + (dy + 6) * (dy + 6)) / 35);
                    float relief = 174 - dx * 1.4f - dy * 2.4f + 48 * highlight;
                    if(!isInsideBadgeSymbol(badge, dx - 1, dy - 1))
                        relief = 244;
                    else if(!isInsideBadgeSymbol(badge, dx + 1, dy + 1))
                        relief = 72;
                    pixels[i] = pixels[i + 1] = pixels[i + 2] =
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, relief)));
                }
                else if(isInsideBadgeSymbol(badge, dx - 1.5f, dy - 1.5f))
                    pixels[i] = pixels[i + 1] = pixels[i + 2] = 4;
                pixels[i + 3] = static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, 31 - radius)) * 255);
            }
        }
        createIconImage(mana ? "ManaBadge" : "GoldBadge", pixels, badgeSize, badgeSize);
    }
}

void createNavigationImages()
{
    createMiniMapCornerImages();
    createMapZoomImage();
    createCategoryImages();
    createUtilityImages();
    createBadgeImages();
}

void scaleDimension(CEGUI::UDim& dimension, float scale)
{
    dimension.d_offset *= scale;
}

CEGUI::URect scaleRect(const CEGUI::URect& rect, float scale)
{
    CEGUI::URect scaled(rect);
    scaleDimension(scaled.d_min.d_x, scale);
    scaleDimension(scaled.d_min.d_y, scale);
    scaleDimension(scaled.d_max.d_x, scale);
    scaleDimension(scaled.d_max.d_y, scale);
    return scaled;
}

CEGUI::USize scaleSize(const CEGUI::USize& size, float scale)
{
    CEGUI::USize scaled(size);
    scaleDimension(scaled.d_width, scale);
    scaleDimension(scaled.d_height, scale);
    return scaled;
}

bool shouldScaleImage(const CEGUI::String& ceguiName)
{
    const std::string name(ceguiName.c_str());
    return name.compare(0, 17, "OpenDungeonsSkin/") == 0
        || name.compare(0, 18, "OpenDungeonsIcons/") == 0
        || name.compare(0, 17, "ODMainMenuButton/") == 0
        || name.compare(0, 13, "ODHudSurface/") == 0
        || name.compare(0, 7, "ODLogo/") == 0;
}

std::string scaleFormattedImageSizes(const CEGUI::String& ceguiText, float scale)
{
    std::string text(ceguiText.c_str());
    size_t tagStart = 0;
    while((tagStart = text.find("[image-size='", tagStart)) != std::string::npos)
    {
        size_t tagEnd = text.find("']", tagStart);
        if(tagEnd == std::string::npos)
            break;

        const char* dimensions[] = {"w:", "h:"};
        for(const char* dimension : dimensions)
        {
            const size_t marker = text.find(dimension, tagStart);
            if(marker == std::string::npos || marker >= tagEnd)
                continue;

            const size_t valueStart = marker + 2;
            size_t valueEnd = valueStart;
            while(valueEnd < tagEnd && ((text[valueEnd] >= '0' && text[valueEnd] <= '9') || text[valueEnd] == '.'))
                ++valueEnd;

            float value = 0.0f;
            std::istringstream valueParser(text.substr(valueStart, valueEnd - valueStart));
            if(!(valueParser >> value))
                continue;

            std::ostringstream scaledValue;
            scaledValue << std::max(1, static_cast<int>(value * scale + 0.5f));
            text.replace(valueStart, valueEnd - valueStart, scaledValue.str());
            tagEnd = text.find("']", tagStart);
        }

        tagStart = tagEnd + 2;
    }
    return text;
}
}

Gui::Gui(SoundEffectsManager* soundEffectsManager, const std::string& ceguiLogFileName, Ogre::RenderTarget &renderTarget)
  : mUserScale(1.0f),
    mSoundEffectsManager(soundEffectsManager)
{
    OD_LOG_INF("*** Initializing CEGUI ***");
    CEGUI::OgreRenderer& renderer = CEGUI::OgreRenderer::create(renderTarget);
    OD_LOG_INF("OgreRenderer created");
    CEGUI::OgreResourceProvider& rp = CEGUI::OgreRenderer::createOgreResourceProvider();
    OD_LOG_INF("OgreResourceProvider created");
    CEGUI::OgreImageCodec& ic = CEGUI::OgreRenderer::createOgreImageCodec();
    OD_LOG_INF("OgreImageCodec created");
    CEGUI::System::create(renderer, &rp, static_cast<CEGUI::XMLParser*>(nullptr), &ic, nullptr, "",
                          reinterpret_cast<const CEGUI::utf8*>(ceguiLogFileName.c_str()));
    OD_LOG_INF("CEGUI::System created");

    CEGUI::SchemeManager::getSingleton().createFromFile("ODSkin.scheme");
    OD_LOG_INF("CEGUI::SchemeManager created");
    createHandFeedbackImage();
    createNavigationImages();

    float configuredScalePercent = 100.0f;
    std::istringstream scaleParser(ConfigManager::getSingleton().getGameValue(Config::UI_SCALE, "100", false));
    if(!(scaleParser >> configuredScalePercent))
        configuredScalePercent = 100.0f;
    configuredScalePercent = std::max(static_cast<float>(MIN_UI_SCALE_PERCENT),
        std::min(static_cast<float>(MAX_UI_SCALE_PERCENT), configuredScalePercent));
    mUserScale = configuredScalePercent / 100.0f;
    updateResourceScaling(renderer.getDisplaySize());

    // We want Ogre overlays to be displayed in front of CEGUI. According to
    // http://cegui.org.uk/forum/viewtopic.php?f=10&t=5694
    // the best way is to disable CEGUI auto rendering by calling setFrameControlExecutionEnabled
    // and render CEGUI in an Ogre::RenderQueueListener (done in ODFrameListener) by calling
    // CEGUI::System::getSingleton().renderAllGUIContexts();
    renderer.setFrameControlExecutionEnabled(false);

    // Needed to get the correct offset when using up to CEGUI 0.8.4
    // We're thus using an empty mouse cursor.
    CEGUI::GUIContext& context = CEGUI::System::getSingleton().getDefaultGUIContext();
    context.getMouseCursor().setDefaultImage("OpenDungeonsSkin/MouseArrow");
    context.getMouseCursor().setVisible(true);
    context.setDefaultTooltipType("OD/Tooltip");
    CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
    mSheets[hideGui] = wmgr->createWindow("DefaultWindow", "DummyWindow");
    mSheets[inGameMenu] = wmgr->loadLayoutFromFile("ModeGame.layout");
    mSheets[advertisment] = wmgr->loadLayoutFromFile("Advertisment.layout");
    mSheets[mainMenu] = wmgr->loadLayoutFromFile("MenuMain.layout");
    mSheets[skirmishMenu] = wmgr->loadLayoutFromFile("MenuSkirmish.layout");
    mSheets[multiplayerClientMenu] = wmgr->loadLayoutFromFile("MenuMultiplayerClient.layout");
    mSheets[multiplayerServerMenu] = wmgr->loadLayoutFromFile("MenuMultiplayerServer.layout");
    mSheets[multiMasterServerJoinMenu] = wmgr->loadLayoutFromFile("MenuMasterServerJoin.layout");
    mSheets[editorModeGui] =  wmgr->loadLayoutFromFile("ModeEditor.layout");
    mSheets[editorNewMenu] =  wmgr->loadLayoutFromFile("MenuEditorNew.layout");
    mSheets[editorLoadMenu] =  wmgr->loadLayoutFromFile("MenuEditorLoad.layout");
    mSheets[configureSeats] =  wmgr->loadLayoutFromFile("MenuConfigureSeats.layout");
    mSheets[replayMenu] =  wmgr->loadLayoutFromFile("MenuReplay.layout");
    mSheets[loadSavedGameMenu] =  wmgr->loadLayoutFromFile("MenuLoad.layout");
    mSheets[console] = wmgr->loadLayoutFromFile("WindowConsole.layout");

    mDisplaySizeChangedConnection = CEGUI::System::getSingleton().subscribeEvent(
        CEGUI::System::EventDisplaySizeChanged,
        CEGUI::Event::Subscriber(&Gui::onDisplaySizeChanged, this));
    mWindowDestroyedConnection = wmgr->subscribeEvent(
        CEGUI::WindowManager::EventWindowDestroyed,
        CEGUI::Event::Subscriber(&Gui::onWindowDestroyed, this));

    for(const std::pair<const guiSheet, CEGUI::Window*>& sheet : mSheets)
        registerWindow(sheet.second);
    applyScale(renderer.getDisplaySize());

    // Set the game version
    mSheets[mainMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[skirmishMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiplayerServerMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiplayerClientMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[editorNewMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[editorLoadMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[replayMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[loadSavedGameMenu]->getChild("VersionText")->setText(ODApplication::VERSION);
    mSheets[multiMasterServerJoinMenu]->getChild("VersionText")->setText(ODApplication::VERSION);

    // Add sound to button clicks
    CEGUI::GlobalEventSet& ges = CEGUI::GlobalEventSet::getSingleton();
    ges.subscribeEvent(
        CEGUI::PushButton::EventNamespace + "/" + CEGUI::PushButton::EventClicked,
        CEGUI::Event::Subscriber(&Gui::playButtonClickSound, this));
}

Gui::~Gui()
{
    mDisplaySizeChangedConnection.disconnect();
    mWindowDestroyedConnection.disconnect();
    //This also calls CEGUI::System::destroy();
    CEGUI::OgreRenderer::destroySystem();
}

CEGUI::MouseButton Gui::convertButton(OIS::MouseButtonID buttonID)
{
    //OIS has 2 more button ids than CEGUI.
    if(static_cast<int>(buttonID) < static_cast<int>(CEGUI::MouseButton::MouseButtonCount))
        return static_cast<CEGUI::MouseButton>(buttonID);
    return CEGUI::MouseButton::NoButton;
}

void Gui::loadGuiSheet(guiSheet newSheet)
{
    registerWindowHierarchy(mSheets[newSheet]);
    CEGUI::System::getSingletonPtr()->getDefaultGUIContext().setRootWindow(mSheets[newSheet]);
    // This shouldn't be needed, but the gui seems to not allways change when using hideGui without it.
    CEGUI::System::getSingletonPtr()->getDefaultGUIContext().markAsDirty();
}

void Gui::registerWindowHierarchy(CEGUI::Window* window)
{
    if(window == nullptr)
        return;

    registerWindow(window);
    applyScale(CEGUI::System::getSingleton().getRenderer()->getDisplaySize());
}

void Gui::registerWindow(CEGUI::Window* window)
{
    if(window->isPropertyPresent(NAVIGATION_FRAME_NAME))
    {
        for(CEGUI::Window* parent = window->getParent(); parent != nullptr; parent = parent->getParent())
        {
            if(parent->isUserStringDefined(NAVIGATION_FRAME_NAME) &&
               parent->getUserString(NAVIGATION_FRAME_NAME) == "true")
            {
                window->setProperty(NAVIGATION_FRAME_NAME, "True");
                break;
            }
        }
    }
    CEGUI::TabButton* tabButton = dynamic_cast<CEGUI::TabButton*>(window);
    if(tabButton != nullptr && window->isPropertyPresent(NAVIGATION_COLOUR_NAME))
    {
        CEGUI::Window* page = tabButton->getTargetWindow();
        if(page != nullptr && page->isUserStringDefined(NAVIGATION_COLOUR_NAME))
            window->setProperty(NAVIGATION_COLOUR_NAME, page->getUserString(NAVIGATION_COLOUR_NAME));
        if(page != nullptr && page->isUserStringDefined(NAVIGATION_IMAGE_NAME) &&
           window->isPropertyPresent(NAVIGATION_IMAGE_NAME))
            window->setProperty(NAVIGATION_IMAGE_NAME, page->getUserString(NAVIGATION_IMAGE_NAME));
    }
    if(!window->isAutoWindow() && mScaledWindows.find(window) == mScaledWindows.end())
    {
        WindowScaleData data;
        data.area = window->getArea();
        data.minSize = window->getMinSize();
        data.maxSize = window->getMaxSize();
        data.text = window->getText();
        data.hasFormattedImageSize = std::string(data.text.c_str()).find("[image-size='") != std::string::npos;
        data.hasTabHeight = false;

        CEGUI::TabControl* tabControl = dynamic_cast<CEGUI::TabControl*>(window);
        if(tabControl != nullptr)
        {
            data.tabHeight = tabControl->getTabHeight();
            data.tabTextPadding = tabControl->getTabTextPadding();
            data.hasTabHeight = true;
        }

        mScaledWindows.insert(std::make_pair(window, data));
    }

    for(size_t i = 0; i < window->getChildCount(); ++i)
        registerWindow(window->getChildAtIdx(i));
}

void Gui::setUserScalePercent(float scalePercent)
{
    if(scalePercent != scalePercent)
        scalePercent = 100.0f;

    scalePercent = std::max(static_cast<float>(MIN_UI_SCALE_PERCENT),
        std::min(static_cast<float>(MAX_UI_SCALE_PERCENT), scalePercent));
    mUserScale = scalePercent / 100.0f;
    applyScale(CEGUI::System::getSingleton().getRenderer()->getDisplaySize());
}

void Gui::arrangeActionButtonPanels(CEGUI::Window* gameSheet)
{
    arrangeRoomButtons(gameSheet->getChild(TAB_ROOMS));
    arrangeTrapButtons(gameSheet->getChild(TAB_TRAPS));
    arrangeSpellButtons(gameSheet->getChild(TAB_SPELLS));
}

void Gui::arrangeRoomButtons(CEGUI::Window* rooms)
{
    rooms->getChild("DestroyRoomButton")->hide();
    arrangeActionButtons(rooms, {"DormitoryButton", "HatcheryButton", "LibraryButton", "TrainingHallButton",
        "TreasuryButton", "WorkshopButton", "CasinoButton", "PrisonButton", "WoodenBridgeButton",
        "TortureButton", "StoneBridgeButton", "CryptButton", "ArenaButton"});
}

void Gui::arrangeTrapButtons(CEGUI::Window* traps)
{
    traps->getChild("DestroyTrapButton")->hide();
    arrangeActionButtons(traps, {"CannonButton", "SpikeTrapButton", "BoulderTrapButton", "WoodenDoorTrapButton"});
}

void Gui::arrangeSpellButtons(CEGUI::Window* spells)
{
    arrangeActionButtons(spells, {"SummonWorkerButton", "CallToWarButton", "CreatureHealButton",
        "CreatureExplosionButton", "CreatureHasteButton", "CreatureDefenseButton", "CreatureSlowButton",
        "CreatureStrengthButton", "CreatureWeakButton", "SpellEyeEvilButton"});
}

void Gui::arrangeActionButtons(CEGUI::Window* panel, std::initializer_list<const char*> names)
{
    const CEGUI::Sizef displaySize = CEGUI::System::getSingleton().getRenderer()->getDisplaySize();
    const float scale = std::min(displaySize.d_width / LAYOUT_DESIGN_WIDTH,
        displaySize.d_height / LAYOUT_DESIGN_HEIGHT) * mUserScale;
    std::vector<CEGUI::Window*> buttons;
    for(const char* name : names)
    {
        CEGUI::Window* button = panel->getChild(name);
        if(button->isVisible())
            buttons.push_back(button);
    }

    const bool large = buttons.size() <= MAX_LARGE_ACTION_BUTTONS &&
        (ACTION_BUTTON_MARGIN_X + (LARGE_ACTION_BUTTON_SIDE + ACTION_BUTTON_SPACING) * buttons.size() -
        ACTION_BUTTON_SPACING) * scale <= panel->getPixelSize().d_width;
    const float side = large ? LARGE_ACTION_BUTTON_SIDE : SMALL_ACTION_BUTTON_SIDE;
    for(size_t index = 0; index < buttons.size(); ++index)
    {
        CEGUI::Window* button = buttons[index];
        const size_t column = large ? index : index / 2;
        const float x = ACTION_BUTTON_MARGIN_X + (side + ACTION_BUTTON_SPACING) * static_cast<float>(column);
        const float y = ACTION_BUTTON_MARGIN_Y +
            (large ? 0.0f : SMALL_ACTION_BUTTON_ROW_PITCH * static_cast<float>(index % 2));
        std::map<CEGUI::Window*, WindowScaleData>::iterator scaled = mScaledWindows.find(button);
        if(scaled == mScaledWindows.end())
            continue;
        WindowScaleData& data = scaled->second;
        data.area = CEGUI::URect(CEGUI::UDim(0, x), CEGUI::UDim(0, y),
            CEGUI::UDim(0, x + side), CEGUI::UDim(0, y + side));
        applyScale(button, data, scale);
    }
}

bool Gui::onDisplaySizeChanged(const CEGUI::EventArgs& e)
{
    const CEGUI::DisplayEventArgs& displayEvent = static_cast<const CEGUI::DisplayEventArgs&>(e);
    applyScale(displayEvent.size);
    return true;
}

bool Gui::onWindowDestroyed(const CEGUI::EventArgs& e)
{
    const CEGUI::WindowEventArgs& windowEvent = static_cast<const CEGUI::WindowEventArgs&>(e);
    mScaledWindows.erase(windowEvent.window);
    return true;
}

void Gui::applyScale(const CEGUI::Sizef& displaySize)
{
    const float resolutionScale = std::min(displaySize.d_width / LAYOUT_DESIGN_WIDTH,
        displaySize.d_height / LAYOUT_DESIGN_HEIGHT);
    const float scale = resolutionScale * mUserScale;

    updateResourceScaling(displaySize);

    for(const std::pair<CEGUI::Window* const, WindowScaleData>& scaledWindow : mScaledWindows)
        applyScale(scaledWindow.first, scaledWindow.second, scale);

    const std::map<guiSheet, CEGUI::Window*>::iterator gameSheet = mSheets.find(inGameMenu);
    if(gameSheet != mSheets.end())
    {
        arrangeActionButtonPanels(gameSheet->second);
    }

    CEGUI::System::getSingleton().getDefaultGUIContext().markAsDirty();
}

void Gui::applyScale(CEGUI::Window* window, const WindowScaleData& data, float scale)
{
    window->setMinSize(scaleSize(data.minSize, scale));
    window->setMaxSize(scaleSize(data.maxSize, scale));
    window->setArea(scaleRect(data.area, scale));

    if(data.hasFormattedImageSize)
        window->setText(scaleFormattedImageSizes(data.text, scale));

    if(data.hasTabHeight)
    {
        CEGUI::UDim tabHeight(data.tabHeight);
        scaleDimension(tabHeight, scale);
        static_cast<CEGUI::TabControl*>(window)->setTabHeight(tabHeight);
        CEGUI::UDim tabTextPadding(data.tabTextPadding);
        scaleDimension(tabTextPadding, scale);
        static_cast<CEGUI::TabControl*>(window)->setTabTextPadding(tabTextPadding);
    }
}

void Gui::updateResourceScaling(const CEGUI::Sizef& displaySize)
{
    const CEGUI::Sizef fontNativeResolution(FONT_DESIGN_WIDTH / mUserScale,
        FONT_DESIGN_HEIGHT / mUserScale);
    CEGUI::FontManager::FontIterator font = CEGUI::FontManager::getSingleton().getIterator();
    while(!font.isAtEnd())
    {
        font.getCurrentValue()->setNativeResolution(fontNativeResolution);
        font.getCurrentValue()->setAutoScaled(CEGUI::ASM_Min);
        ++font;
    }

    const CEGUI::Sizef imageNativeResolution(LAYOUT_DESIGN_WIDTH / mUserScale,
        LAYOUT_DESIGN_HEIGHT / mUserScale);
    CEGUI::ImageManager::ImageIterator image = CEGUI::ImageManager::getSingleton().getIterator();
    while(!image.isAtEnd())
    {
        if(shouldScaleImage(image.getCurrentKey()))
        {
            CEGUI::BasicImage* basicImage = dynamic_cast<CEGUI::BasicImage*>(image.getCurrentValue().first);
            if(basicImage != nullptr)
            {
                basicImage->setNativeResolution(imageNativeResolution);
                basicImage->setAutoScaled(CEGUI::ASM_Min);
            }
        }
        ++image;
    }

    CEGUI::FontManager::getSingleton().notifyDisplaySizeChanged(displaySize);
    CEGUI::ImageManager::getSingleton().notifyDisplaySizeChanged(displaySize);
}

CEGUI::Window* Gui::getGuiSheet(guiSheet sheet)
{
    auto it = mSheets.find(sheet);
    if(it != mSheets.end())
    {
        return it->second;
    }

    return nullptr;
}

void Gui::setRenderTarget(Ogre::RenderTarget& renderTarget)
{
    static_cast<CEGUI::OgreRenderer*>(CEGUI::System::getSingleton().getRenderer())
        ->setDefaultRootRenderTarget(renderTarget);
}

bool Gui::playButtonClickSound(const CEGUI::EventArgs&)
{
    mSoundEffectsManager->playRelativeSound(SoundRelativeInterface::Click);
    return true;
}

/* These constants are used to access the GUI element
 * NOTE: when add/remove/rename a GUI element, don't forget to change it here
 */
const std::string Gui::DISPLAY_GOLD = "HorizontalPipe/GoldDisplay";
const std::string Gui::DISPLAY_MANA = "HorizontalPipe/ManaDisplay";
const std::string Gui::DISPLAY_TERRITORY = "PlayerSettingsWindow/TerritoryDisplay";
const std::string Gui::DISPLAY_CREATURES = "PlayerSettingsWindow/CreaturesDisplay";
const std::string Gui::MINIMAP = "MiniMap";
const std::string Gui::OBJECTIVE_TEXT = "ObjectivesWindow/ObjectivesText";
const std::string Gui::MAIN_TABCONTROL = "MainTabControl";
const std::string Gui::TAB_ROOMS = "MainTabControl/Rooms";
const std::string Gui::BUTTON_TEMPLE = "MainTabControl/Rooms/TempleButton";
const std::string Gui::BUTTON_PORTAL = "MainTabControl/Rooms/PortalButton";
const std::string Gui::BUTTON_PORTAL_WAVE = "MainTabControl/Rooms/WavePortalButton";
const std::string Gui::BUTTON_DESTROY_ROOM = "MainTabControl/Rooms/DestroyRoomButton";
const std::string Gui::TAB_TRAPS = "MainTabControl/Traps";
const std::string Gui::BUTTON_DESTROY_TRAP = "MainTabControl/Traps/DestroyTrapButton";
const std::string Gui::TAB_SPELLS = "MainTabControl/Spells";
const std::string Gui::TAB_CREATURES = "MainTabControl/Creatures";
const std::string Gui::BUTTON_CREATURE_WORKER = "MainTabControl/Creatures/WorkerButton";
const std::string Gui::BUTTON_CREATURE_FIGHTER = "MainTabControl/Creatures/FighterButton";
const std::string Gui::TAB_COMBAT = "MainTabControl/Combat";

const std::string Gui::MM_BACKGROUND = "Background";
const std::string Gui::MM_WELCOME_MESSAGE = "WelcomeBanner";
const std::string Gui::EXIT_CONFIRMATION_POPUP = "ConfirmExit";
const std::string Gui::EXIT_CONFIRMATION_POPUP_YES_BUTTON = "ConfirmExit/YesOption";
const std::string Gui::EXIT_CONFIRMATION_POPUP_NO_BUTTON = "ConfirmExit/NoOption";

const std::string Gui::SKM_TEXT_LOADING = "LoadingText";
const std::string Gui::SKM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchGameButton";
const std::string Gui::SKM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::SKM_LIST_LEVEL_TYPES = "LevelWindowFrame/LevelTypeSelect";
const std::string Gui::SKM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";

const std::string Gui::MPM_TEXT_LOADING = "LoadingText";
const std::string Gui::MPM_BUTTON_SERVER = "LevelWindowFrame/ServerButton";
const std::string Gui::MPM_BUTTON_CLIENT = "LevelWindowFrame/ClientButton";
const std::string Gui::MPM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::MPM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";
const std::string Gui::MPM_EDIT_IP = "LevelWindowFrame/IpEdit";
const std::string Gui::MPM_EDIT_NICK = "LevelWindowFrame/NickEdit";

const std::string Gui::EDM_TEXT_LOADING = "LoadingText";
const std::string Gui::EDM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchEditorButton";
const std::string Gui::EDM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::EDM_LIST_LEVELS = "LevelWindowFrame/LevelSelect";
const std::string Gui::EDM_LIST_LEVEL_TYPES = "LevelWindowFrame/LevelTypeSelect";

const std::string Gui::EDITOR = "MainTabControl";
const std::string Gui::EDITOR_LAVA_BUTTON = "MainTabControl/Tiles/LavaButton";
const std::string Gui::EDITOR_GOLD_BUTTON = "MainTabControl/Tiles/GoldButton";
const std::string Gui::EDITOR_DIRT_BUTTON = "MainTabControl/Tiles/DirtButton";
const std::string Gui::EDITOR_WATER_BUTTON = "MainTabControl/Tiles/WaterButton";
const std::string Gui::EDITOR_ROCK_BUTTON = "MainTabControl/Tiles/RockButton";
const std::string Gui::EDITOR_CLAIMED_BUTTON = "MainTabControl/Tiles/ClaimedButton";
const std::string Gui::EDITOR_GEM_BUTTON = "MainTabControl/Tiles/GemButton";
const std::string Gui::EDITOR_FULLNESS = "HorizontalPipe/FullnessDisplay";
const std::string Gui::EDITOR_CURSOR_POS = "HorizontalPipe/PositionDisplay";
const std::string Gui::EDITOR_SEAT_ID = "HorizontalPipe/SeatIdDisplay";
const std::string Gui::EDITOR_CREATURE_SPAWN = "HorizontalPipe/CreatureSpawnDisplay";
const std::string Gui::EDITOR_LEVEL_NAME = "LevelNameDisplay";
const std::string Gui::EDITOR_MAPLIGHT_BUTTON = "MainTabControl/Lights/MapLightButton";

const std::string Gui::REM_TEXT_LOADING = "LoadingText";
const std::string Gui::REM_BUTTON_LAUNCH = "LevelWindowFrame/LaunchReplayButton";
const std::string Gui::REM_BUTTON_DELETE = "LevelWindowFrame/DeleteReplayButton";
const std::string Gui::REM_BUTTON_BACK = "LevelWindowFrame/BackButton";
const std::string Gui::REM_LIST_REPLAYS = "LevelWindowFrame/ReplaySelect";
