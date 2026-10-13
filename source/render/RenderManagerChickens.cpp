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

// Looks and motion of the hatchery animals on the client: eggs in straw, chicks, the rooster, and the
// nest or loose feathers next to the coops. Everything is procedural: the chicken mesh is scaled and tinted,
// the egg, straw, comb and tail are meshes made in code, the motion comes from a pose name that the server
// sends as animation name (see ChickenPose.h). Nothing here changes the game state.

#include "render/RenderManager.h"

#include "entities/BuildingObject.h"
#include "entities/ChickenEntity.h"
#include "entities/ChickenPose.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "rooms/Room.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreEntity.h>
#include <OgreManualObject.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreParticleSystem.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSubEntity.h>
#include <OgreTechnique.h>

#include <algorithm>
#include <cmath>

namespace
{
const std::string MeshEgg = "ChickenEgg";
const std::string MeshComb = "ChickenRoosterComb";
const std::string MeshTail = "ChickenRoosterTail";
const std::string MeshNest = "ChickenCoopNest";
const std::string MeshFeathers = "ChickenLooseFeathers";
const std::string MaterialGroup = "Graphics";

//! Small deterministic random numbers so that the meshes look the same every time
class ShapeRandom
{
public:
    ShapeRandom(uint32_t seed) :
        mState(seed)
    {}

    //! A number in [0, 1)
    float next()
    {
        mState = mState * 1664525u + 1013904223u;
        return static_cast<float>((mState >> 8) & 0xFFFF) / 65536.0f;
    }

