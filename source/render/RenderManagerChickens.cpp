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

// Looks and motion of the hatchery animals on the client: eggs in straw, chicks, the rooster, the straw nests
// scattered over the hatchery and loose feathers next to the coops. The egg, chick and rooster have meshes of their
// own (ChickenEgg, ChickenEggCracked, ChickenChick, ChickenRooster, made in Blender, the chick and rooster share the
// hen skeleton with its clips), so has the nest (ChickenNest). The loose feathers are a mesh made in code. The motion
// comes from a pose name that the server sends as animation name (see ChickenPose.h), a little procedural motion is
// added on top.
// Nothing here changes the game state.

#include "render/RenderManager.h"

#include "entities/BuildingObject.h"
#include "entities/ChickenEntity.h"
#include "entities/ChickenPose.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "gamemap/GameMap.h"
#include "rooms/HatcheryNestField.h"
#include "rooms/HatcheryCoopHouse.h"
#include "rooms/Room.h"
#include "rooms/RoomHatchery.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

#include <OgreAnimationState.h>
#include <OgreEntity.h>
#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreParticleSystem.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSkeletonInstance.h>
#include <OgreSubEntity.h>

#include <algorithm>
#include <cmath>

namespace
{
const std::string MeshEggCracked = "ChickenEggCracked";
const std::string MeshNest = "ChickenNest";
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

//! \brief Starts a mesh made in code with the given material and returns the object to add triangles to.
Ogre::ManualObject* beginShape(Ogre::SceneManager* sceneManager, const std::string& material)
{
    Ogre::ManualObject* object = sceneManager->createManualObject();
    object->begin(material, Ogre::RenderOperation::OT_TRIANGLE_LIST, MaterialGroup);
    return object;
}

//! \brief Turns the shape into a mesh with the given name and frees the manual object.
void finishShape(Ogre::SceneManager* sceneManager, Ogre::ManualObject* object, const std::string& meshName)
{
    object->convertToMesh(meshName + ".mesh", MaterialGroup);
    sceneManager->destroyManualObject(object);
}

//! \brief Makes the loose feathers that show next to a coop of an abandoned hatchery.
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

//! \brief Builds the named mesh with the given function unless it exists already.
void ensureMesh(Ogre::SceneManager* sceneManager, const std::string& name, void (*build)(Ogre::SceneManager*))
{
    if(Ogre::MeshManager::getSingleton().resourceExists(name + ".mesh", MaterialGroup))
        return;

    build(sceneManager);
}

//! \brief Reads one number of the room config, the default is used when the key is missing.
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
            return configValue("HatcheryRoosterScale", 1.25f);
        default:
            return 1.0f;
    }
}

//! The particle system of the feathers of an animal: the colours of its plumage
const std::string& featherSystem(ChickenKind kind)
{
    static const std::string rooster = "ChickenFeathersRooster";
    static const std::string hen = "ChickenFeathers";
    return (kind == ChickenKind::rooster) ? rooster : hen;
}

//! Mating (look only): a hen is "the one he caught" when she is nearer than this (squared distance in tiles)
static const float MOUNT_PARTNER_REACH_SQUARED = 2.25f;
//! Mating: seconds the rooster needs to climb on the hen, and the same to climb down
static const float MOUNT_CLIMB_SECONDS = 0.4f;
//! Mating: seconds of the whole climb (up, sitting, down); the Duck clip and the server pose last as long
static const float MOUNT_SECONDS = 1.9f;
//! Mating: second of the pose in which the feathers of the hen fly a second time
static const float MOUNT_SECOND_BURST_SECONDS = 1.0f;
//! Mating: how high he sits, as a part of the height of the ducked hen
static const float MOUNT_HEIGHT_FACTOR = 0.85f;
//! Mating: height factor of the hen when she is ducked under him (only drawn when the skeleton has no Duck clip)
static const float MOUNT_HEN_CROUCH = 0.6f;
//! Mating: extra length of the rooster from the beating wings, and the wing speed
static const float MOUNT_WING_STRETCH = 0.3f;
static const float MOUNT_WING_SPEED = 20.0f;
//! Mating: height (tiles) of the arc in which he jumps up
static const float MOUNT_JUMP_ARC = 0.08f;
//! Mating: how much of the quivering of the cackle the hen keeps while she is ducked
static const float MOUNT_HEN_QUIVER = 0.4f;
//! Mating: seconds before the end of the Mount clip at which the Tread clip follows (an ended clip would send the
//! animal to idle and end the pose)
static const float MOUNT_CLIP_OVERLAP_SECONDS = 0.05f;

