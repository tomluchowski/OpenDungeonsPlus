/*
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

#include "render/TreasuryGoldMesh.h"

#include "rooms/TreasuryGoldLayer.h"

#include <OgreColourValue.h>
#include <OgreManualObject.h>
#include <OgreMeshManager.h>
#include <OgreSceneManager.h>
#include <OgreVector3.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace TreasuryGoldMesh
{
namespace
{
const std::string PileMaterial = "TreasuryGoldPile";
const std::string ReducedSuffix = "_r";
// Coins, gems and spilled coins lie in a second section of the pile mesh that takes its colour from the vertices
const std::string DetailMaterial = "TreasuryGoldDetail";
// Texture repeats per tile, so the coins stay small
const float TextureRepeat = 2.0f;
// Corners of a coin lying on top (a round fan) and of a coin spilled at the edge
const int CoinSides = 12;
const int SpillSides = 8;
// Corners of the girdle of a cut gem
const int GemSides = 5;
// A coin: the middle rises by CoinDome, the rim is darker (CoinRimShade) and its normal leans outward (CoinRound)
const float CoinDome = 0.008f;
const float CoinRimShade = 0.7f;
const float CoinRound = 0.7f;
// Size of the coins and gems lying on the gold, in tile units (radius, gem size)
const float TopCoinRadius = 0.06f;
const float ScatterCoinRadius = 0.05f;
const float SpillCoinRadius = 0.047f;
const float GemSize = 0.04f;
// The surface of a pile is a round fan: the middle and rings of PileSectors points out to the foot of the heap.
// The rings only reach as far as the heap, leaving the floor visible beyond its irregular edge.
const int PileSectors = 16;
const int FullRings = 2;
const int ReducedRings = 1;
// The first ring of the full fan lies at about a third of the foot radius (the heap is steepest there)
const float RingSpread = 0.65f;

Detail currentDetail = Detail::full;

//! The round surface of a pile: the points (u, v, height) and the triangles (three indices each)
struct RoundSurface
{
    std::vector<Ogre::Vector3> mPoint;
    std::vector<int> mIndex;
};

//! Two triangles per sector between the points starting at inner and those starting at outer (counter-clockwise
//! seen from above)
void addBand(RoundSurface& surface, int inner, int outer)
{
    for(int sector = 0; sector < PileSectors; ++sector)
    {
        const int next = (sector + 1) % PileSectors;
        surface.mIndex.push_back(inner + sector);
        surface.mIndex.push_back(outer + sector);
        surface.mIndex.push_back(outer + next);
        surface.mIndex.push_back(inner + sector);
        surface.mIndex.push_back(outer + next);
        surface.mIndex.push_back(inner + next);
    }
}

//! Builds the round surface of the pile with the given number of rings: the middle point, a fan to the first ring
//! and a band from ring to ring. The outer ring is the irregular foot of the heap.
void buildRoundSurface(const TreasuryGoldLayer::PileShape& shape, int rings, RoundSurface& surface)
{
    surface.mPoint.clear();
    surface.mIndex.clear();
    surface.mPoint.push_back(Ogre::Vector3(0.5f, 0.5f, TreasuryGoldLayer::heightAt(shape, 0.5f, 0.5f)));
    for(int ring = 1; ring <= rings; ++ring)
    {
        const float along = static_cast<float>(ring) / static_cast<float>(rings);
        const float fraction = 1.0f - std::pow(1.0f - along, RingSpread);
        for(int sector = 0; sector < PileSectors; ++sector)
        {
            const float angle = 6.2831853f * static_cast<float>(sector) / static_cast<float>(PileSectors);
            const float dirU = std::cos(angle);
            const float dirV = std::sin(angle);
            const float reach = fraction * TreasuryGoldLayer::pileRadiusAt(shape, 0.5f + dirU, 0.5f + dirV);
            const float u = 0.5f + dirU * reach;
            const float v = 0.5f + dirV * reach;
            surface.mPoint.push_back(Ogre::Vector3(u, v, TreasuryGoldLayer::heightAt(shape, u, v)));
        }
    }
    // The middle fan and the bands from ring to ring
    for(int sector = 0; sector < PileSectors; ++sector)
    {
        surface.mIndex.push_back(0);
        surface.mIndex.push_back(1 + sector);
        surface.mIndex.push_back(1 + (sector + 1) % PileSectors);
    }
    for(int ring = 1; ring < rings; ++ring)
        addBand(surface, 1 + (ring - 1) * PileSectors, 1 + ring * PileSectors);
}

//! Normal of the pile surface from the neighbouring heights
Ogre::Vector3 pileNormal(const TreasuryGoldLayer::PileShape& shape, float u, float v)
{
    const float step = 0.02f;
    const float dx = TreasuryGoldLayer::heightAt(shape, u + step, v) - TreasuryGoldLayer::heightAt(shape, u - step, v);
    const float dy = TreasuryGoldLayer::heightAt(shape, u, v + step) - TreasuryGoldLayer::heightAt(shape, u, v - step);
    Ogre::Vector3 normal(-dx / (2.0f * step), -dy / (2.0f * step), 1.0f);
    normal.normalise();
    return normal;
}

//! Height of the round surface at (u, v) as the triangles of the mesh have it: the coarse mesh cuts the exact
//! heap a little, and the coins and gems have to lie on the mesh. Falls back to the exact height.
float roundHeightAt(const TreasuryGoldLayer::PileShape& shape, const RoundSurface& surface, float u, float v)
{
    for(std::size_t i = 0; i + 2 < surface.mIndex.size(); i += 3)
    {
        const Ogre::Vector3& a = surface.mPoint[surface.mIndex[i]];
        const Ogre::Vector3& b = surface.mPoint[surface.mIndex[i + 1]];
        const Ogre::Vector3& c = surface.mPoint[surface.mIndex[i + 2]];
        const float denominator = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if(std::fabs(denominator) < 1.0e-9f)
            continue;
        const float weightA = ((b.y - c.y) * (u - c.x) + (c.x - b.x) * (v - c.y)) / denominator;
        const float weightB = ((c.y - a.y) * (u - c.x) + (a.x - c.x) * (v - c.y)) / denominator;
        const float weightC = 1.0f - weightA - weightB;
        if(weightA >= -0.0001f && weightB >= -0.0001f && weightC >= -0.0001f)
            return weightA * a.z + weightB * b.z + weightC * c.z;
    }
    return TreasuryGoldLayer::heightAt(shape, u, v);
}

//! The point on the pile at (u, v) for a coin or gem: on the round mesh when there is one, else on the exact surface
Ogre::Vector3 detailPoint(const TreasuryGoldLayer::PileShape& shape, float u, float v, const RoundSurface* surface)
{
    Ogre::Vector3 point(u - 0.5f, v - 0.5f, TreasuryGoldLayer::heightAt(shape, u, v));
    if(surface != nullptr)
        point.z = roundHeightAt(shape, *surface, u, v);
    return point;
}

//! \brief The colour of one coin: a gold tone that varies a little with its position, so neighbouring coins differ.
//! \param a,b Position of the coin in the tile (0 to 1), used as the seed of the variation.
Ogre::ColourValue goldColour(float a, float b)
{
    const float shade = 0.8f + 0.3f * TreasuryGoldLayer::hash01(static_cast<int>(a * 1000.0f),
        static_cast<int>(b * 1000.0f));
    return Ogre::ColourValue(std::min(1.0f, shade), std::min(1.0f, 0.8f * shade), 0.3f * shade, 1.0f);
}

//! A flat disc (a fan of the given number of sides) lying on the surface. The middle sits a little higher and
//! is lighter, the rim is rounded away and darker, so a coin reads as a coin and not as a flat patch.
void addCoin(Ogre::ManualObject* object, const Ogre::Vector3& centre, const Ogre::Vector3& up, float radius,
    int sides, const Ogre::ColourValue& colour)
{
    Ogre::Vector3 axisA = up.crossProduct(Ogre::Vector3::UNIT_Y);
    if(axisA.squaredLength() < 0.0001f)
        axisA = up.crossProduct(Ogre::Vector3::UNIT_X);
    axisA.normalise();
    const Ogre::Vector3 axisB = up.crossProduct(axisA);
    const Ogre::ColourValue rimColour(colour.r * CoinRimShade, colour.g * CoinRimShade, colour.b * CoinRimShade,
        1.0f);

    const int baseIndex = static_cast<int>(object->getCurrentVertexCount());
    object->position(centre + up * CoinDome);
    object->normal(up);
    object->textureCoord(0.0f, 0.0f);
    object->colour(colour);
    for(int i = 0; i < sides; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        const Ogre::Vector3 radial = axisA * std::cos(angle) + axisB * std::sin(angle);
        Ogre::Vector3 rimNormal = up + radial * CoinRound;
        rimNormal.normalise();
        object->position(centre + radial * radius);
        object->normal(rimNormal);
        object->textureCoord(0.0f, 0.0f);
        object->colour(rimColour);
    }
    for(int i = 0; i < sides; ++i)
        object->triangle(baseIndex, baseIndex + 1 + i, baseIndex + 1 + (i + 1) % sides);
}

//! A small cut gem: a flat table on top, a crown down to the girdle and a pavilion down to a point
void addGem(Ogre::ManualObject* object, const Ogre::Vector3& centre, float size, const Ogre::ColourValue& colour)
{
    const float tableRadius = 0.55f;
    const float tableHeight = 0.7f;
    const float girdleHeight = 0.25f;
    const float tipHeight = -1.0f;
    const int baseIndex = static_cast<int>(object->getCurrentVertexCount());
    // Vertices: table middle, table ring, girdle ring, tip
    std::vector<Ogre::Vector3> corners;
    corners.push_back(Ogre::Vector3(0.0f, 0.0f, tableHeight));
    for(int i = 0; i < GemSides; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(GemSides);
        corners.push_back(Ogre::Vector3(tableRadius * std::cos(angle), tableRadius * std::sin(angle), tableHeight));
    }
    for(int i = 0; i < GemSides; ++i)
    {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(GemSides);
        corners.push_back(Ogre::Vector3(std::cos(angle), std::sin(angle), girdleHeight));
    }
    corners.push_back(Ogre::Vector3(0.0f, 0.0f, tipHeight));
    for(std::size_t i = 0; i < corners.size(); ++i)
    {
        Ogre::Vector3 normal = corners[i];
        if(i == 0)
            normal = Ogre::Vector3::UNIT_Z;
        normal.normalise();
        object->position(centre + corners[i] * size);
        object->normal(normal);
        object->textureCoord(0.0f, 0.0f);
        object->colour(colour);
    }
    // Counter-clockwise seen from outside: the table, the crown, the pavilion
    const int table = baseIndex + 1;
    const int girdle = baseIndex + 1 + GemSides;
    const int tip = baseIndex + 1 + 2 * GemSides;
    for(int i = 0; i < GemSides; ++i)
    {
        const int next = (i + 1) % GemSides;
        object->triangle(baseIndex, table + i, table + next);
        object->triangle(table + i, girdle + i, girdle + next);
        object->triangle(table + i, girdle + next, table + next);
        object->triangle(tip, girdle + next, girdle + i);
    }
}

//! \brief The colour of one gem: red, green, blue or purple, chosen from the pile variant and the index of the gem.
//! \param variant Variant of the pile shape.
//! \param index Number of the gem in the pile.
Ogre::ColourValue gemColour(int variant, int index)
{
    switch((variant + index) % 4)
    {
        case 0:
            return Ogre::ColourValue(0.9f, 0.1f, 0.15f, 1.0f);
        case 1:
            return Ogre::ColourValue(0.15f, 0.8f, 0.3f, 1.0f);
        case 2:
            return Ogre::ColourValue(0.2f, 0.4f, 0.95f, 1.0f);
        default:
            return Ogre::ColourValue(0.7f, 0.3f, 0.9f, 1.0f);
    }
}

//! Whether the shape carries any coins or gems (the detail section is left out when it does not)
bool hasDetail(const TreasuryGoldLayer::PileShape& shape)
{
    if(shape.mLevel == 0)
        return true;
    return TreasuryGoldLayer::topCoinCount(shape) > 0 || TreasuryGoldLayer::gemCount(shape) > 0
        || TreasuryGoldLayer::spillCoinsPerEdge(shape) > 0;
}

//! Single coins on top of the heap, scattered gems, coins spilled at the open edges; for a tile without
//! gold only a few coins on the bare floor
void addDetail(Ogre::ManualObject* object, const TreasuryGoldLayer::PileShape& shape,
    const RoundSurface* surface = nullptr)
{
    const int seed = shape.mVariant * 17 + shape.mLevel;
    if(shape.mLevel == 0)
    {
        for(int i = 0; i < TreasuryGoldLayer::scatterCoins; ++i)
        {
            const float u = 0.15f + 0.7f * TreasuryGoldLayer::hash01(seed, 31 * i + 1);
            const float v = 0.15f + 0.7f * TreasuryGoldLayer::hash01(seed, 31 * i + 2);
            addCoin(object, Ogre::Vector3(u - 0.5f, v - 0.5f, 0.008f), Ogre::Vector3::UNIT_Z, ScatterCoinRadius, CoinSides,
                goldColour(u, v));
        }
        return;
    }

    for(int i = 0; i < TreasuryGoldLayer::topCoinCount(shape); ++i)
    {
        // On the heap: inside 85 % of its radius
        const float reach = 0.85f * 0.92f * TreasuryGoldLayer::pileRadius(shape.mLevel)
            * std::sqrt(TreasuryGoldLayer::hash01(seed, 7 * i + 3));
        const float turn = 6.2831853f * TreasuryGoldLayer::hash01(seed, 7 * i + 4);
        const float u = 0.5f + reach * std::cos(turn);
        const float v = 0.5f + reach * std::sin(turn);
        Ogre::Vector3 up = pileNormal(shape, u, v);
        // Each coin leans a little differently
        const float lean = 6.2831853f * TreasuryGoldLayer::hash01(seed, 7 * i + 5);
        up += Ogre::Vector3(std::cos(lean), std::sin(lean), 0.0f) * 0.35f;
        up.normalise();
        Ogre::Vector3 centre = detailPoint(shape, u, v, surface);
        centre.z += 0.014f;
        addCoin(object, centre, up, TopCoinRadius, CoinSides, goldColour(u, v));
    }

    for(int i = 0; i < TreasuryGoldLayer::gemCount(shape); ++i)
    {
        const float reach = 0.6f * 0.92f * TreasuryGoldLayer::pileRadius(shape.mLevel)
            * std::sqrt(TreasuryGoldLayer::hash01(seed, 11 * i + 8));
        const float turn = 6.2831853f * TreasuryGoldLayer::hash01(seed, 11 * i + 9);
        const float u = 0.5f + reach * std::cos(turn);
        const float v = 0.5f + reach * std::sin(turn);
        Ogre::Vector3 centre = detailPoint(shape, u, v, surface);
        centre.z += 0.02f;
        addGem(object, centre, GemSize, gemColour(shape.mVariant, i));
    }

    // Overflow: coins that rolled down to the foot of the pile at the open edges
    const int perEdge = TreasuryGoldLayer::spillCoinsPerEdge(shape);
    for(int edge = 0; edge < 4; ++edge)
    {
        if(perEdge == 0 || !TreasuryGoldLayer::edgeOpen(shape, edge))
            continue;
        for(int i = 0; i < perEdge; ++i)
        {
            const float along = (i == 0) ? 0.3f + 0.1f * TreasuryGoldLayer::hash01(seed, edge) :
                0.6f + 0.1f * TreasuryGoldLayer::hash01(seed, edge + 4);
            const float across = 0.92f;
            float u = along;
            float v = along;
            if(edge == 0)
                v = across;
            else if(edge == 1)
                u = across;
            else if(edge == 2)
                v = 1.0f - across;
            else
                u = 1.0f - across;
            Ogre::Vector3 centre = detailPoint(shape, u, v, surface);
            centre.z += 0.01f;
            addCoin(object, centre, pileNormal(shape, u, v), SpillCoinRadius, SpillSides, goldColour(u, v));
        }
    }
}

//! Fills the sections of a pile: the round surface (none for a tile without gold, which only has scattered coins)
//! and the coins and gems.
void fillPile(Ogre::ManualObject* object, const TreasuryGoldLayer::PileShape& shape, int rings, bool withDetail)
{
    RoundSurface roundSurface;
    if(shape.mLevel > 0)
    {
        object->begin(PileMaterial, Ogre::RenderOperation::OT_TRIANGLE_LIST, "Graphics");
        buildRoundSurface(shape, rings, roundSurface);
        for(std::size_t i = 0; i < roundSurface.mPoint.size(); ++i)
        {
            const float u = roundSurface.mPoint[i].x;
            const float v = roundSurface.mPoint[i].y;
            object->position(u - 0.5f, v - 0.5f, roundSurface.mPoint[i].z);
            object->normal(pileNormal(shape, u, v));
            object->textureCoord(u * TextureRepeat, (1.0f - v) * TextureRepeat);
        }
        for(std::size_t i = 0; i < roundSurface.mIndex.size(); i += 3)
            object->triangle(roundSurface.mIndex[i], roundSurface.mIndex[i + 1], roundSurface.mIndex[i + 2]);
        object->end();
    }

    if(withDetail && hasDetail(shape))
    {
        object->begin(DetailMaterial, Ogre::RenderOperation::OT_TRIANGLE_LIST, "Graphics");
        addDetail(object, shape, shape.mLevel > 0 ? &roundSurface : nullptr);
        object->end();
    }
}

//! Builds the mesh of a pile with the given number of rings and stores it under the resource name.
void buildPileMesh(Ogre::SceneManager* sceneManager, const std::string& resourceName,
    const TreasuryGoldLayer::PileShape& shape, int rings, bool withDetail)
{
    Ogre::ManualObject* object = sceneManager->createManualObject();
    fillPile(object, shape, rings, withDetail);
    object->convertToMesh(resourceName, "Graphics");
    sceneManager->destroyManualObject(object);
}
}

//! \brief Reads the detail option text ("full", "reduced" or "off"); anything else means full.
Detail detailFromString(const std::string& text)
{
    if(text == "reduced")
        return Detail::reduced;
    if(text == "off")
        return Detail::off;
    return Detail::full;
}

//! \brief The option text of the detail ("full", "reduced" or "off").
const char* detailToString(Detail detail)
{
    switch(detail)
    {
        case Detail::reduced:
            return "reduced";
        case Detail::off:
            return "off";
        default:
            return "full";
    }
}

//! \brief Sets how much of the gold layer is drawn; the piles are built with this detail from now on.
void setDetail(Detail detail)
{
    currentDetail = detail;
}

//! \brief The detail the gold layer is drawn with at the moment.
Detail getDetail()
{
    return currentDetail;
}

std::string prepareMesh(Ogre::SceneManager* sceneManager, const std::string& meshName, bool farAway)
{
    TreasuryGoldLayer::PileShape shape;
    if(!TreasuryGoldLayer::parseMeshName(meshName, shape))
        return meshName;

    // Without gold there is nothing but a few coins on the floor, and those only at the full detail
    if(shape.mLevel == 0 && (currentDetail != Detail::full || farAway))
        return std::string();

    // Off keeps the round pile surface, without decorative coins or gems.
    const bool reduced = (currentDetail != Detail::full) || (farAway && currentDetail == Detail::full);
    const std::string name = reduced ? meshName + ReducedSuffix : meshName;
    if(!Ogre::MeshManager::getSingleton().resourceExists(name + ".mesh", "Graphics"))
        buildPileMesh(sceneManager, name + ".mesh", shape, reduced ? ReducedRings : FullRings, !reduced);

    return name;
}
}