    //! A number in [-1, 1)
    float nextSigned()
    { return next() * 2.0f - 1.0f; }

private:
    uint32_t mState;
};

void addQuad(Ogre::ManualObject* object, uint32_t& index, const Ogre::Vector3& p0, const Ogre::Vector3& p1,
    const Ogre::Vector3& p2, const Ogre::Vector3& p3, const Ogre::Vector3& normal)
{
    object->position(p0);
    object->normal(normal);
    object->textureCoord(0.0f, 0.0f);
    object->position(p1);
    object->normal(normal);
    object->textureCoord(1.0f, 0.0f);
    object->position(p2);
    object->normal(normal);
    object->textureCoord(1.0f, 1.0f);
    object->position(p3);
    object->normal(normal);
    object->textureCoord(0.0f, 1.0f);
    object->triangle(index, index + 1, index + 2);
    object->triangle(index, index + 2, index + 3);
    index += 4;
}

//! An egg standing on base: wide at the bottom, narrower at the top
void addEgg(Ogre::ManualObject* object, uint32_t& index, const Ogre::Vector3& base, float radius, float height)
{
    const int rings = 8;
    const int segments = 12;
    const uint32_t first = index;
    const float centerZ = base.z + height * 0.5f;
    for(int ring = 0; ring <= rings; ++ring)
    {
        const float u = Ogre::Math::PI * static_cast<float>(ring) / rings;
        const float z = base.z + height * (0.5f - 0.5f * std::cos(u));
        const float rad = radius * std::sin(u) * (1.0f + 0.12f * std::cos(u));
        for(int seg = 0; seg < segments; ++seg)
        {
            const float a = Ogre::Math::TWO_PI * static_cast<float>(seg) / segments;
            const float x = base.x + rad * std::cos(a);
            const float y = base.y + rad * std::sin(a);
            Ogre::Vector3 normal((x - base.x) / (radius * radius), (y - base.y) / (radius * radius),
                (z - centerZ) / (height * height * 0.25f));
            normal.normalise();
            object->position(x, y, z);
            object->normal(normal);
            object->textureCoord(static_cast<float>(seg) / segments, static_cast<float>(ring) / rings);
            ++index;
        }
    }
    for(int ring = 0; ring < rings; ++ring)
    {
        for(int seg = 0; seg < segments; ++seg)
        {
            const uint32_t a = first + ring * segments + seg;
            const uint32_t b = first + ring * segments + (seg + 1) % segments;
            const uint32_t c = a + segments;
            const uint32_t d = b + segments;
            object->triangle(a, c, b);
            object->triangle(b, c, d);
        }
    }
}

//! A nest of loose straw blades lying around a point
void addStraw(Ogre::ManualObject* object, uint32_t& index, const Ogre::Vector3& center, float innerRadius,
    float outerRadius, int count, uint32_t seed)
{
    ShapeRandom random(seed);
    for(int i = 0; i < count; ++i)
    {
        const float angle = Ogre::Math::TWO_PI * random.next();
        const float start = innerRadius + (outerRadius - innerRadius) * 0.5f * random.next();
        const float length = (outerRadius - start) * (0.6f + 0.4f * random.next());
        const float turn = angle + 0.7f * random.nextSigned();
        const Ogre::Vector3 from(center.x + start * std::cos(angle), center.y + start * std::sin(angle),
            center.z + 0.006f + 0.014f * random.next());
        const Ogre::Vector3 direction(std::cos(turn), std::sin(turn), -0.15f * random.next());
        const Ogre::Vector3 to = from + direction * length;
        const Ogre::Vector3 side(-direction.y, direction.x, 0.0f);
        const Ogre::Vector3 half = side * 0.0045f;
        addQuad(object, index, from - half, from + half, to + half, to - half, Ogre::Vector3::UNIT_Z);
    }
}

Ogre::ManualObject* beginShape(Ogre::SceneManager* sceneManager, const std::string& material)
{
    Ogre::ManualObject* object = sceneManager->createManualObject();
    object->begin(material, Ogre::RenderOperation::OT_TRIANGLE_LIST, MaterialGroup);
    return object;
}

void finishShape(Ogre::SceneManager* sceneManager, Ogre::ManualObject* object, const std::string& meshName)
{
    object->convertToMesh(meshName + ".mesh", MaterialGroup);
    sceneManager->destroyManualObject(object);
}

void buildEgg(Ogre::SceneManager* sceneManager)
{
    Ogre::ManualObject* object = beginShape(sceneManager, "ChickenStraw");
    uint32_t index = 0;
    addStraw(object, index, Ogre::Vector3(0.0f, 0.0f, 0.0f), 0.02f, 0.095f, 26, 11u);
    object->end();
    object->begin("ChickenEgg", Ogre::RenderOperation::OT_TRIANGLE_LIST, MaterialGroup);
    index = 0;
    addEgg(object, index, Ogre::Vector3(0.0f, 0.0f, 0.012f), 0.03f, 0.08f);
    object->end();
    finishShape(sceneManager, object, MeshEgg);
}

void buildNest(Ogre::SceneManager* sceneManager)
{
    Ogre::ManualObject* object = beginShape(sceneManager, "ChickenStraw");
    uint32_t index = 0;
    addStraw(object, index, Ogre::Vector3(0.0f, 0.0f, 0.0f), 0.03f, 0.17f, 46, 23u);
    object->end();
    object->begin("ChickenEgg", Ogre::RenderOperation::OT_TRIANGLE_LIST, MaterialGroup);
    index = 0;
    addEgg(object, index, Ogre::Vector3(-0.045f, -0.03f, 0.012f), 0.03f, 0.08f);
    addEgg(object, index, Ogre::Vector3(0.05f, -0.035f, 0.012f), 0.03f, 0.08f);
    addEgg(object, index, Ogre::Vector3(0.0f, 0.05f, 0.012f), 0.03f, 0.08f);
    object->end();
    finishShape(sceneManager, object, MeshNest);
}

void buildFeathers(Ogre::SceneManager* sceneManager)
{
    Ogre::ManualObject* object = beginShape(sceneManager, "ChickenFeatherDecor");
    uint32_t index = 0;
    ShapeRandom random(5u);
    for(int i = 0; i < 7; ++i)
    {
        const Ogre::Vector3 center(0.3f * random.nextSigned(), 0.3f * random.nextSigned(), 0.004f);
        const float angle = Ogre::Math::TWO_PI * random.next();
        const Ogre::Vector3 direction(std::cos(angle), std::sin(angle), 0.0f);
        const Ogre::Vector3 side(-direction.y, direction.x, 0.0f);
        // A feather is a slim leaf of three pieces that is a little bent
        Ogre::Vector3 previous = center - direction * 0.05f;
        float previousWidth = 0.0f;
        for(int part = 1; part <= 3; ++part)
        {
            const float t = static_cast<float>(part) / 3.0f;
            const float width = 0.012f * std::sin(Ogre::Math::PI * (0.15f + 0.75f * t));
            const Ogre::Vector3 next = center - direction * 0.05f + direction * 0.1f * t + side * 0.01f * t * t;
            addQuad(object, index, previous - side * previousWidth, previous + side * previousWidth,
                next + side * width, next - side * width, Ogre::Vector3::UNIT_Z);
            previous = next;
            previousWidth = width;
        }
    }
    object->end();
    finishShape(sceneManager, object, MeshFeathers);
}

//! A flat shape in the y-z plane with some thickness along x
void addComb(Ogre::ManualObject* object, uint32_t& index)
{
    // Outline of the comb (y, z), the head of the chicken looks along -y
    static const float outline[][2] = {
        {-0.150f, 0.165f}, {-0.141f, 0.198f}, {-0.130f, 0.172f}, {-0.120f, 0.207f},
        {-0.109f, 0.172f}, {-0.099f, 0.195f}, {-0.090f, 0.163f}};
    const int count = sizeof(outline) / sizeof(outline[0]);
    const float half = 0.007f;
    const float centerY = -0.12f;
    const float centerZ = 0.166f;
    for(int side = 0; side < 2; ++side)
    {
        const float x = side == 0 ? -half : half;
        const Ogre::Vector3 normal(side == 0 ? -1.0f : 1.0f, 0.0f, 0.0f);
        for(int i = 0; i + 1 < count; ++i)
        {
            object->position(x, centerY, centerZ);
            object->normal(normal);
            object->textureCoord(0.5f, 0.5f);
            object->position(x, outline[i][0], outline[i][1]);
            object->normal(normal);
            object->textureCoord(0.0f, 0.0f);
            object->position(x, outline[i + 1][0], outline[i + 1][1]);
            object->normal(normal);
            object->textureCoord(1.0f, 0.0f);
            object->triangle(index, index + 1, index + 2);
            index += 3;
        }
    }
    // The upper edge
    for(int i = 0; i + 1 < count; ++i)
    {
        Ogre::Vector3 edge(0.0f, outline[i + 1][0] - outline[i][0], outline[i + 1][1] - outline[i][1]);
        Ogre::Vector3 normal(0.0f, -edge.z, edge.y);
        normal.normalise();
        addQuad(object, index, Ogre::Vector3(-half, outline[i][0], outline[i][1]),
            Ogre::Vector3(half, outline[i][0], outline[i][1]), Ogre::Vector3(half, outline[i + 1][0], outline[i + 1][1]),
            Ogre::Vector3(-half, outline[i + 1][0], outline[i + 1][1]), normal);
    }
}

void buildComb(Ogre::SceneManager* sceneManager)
{
    Ogre::ManualObject* object = beginShape(sceneManager, "ChickenRoosterComb");
    uint32_t index = 0;
    addComb(object, index);
    object->end();
    finishShape(sceneManager, object, MeshComb);
}

//! Long curved tail feathers that stand up behind the body
void buildTail(Ogre::SceneManager* sceneManager)
{
    Ogre::ManualObject* object = beginShape(sceneManager, "ChickenRoosterTail");
    uint32_t index = 0;
    static const float spread[] = {-0.045f, -0.015f, 0.015f, 0.045f};
    static const float lift[] = {0.19f, 0.25f, 0.25f, 0.19f};
    for(int feather = 0; feather < 4; ++feather)
    {
        Ogre::Vector3 previous(0.0f, 0.12f, 0.1f);
        float previousWidth = 0.008f;
        for(int part = 1; part <= 4; ++part)
        {
            const float t = static_cast<float>(part) / 4.0f;
            const Ogre::Vector3 next(spread[feather] * t, 0.12f + 0.11f * t + 0.05f * t * t,
                0.1f + (lift[feather] - 0.1f) * std::sin(t * 1.35f) / std::sin(1.35f));
            const float width = 0.012f * (1.0f - 0.55f * t);
            const Ogre::Vector3 side(1.0f, 0.0f, 0.0f);
            addQuad(object, index, previous - side * previousWidth, previous + side * previousWidth,
                next + side * width, next - side * width, Ogre::Vector3(0.0f, 1.0f, 0.2f).normalisedCopy());
            previous = next;
            previousWidth = width;
        }
    }
    object->end();
    finishShape(sceneManager, object, MeshTail);
}

void ensureMesh(Ogre::SceneManager* sceneManager, const std::string& name, void (*build)(Ogre::SceneManager*))
{
    if(Ogre::MeshManager::getSingleton().resourceExists(name + ".mesh", MaterialGroup))
        return;

    build(sceneManager);
}

float configValue(const std::string& key, float defaultValue)
{
    return static_cast<float>(ConfigManager::getSingleton().getRoomConfigDoubleOrDefault(key, defaultValue));
}

//! Scale of the animal by kind
float kindScale(ChickenKind kind)
{
    switch(kind)
    {
        case ChickenKind::chick:
            return configValue("HatcheryChickScale", 0.5f);
        case ChickenKind::rooster:
            return configValue("HatcheryRoosterScale", 1.4f);
        default:
            return 1.0f;
    }
}

//! Tint of the animal by kind, the white of a hen is the texture itself
Ogre::ColourValue kindTint(ChickenKind kind)
{
    switch(kind)
    {
        case ChickenKind::chick:
            return Ogre::ColourValue(configValue("HatcheryChickTintR", 1.5f), configValue("HatcheryChickTintG", 1.35f),
                configValue("HatcheryChickTintB", 0.45f), 1.0f);
        case ChickenKind::rooster:
            return Ogre::ColourValue(configValue("HatcheryRoosterTintR", 1.3f), configValue("HatcheryRoosterTintG", 0.7f),
                configValue("HatcheryRoosterTintB", 0.5f), 1.0f);
        default:
            return Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

const std::string TintChick = "_Chick";
const std::string TintRooster = "_Rooster";

std::string stripTintSuffix(const std::string& name)
{
    for(const std::string* suffix : {&TintChick, &TintRooster})
    {
        if(name.size() > suffix->size() && name.compare(name.size() - suffix->size(), suffix->size(), *suffix) == 0)
            return name.substr(0, name.size() - suffix->size());
    }
    return name;
}

//! Name of the material for a kind: the original material, or a copy multiplied by the tint
std::string tintedMaterial(const std::string& baseName, ChickenKind kind, const std::string& group)
{
    if((kind != ChickenKind::chick) && (kind != ChickenKind::rooster))
        return baseName;

    const std::string name = baseName + (kind == ChickenKind::chick ? TintChick : TintRooster);
    Ogre::MaterialManager& materials = Ogre::MaterialManager::getSingleton();
    if(materials.resourceExists(name, group))
        return name;

    Ogre::MaterialPtr original = materials.getByName(baseName, group);
    if(original.isNull())
        return baseName;

    Ogre::MaterialPtr copy = original->clone(name);
    const Ogre::ColourValue tint = kindTint(kind);
    for(unsigned short t = 0; t < copy->getNumTechniques(); ++t)
    {
        Ogre::Technique* technique = copy->getTechnique(t);
        for(unsigned short p = 0; p < technique->getNumPasses(); ++p)
        {
            Ogre::Pass* pass = technique->getPass(p);
            pass->setAmbient(pass->getAmbient() * tint);
            pass->setDiffuse(pass->getDiffuse() * tint);
        }
    }
    return name;
}
} // namespace

void RenderManager::rrEnsureChickenMesh(const std::string& meshName)
{
    if(meshName == MeshEgg)
        ensureMesh(mSceneManager, MeshEgg, buildEgg);
    else if(meshName == MeshComb)
        ensureMesh(mSceneManager, MeshComb, buildComb);
    else if(meshName == MeshTail)
        ensureMesh(mSceneManager, MeshTail, buildTail);
    else if(meshName == MeshNest)
        ensureMesh(mSceneManager, MeshNest, buildNest);
    else if(meshName == MeshFeathers)
        ensureMesh(mSceneManager, MeshFeathers, buildFeathers);
}

void RenderManager::rrCreateChickenLook(ChickenEntity* chicken)
{
    Ogre::SceneNode* node = chicken->getEntityNode();
    const std::string entityName = chicken->getOgreNamePrefix() + chicken->getName();
    if((node == nullptr) || !mSceneManager->hasEntity(entityName))
        return;

    rrDestroyChickenLook(chicken);

    // The entity moves to a child node: the parent follows the server (position, turning), the child
    // carries the scale and the little procedural motions
    ChickenLook look;
    Ogre::Entity* entity = mSceneManager->getEntity(entityName);
    look.mNode = node->createChildSceneNode(entityName + "_look");
    node->detachObject(entity);
    look.mNode->attachObject(entity);
    look.mEntity = entity;
    look.mTime = 0.0f;
    look.mPoseTime = 0.0f;
    look.mPhase = static_cast<Ogre::Real>(++mChickenLookNumber % 97) * 0.37f;
    look.mFeatherBursts = 0;
    mChickenLooks[chicken] = look;
    applyChickenKindLook(chicken);
}

void RenderManager::rrDestroyChickenLook(ChickenEntity* chicken)
{
    std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end())
        return;

    ChickenLook& look = it->second;
    for(Ogre::Entity* accessory : look.mAccessories)
    {
        look.mNode->detachObject(accessory);
        mSceneManager->destroyEntity(accessory);
    }

    // The entity goes back to the node of the animal, which destroys it
    Ogre::SceneNode* node = chicken->getEntityNode();
    look.mNode->detachObject(look.mEntity);
    if(node != nullptr)
    {
        node->attachObject(look.mEntity);
        node->removeAndDestroyChild(look.mNode->getName());
    }
    mChickenLooks.erase(it);
}

void RenderManager::clearChickenLooks()
{
    mChickenLooks.clear();
    mCoopDecors.clear();
}

void RenderManager::applyChickenKindLook(ChickenEntity* chicken)
{
    std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end())
        return;

