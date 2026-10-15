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
#include "game/HeartHealthRing.h"
#include "render/RenderManager.h"

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
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/ScrollablePane.h>
#include <CEGUI/widgets/ScrolledContainer.h>
#include <CEGUI/widgets/Tooltip.h>
#include <CEGUI/Event.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <functional>

namespace
{
const float LAYOUT_DESIGN_WIDTH = 1024.0f;
const float LAYOUT_DESIGN_HEIGHT = 768.0f;
const float FONT_DESIGN_WIDTH = 800.0f;
const float FONT_DESIGN_HEIGHT = 600.0f;

void createHandFeedbackImage()
{
    // Original project artwork: the reference's prohibition shape, without copied assets.
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

class MiniMapCornerButton : public CEGUI::PushButton
{
public:
    static const CEGUI::String WidgetTypeName;
    MiniMapCornerButton(const CEGUI::String& type, const CEGUI::String& name) : CEGUI::PushButton(type, name) {}

    bool isHit(const CEGUI::Vector2f& position, bool allowDisabled = false) const override
    {
        if(!CEGUI::PushButton::isHit(position, allowDisabled))
            return false;
        // Use the actual map bounds: pixel rounding can differ from this button.
        const CEGUI::Rectf& map = getParent()->getChild("MiniMap")->getUnclippedOuterRect().get();
        const float x = (position.d_x - map.left() - map.getWidth() * 0.5f) / (map.getWidth() * 0.5f);
        const float y = (position.d_y - map.top() - map.getHeight() * 0.5f) / (map.getHeight() * 0.5f);
        return x * x + y * y >= 1.0f;
    }
};
const CEGUI::String MiniMapCornerButton::WidgetTypeName("OD/MiniMapCornerBase");

void createMiniMapCornerImages()
{
    CEGUI::WindowFactoryManager::addFactory<CEGUI::TplWindowFactory<MiniMapCornerButton>>();
    const int size = 128;
    for(int corner = 0; corner < 4; ++corner)
    {
        std::vector<unsigned char> pixels(size * size * 4, 0);
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                const float u = ((corner & 1) ? size - x - 0.5f : x + 0.5f) * 44.0f / size;
                const float v = ((corner & 2) ? size - y - 0.5f : y + 0.5f) * 44.0f / size;
                const float curve = std::sqrt((88.0f - u) * (88.0f - u) + (88.0f - v) * (88.0f - v)) - 88.0f;
                const float edge = std::min(curve, std::min(std::min(u, v), std::min(44.0f - u, 44.0f - v)));
                if(edge <= 0.0f)
                    continue;
                const float shade = edge < 1.0f ? 12.0f : edge < 2.0f ? 116.0f : edge < 3.0f ? 62.0f : 24.0f - 10.0f * y / size;
                const int i = (y * size + x) * 4;
                pixels[i] = static_cast<unsigned char>(shade);
                pixels[i + 1] = static_cast<unsigned char>(shade + 3.0f);
                pixels[i + 2] = static_cast<unsigned char>(shade + 5.0f);
                pixels[i + 3] = static_cast<unsigned char>(255.0f * std::min(1.0f, edge * size / 44.0f));
            }
        }
        const std::string name = "MiniMapCorner" + std::to_string(corner);
        CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture(name);
        texture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
        CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
            "BasicImage", "OpenDungeonsIcons/" + name));
        image.setTexture(&texture);
        image.setArea(CEGUI::Rectf(0, 0, size, size));
    }
}

const int BADGE_SIZE = 128;