//! An egg in a nest lies on the straw of the nest: the straw of the egg mesh is hidden
void hideEggStraw(Ogre::Entity* entity)
{
    for(unsigned int i = 0; i < entity->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity* part = entity->getSubEntity(i);
        if(part->getMaterialName() == "ChickenStraw")
            part->setVisible(false);
    }
}

} // namespace

void RenderManager::rrEnsureChickenMesh(const std::string& meshName)
{
    if(meshName == MeshFeathers)
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
    look.mFightPartner = nullptr;
    look.mFightLeader = false;
    look.mFightTimer = 0.0f;
    look.mMountPartner = nullptr;
    look.mMountCrouch = 0.0f;
    look.mMountPhase = 0;
    look.mNestEgg = false;
    mChickenLooks[chicken] = look;
    applyChickenKindLook(chicken);
}

//! \brief Removes the look of an animal; no other animal keeps it as its fight or mating partner.
void RenderManager::rrDestroyChickenLook(ChickenEntity* chicken)
{
    std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end())
        return;

    ChickenLook& look = it->second;
    for(std::map<ChickenEntity*, ChickenLook>::iterator other = mChickenLooks.begin(); other != mChickenLooks.end(); ++other)
    {
        if(other->second.mFightPartner == chicken)
            other->second.mFightPartner = nullptr;
        if(other->second.mMountPartner == chicken)
            other->second.mMountPartner = nullptr;
    }
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
    mNestFields.clear();
    mServerNests.clear();
}

void RenderManager::applyChickenKindLook(ChickenEntity* chicken)
{
    std::map<ChickenEntity*, ChickenLook>::iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end())
        return;

    ChickenLook& look = it->second;
    const ChickenKind kind = chicken->getKind();

    // The shell of a cracked egg is gone
    for(Ogre::Entity* accessory : look.mAccessories)
    {
        look.mNode->detachObject(accessory);
        mSceneManager->destroyEntity(accessory);
    }
    look.mAccessories.clear();
    look.mEntity->setVisible(true);
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

void RenderManager::rrEggTrampled(const Ogre::Vector3& position)
{
    // Shell pieces and yolk (one system with two emitters), and the feathers of a hen that is startled by it
    createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.04f), "ChickenEggTrample");
    createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.1f));
}

void RenderManager::rrSetHatcheryNests(const std::string& roomName, const std::vector<HatcheryNestField::Place>& places,
    const std::vector<HatcheryNestField::Place>& feathers)
{
    if(places.empty())
    {
        std::map<std::string, NestField>::iterator field = mNestFields.find(roomName);
        if(field != mNestFields.end())
        {
            destroyNestField(field->second);
            mNestFields.erase(field);
        }
        mServerNests.erase(roomName);
        return;
    }
    ServerNests& nests = mServerNests[roomName];
    nests.mPlaces = places;
    nests.mFeathers = feathers;
    ++nests.mVersion;
}