    ChickenLook& look = it->second;
    const ChickenKind kind = chicken->getKind();
    for(unsigned int sub = 0; sub < look.mEntity->getNumSubEntities(); ++sub)
    {
        Ogre::SubEntity* part = look.mEntity->getSubEntity(sub);
        const std::string baseName = stripTintSuffix(part->getMaterialName());
        const std::string name = tintedMaterial(baseName, kind, part->getMaterial()->getGroup());
        if(name != part->getMaterialName())
            part->setMaterialName(name, part->getMaterial()->getGroup());
    }

    // The rooster has a red comb and long tail feathers
    for(Ogre::Entity* accessory : look.mAccessories)
    {
        look.mNode->detachObject(accessory);
        mSceneManager->destroyEntity(accessory);
    }
    look.mAccessories.clear();
    if(kind == ChickenKind::rooster)
    {
        for(const std::string* meshName : {&MeshComb, &MeshTail})
        {
            rrEnsureChickenMesh(*meshName);
            Ogre::Entity* accessory = mSceneManager->createEntity(
                look.mNode->getName() + "_" + *meshName, *meshName + ".mesh");
            accessory->setQueryFlags(0);
            look.mNode->attachObject(accessory);
            look.mAccessories.push_back(accessory);
        }
    }
    look.mNode->setScale(Ogre::Vector3::UNIT_SCALE * kindScale(kind));
}