float badgeClamp(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

//! \brief Pulls the colour (0-255 per channel) towards the given colour by the given amount (0-1).
void badgeMix(float* colour, float red, float green, float blue, float amount)
{
    const float weight = badgeClamp(amount, 0.0f, 1.0f);
    colour[0] += (red - colour[0]) * weight;
    colour[1] += (green - colour[1]) * weight;
    colour[2] += (blue - colour[2]) * weight;
}

void badgeSet(float* colour, float red, float green, float blue, float scale)
{
    colour[0] = red * scale;
    colour[1] = green * scale;
    colour[2] = blue * scale;
}

//! \brief Fine grain in [-1, 1] that only depends on the pixel.
float badgeNoise(int x, int y)
{
    const unsigned int hash = (static_cast<unsigned int>(x) * 73856093u) ^ (static_cast<unsigned int>(y) * 19349663u);
    return static_cast<float>(hash % 7u) / 3.0f - 1.0f;
}

float badgeSmoothstep(float low, float high, float value)
{
    const float t = badgeClamp((value - low) / (high - low), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

//! \brief Smooth union of two distance fields; k is the width of the blend.
float badgeSmoothMin(float first, float second, float k)
{
    const float h = badgeClamp(0.5f + 0.5f * (second - first) / k, 0.0f, 1.0f);
    return second + (first - second) * h - k * h * (1.0f - h);
}

float badgeHash(int x, int y)
{
    unsigned int hash = static_cast<unsigned int>(x) * 374761393u + static_cast<unsigned int>(y) * 668265263u;
    hash = (hash ^ (hash >> 13)) * 1274126177u;
    hash ^= hash >> 16;
    return static_cast<float>(hash & 0xFFFFu) / 65536.0f;
}

//! \brief Smooth noise in [0, 1] that varies over about one unit.
float badgeValueNoise(float x, float y)
{
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const float u = badgeSmoothstep(0.0f, 1.0f, x - fx);
    const float v = badgeSmoothstep(0.0f, 1.0f, y - fy);
    const int ix = static_cast<int>(fx);
    const int iy = static_cast<int>(fy);
    const float top = badgeHash(ix, iy) + (badgeHash(ix + 1, iy) - badgeHash(ix, iy)) * u;
    const float bottom = badgeHash(ix, iy + 1) + (badgeHash(ix + 1, iy + 1) - badgeHash(ix, iy + 1)) * u;
    return top + (bottom - top) * v;
}

//! \brief Three octaves of smooth noise in [0, 1].
float badgeFbm(float x, float y)
{
    return 0.58f * badgeValueNoise(x, y) + 0.30f * badgeValueNoise(2.1f * x + 5.2f, 2.1f * y + 1.3f)
        + 0.12f * badgeValueNoise(4.3f * x + 9.7f, 4.3f * y + 3.1f);
}

float badgeCircle(float x, float y, float centreX, float centreY, float radius)
{
    return std::sqrt((x - centreX) * (x - centreX) + (y - centreY) * (y - centreY)) - radius;
}

//! \brief Distance to a tapered capsule from (ax, ay) with radius ra to (bx, by) with radius rb.
float badgeTaper(float x, float y, float ax, float ay, float bx, float by, float ra, float rb)
{
    const float lx = bx - ax;
    const float ly = by - ay;
    const float along = badgeClamp(((x - ax) * lx + (y - ay) * ly) / (lx * lx + ly * ly), 0.0f, 1.0f);
    return badgeCircle(x, y, ax + lx * along, ay + ly * along, ra + (rb - ra) * along);
}

float badgeBox(float x, float y, float centreX, float centreY, float halfX, float halfY, float corner)
{
    const float qx = std::abs(x - centreX) - halfX + corner;
    const float qy = std::abs(y - centreY) - halfY + corner;
    return std::sqrt(std::max(qx, 0.0f) * std::max(qx, 0.0f) + std::max(qy, 0.0f) * std::max(qy, 0.0f))
        + std::min(std::max(qx, qy), 0.0f) - corner;
}

//! \brief Height of a rounded ridge whose inner depth is given: 0 at the edge, 1 at the full
//! depth and beyond.
float badgeRound(float depth, float width)
{
    const float t = badgeClamp(depth / width, 0.0f, 1.0f);
    return std::sqrt(1.0f - (1.0f - t) * (1.0f - t));
}

//! \brief Outline of the dungeon heart: a heavy, slightly lopsided heart muscle with two lobes, a
//! blunt tip and two vessels rising at the top. Negative inside.
float badgeHeartDistance(float x, float y)
{
    float d = badgeCircle(x, y, -5.3f, -3.6f, 7.3f);
    d = badgeSmoothMin(d, badgeCircle(x, y, 5.9f, -4.2f, 6.7f), 1.8f);
    d = badgeSmoothMin(d, badgeTaper(x, y, 0.2f, -1.5f, 1.0f, 10.6f, 10.2f, 2.4f), 3.4f);
    d = badgeSmoothMin(d, badgeTaper(x, y, 1.6f, -8.0f, 3.2f, -13.6f, 2.5f, 1.9f), 1.6f);
    d = badgeSmoothMin(d, badgeTaper(x, y, -2.8f, -9.6f, -5.4f, -14.0f, 1.8f, 1.4f), 1.4f);
    return d;
}

//! \brief Raised veins on the heart, 1 on the vein and 0 beside it.
float badgeHeartVeins(float x, float y)
{
    float d = badgeTaper(x, y, 1.6f, -7.0f, -0.8f, -1.0f, 0.35f, 0.35f);
    d = std::min(d, badgeTaper(x, y, -0.8f, -1.0f, -4.4f, 5.0f, 0.35f, 0.3f));
    d = std::min(d, badgeTaper(x, y, -4.4f, 5.0f, -2.2f, 10.0f, 0.3f, 0.2f));
    d = std::min(d, badgeTaper(x, y, -0.8f, -1.0f, 3.8f, 1.2f, 0.3f, 0.25f));
    d = std::min(d, badgeTaper(x, y, 4.6f, -9.6f, 6.4f, -3.4f, 0.35f, 0.35f));
    d = std::min(d, badgeTaper(x, y, 6.4f, -3.4f, 4.4f, 3.6f, 0.35f, 0.3f));
    d = std::min(d, badgeTaper(x, y, -8.2f, -7.4f, -7.4f, -1.4f, 0.3f, 0.3f));
    d = std::min(d, badgeTaper(x, y, -7.4f, -1.4f, -5.2f, 3.6f, 0.3f, 0.25f));
    return 1.0f - badgeSmoothstep(0.1f, 0.95f, d);
}

//! \brief Relief of the heart: a plump dome with a rounded rim, veins and a fleshy grain.
float badgeHeartHeight(float x, float y)
{
    const float depth = -badgeHeartDistance(x, y);
    if(depth <= 0.0f)
        return 0.0f;
    const float veins = badgeHeartVeins(x, y) * badgeSmoothstep(0.3f, 1.5f, depth);
    return 5.0f * badgeRound(depth, 5.5f) + 2.2f * badgeSmoothstep(0.0f, 12.0f, depth) + 0.8f * veins
        + 0.5f * badgeFbm(x * 0.55f, y * 0.55f);
}

//! \brief Outline of the skull on the coin. Negative on the bone, positive in the eye sockets,
//! the nose and the gaps between the teeth.
float badgeSkullDistance(float x, float y)
{
    float d = badgeCircle(x, y, 0.0f, -2.8f, 8.0f);
    d = badgeSmoothMin(d, badgeBox(x, y, 0.0f, 5.6f, 4.6f, 3.4f, 1.8f), 3.0f);
    d = std::max(d, -badgeCircle(x, y, -3.5f, -1.6f, 2.6f));
    d = std::max(d, -badgeCircle(x, y, 3.5f, -1.6f, 2.6f));
    d = std::max(d, -badgeTaper(x, y, 0.0f, 2.6f, 0.0f, 4.0f, 0.5f, 1.3f));
    const float teeth = std::max(std::abs(std::abs(x) - 2.2f) - 0.3f, std::abs(y - 7.6f) - 1.5f);
    d = std::max(d, -std::min(teeth, std::max(std::abs(x) - 0.3f, std::abs(y - 7.6f) - 1.5f)));
    return d;
}

//! \brief Relief of the coin: a raised reeded rim, a ring of pearls and the skull on a hammered field.
float badgeCoinHeight(float x, float y)
{
    const float radius = std::sqrt(x * x + y * y);
    if(radius > 19.3f)
        return -2.0f;
    const float angle = std::atan2(x, -y);
    const float ridge = badgeClamp((radius - 14.9f) / 4.4f, 0.0f, 1.0f);
    float height = 0.7f + 0.5f * badgeFbm(x * 0.9f, y * 0.9f) - 0.25f * badgeSmoothstep(0.0f, 14.0f, radius);
    height += 2.6f * std::pow(std::sin(HeartHealthRing::PI * ridge), 0.75f) * (1.0f + 0.10f * std::cos(angle * 64.0f));
    const float pearl = (radius - 13.4f) / 0.75f;
    height += 0.9f * std::exp(-pearl * pearl) * (0.5f + 0.5f * std::cos(angle * 44.0f));
    const float skull = badgeSkullDistance(x, y);
    if(skull < 0.0f)
        height += 2.6f * badgeRound(-skull, 1.5f) + 0.35f * badgeFbm(x * 1.4f + 3.0f, y * 1.4f);
    return height;
}

//! \brief Unit normal of a relief given as a height function, pointing out of the picture.
void badgeNormal(float (*height)(float, float), float x, float y, float* normal)
{
    const float step = 0.3f;
    const float gradientX = (height(x + step, y) - height(x - step, y)) / (2.0f * step);
    const float gradientY = (height(x, y + step) - height(x, y - step)) / (2.0f * step);
    const float length = std::sqrt(gradientX * gradientX + gradientY * gradientY + 1.0f);
    normal[0] = -gradientX / length;
    normal[1] = -gradientY / length;
    normal[2] = 1.0f / length;
}

//! \brief Diffuse light on a surface with the given unit normal, from a light at the top left.
float badgeDiffuse(const float* normal)
{
    return std::max(0.0f, normal[0] * -0.50f + normal[1] * -0.60f + normal[2] * 0.62f);
}

//! \brief Strength of the highlight on a surface with the given unit normal (the same light, seen
//! from the picture): high where the surface faces halfway between the light and the viewer.
float badgeHalfway(const float* normal)
{
    return std::max(0.0f, normal[0] * -0.26f + normal[1] * -0.31f + normal[2] * 0.91f);
}

//! \brief How much lower than its surroundings the relief lies at a point (0 to 1): dirt and shadow collect there.
float badgeCavity(float (*height)(float, float), float x, float y, float reach)
{
    const float around = 0.25f * (height(x + reach, y) + height(x - reach, y) + height(x, y + reach) + height(x, y - reach));
    return badgeClamp((around - height(x, y)) * 0.45f, 0.0f, 1.0f);
}

//! \brief The outer frame (radius above 25.4): blackened iron with a bronze edge line, a bright
//! bronze lip towards the ring, twelve domed bronze rivets and a dark contour.
void badgeFrame(float* colour, float dx, float dy, float radius, float light, float grain)
{
    const float slope = badgeClamp((radius - 28.2f) / 2.8f, -1.0f, 1.0f);
    const float face = std::sqrt(std::max(0.0f, 1 - slope * slope));
    const float value = badgeClamp(0.11f + 0.12f * face + 0.20f * light * slope + 0.022f * grain, 0.05f, 1.0f);
    badgeSet(colour, 255, 214, 172, value);
    if(radius > 29.5f && radius < 30.2f)
        badgeSet(colour, 196, 136, 66, 0.62f + 0.38f * light);
    if(radius < 26.2f)
        badgeSet(colour, 224, 166, 88, 0.72f + 0.38f * light);
    const float degrees = std::atan2(dx, -dy) * HeartHealthRing::RADIANS_TO_DEGREES;
    const float step = std::fmod(degrees + 375.0f, 30.0f);
    const float tangent = (step > 15 ? step - 30 : step) * HeartHealthRing::DEGREES_TO_RADIANS * radius;
    const float rivet = std::sqrt(tangent * tangent + (radius - 28.2f) * (radius - 28.2f));
    if(rivet < 1.35f)
        badgeSet(colour, 190, 132, 70, 0.55f + 0.75f * (1 - rivet / 1.35f) * (0.5f + 0.5f * light) + 0.3f * light);
    else if(rivet < 2.0f)
        badgeMix(colour, 6, 3, 2, 0.6f);
    if(radius > 30.3f)
    {
        colour[0] *= 0.35f;
        colour[1] *= 0.33f;
        colour[2] *= 0.32f;
    }
}

//! \brief The ring channel of the heart badge: six emerald gems, lit or dark obsidian, set
//! between bronze spokes.
void badgeGemRing(float* colour, float dx, float dy, float radius, float light, bool lit)
{
    const float across = (radius - 20.8f) / 4.6f;
    const float dome = std::sin(HeartHealthRing::PI * badgeClamp(across, 0.0f, 1.0f));
    const float inSegment = std::fmod(HeartHealthRing::ringDegrees(dx, dy), HeartHealthRing::SEGMENT_DEGREES);
    if(HeartHealthRing::isSpoke(dx, dy))
    {
        const float offset = std::abs(inSegment < HeartHealthRing::SEGMENT_DEGREES / 2.0f ? inSegment : inSegment - HeartHealthRing::SEGMENT_DEGREES)
            / HeartHealthRing::SPOKE_HALF_DEGREES;
        badgeSet(colour, 226, 168, 92, (0.76f + 0.30f * light) * (0.68f + 0.32f * dome) * (1.08f - 0.30f * offset));
        if(offset > 0.82f)
            badgeMix(colour, 12, 6, 3, 0.55f);
        return;
    }
    const float edge = std::min(inSegment - HeartHealthRing::SPOKE_HALF_DEGREES,
        HeartHealthRing::SEGMENT_DEGREES - HeartHealthRing::SPOKE_HALF_DEGREES - inSegment);
    const float bevel = badgeClamp(edge * HeartHealthRing::DEGREES_TO_RADIANS * radius / 1.3f, 0.0f, 1.0f);
    const float relief = dome * (0.45f + 0.55f * bevel);
    const float shine = std::pow(std::max(0.0f, light), 5.0f) * dome * dome * bevel;
    if(lit)
    {
        const float depth = std::pow(relief, 1.3f);
        colour[0] = 4 + 30 * depth;
        colour[1] = 66 + 142 * depth;
        colour[2] = 40 + 92 * depth;
        badgeMix(colour, 232, 255, 240, 0.75f * shine);
    }
    else
    {
        colour[0] = 22 + 26 * relief;
        colour[1] = 15 + 15 * relief;
        colour[2] = 18 + 17 * relief;
        badgeMix(colour, 120, 100, 96, 0.5f * shine);
    }
}

//! \brief The ring channel of the gold badge: a band of gold beads.
void badgeBeadRing(float* colour, float dx, float dy, float radius, float light)
{
    const float phase = std::fmod(HeartHealthRing::ringDegrees(dx, dy), 15.0f) - 7.5f;
    const float along = phase * HeartHealthRing::DEGREES_TO_RADIANS * radius;
    const float across = radius - 23.1f;
    const float distance = std::sqrt(along * along + across * across) / 2.15f;
    if(distance >= 1.0f)
    {
        badgeSet(colour, 60, 36, 12, 0.6f);
        return;
    }
    const float height = std::sqrt(1 - distance * distance);
    badgeSet(colour, 246, 190, 74, 0.35f + 0.42f * height + 0.34f * light * (1 - height));
    badgeMix(colour, 255, 244, 190, 0.7f * std::pow(std::max(0.0f, light * height), 6.0f));
}

//! \brief The well of the heart badge: dark stone with glowing embers at its foot and the dungeon
//! heart, a lit muscle with raised veins, an inner glow and a wet shine, on top of it. While the
//! heart is under attack a magenta glow lies behind it.
void badgeHeartWell(float* colour, float dx, float dy, float radius, float light, bool underAttack)
{
    const float low = std::max(0.0f, dy / 20.0f);
    colour[0] = 38 - 12 * radius / 20.8f + 74 * low * low;
    colour[1] = 22 - 8 * radius / 20.8f + 26 * low * low;
    colour[2] = 20 - 7 * radius / 20.8f + 6 * low * low;
    if(underAttack)
    {
        const float glow = 0.4f + 0.6f * (radius / 22) * (radius / 22);
        colour[0] += (225 - colour[0]) * glow;
        colour[1] += (35 - colour[1]) * glow;
        colour[2] += (190 - colour[2]) * glow;
    }
    if(radius > 19.4f)
        badgeMix(colour, 4, 2, 2, 0.30f + 0.32f * light);
    const float halo = std::exp(-((dx * dx + (dy + 1) * (dy + 1)) / 150.0f)) * 0.42f;
    badgeMix(colour, 190, 32, 28, halo * (underAttack ? 0.5f : 1.0f));
    if(radius > 19.4f)
        return;
    const float outline = badgeHeartDistance(dx, dy);
    const float shadow = badgeHeartDistance(dx - 1.3f, dy - 2.0f);
    badgeMix(colour, 3, 1, 2, 0.78f * (1.0f - badgeSmoothstep(-1.0f, 3.2f, shadow)));
    const float cover = badgeClamp(0.5f - outline * 2.0f, 0.0f, 1.0f);
    if(cover <= 0.0f)
        return;
    float normal[3];
    badgeNormal(badgeHeartHeight, dx, dy, normal);
    const float depth = std::max(0.0f, -outline);
    const float veins = badgeHeartVeins(dx, dy) * badgeSmoothstep(0.3f, 1.5f, depth);
    const float diffuse = badgeDiffuse(normal);
    const float halfway = badgeHalfway(normal);
    const float grain = badgeFbm(dx * 0.8f + 11.0f, dy * 0.8f);
    const float cavity = badgeCavity(badgeHeartHeight, dx, dy, 1.6f);
    const float occlusion = (0.52f + 0.48f * badgeSmoothstep(0.0f, 3.4f, depth)) * (1.0f - 0.55f * cavity);
    const float pulse = std::exp(-((dx - 0.6f) * (dx - 0.6f) + (dy - 3.0f) * (dy - 3.0f)) / 70.0f);
    float red = 184 - 70 * veins;
    float green = 14 - 6 * veins;
    float blue = 24 - 4 * veins;
    const float mottle = 0.80f + 0.40f * grain;
    const float lit = (0.13f + 1.05f * diffuse) * occlusion * mottle;
    float heart[3] = {red * lit, green * lit, blue * lit};
    heart[0] += 255 * 0.55f * pulse * (1.0f - 0.55f * diffuse) * occlusion;
    heart[1] += 84 * 0.55f * pulse * (1.0f - 0.55f * diffuse) * occlusion;
    heart[2] += 22 * 0.55f * pulse * (1.0f - 0.55f * diffuse) * occlusion;
    const float rim = std::pow(1.0f - normal[2], 2.0f) * badgeClamp(normal[0] * 0.75f + normal[1] * 0.65f + 0.15f, 0.0f, 1.0f);
    heart[0] += 255 * 0.85f * rim;
    heart[1] += 112 * 0.85f * rim;
    heart[2] += 58 * 0.85f * rim;
    const float sharp = std::pow(halfway, 44.0f) * (1.0f - 0.7f * cavity);
    const float broad = std::pow(halfway, 9.0f);
    heart[0] += 255 * (0.95f * sharp + 0.10f * broad);
    heart[1] += 236 * (0.95f * sharp + 0.05f * broad);
    heart[2] += 226 * (0.95f * sharp + 0.05f * broad);
    if(underAttack)
    {
        heart[0] += 40;
        heart[1] += 14;
        heart[2] += 16;
    }
    badgeMix(colour, heart[0], heart[1], heart[2], cover);
}

//! \brief The well of the gold badge: a worn gold coin with a reeded rim, a ring of pearls and a
//! skull struck into a hammered field, lit from the top left.
void badgeCoinWell(float* colour, float dx, float dy, float radius, float light)
{
    badgeSet(colour, 60, 40, 22, 0.45f);
    if(radius > 19.4f)
    {
        badgeMix(colour, 4, 2, 2, 0.30f + 0.32f * light);
        return;
    }
    if(radius > 19.0f)
    {
        badgeSet(colour, 24, 14, 6, 1.0f);
        return;
    }
    float normal[3];
    badgeNormal(badgeCoinHeight, dx, dy, normal);
    const float diffuse = badgeDiffuse(normal);
    const float halfway = badgeHalfway(normal);
    const float cavity = badgeCavity(badgeCoinHeight, dx, dy, 1.4f);
    const float socket = std::min(badgeCircle(dx, dy, -3.5f, -1.6f, 2.6f), badgeCircle(dx, dy, 3.5f, -1.6f, 2.6f));
    const float hollow = 1.0f - badgeSmoothstep(-0.3f, 0.5f, socket);
    const float dirt = badgeFbm(dx * 1.7f + 20.0f, dy * 1.7f);
    const float tone = std::pow(diffuse, 0.85f);
    float gold[3] = {64 + 172 * tone, 36 + 128 * tone, 8 + 40 * tone};
    const float reflection = 0.80f + 0.36f * badgeClamp(0.5f - 0.5f * normal[1] + 0.25f * normal[0], 0.0f, 1.0f);
    gold[0] *= reflection;
    gold[1] *= reflection;
    gold[2] *= reflection;
    const float wear = badgeSmoothstep(0.55f, 0.85f, dirt);
    gold[0] += (80 - gold[0]) * (0.55f * cavity + 0.25f * wear);
    gold[1] += (58 - gold[1]) * (0.55f * cavity + 0.25f * wear);
    gold[2] += (18 - gold[2]) * (0.55f * cavity + 0.25f * wear);
    const float patina = badgeSmoothstep(0.25f, 0.8f, cavity) * badgeSmoothstep(0.45f, 0.8f, dirt);
    gold[0] += (52 - gold[0]) * 0.5f * patina;
    gold[1] += (58 - gold[1]) * 0.5f * patina;
    gold[2] += (22 - gold[2]) * 0.5f * patina;
    const float sharp = std::pow(halfway, 40.0f) * (1.0f - cavity);
    const float broad = std::pow(halfway, 10.0f);
    gold[0] += 255 * (0.85f * sharp + 0.12f * broad);
    gold[1] += 236 * (0.85f * sharp + 0.10f * broad);
    gold[2] += 170 * (0.85f * sharp + 0.06f * broad);
    const float scratchA = 1.0f - badgeSmoothstep(0.0f, 0.22f, std::abs(0.62f * dx + 0.78f * dy - 3.1f));
    const float scratchB = 1.0f - badgeSmoothstep(0.0f, 0.18f, std::abs(0.94f * dx - 0.34f * dy + 6.5f));
    const float scratch = (scratchA * badgeSmoothstep(-6.0f, -3.0f, dx) * (1.0f - badgeSmoothstep(4.0f, 7.0f, dx))
        + scratchB * badgeSmoothstep(-3.0f, 0.0f, dy) * (1.0f - badgeSmoothstep(8.0f, 10.0f, dy))) * 0.32f;
    gold[0] += 200 * scratch;
    gold[1] += 170 * scratch;
    gold[2] += 110 * scratch;
    const float outerShade = 1.0f - 0.45f * badgeSmoothstep(18.3f, 19.0f, radius);
    const float darkness = (1.0f - 0.78f * hollow) * outerShade;
    colour[0] = gold[0] * darkness;
    colour[1] = gold[1] * darkness;
    colour[2] = gold[2] * darkness;
}

//! \brief Draws a 128x128 HUD badge in the look of a forged dungeon medallion: a blackened iron
//! frame with bronze rivets around a ring and a well. The ring of the heart badge (badge 0)
//! shows the health of the dungeon heart as six emerald gems separated by bronze spokes, its
//! well holds a ruby heart and glows magenta while the heart is under attack. The gold badge
//! (badge 1) is a beaded ring around an embossed coin.
void drawBadgePixels(std::vector<unsigned char>& pixels, int badge, float healthFraction, bool underAttack)
{
    const int badgeSize = BADGE_SIZE;
    pixels.resize(badgeSize * badgeSize * 4);
    for(int y = 0; y < badgeSize; ++y)
    {
        for(int x = 0; x < badgeSize; ++x)
        {
            const float dx = (x + 0.5f) * 64 / badgeSize - 32;
            const float dy = (y + 0.5f) * 64 / badgeSize - 32;
            const float radius = std::sqrt(dx * dx + dy * dy);
            const float light = -(dx + dy) / std::max(1.0f, radius * 1.414214f);
            float colour[3] = {0.0f, 0.0f, 0.0f};
            if(radius > 25.4f)
                badgeFrame(colour, dx, dy, radius, light, badgeNoise(x, y));
            else if(radius > 20.8f)
            {
                if(badge == 0)
                    badgeGemRing(colour, dx, dy, radius, light, HeartHealthRing::isRingLit(dx, dy, healthFraction));
                else
                    badgeBeadRing(colour, dx, dy, radius, light);
            }
            else
            {
                // The well is drawn with 3x3 samples per pixel for soft edges
                float sum[3] = {0.0f, 0.0f, 0.0f};
                for(int sampleY = 0; sampleY < 3; ++sampleY)
                {
                    for(int sampleX = 0; sampleX < 3; ++sampleX)
                    {
                        const float sx = dx + (sampleX - 1) * 64.0f / (3 * badgeSize);
                        const float sy = dy + (sampleY - 1) * 64.0f / (3 * badgeSize);
                        const float sampleRadius = std::sqrt(sx * sx + sy * sy);
                        const float sampleLight = -(sx + sy) / std::max(1.0f, sampleRadius * 1.414214f);
                        float sample[3] = {0.0f, 0.0f, 0.0f};
                        if(badge == 0)
                            badgeHeartWell(sample, sx, sy, sampleRadius, sampleLight, underAttack);
                        else
                            badgeCoinWell(sample, sx, sy, sampleRadius, sampleLight);
                        for(int channel = 0; channel < 3; ++channel)
                            sum[channel] += badgeClamp(sample[channel], 0.0f, 255.0f);
                    }
                }
                for(int channel = 0; channel < 3; ++channel)
                    colour[channel] = sum[channel] / 9.0f;
            }
            const int i = (y * badgeSize + x) * 4;
            for(int channel = 0; channel < 3; ++channel)
                pixels[i + channel] = static_cast<unsigned char>(badgeClamp(colour[channel], 0.0f, 255.0f));
            pixels[i + 3] = static_cast<unsigned char>(badgeClamp(31 - radius, 0.0f, 1.0f) * 255);
        }
    }
}

void createNavigationImages()
{
    createMiniMapCornerImages();
    const int size = 64;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    const float handleStart = 36.0f;
    const float handleEnd = 53.0f;
    const float handleLengthSquared = 2.0f * (handleEnd - handleStart) * (handleEnd - handleStart);
    for(int y = 0; y < size; ++y)
    {
        for(int x = 0; x < size; ++x)
        {
            const float dx = x + 0.5f - 25.0f;
            const float dy = y + 0.5f - 25.0f;
            const float ringDistance = std::abs(std::sqrt(dx * dx + dy * dy) - 15.0f);
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
                std::max(3.0f - ringDistance, 3.0f - handleDistance)));
            const int i = (y * size + x) * 4;
            pixels[i] = 232;
            pixels[i + 1] = 226;
            pixels[i + 2] = 202;
            pixels[i + 3] = static_cast<unsigned char>(coverage * 255.0f);
        }
    }
    CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->createTexture("MapZoom");
    texture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
        "BasicImage", "OpenDungeonsIcons/MapZoom"));
    image.setTexture(&texture);
    image.setArea(CEGUI::Rectf(0, 0, size, size));

    for(int y = 0; y < size; ++y)
    {
        for(int x = 0; x < size; ++x)
        {
            const bool arrowHead = y >= 10 && y <= 28 && std::abs(x - 36) <= y - 10;
            const bool arrowStem = x >= 29 && x <= 43 && y >= 25 && y <= 49;
            const bool returnArm = x >= 11 && x <= 43 && y >= 37 && y <= 49;
            const int i = (y * size + x) * 4;
            pixels[i] = pixels[i + 1] = pixels[i + 2] = 232;
            pixels[i + 3] = arrowHead || arrowStem || returnArm ? 255 : 0;
        }
    }
    CEGUI::Texture& returnTexture = CEGUI::System::getSingleton().getRenderer()->createTexture("MenuReturn");
    returnTexture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
    CEGUI::BasicImage& returnImage = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
        "BasicImage", "OpenDungeonsIcons/MenuReturn"));
    returnImage.setTexture(&returnTexture);
    returnImage.setArea(CEGUI::Rectf(0, 0, size, size));

    const char* categories[] = {"NavigationCreatures", "NavigationRooms", "NavigationSpells", "NavigationWorkshop"};
    for(int category = 0; category < 4; ++category)
    {
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                int coverage = 0;
                for(int sy = 0; sy < 4; ++sy)
                {
                    for(int sx = 0; sx < 4; ++sx)
                    {
                        const float px = x + (sx + 0.5f) * 0.25f;
                        const float py = y + (sy + 0.5f) * 0.25f;
                        std::function<bool(float, float, float, float, float)> line = [&](float ax, float ay, float bx, float by, float radius)
                        {
                            const float dx = bx - ax;
                            const float dy = by - ay;
                            const float t = std::max(0.0f, std::min(1.0f,
                                ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
                            const float ex = px - ax - t * dx;
                            const float ey = py - ay - t * dy;
                            return ex * ex + ey * ey <= radius * radius;
                        };
                        bool inside = false;
                        if(category == 0)
                        {
                            const float dx = px - 32.0f;
                            const float dy = py - 13.0f;
                            inside = dx * dx + dy * dy <= 25.0f
                                || (py >= 21 && py <= 40 && std::abs(dx) <= 9 - (py - 21) * 0.2f)
                                || line(24, 24, 16, 40, 2.5f) || line(40, 24, 48, 40, 2.5f)
                                || line(29, 38, 24, 55, 3) || line(35, 38, 40, 55, 3);
                        }
                        else if(category == 1)
                        {
                            inside = (py >= 9 && py <= 30 && std::abs(px - 32) <= (py - 9) * 1.2f)
                                || (px >= 13 && px <= 51 && py >= 27 && py <= 55);
                            if(px >= 27 && px <= 37 && py >= 39)
                                inside = false;
                        }
                        else if(category == 2)
                        {
                            inside = line(12, 54, 43, 23, 2.5f)
                                || line(44, 9, 44, 16, 1.5f) || line(51, 22, 58, 22, 1.5f)
                                || line(31, 11, 35, 15, 1.5f) || line(51, 14, 55, 10, 1.5f)
                                || line(51, 29, 55, 33, 1.5f) || line(31, 27, 35, 23, 1.5f);
                        }
                        else
                        {
                            inside = line(23, 20, 52, 51, 3)
                                || line(8, 27, 17, 14, 2.5f) || line(17, 14, 30, 9, 2.5f)
                                || line(30, 9, 43, 11, 2.5f) || line(17, 14, 26, 22, 3);
                        }
                        coverage += inside ? 1 : 0;
                    }
                }
                const int i = (y * size + x) * 4;
                pixels[i] = pixels[i + 1] = pixels[i + 2] = static_cast<unsigned char>(244 - y);
                pixels[i + 3] = static_cast<unsigned char>(coverage * 255 / 16);
            }
        }
        CEGUI::Texture& categoryTexture = CEGUI::System::getSingleton().getRenderer()->createTexture(categories[category]);
        categoryTexture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
        CEGUI::BasicImage& categoryImage = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
            "BasicImage", std::string("OpenDungeonsIcons/") + categories[category]));
        categoryImage.setTexture(&categoryTexture);
        categoryImage.setArea(CEGUI::Rectf(0, 0, size, size));
    }

    const char* utilities[] = {"NavigationPanel", "NavigationObjectives", "NavigationMessages"};
    for(int utility = 0; utility < 3; ++utility)
    {
        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                int coverage = 0;
                for(int sy = 0; sy < 4; ++sy)
                {
                    for(int sx = 0; sx < 4; ++sx)
                    {
                        // Utility cells are narrower than the square category cells.
                        const float px = (x + (sx + 0.5f) * 0.25f) * 32.0f / size;
                        const float py = (y + (sy + 0.5f) * 0.25f) * 52.0f / size;
                        std::function<bool(float, float, float, float, float)> line = [&](float ax, float ay, float bx, float by, float radius)
                        {
                            const float dx = bx - ax;
                            const float dy = by - ay;
                            const float t = std::max(0.0f, std::min(1.0f,
                                ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
                            const float ex = px - ax - t * dx;
                            const float ey = py - ay - t * dy;
                            return ex * ex + ey * ey <= radius * radius;
                        };
                        bool inside;
                        if(utility == 0)
                            inside = line(7, 14, 16, 6, 1.5f) || line(16, 6, 25, 14, 1.5f)
                                || line(7, 38, 16, 46, 1.5f) || line(16, 46, 25, 38, 1.5f);
                        else if(utility == 1)
                        {
                            const float dx = (px - 16) / 10;
                            const float dy = std::abs(py - 26);
                            const float lid = 5 * (1 - dx * dx);
                            inside = (std::abs(dx) <= 1 && std::abs(dy - lid) <= 1.1f)
                                || (px - 16) * (px - 16) + dy * dy <= 9;
                        }
                        else
                            inside = (px - 18) * (px - 18) + (py - 14) * (py - 14) <= 4
                                || line(16, 23, 13, 38, 1.5f)
                                || line(12, 23, 17, 23, 1) || line(12, 38, 18, 38, 1);
                        coverage += inside ? 1 : 0;
                    }
                }
                const int i = (y * size + x) * 4;
                pixels[i] = utility == 2 ? 140 : 224;
                pixels[i + 1] = utility == 2 ? 235 : 226;
                pixels[i + 2] = utility == 2 ? 255 : 218;
                pixels[i + 3] = static_cast<unsigned char>(coverage * 255 / 16);
            }
        }
        CEGUI::Texture& utilityTexture = CEGUI::System::getSingleton().getRenderer()->createTexture(utilities[utility]);
        utilityTexture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
        CEGUI::BasicImage& utilityImage = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
            "BasicImage", std::string("OpenDungeonsIcons/") + utilities[utility]));
        utilityImage.setTexture(&utilityTexture);
        utilityImage.setArea(CEGUI::Rectf(0, 0, size, size));
        if(utility == 2)
        {
            for(size_t i = 0; i < pixels.size(); i += 4)
                pixels[i] = pixels[i + 1] = pixels[i + 2] = 224;
            CEGUI::Texture& readTexture = CEGUI::System::getSingleton().getRenderer()->createTexture("NavigationMessagesRead");
            readTexture.loadFromMemory(pixels.data(), CEGUI::Sizef(size, size), CEGUI::Texture::PF_RGBA);
            CEGUI::BasicImage& readImage = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
                "BasicImage", "OpenDungeonsIcons/NavigationMessagesRead"));
            readImage.setTexture(&readTexture);
            readImage.setArea(CEGUI::Rectf(0, 0, size, size));
        }
    }

    const int badgeSize = BADGE_SIZE;
    for(int badge = 0; badge < 2; ++badge)
    {
        drawBadgePixels(pixels, badge, 1.0f, false);
        const std::string name = badge == 0 ? "ManaBadge" : "GoldBadge";
        CEGUI::Texture& badgeTexture = CEGUI::System::getSingleton().getRenderer()->createTexture(name);
        badgeTexture.loadFromMemory(pixels.data(), CEGUI::Sizef(badgeSize, badgeSize), CEGUI::Texture::PF_RGBA);
        CEGUI::BasicImage& badgeImage = static_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingleton().create(
            "BasicImage", "OpenDungeonsIcons/" + name));
        badgeImage.setTexture(&badgeTexture);
        badgeImage.setArea(CEGUI::Rectf(0, 0, badgeSize, badgeSize));
    }
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
    context.getDefaultTooltipObject()->subscribeEvent(CEGUI::Window::EventMoved,
        CEGUI::Event::Subscriber([](const CEGUI::EventArgs& e)
        {
            CEGUI::Tooltip* tooltip = static_cast<CEGUI::Tooltip*>(
                static_cast<const CEGUI::ElementEventArgs&>(e).element);
            const CEGUI::Window* target = tooltip->getTargetWindow();
            const CEGUI::Window* buttons = target == nullptr ? nullptr : target->getParent();
            const bool category = buttons != nullptr && buttons->getName() == "__auto_TabPane__Buttons" &&
                buttons->getParent() != nullptr && buttons->getParent()->getName() == MAIN_TABCONTROL.c_str();
            const CEGUI::Font* font = category ? &CEGUI::FontManager::getSingleton().get("MedievalSharp-13") : nullptr;
            if(tooltip->getFont(false) != font)
            {
                tooltip->setFont(font);
                tooltip->sizeSelf();
            }
            const CEGUI::Rectf bounds = tooltip->getUnclippedOuterRect().get();
            const CEGUI::Sizef screen = tooltip->getRootContainerSize();
            const CEGUI::Vector2f cursor = tooltip->getGUIContext().getMouseCursor().getPosition();
            CEGUI::Rectf hand(cursor, CEGUI::Sizef(0, 0));
            if(RenderManager::getSingletonPtr() != nullptr)
            {
                const Ogre::FloatRect area = RenderManager::getSingleton().getHandCursorBounds(
                    cursor.d_x / screen.d_width, cursor.d_y / screen.d_height);
                hand = CEGUI::Rectf(area.left * screen.d_width, area.top * screen.d_height,
                    area.right * screen.d_width, area.bottom * screen.d_height);
            }
            const float gap = tooltip->getFont()->getFontHeight() * 0.25f;
            float x = hand.right() + gap;
            float y = cursor.d_y - bounds.getHeight() * 0.5f;
            if(x + bounds.getWidth() > screen.d_width)
            {
                x = hand.left() - bounds.getWidth() - gap;
                if(x < 0.0f)
                {
                    x = cursor.d_x;
                    y = hand.top() - bounds.getHeight() - gap;
                    if(y < 0.0f)
                        y = hand.bottom() + gap;
                }
            }
            x = std::max(0.0f, std::min(x, screen.d_width - bounds.getWidth()));
            y = std::max(0.0f, std::min(y, screen.d_height - bounds.getHeight()));
            if(x != bounds.left() || y != bounds.top())
                tooltip->setPosition(CEGUI::UVector2(CEGUI::UDim(0, x), CEGUI::UDim(0, y)));
            return true;
        }));
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
    if(window->isPropertyPresent("NavigationFrame"))
    {
        for(CEGUI::Window* parent = window->getParent(); parent != nullptr; parent = parent->getParent())
        {
            if(parent->isUserStringDefined("NavigationFrame") && parent->getUserString("NavigationFrame") == "true")
            {
                window->setProperty("NavigationFrame", "True");
                break;
            }
        }
    }
    CEGUI::TabButton* tabButton = dynamic_cast<CEGUI::TabButton*>(window);
    if(tabButton != nullptr && window->isPropertyPresent("NavigationColour"))
    {
        CEGUI::Window* page = tabButton->getTargetWindow();
        if(page != nullptr && page->isUserStringDefined("NavigationColour"))
            window->setProperty("NavigationColour", page->getUserString("NavigationColour"));
        if(page != nullptr && page->isUserStringDefined("NavigationImage") && window->isPropertyPresent("NavigationImage"))
            window->setProperty("NavigationImage", page->getUserString("NavigationImage"));
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
    arrangeActionButtons(traps, {"WoodenDoorTrapButton", "CannonButton", "SpikeTrapButton", "BoulderTrapButton"});
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

    const bool large = buttons.size() <= 6 &&
        (20.0f + 106.0f * buttons.size() - 4.0f) * scale <= panel->getPixelSize().d_width;
    const float side = large ? 102.0f : 52.0f;
    for(size_t index = 0; index < buttons.size(); ++index)
    {
        CEGUI::Window* button = buttons[index];
        const float x = 20.0f + (side + 4.0f) * static_cast<float>(large ? index : index / 2);
        const float y = 6.0f + (large ? 0.0f : 56.0f * static_cast<float>(index % 2));
        WindowScaleData& data = mScaledWindows.at(button);
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
        arrangeRoomButtons(gameSheet->second->getChild(TAB_ROOMS));
        arrangeTrapButtons(gameSheet->second->getChild(TAB_TRAPS));
        arrangeSpellButtons(gameSheet->second->getChild(TAB_SPELLS));
    }

    for(const std::pair<CEGUI::Window* const, WindowScaleData>& scaledWindow : mScaledWindows)
    {
        CEGUI::ScrollablePane* pane = dynamic_cast<CEGUI::ScrollablePane*>(scaledWindow.first);
        if(pane == nullptr || !pane->isUserStringDefined("VisibleControlExtent"))
            continue;
        const CEGUI::ScrolledContainer* content = pane->getContentPane();
        const CEGUI::Vector2f origin = content->getUnclippedOuterRect().get().getPosition();
        CEGUI::Rectf extent(0, 0, 0, 0);
        bool firstVisibleControl = true;
        for(size_t i = 0; i < content->getChildCount(); ++i)
        {
            CEGUI::Window* child = content->getChildAtIdx(i);
            if(!child->isVisible())
                continue;
            CEGUI::Rectf area = child->getUnclippedOuterRect().get();
            if(dynamic_cast<CEGUI::Combobox*>(child) != nullptr)
                area.d_max.d_y = child->getChild("__auto_editbox__")->getUnclippedOuterRect().get().bottom();
            if(pane->isUserStringDefined("TrimLeadingSpace"))
                extent.d_min.d_y = firstVisibleControl ? area.top() - origin.d_y :
                    std::min(extent.top(), area.top() - origin.d_y);
            firstVisibleControl = false;
            extent.d_max.d_x = std::max(extent.right(), area.right() - origin.d_x);
            extent.d_max.d_y = std::max(extent.bottom(), area.bottom() - origin.d_y);
        }
        pane->setContentPaneArea(extent);
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

void Gui::updateHeartBadge(float healthFraction, bool underAttack)
{
    std::vector<unsigned char> pixels;
    drawBadgePixels(pixels, 0, healthFraction, underAttack);
    CEGUI::Texture& texture = CEGUI::System::getSingleton().getRenderer()->getTexture("ManaBadge");
    // Write into the existing texture: loadFromMemory of the Ogre renderer makes a new Ogre texture,
    // while the badge window keeps drawing the old one from its cached geometry, so the ring never changed
    texture.blitFromMemory(pixels.data(), CEGUI::Rectf(0.0f, 0.0f, static_cast<float>(BADGE_SIZE),
        static_cast<float>(BADGE_SIZE)));
    CEGUI::System::getSingleton().getDefaultGUIContext().markAsDirty();
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