void RenderManager::rrChickenFight(ChickenEntity* first, ChickenEntity* second, uint32_t phase)
{
    std::map<ChickenEntity*, ChickenLook>::iterator firstLook = mChickenLooks.find(first);
    std::map<ChickenEntity*, ChickenLook>::iterator secondLook = mChickenLooks.find(second);
    if(firstLook != mChickenLooks.end())
    {
        firstLook->second.mFightPartner = (phase == 0) ? second : nullptr;
        firstLook->second.mFightLeader = true;
        firstLook->second.mFightTimer = 0.0f;
    }
    if(secondLook != mChickenLooks.end())
    {
        secondLook->second.mFightPartner = (phase == 0) ? first : nullptr;
        secondLook->second.mFightLeader = false;
        secondLook->second.mFightTimer = 0.0f;
    }

    // The fight is over: the loser goes down in a last cloud of feathers
    if(phase == 1)
    {
        const Ogre::Vector3 middle = (first->getPosition() + second->getPosition()) * 0.5f;
        createChickenFeatherEffect(middle + Ogre::Vector3(0.0f, 0.0f, 0.15f), featherSystem(first->getKind()));
        createChickenFeatherEffect(second->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.1f), featherSystem(second->getKind()));
    }
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

    look.mMountPartner = nullptr;
    look.mMountPhase = 0;
    if(pose == ChickenPose::mount)
    {
        // The Mount clip is started with the pose
        look.mMountPhase = 1;
        // The hen he caught is the nearest one; he climbs on her back (the feathers fly when he is up)
        Ogre::Real nearestDistance = MOUNT_PARTNER_REACH_SQUARED;
        for(std::map<ChickenEntity*, ChickenLook>::iterator other = mChickenLooks.begin(); other != mChickenLooks.end(); ++other)
        {
            if((other->first == chicken) || (other->first->getKind() != ChickenKind::hen))
                continue;

            const Ogre::Real distance = other->first->getPosition().squaredDistance(position);
            if(distance < nearestDistance)
            {
                nearestDistance = distance;
                look.mMountPartner = other->first;
            }
        }
    }
    else if((pose == ChickenPose::flee) && (chicken->getKind() == ChickenKind::hen))
    {
        // A hen scatters from a hungry creature: a few feathers fly
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.12f));
    }
    else if(pose == ChickenPose::flutter)
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.1f), featherSystem(chicken->getKind()));
    else if(pose == ChickenPose::fight)
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.15f), featherSystem(chicken->getKind()));
    else if(pose == ChickenPose::protest)
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.2f), featherSystem(chicken->getKind()));
    else if(pose == ChickenPose::emerge)
    {
        createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, 0.1f), featherSystem(chicken->getKind()));

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
        {
            if(nearest->second.mDoor != nullptr)
            {
                // The door of the coop mesh swings once
                nearest->second.mDoor->setTimePosition(0.0f);
                nearest->second.mDoor->setLoop(false);
                nearest->second.mDoor->setEnabled(true);
            }
            else
                nearest->second.mShake = 0.8f;
        }
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
    decor.mDoor = nullptr;

    // The coop mesh with a skeleton has a door of its own, the old mesh shakes
    if((node->numAttachedObjects() > 0) && (node->getAttachedObject(0)->getMovableType() == "Entity"))
    {
        Ogre::Entity* body = static_cast<Ogre::Entity*>(node->getAttachedObject(0));
        if(body->hasSkeleton() && body->getSkeleton()->hasAnimation(HatcheryCoopHouse::doorClip))
        {
            decor.mDoor = body->getAnimationState(HatcheryCoopHouse::doorClip);
            decor.mDoor->setLoop(false);
            decor.mDoor->setEnabled(false);
        }
    }

    decor.mNode = node->createChildSceneNode(name + "_node", Ogre::Vector3(0.9f, 0.0f, 0.0f));
    rrEnsureChickenMesh(MeshFeathers);
    decor.mFeathers = mSceneManager->createEntity(name + "_feathers", MeshFeathers + ".mesh");
    decor.mFeathers->setQueryFlags(0);
    decor.mNode->attachObject(decor.mFeathers);
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
    decor.mNode->detachObject(decor.mFeathers);
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
        // Sideways shift of the look against the node of the animal (a sleeping chick tucks in under a hen)
        Ogre::Vector3 shift = Ogre::Vector3::ZERO;
        Ogre::Real pitch = 0.0f;
        Ogre::Real roll = 0.0f;
        Ogre::Vector3 stretch = Ogre::Vector3::UNIT_SCALE;

        if(kind == ChickenKind::egg)
        {
            // An egg a little above the ground lies in a nest (the server puts it there)
            if(!look.mNestEgg && (chicken->getPosition().z > 0.01f))
            {
                look.mNestEgg = true;
                hideEggStraw(look.mEntity);
            }

            // The egg rocks from side to side and gets more restless until it breaks
            if(pose == ChickenPose::wobble)
            {
                // Shortly before it hatches the shell shows its cracks
                if(look.mAccessories.empty() && (p > 0.8f))
                {
                    Ogre::Entity* cracked = mSceneManager->createEntity(
                        look.mNode->getName() + "_" + MeshEggCracked, MeshEggCracked + ".mesh");
                    cracked->setQueryFlags(0);
                    look.mNode->attachObject(cracked);
                    look.mAccessories.push_back(cracked);
                    look.mEntity->setVisible(false);
                }
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
                if(look.mEntity->getSkeleton()->hasAnimation("Run"))
                {
                    // The run clip leans forward and spreads the wings, only the hops are added
                    lift = 0.022f * std::fabs(std::sin(t * 16.0f));
                    roll = 6.0f * std::sin(t * 8.0f);
                }
                else
                {
                    // Head forward, wings out, quick hops
                    pitch = 14.0f;
                    stretch = Ogre::Vector3(1.0f + 0.2f * std::fabs(std::sin(t * 22.0f)), 1.0f, 1.0f);
                    lift = 0.022f * std::fabs(std::sin(t * 16.0f));
                    roll = 6.0f * std::sin(t * 8.0f);
                }
            }
            else if(pose == ChickenPose::crow)
            {
                const Ogre::Real strength = std::min(1.0f, p * 4.0f);
                if(look.mEntity->getSkeleton()->hasAnimation("Crow"))
                {
                    // The crow clip throws the head back and beats the wings, the body only rises a little
                    lift = 0.01f * strength;
                }
                else
                {
                    // Head thrown back, the body stretches, the wings beat
                    pitch = -38.0f * strength;
                    stretch = Ogre::Vector3(1.0f + 0.3f * std::fabs(std::sin(t * 14.0f)) * strength, 1.0f, 1.0f + 0.14f * strength);
                    lift = 0.01f * strength;
                }
            }
            else if(pose == ChickenPose::perch)
                pitch = -6.0f;
            else if(pose == ChickenPose::guard)
            {
                // Puffed up and flapping, with a lunge of the head at the creature in front of him, then back
                const Ogre::Real puff = 1.28f + 0.03f * std::sin(t * 20.0f);
                stretch = Ogre::Vector3(puff + 0.15f * std::fabs(std::sin(t * 18.0f)), puff, puff);
                pitch = 10.0f + 24.0f * std::max(0.0f, std::sin(t * 7.0f));
            }
            else if(pose == ChickenPose::fight)
            {
                // Puffed up, wings beating, pecking and lunging at the other rooster, little hops
                const Ogre::Real puff = 1.15f + 0.03f * std::sin(t * 20.0f);
                stretch = Ogre::Vector3(puff + 0.2f * std::fabs(std::sin(t * 19.0f)), puff, puff);
                pitch = 12.0f + 16.0f * std::sin(t * 11.0f);
                roll = 9.0f * std::sin(t * 7.0f);
                lift = 0.035f * std::fabs(std::sin(t * 8.0f));
            }
            else if(pose == ChickenPose::protest)
            {
                // Held in the hand: puffed up, wings beating, kicking and rolling about
                const Ogre::Real puff = 1.25f + 0.05f * std::sin(t * 24.0f);
                stretch = Ogre::Vector3(puff + 0.3f * std::fabs(std::sin(t * 21.0f)), puff, puff);
                pitch = -12.0f;
                roll = 14.0f * std::sin(t * 22.0f);
                lift = 0.03f * std::fabs(std::sin(t * 14.0f));
            }
            else if(pose == ChickenPose::scratch)
            {
                // A hen scratches the ground with quick strokes, then pecks
                pitch = 14.0f + 10.0f * std::sin(p * 12.0f);
                lift = 0.003f * std::fabs(std::sin(p * 12.0f));
            }
            else if(pose == ChickenPose::flutter)
            {
                // The flutter clip lifts the hen and beats the wings, without it she is stretched and lifted here
                if(!look.mEntity->getSkeleton()->hasAnimation("Flutter"))
                {
                    // Flaps up for a moment with the wings out and settles down again
                    const Ogre::Real rise = std::sin(std::min(1.0f, p * 1.4f) * pi);
                    lift = 0.14f * rise;
                    stretch = Ogre::Vector3(1.0f + 0.25f * std::fabs(std::sin(p * 30.0f)) * rise, 1.0f, 1.0f);
                    pitch = -10.0f * rise;
                }
            }
            else if(pose == ChickenPose::lay)
            {
                // The lay clip sits down, fluffs up and stands up, without it the body is squashed here
                if(look.mEntity->getSkeleton()->hasAnimation("Lay"))
                {
                    // Nothing to add
                }
                else if(p < 1.2f)
                    stretch = Ogre::Vector3(1.14f, 1.1f, 0.78f + 0.03f * std::sin(p * 25.0f));
                else
                    stretch = Ogre::Vector3(1.0f, 1.0f, 1.12f);
            }
            else if(pose == ChickenPose::cackle)
            {
                if((look.mMountCrouch > 0.0f) && look.mEntity->getSkeleton()->hasAnimation("Duck"))
                {
                    // The duck clip ducks her under him, nothing is added
                }
                else if(look.mMountCrouch > 0.0f)
                {
                    // The rooster sits on her: she ducks down under him and quivers
                    stretch = Ogre::Vector3(1.0f + 0.1f * look.mMountCrouch, 1.0f + 0.1f * look.mMountCrouch,
                        1.0f - (1.0f - MOUNT_HEN_CROUCH) * look.mMountCrouch);
                    roll = 9.0f * MOUNT_HEN_QUIVER * look.mMountCrouch * std::sin(p * 28.0f);
                }
                else
                {
                    roll = 9.0f * std::sin(p * 28.0f);
                    lift = 0.025f * std::fabs(std::sin(p * 11.0f));
                }
            }
            else if(pose == ChickenPose::mount)
            {
                // He climbs on the back of the hen he caught, treads and beats his wings there, and climbs down
                std::map<ChickenEntity*, ChickenLook>::iterator hen = (look.mMountPartner != nullptr) ?
                    mChickenLooks.find(look.mMountPartner) : mChickenLooks.end();
                const Ogre::Real climb = std::max(0.05f, MOUNT_CLIMB_SECONDS);
                const Ogre::Real upRatio = std::min(1.0f, p / climb);
                const Ogre::Real downRatio = std::min(1.0f, std::max(0.0f, (MOUNT_SECONDS - p) / climb));
                const Ogre::Real on = std::min(upRatio * upRatio * (3.0f - 2.0f * upRatio),
                    downRatio * downRatio * (3.0f - 2.0f * downRatio));
                if(look.mEntity->getSkeleton()->hasAnimation("Tread"))
                {
                    // The clips follow the phases: Mount (climb), Tread (looped), Dismount (climb down); their lengths
                    // are the climb time and the rest of the 1.9 s of the pose. Tread follows shortly before the Mount
                    // clip ends: an ended clip would send the animal to idle and end the pose
                    const int phase = (p < climb - MOUNT_CLIP_OVERLAP_SECONDS) ? 1 : ((p < MOUNT_SECONDS - climb) ? 2 : 3);
                    if(phase != look.mMountPhase)
                    {
                        look.mMountPhase = phase;
                        const char* const clips[3] = {"Mount", "Tread", "Dismount"};
                        chicken->setAnimationState(setEntityAnimation(look.mEntity, clips[phase - 1], phase == 2));
                    }
                }
                // Beating the wings and treading only while he sits on her
                const Ogre::Real sitting = std::max(0.0f, on * 2.0f - 1.0f);
                pitch = 40.0f * on;
                stretch = Ogre::Vector3(1.0f + MOUNT_WING_STRETCH * std::fabs(std::sin(p * MOUNT_WING_SPEED)) * sitting,
                    1.0f, 1.0f);
                lift = 0.07f * std::fabs(std::sin(p * 8.0f)) * sitting;
                if(hen != mChickenLooks.end())
                {
                    // The back of the hen is as high as she is when she is ducked; he jumps up in an arc
                    const Ogre::Real henHeight = hen->second.mEntity->getBoundingBox().getMaximum().z *
                        kindScale(ChickenKind::hen) * MOUNT_HEN_CROUCH;
                    lift += henHeight * MOUNT_HEIGHT_FACTOR * on + MOUNT_JUMP_ARC * std::sin(std::min(upRatio, 1.0f) * pi) *
                        (p < climb ? 1.0f : 0.0f);
                    hen->second.mMountCrouch = on;

                    // He moves over her: the way from his place to hers in the frame of his node
                    Ogre::SceneNode* parent = look.mNode->getParentSceneNode();
                    Ogre::SceneNode* henParent = hen->second.mNode->getParentSceneNode();
                    if((parent != nullptr) && (henParent != nullptr))
                    {
                        Ogre::Vector3 toHen = parent->convertWorldToLocalPosition(henParent->_getDerivedPosition());
                        toHen.z = 0.0f;
                        shift = toHen * on;
                    }
                }
                if((look.mFeatherBursts == 0) && (p > climb))
                {
                    // He has landed: the feathers of both fly
                    look.mFeatherBursts = 1;
                    createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.25f), featherSystem(kind));
                    if(look.mMountPartner != nullptr)
                    {
                        createChickenFeatherEffect(look.mMountPartner->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.15f),
                            featherSystem(ChickenKind::hen));
                    }
                }
                else if((look.mFeatherBursts == 1) && (p > MOUNT_SECOND_BURST_SECONDS) && (look.mMountPartner != nullptr))
                {
                    look.mFeatherBursts = 2;
                    createChickenFeatherEffect(look.mMountPartner->getPosition() + Ogre::Vector3(0.0f, 0.0f, 0.2f),
                        featherSystem(ChickenKind::hen));
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

        // Feather clouds between two fighting roosters, made by the first of the two
        if((look.mFightPartner != nullptr) && look.mFightLeader && (pose == ChickenPose::fight) &&
           (mChickenLooks.count(look.mFightPartner) > 0))
        {
            look.mFightTimer += timeSinceLastFrame;
            if(look.mFightTimer >= configValue("HatcheryFightFeatherSeconds", 0.7f))
            {
                look.mFightTimer = 0.0f;
                const Ogre::Vector3 middle = (chicken->getPosition() + look.mFightPartner->getPosition()) * 0.5f;
                createChickenFeatherEffect(middle + Ogre::Vector3(0.0f, 0.0f, 0.15f), featherSystem(ChickenKind::rooster));
            }
        }

        // The duck of a hen under a rooster is set again by him in every frame
        look.mMountCrouch = 0.0f;
        look.mNode->setPosition(shift.x, shift.y, lift);
        look.mNode->setScale(Ogre::Vector3::UNIT_SCALE * kindScale(kind) * stretch);
        look.mNode->setOrientation(Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X) *
            Ogre::Quaternion(Ogre::Degree(roll), Ogre::Vector3::UNIT_Y));
    }

    // Coops: loose feathers when the hatchery is empty, a shaking door. The straw nests of the hatchery are drawn from
    // the places the server sent (see updateNestFields).
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
                // The rooster alone does not keep a hatchery alive: hens, chicks and eggs do
                for(Tile* roomTile : room->getCoveredTiles())
                {
                    for(GameEntity* entity : roomTile->getEntitiesInTile())
                    {
                        if((entity == nullptr) || (entity->getObjectType() != GameEntityType::chickenEntity))
                            continue;

                        ChickenEntity* animal = static_cast<ChickenEntity*>(entity);
                        if(animal->getKind() != ChickenKind::rooster)
                            ++animals;
                    }
                }
            }
            decor.mFeathers->setVisible(animals == 0);
        }

        // The door clip of the coop mesh plays once and then rests closed
        if((decor.mDoor != nullptr) && decor.mDoor->getEnabled())
        {
            decor.mDoor->addTime(timeSinceLastFrame);
            if(decor.mDoor->hasEnded())
            {
                decor.mDoor->setEnabled(false);
                decor.mDoor->setTimePosition(0.0f);
            }
            continue;
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

    if(check)
        updateNestFields();
}