void RenderManager::rrUpdateChickenLook(ChickenEntity* chicken)
{
    applyChickenKindLook(chicken);
}

void RenderManager::rrChickenHatched(ChickenEntity* chicken)
{
    createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.05f), "ChickenEggShell");
}

void RenderManager::rrSetChickenPose(ChickenEntity* chicken, const std::string& pose)
{
    std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end())
        return;

    ChickenLook& look = it->second;
    if(look.mPose == pose)
        return;

    look.mPose = pose;
    look.mPoseTime = 0.0f;
    look.mFeatherBursts = 0;
    const Ogre::Vector3 position = chicken->getPosition();

    if(pose == ChickenPose::mount)
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.2f));
    else if(pose == ChickenPose::cackle)
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.12f));
    else if(pose == ChickenPose::emerge)
    {
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.1f));

        // The door of the closest coop swings
        std::map<BuildingObject*, CoopDecor>::iterator nearest = mCoopDecors.end();
        Ogre::Real nearestDistance = 4.0f;
        for(std::map<BuildingObject*, CoopDecor>::iterator coop = mCoopDecors.begin(); coop != mCoopDecors.end(); ++coop)
        {
            Ogre::Real distance = coop->first->getPosition().squaredDistance(position);
            if(distance < nearestDistance)
            {
                nearest = coop;
                nearestDistance = distance;
            }
        }
        if(nearest != mCoopDecors.end())
            nearest->second.mShake = 0.8f;
    }
}

