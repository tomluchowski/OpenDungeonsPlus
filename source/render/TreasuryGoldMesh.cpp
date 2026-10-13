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

#include <OgreManualObject.h>
#include <OgreMeshManager.h>
#include <OgreSceneManager.h>
#include <OgreVector3.h>

#include <cmath>
#include <vector>

namespace TreasuryGoldMesh
{
namespace
{
const std::string PileMaterial = "TreasuryGoldPile";
const std::string ReducedSuffix = "_r";
// Texture repeats per tile, so the gold grain stays small
const float TextureRepeat = 2.0f;
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

//! Builds the mesh of a pile with the given number of rings and stores it under the resource name.
void buildPileMesh(Ogre::SceneManager* sceneManager, const std::string& resourceName,
    const TreasuryGoldLayer::PileShape& shape, int rings)
{
    Ogre::ManualObject* object = sceneManager->createManualObject();
    object->begin(PileMaterial, Ogre::RenderOperation::OT_TRIANGLE_LIST, "Graphics");

    RoundSurface surface;
    buildRoundSurface(shape, rings, surface);
    for(std::size_t i = 0; i < surface.mPoint.size(); ++i)
    {
        const float u = surface.mPoint[i].x;
        const float v = surface.mPoint[i].y;
        object->position(u - 0.5f, v - 0.5f, surface.mPoint[i].z);
        object->normal(pileNormal(shape, u, v));
        object->textureCoord(u * TextureRepeat, (1.0f - v) * TextureRepeat);
    }
    for(std::size_t i = 0; i < surface.mIndex.size(); i += 3)
        object->triangle(surface.mIndex[i], surface.mIndex[i + 1], surface.mIndex[i + 2]);

    object->end();
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

    // Off keeps the round pile surface, with the coarse mesh
    const bool reduced = (currentDetail != Detail::full) || (farAway && currentDetail == Detail::full);
    const std::string name = reduced ? meshName + ReducedSuffix : meshName;
    if(!Ogre::MeshManager::getSingleton().resourceExists(name + ".mesh", "Graphics"))
        buildPileMesh(sceneManager, name + ".mesh", shape, reduced ? ReducedRings : FullRings);

    return name;
}
}
