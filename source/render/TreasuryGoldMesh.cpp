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

namespace TreasuryGoldMesh
{
namespace
{
const std::string PileMaterial = "TreasuryGoldPile";
const std::string ReducedSuffix = "_r";
// Texture repeats per tile, so the coins stay small
const float TextureRepeat = 2.0f;

Detail currentDetail = Detail::full;

Ogre::Vector3 pilePoint(const TreasuryGoldLayer::PileShape& shape, float u, float v)
{
    return Ogre::Vector3(u - 0.5f, v - 0.5f, TreasuryGoldLayer::heightAt(shape, u, v));
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

void buildPileMesh(Ogre::SceneManager* sceneManager, const std::string& resourceName,
    const TreasuryGoldLayer::PileShape& shape, int divisions)
{
    Ogre::ManualObject* object = sceneManager->createManualObject();
    object->begin(PileMaterial, Ogre::RenderOperation::OT_TRIANGLE_LIST, "Graphics");

    for(int j = 0; j <= divisions; ++j)
    {
        for(int i = 0; i <= divisions; ++i)
        {
            const float u = static_cast<float>(i) / static_cast<float>(divisions);
            const float v = static_cast<float>(j) / static_cast<float>(divisions);
            object->position(pilePoint(shape, u, v));
            object->normal(pileNormal(shape, u, v));
            object->textureCoord(u * TextureRepeat, (1.0f - v) * TextureRepeat);
        }
    }

    const int row = divisions + 1;
    for(int j = 0; j < divisions; ++j)
    {
        for(int i = 0; i < divisions; ++i)
        {
            const int a = j * row + i;
            const int b = a + 1;
            const int c = a + row + 1;
            const int d = a + row;
            // Counter-clockwise seen from above
            object->triangle(a, b, c);
            object->triangle(a, c, d);
        }
    }

    object->end();
    object->convertToMesh(resourceName, "Graphics");
    sceneManager->destroyManualObject(object);
}
}

Detail detailFromString(const std::string& text)
{
    if(text == "reduced")
        return Detail::reduced;
    if(text == "off")
        return Detail::off;
    return Detail::full;
}

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

void setDetail(Detail detail)
{
    currentDetail = detail;
}

Detail getDetail()
{
    return currentDetail;
}

std::string prepareMesh(Ogre::SceneManager* sceneManager, const std::string& meshName)
{
    TreasuryGoldLayer::PileShape shape;
    if(!TreasuryGoldLayer::parseMeshName(meshName, shape))
        return meshName;

    if(currentDetail == Detail::off)
        return TreasuryGoldLayer::classicMeshForLevel(shape.mLevel);

    const bool reduced = (currentDetail == Detail::reduced);
    const std::string name = reduced ? meshName + ReducedSuffix : meshName;
    if(!Ogre::MeshManager::getSingleton().resourceExists(name + ".mesh", "Graphics"))
        buildPileMesh(sceneManager, name + ".mesh", shape, reduced ? 2 : 6);

    return name;
}
}