void RenderManager::rrCreateCoopDecor(BuildingObject* coop)
{
    Ogre::SceneNode* node = coop->getEntityNode();
    if((node == nullptr) || (mCoopDecors.count(coop) > 0))
        return;

    const std::string name = coop->getOgreNamePrefix() + coop->getName() + "_decor";
    CoopDecor decor;
    decor.mShake = 0.0f;
    decor.mNode = node->createChildSceneNode(name + "_node", Ogre::Vector3(0.9f, 0.0f, 0.0f));
    rrEnsureChickenMesh(MeshNest);
    rrEnsureChickenMesh(MeshFeathers);
    decor.mNest = mSceneManager->createEntity(name + "_nest", MeshNest + ".mesh");
    decor.mFeathers = mSceneManager->createEntity(name + "_feathers", MeshFeathers + ".mesh");
    decor.mNest->setQueryFlags(0);
    decor.mFeathers->setQueryFlags(0);
    decor.mNode->attachObject(decor.mNest);
    decor.mNode->attachObject(decor.mFeathers);
    decor.mNest->setVisible(false);
    decor.mFeathers->setVisible(false);
    mCoopDecors[coop] = decor;
    // The first check is done at once
    mCoopDecorTimer = 10.0f;
}

void RenderManager::rrDestroyCoopDecor(BuildingObject* coop)
{
    std::map<BuildingObject*, CoopDecor>::iterator it = mCoopDecors.find(coop);
    if(it == mCoopDecors.end())
        return;

    CoopDecor& decor = it->second;
    decor.mNode->detachObject(decor.mNest);
    decor.mNode->detachObject(decor.mFeathers);
    mSceneManager->destroyEntity(decor.mNest);
    mSceneManager->destroyEntity(decor.mFeathers);
    if(coop->getEntityNode() != nullptr)
    {
        coop->getEntityNode()->removeAndDestroyChild(decor.mNode->getName());
    }
    mCoopDecors.erase(it);
}