void RenderManager::destroyNestField(NestField& field)
{
    for(uint32_t i = 0; i < field.mEntities.size(); ++i)
    {
        field.mNodes[i]->detachObject(field.mEntities[i]);
        mSceneManager->destroyEntity(field.mEntities[i]);
        mSceneManager->destroySceneNode(field.mNodes[i]);
    }
    field.mEntities.clear();
    field.mNodes.clear();
}

void RenderManager::updateNestFields()
{
    if(mGameMap == nullptr)
        return;

    for(std::map<std::string, ServerNests>::const_iterator sent = mServerNests.begin(); sent != mServerNests.end(); ++sent)
    {
        const uint32_t key = sent->second.mVersion;
        std::map<std::string, NestField>::iterator existing = mNestFields.find(sent->first);
        if((existing != mNestFields.end()) && (existing->second.mKey != key))
        {
            destroyNestField(existing->second);
            mNestFields.erase(existing);
            existing = mNestFields.end();
        }
        if(existing == mNestFields.end())
        {
            NestField created;
            created.mKey = key;
            const std::vector<HatcheryNestField::Place>& places = sent->second.mPlaces;
            for(uint32_t i = 0; i < places.size(); ++i)
            {
                const std::string name = "HatcheryNest_" + sent->first + "_" + std::to_string(i);
                Ogre::SceneNode* node = mSceneManager->getRootSceneNode()->createChildSceneNode(name + "_node",
                    Ogre::Vector3(static_cast<Ogre::Real>(places[i].mX), static_cast<Ogre::Real>(places[i].mY), 0.0f));
                node->setOrientation(Ogre::Quaternion(Ogre::Degree(static_cast<Ogre::Real>(places[i].mAngle)),
                    Ogre::Vector3::UNIT_Z));
                Ogre::Entity* entity = mSceneManager->createEntity(name, MeshNest + ".mesh");
                entity->setQueryFlags(0);
                entity->setVisible(false);
                node->attachObject(entity);
                created.mNodes.push_back(node);
                created.mEntities.push_back(entity);
            }
            existing = mNestFields.insert(std::make_pair(sent->first, created)).first;
        }

        // A nest shows while its tile is in sight and still part of a hatchery
        for(uint32_t i = 0; i < existing->second.mEntities.size(); ++i)
        {
            const HatcheryNestField::Place& place = sent->second.mPlaces[i];
            Tile* tile = mGameMap->getTile(static_cast<int>(std::floor(place.mX + 0.5)),
                static_cast<int>(std::floor(place.mY + 0.5)));
            existing->second.mEntities[i]->setVisible((tile != nullptr) && tile->getLocalPlayerHasVision() &&
                (tile->getTileVisual() == TileVisual::hatcheryRoom));
        }
    }
}