void RenderManager::updateChickenLooks(Ogre::Real timeSinceLastFrame)
{
    const Ogre::Real pi = Ogre::Math::PI;
    for(std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.begin(); it != mChickenLooks.end(); ++it)
    {
        ChickenEntity* chicken = it->first;
        ChickenLook& look = it->second;
        look.mTime += timeSinceLastFrame;
        look.mPoseTime += timeSinceLastFrame;
        const Ogre::Real t = look.mTime + look.mPhase;
        const Ogre::Real p = look.mPoseTime;
        const ChickenKind kind = chicken->getKind();
        const std::string& pose = look.mPose;

        Ogre::Real lift = 0.0f;
        Ogre::Real pitch = 0.0f;
        Ogre::Real roll = 0.0f;
        Ogre::Vector3 stretch = Ogre::Vector3::UNIT_SCALE;

        if(kind == ChickenKind::egg)
        {
            // The egg rocks from side to side and gets more restless until it breaks
            if(pose == ChickenPose::wobble)
            {
                const Ogre::Real strength = std::min(1.0f, p * 2.5f);
                roll = 11.0f * strength * std::sin(p * 17.0f);
                pitch = 7.0f * strength * std::cos(p * 13.0f);
                lift = 0.004f * strength * std::fabs(std::sin(p * 17.0f));
            }
        }
        else
        {
            const bool moving = chicken->isMoving();
            if(pose == ChickenPose::strut)
            {
                // Chest out, head up, a proud little bounce with each step
                pitch = -8.0f;
                stretch = Ogre::Vector3(1.06f, 1.0f, 1.05f);
                lift = 0.012f * std::fabs(std::sin(t * 9.0f));
            }
            else if(pose == ChickenPose::chase || pose == ChickenPose::flee)
            {
                // Head forward, wings out, quick hops
                pitch = 14.0f;
                stretch = Ogre::Vector3(1.0f + 0.2f * std::fabs(std::sin(t * 22.0f)), 1.0f, 1.0f);
                lift = 0.022f * std::fabs(std::sin(t * 16.0f));
                roll = 6.0f * std::sin(t * 8.0f);
            }
            else if(pose == ChickenPose::crow)
            {
                // Head thrown back, the body stretches, the wings beat
                const Ogre::Real strength = std::min(1.0f, p * 4.0f);
                pitch = -38.0f * strength;
                stretch = Ogre::Vector3(1.0f + 0.3f * std::fabs(std::sin(t * 14.0f)) * strength, 1.0f, 1.0f + 0.14f * strength);
                lift = 0.01f * strength;
            }
            else if(pose == ChickenPose::perch)
                pitch = -6.0f;
            else if(pose == ChickenPose::roost)
            {
                // Asleep: sunk down, rounded, breathing
                const Ogre::Real breath = 0.025f * std::sin(t * 1.8f);
                stretch = Ogre::Vector3(1.12f + breath, 1.08f + breath, 0.68f + breath * 2.0f);
                lift = -0.004f;
            }
            else if(pose == ChickenPose::guard)
            {
                // Puffed up and flapping
                const Ogre::Real puff = 1.28f + 0.03f * std::sin(t * 20.0f);
                stretch = Ogre::Vector3(puff + 0.15f * std::fabs(std::sin(t * 18.0f)), puff, puff);
                pitch = 10.0f;
            }
            else if(pose == ChickenPose::lead)
            {
                // Scratches the ground
                pitch = 18.0f + 12.0f * std::sin(t * 9.0f);
                lift = 0.004f * std::fabs(std::sin(t * 9.0f));
            }
            else if(pose == ChickenPose::lay)
            {
                // Sits down and fluffs up, then stands up proudly
                if(p < 1.2f)
                    stretch = Ogre::Vector3(1.14f, 1.1f, 0.78f + 0.03f * std::sin(p * 25.0f));
                else
                    stretch = Ogre::Vector3(1.0f, 1.0f, 1.12f);
            }
            else if(pose == ChickenPose::cackle)
            {
                roll = 9.0f * std::sin(p * 28.0f);
                lift = 0.025f * std::fabs(std::sin(p * 11.0f));
            }
            else if(pose == ChickenPose::mount)
            {
                // Leans forward and bounces on the hen, a second burst of feathers flies
                pitch = 40.0f;
                lift = 0.07f * std::fabs(std::sin(p * 8.0f));
                if((look.mFeatherBursts == 0) && (p > 0.5f))
                {
                    look.mFeatherBursts = 1;
                    createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.2f));
                }
            }
            else if(pose == ChickenPose::emerge)
            {
                // Pops out of the coop
                const Ogre::Real grow = std::min(1.0f, p * 2.0f);
                stretch = Ogre::Vector3::UNIT_SCALE * (0.5f + 0.5f * grow);
                lift = 0.03f * std::sin(std::min(1.0f, p * 2.0f) * pi);
            }
            else if(moving)
            {
                // Walking: a small bob, chicks hop more
                const Ogre::Real speed = (kind == ChickenKind::chick) ? 14.0f : 9.0f;
                lift = ((kind == ChickenKind::chick) ? 0.01f : 0.006f) * std::fabs(std::sin(t * speed));
                pitch = 3.0f * std::sin(t * speed);
            }
            else if(kind == ChickenKind::chick)
            {
                // A chick peeps now and then with a little hop
                const Ogre::Real peep = std::max(0.0f, std::sin(t * 2.1f));
                lift = 0.012f * std::pow(peep, 12.0f);
            }
        }

        look.mNode->setPosition(0.0f, 0.0f, lift);
        look.mNode->setScale(Ogre::Vector3::UNIT_SCALE * kindScale(kind) * stretch);
        look.mNode->setOrientation(Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X) *
            Ogre::Quaternion(Ogre::Degree(roll), Ogre::Vector3::UNIT_Y));
    }

    // Coops: a nest with eggs while the hatchery lives, a few loose feathers when it is empty, a shaking door
    mCoopDecorTimer += timeSinceLastFrame;
    const bool check = mCoopDecorTimer >= 1.5f;
    if(check)
        mCoopDecorTimer = 0.0f;
    for(std::map<BuildingObject*, CoopDecor>::iterator it = mCoopDecors.begin(); it != mCoopDecors.end(); ++it)
    {
        BuildingObject* coop = it->first;
        CoopDecor& decor = it->second;
        if(check)
        {
            uint32_t animals = 0;
            Tile* tile = coop->getPositionTile();
            Room* room = (tile == nullptr) ? nullptr : tile->getCoveringRoom();
            if(room != nullptr)
            {
                for(Tile* roomTile : room->getCoveredTiles())
                    animals += roomTile->countEntitiesOnTile(GameEntityType::chickenEntity);
            }
            decor.mNest->setVisible(animals > 0);
            decor.mFeathers->setVisible(animals == 0);
        }

        // The coop shakes for a moment when an animal comes out
        Ogre::SceneNode* node = coop->getEntityNode();
        if(node == nullptr)
            continue;

        const Ogre::Quaternion base(Ogre::Degree(coop->getRotationAngle()), Ogre::Vector3::UNIT_Z);
        if(decor.mShake > 0.0f)
        {
            decor.mShake = std::max(0.0f, decor.mShake - timeSinceLastFrame);
            const Ogre::Real swing = 3.0f * decor.mShake * std::sin(decor.mShake * 40.0f);
            node->setOrientation(base * Ogre::Quaternion(Ogre::Degree(swing), Ogre::Vector3::UNIT_X));
        }
        else if(decor.mShake == 0.0f)
        {
            node->setOrientation(base);
            decor.mShake = -1.0f;
        }
        else if(decor.mShake < 0.0f)
        {
            // Resting
        }
    }
}
