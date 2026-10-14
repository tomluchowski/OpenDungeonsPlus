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
#include <OgreBone.h>
#include <OgreCamera.h>
#include <OgreEntity.h>
#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreParticleSystem.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSkeletonInstance.h>
#include <OgreSubEntity.h>
#include <OgreViewport.h>

#include <algorithm>
#include <cmath>

namespace
{
const std::string MeshEggCracked = "ChickenEggCracked";
const std::string MeshNest = "ChickenNest";
const std::string MeshFeathers = "ChickenLooseFeathers";
const std::string MaterialGroup = "Graphics";

//! \brief Height above the egg at which the shell pieces fly when it hatches.
const float FEATHER_HEIGHT_HATCH_SHELL = 0.05f;
//! \brief Height at which the shell pieces and yolk of a trampled egg start.
const float FEATHER_HEIGHT_TRAMPLE_SHELL = 0.04f;
//! \brief Height of the feather cloud of the hen when the rooster has landed on her (second burst).
const float FEATHER_HEIGHT_MOUNT = 0.2f;
//! \brief Height of the feather cloud above the rooster when he has landed on a hen.
const float FEATHER_HEIGHT_MOUNT_LANDING = 0.25f;
//! \brief Height of the first feather cloud of the hen when the rooster has landed on her.
const float FEATHER_HEIGHT_MOUNT_HEN = 0.15f;
//! \brief Height of the feather cloud when a hen flutters.
const float FEATHER_HEIGHT_FLUTTER = 0.1f;
//! \brief Height of the feather cloud when an animal comes out of the coop.
const float FEATHER_HEIGHT_EMERGE = 0.1f;
//! \brief Seconds a coop without a door clip shakes when an animal comes out.
const float COOP_SHAKE_SECONDS = 0.8f;
//! \brief Largest distance in tiles at which a coop counts as the one a chicken comes out of.
const float COOP_DOOR_SEARCH_DISTANCE = 4.0f;
//! \brief Seconds before the coop decorations are checked for the first time.
const float COOP_DECOR_START_SECONDS = 10.0f;
//! \brief Seconds between two checks of the coop decorations.
const float COOP_DECOR_CHECK_SECONDS = 1.5f;
//! \brief Strength of the swing of the coop door while the coop shakes.
const float COOP_DOOR_SWING_STRENGTH = 3.0f;
//! \brief Speed of the swing of the coop door while the coop shakes.
const float COOP_DOOR_SWING_SPEED = 40.0f;
//! \brief Number of different phase offsets the idle motion of the chickens cycles through.
const uint32_t LOOK_PHASE_COUNT = 97;
//! \brief Step of the phase offset between two chickens.
const float LOOK_PHASE_STEP = 0.37f;
//! \brief Height above the ground from which an egg counts as lying in a nest.
const float NEST_EGG_MIN_HEIGHT = 0.01f;
//! \brief Seconds of the wobble after which the shell shows its cracks.
const float EGG_CRACK_TIME = 0.8f;
//! \brief Factor by which the wobble of an egg grows with time.
const float EGG_WOBBLE_RAMP = 2.5f;
//! \brief Roll of the wobbling egg in degrees.
const float EGG_ROLL_DEGREES = 11.0f;
//! \brief Speed of the roll and lift of the wobbling egg.
const float EGG_ROLL_SPEED = 17.0f;
//! \brief Pitch of the wobbling egg in degrees.
const float EGG_PITCH_DEGREES = 7.0f;
//! \brief Speed of the pitch of the wobbling egg.
const float EGG_PITCH_SPEED = 13.0f;
//! \brief Height the wobbling egg rises.
const float EGG_WOBBLE_LIFT = 0.004f;
//! \brief Pitch of a strutting animal (chest out) in degrees.
const float STRUT_PITCH_DEGREES = -8.0f;
//! \brief Stretch of a strutting animal along its length.
const float STRUT_STRETCH_X = 1.06f;
//! \brief Stretch of a strutting animal in height.
const float STRUT_STRETCH_Z = 1.05f;
//! \brief Height of the bounce with each step of a strutting animal.
const float STRUT_BOUNCE_LIFT = 0.012f;
//! \brief Speed of the bounce of a strutting animal.
const float STRUT_BOUNCE_SPEED = 9.0f;
//! \brief Height of the hops of a running animal.
const float RUN_HOP_LIFT = 0.022f;
//! \brief Speed of the hops of a running animal.
const float RUN_HOP_SPEED = 16.0f;
//! \brief Roll of a running animal in degrees.
const float RUN_ROLL_DEGREES = 6.0f;
//! \brief Speed of the roll of a running animal.
const float RUN_ROLL_SPEED = 8.0f;
//! \brief Pitch of a running animal without a run clip (head forward) in degrees.
const float RUN_PITCH_DEGREES = 14.0f;
//! \brief Stretch of the wings of a running animal without a run clip.
const float RUN_WING_STRETCH = 0.2f;
//! \brief Speed of the wing stretch of a running animal without a run clip.
const float RUN_WING_SPEED = 22.0f;
//! \brief Factor by which the crow grows with time.
const float CROW_RAMP = 4.0f;
//! \brief Height the body rises when the rooster crows.
const float CROW_LIFT = 0.01f;
//! \brief Pitch of the rooster with the head thrown back in degrees.
const float CROW_PITCH_DEGREES = -38.0f;
//! \brief Stretch of the wings while the rooster crows.
const float CROW_WING_STRETCH = 0.3f;
//! \brief Speed of the wing beat while the rooster crows.
const float CROW_WING_SPEED = 14.0f;
//! \brief Stretch of the body in height while the rooster crows.
const float CROW_BODY_STRETCH = 0.14f;
//! \brief Pitch of an animal on its perch in degrees.
const float PERCH_PITCH_DEGREES = -6.0f;
//! \brief Size of the puffed up rooster that guards.
const float GUARD_PUFF = 1.28f;
//! \brief How much the puffed up guard size wobbles.
const float GUARD_PUFF_WOBBLE = 0.03f;
//! \brief Speed of the wobble of the guard size.
const float GUARD_PUFF_SPEED = 20.0f;
//! \brief Stretch of the wings of a guarding rooster.
const float GUARD_WING_STRETCH = 0.15f;
//! \brief Speed of the wing beat of a guarding rooster.
const float GUARD_WING_SPEED = 18.0f;
//! \brief Pitch of a guarding rooster between the lunges in degrees.
const float GUARD_PITCH_DEGREES = 10.0f;
//! \brief Extra pitch of the head lunge of a guarding rooster in degrees.
const float GUARD_LUNGE_DEGREES = 24.0f;
//! \brief Speed of the head lunges of a guarding rooster.
const float GUARD_LUNGE_SPEED = 7.0f;
//! \brief Size of a puffed up fighting rooster.
const float FIGHT_PUFF = 1.15f;
//! \brief How much the size of a fighting rooster wobbles.
const float FIGHT_PUFF_WOBBLE = 0.03f;
//! \brief Speed of the wobble of the size of a fighting rooster.
const float FIGHT_PUFF_SPEED = 20.0f;
//! \brief Stretch of the wings of a fighting rooster.
const float FIGHT_WING_STRETCH = 0.2f;
//! \brief Speed of the wing beat of a fighting rooster.
const float FIGHT_WING_SPEED = 19.0f;
//! \brief Base pitch of a fighting rooster in degrees.
const float FIGHT_PITCH_DEGREES = 12.0f;
//! \brief Extra pitch of the pecks and lunges of a fighting rooster in degrees.
const float FIGHT_PECK_DEGREES = 16.0f;
//! \brief Speed of the pecks and lunges of a fighting rooster.
const float FIGHT_PECK_SPEED = 11.0f;
//! \brief Roll of a fighting rooster in degrees.
const float FIGHT_ROLL_DEGREES = 9.0f;
//! \brief Speed of the roll of a fighting rooster.
const float FIGHT_ROLL_SPEED = 7.0f;
//! \brief Height of the little hops of a fighting rooster.
const float FIGHT_HOP_LIFT = 0.035f;
//! \brief Speed of the hops of a fighting rooster.
const float FIGHT_HOP_SPEED = 8.0f;
//! \brief Size of a puffed up animal that is held in the hand.
const float PROTEST_PUFF = 1.25f;
//! \brief How much the size of a held animal wobbles.
const float PROTEST_PUFF_WOBBLE = 0.05f;
//! \brief Speed of the wobble of the size of a held animal.
const float PROTEST_PUFF_SPEED = 24.0f;
//! \brief Stretch of the wings of a held animal.
const float PROTEST_WING_STRETCH = 0.3f;
//! \brief Speed of the wing beat of a held animal.
const float PROTEST_WING_SPEED = 21.0f;
//! \brief Pitch of an animal that is held in the hand in degrees.
const float PROTEST_PITCH_DEGREES = -12.0f;
//! \brief Roll of a held animal in degrees.
const float PROTEST_ROLL_DEGREES = 14.0f;
//! \brief Speed of the roll of a held animal.
const float PROTEST_ROLL_SPEED = 22.0f;
//! \brief Height of the kicks of a held animal.
const float PROTEST_HOP_LIFT = 0.03f;
//! \brief Speed of the kicks of a held animal.
const float PROTEST_HOP_SPEED = 14.0f;
//! \brief Base pitch of a scratching hen in degrees.
const float SCRATCH_PITCH_DEGREES = 14.0f;
//! \brief Extra pitch of the strokes of a scratching hen in degrees.
const float SCRATCH_STROKE_DEGREES = 10.0f;
//! \brief Speed of the strokes of a scratching hen.
const float SCRATCH_STROKE_SPEED = 12.0f;
//! \brief Height of the scratching hen while she strokes.
const float SCRATCH_LIFT = 0.003f;
//! \brief Speed at which a fluttering hen rises and settles.
const float FLUTTER_RISE_SPEED = 1.4f;
//! \brief Height a fluttering hen rises without a flutter clip.
const float FLUTTER_LIFT = 0.14f;
//! \brief Stretch of the wings of a fluttering hen without a flutter clip.
const float FLUTTER_WING_STRETCH = 0.25f;
//! \brief Speed of the wing beat of a fluttering hen without a flutter clip.
const float FLUTTER_WING_SPEED = 30.0f;
//! \brief Pitch of a fluttering hen without a flutter clip in degrees.
const float FLUTTER_PITCH_DEGREES = -10.0f;
//! \brief Seconds a laying hen without a lay clip stays squashed before she stands up.
const float LAY_SIT_SECONDS = 1.2f;
//! \brief Stretch of a laying hen along her length.
const float LAY_SIT_STRETCH_X = 1.14f;
//! \brief Stretch of a laying hen in width.
const float LAY_SIT_STRETCH_Y = 1.1f;
//! \brief Height of a laying hen when she sits.
const float LAY_SIT_STRETCH_Z = 0.78f;
//! \brief How much a sitting hen fluffs up.
const float LAY_SIT_FLUFF = 0.03f;
//! \brief Speed at which a sitting hen fluffs up.
const float LAY_SIT_FLUFF_SPEED = 25.0f;
//! \brief Height of a hen when she stands up after laying.
const float LAY_STAND_STRETCH_Z = 1.12f;
//! \brief Roll of a cackling hen in degrees.
const float CACKLE_ROLL_DEGREES = 9.0f;
//! \brief Speed of the roll of a cackling hen.
const float CACKLE_ROLL_SPEED = 28.0f;
//! \brief Height of the hops of a cackling hen.
const float CACKLE_HOP_LIFT = 0.025f;
//! \brief Speed of the hops of a cackling hen.
const float CACKLE_HOP_SPEED = 11.0f;
//! \brief Forward lean of the rooster on a hen in degrees.
const float MOUNT_PITCH_DEGREES = 40.0f;
//! \brief Height of the bounce of the rooster on a hen.
const float MOUNT_BOUNCE_LIFT = 0.07f;
//! \brief Speed of the bounce of the rooster on a hen.
const float MOUNT_BOUNCE_SPEED = 8.0f;
//! \brief Growth of the stretch of a hen under the rooster while she ducks.
const float MOUNT_HEN_CROUCH_BULGE = 0.1f;
//! \brief Shortest time in seconds the rooster needs to climb on a hen.
const float MOUNT_CLIMB_MIN_SECONDS = 0.05f;
//! \brief Speed at which an animal grows to full size when it comes out of the coop.
const float EMERGE_GROW_SPEED = 2.0f;
//! \brief Size of an animal when it starts to come out of the coop (share of full size).
const float EMERGE_START_SCALE = 0.5f;
//! \brief Height of the hop of an animal that comes out of the coop.
const float EMERGE_LIFT = 0.03f;
//! \brief Speed of the bob of a walking chick.
const float WALK_CHICK_BOB_SPEED = 14.0f;
//! \brief Speed of the bob of a walking hen or rooster.
const float WALK_BOB_SPEED = 9.0f;
//! \brief Height of the bob of a walking chick.
const float WALK_CHICK_BOB_LIFT = 0.01f;
//! \brief Height of the bob of a walking hen or rooster.
const float WALK_BOB_LIFT = 0.006f;
//! \brief Pitch of the bob of a walking animal in degrees.
const float WALK_BOB_PITCH_DEGREES = 3.0f;
//! \brief Speed at which a standing chick peeps now and then.
const float PEEP_SPEED = 2.1f;
//! \brief Height of the little hop of a peeping chick.
const float PEEP_HOP_LIFT = 0.012f;
//! \brief Exponent that makes the peep hops short and rare.
const float PEEP_SHARPNESS = 12.0f;

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

//! \brief Adds one textured quad (two triangles) with a common normal to the shape and advances the vertex index by four.
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
//! Coop exit: the door of a coop lies this far in front of the coop tile (tiles), an animal at the door is within the
//! square root of DOOR_REACH_SQUARED of it, and the door opens when the animal is DOOR_OPEN_MARGIN before it
static const float COOP_DOOR_OFFSET = 0.9f;
static const float COOP_DOOR_REACH_SQUARED = 0.16f;
static const float COOP_DOOR_OPEN_MARGIN = 0.2f;

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

//! \brief Makes the named procedural mesh if it does not exist yet.
//! \param meshName One of the mesh names of the hatchery shapes (egg, comb, tail, nest, feathers).
void RenderManager::rrEnsureChickenMesh(const std::string& meshName)
{
    if(meshName == MeshFeathers)
        ensureMesh(mSceneManager, MeshFeathers, buildFeathers);
}

//! \brief Moves the entity of the animal to a child node that carries scale and procedural motion, then applies the kind look.
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
    look.mPhase = static_cast<Ogre::Real>(++mChickenLookNumber % LOOK_PHASE_COUNT) * LOOK_PHASE_STEP;
    look.mFeatherBursts = 0;
    look.mEmergeFeathersPending = false;
    look.mCoopDoorReplayPending = false;
    look.mCoopExitWalking = false;
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

//! \brief Forgets all animal looks and coop decors.
void RenderManager::clearChickenLooks()
{
    mChickenLooks.clear();
    mCoopDecors.clear();
    mNestFields.clear();
    mServerNests.clear();
}

//! \brief Sets scale, tinted materials and the comb and tail of the rooster for the current kind of the animal.
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

//! \brief The kind of the animal changed: applies the new look.
void RenderManager::rrUpdateChickenLook(ChickenEntity* chicken)
{
    applyChickenKindLook(chicken);
}

//! \brief True when the animal can be seen (in the scene, in the view of the camera) and its clip is playing.
//! Feathers only fly for what the keeper can see.
//! \param chicken The animal.
//! \param clip The name of the clip that has to be playing.
bool RenderManager::chickenClipVisible(ChickenEntity* chicken, const std::string& clip) const
{
    std::map<ChickenEntity*, ChickenLook>::const_iterator it = mChickenLooks.find(chicken);
    if(it == mChickenLooks.end() || it->second.mEntity == nullptr || !it->second.mEntity->isVisible() ||
       it->second.mNode == nullptr || !it->second.mNode->isInSceneGraph() || mViewport == nullptr ||
       mViewport->getCamera() == nullptr || !mViewport->getCamera()->isVisible(it->second.mNode->_getDerivedPosition()) ||
       !it->second.mEntity->hasAnimationState(clip))
        return false;

    const Ogre::AnimationState* animation = it->second.mEntity->getAnimationState(clip);
    return animation->getEnabled() && !animation->hasEnded();
}

//! \brief The egg hatched: lets the shell pieces fly.
void RenderManager::rrChickenHatched(ChickenEntity* chicken)
{
    createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_HATCH_SHELL), "ChickenEggShell");
}

//! \brief An egg was trampled: lets shell pieces and yolk fly and startles the feathers of a hen.
void RenderManager::rrEggTrampled(const Ogre::Vector3& position)
{
    // Shell pieces and yolk have no chicken feather trigger.
    createChickenFeatherEffect(position + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_TRAMPLE_SHELL), "ChickenEggTrample");
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

//! \brief The server says which hen the rooster mounts: the mounting look uses exactly this pair.
//! \param rooster The rooster that climbs on the hen.
//! \param hen The hen he climbs on.
void RenderManager::rrChickenMount(ChickenEntity* rooster, ChickenEntity* hen)
{
    std::map<ChickenEntity*, ChickenLook>::iterator look = mChickenLooks.find(rooster);
    if(look != mChickenLooks.end())
        look->second.mMountPartner = hen;
}

//! \brief Starts or ends the brawl of two roosters; the first one makes the feather clouds, the loser goes down in a last cloud.
//! \param phase 0 when the fight starts, 1 when it is over.
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
}

//! \brief Sets the pose the server chose and starts the feather effect (and the door shake) that belongs to it.
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
    if((pose != ChickenPose::emerge) && !pose.empty())
    {
        look.mEmergeFeathersPending = false;
        look.mCoopDoorReplayPending = false;
        look.mCoopExitWalking = false;
    }
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

            if(!chicken->getMountHenName().empty() && other->first->getName() != chicken->getMountHenName())
                continue;
            const Ogre::Real distance = other->first->getPosition().squaredDistance(position);
            if(distance < nearestDistance)
            {
                nearestDistance = distance;
                look.mMountPartner = other->first;
            }
        }
    }
    else if(pose == ChickenPose::emerge)
    {
        look.mEmergeFeathersPending = true;
        look.mCoopDoorReplayPending = true;
        look.mCoopExitWalking = false;
        // The door of the closest coop swings
        std::map<BuildingObject*, CoopDecor>::iterator nearest = mCoopDecors.end();
        Ogre::Real nearestDistance = COOP_DOOR_SEARCH_DISTANCE;
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
                nearest->second.mShake = COOP_SHAKE_SECONDS;
        }
    }
}

//! \brief Creates the hidden nest and feathers next to the coop; the update shows them when needed.
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

    decor.mNode = node->createChildSceneNode(name + "_node", Ogre::Vector3(COOP_DOOR_OFFSET, 0.0f, 0.0f));
    mCoopDecors[coop] = decor;
    // The first check is done at once
    mCoopDecorTimer = COOP_DECOR_START_SECONDS;
}

//! \brief Removes the nest and the feathers next to the coop.
void RenderManager::rrDestroyCoopDecor(BuildingObject* coop)
{
    std::map<BuildingObject*, CoopDecor>::iterator it = mCoopDecors.find(coop);
    if(it == mCoopDecors.end())
        return;

    CoopDecor& decor = it->second;
    if(coop->getEntityNode() != nullptr)
    {
        coop->getEntityNode()->removeAndDestroyChild(decor.mNode->getName());
    }
    mCoopDecors.erase(it);
}

//! \brief Moves every animal look by its pose (rocking egg, strut, chase, mount, fight ...) and updates the coop decor.
//! \param timeSinceLastFrame Seconds since the last frame.
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
        const bool moving = chicken->isMoving();

        Ogre::Real lift = 0.0f;
        // Sideways shift of the look against the node of the animal (a sleeping chick tucks in under a hen)
        Ogre::Vector3 shift = Ogre::Vector3::ZERO;
        Ogre::Real pitch = 0.0f;
        Ogre::Real roll = 0.0f;
        Ogre::Vector3 stretch = Ogre::Vector3::UNIT_SCALE;

        if(kind == ChickenKind::egg)
        {
            // An egg a little above the ground lies in a nest (the server puts it there)
            if(!look.mNestEgg && (chicken->getPosition().z > NEST_EGG_MIN_HEIGHT))
            {
                look.mNestEgg = true;
                hideEggStraw(look.mEntity);
            }

            // The egg rocks from side to side and gets more restless until it breaks
            if(pose == ChickenPose::wobble)
            {
                // Shortly before it hatches the shell shows its cracks
                if(look.mAccessories.empty() && (p > EGG_CRACK_TIME))
                {
                    Ogre::Entity* cracked = mSceneManager->createEntity(
                        look.mNode->getName() + "_" + MeshEggCracked, MeshEggCracked + ".mesh");
                    cracked->setQueryFlags(0);
                    look.mNode->attachObject(cracked);
                    look.mAccessories.push_back(cracked);
                    look.mEntity->setVisible(false);
                }
                const Ogre::Real strength = std::min(1.0f, p * EGG_WOBBLE_RAMP);
                roll = EGG_ROLL_DEGREES * strength * std::sin(p * EGG_ROLL_SPEED);
                pitch = EGG_PITCH_DEGREES * strength * std::cos(p * EGG_PITCH_SPEED);
                lift = EGG_WOBBLE_LIFT * strength * std::fabs(std::sin(p * EGG_ROLL_SPEED));
            }
        }
        else
        {
            if(pose == ChickenPose::strut)
            {
                // Chest out, head up, a proud little bounce with each step
                pitch = STRUT_PITCH_DEGREES;
                stretch = Ogre::Vector3(STRUT_STRETCH_X, 1.0f, STRUT_STRETCH_Z);
                lift = STRUT_BOUNCE_LIFT * std::fabs(std::sin(t * STRUT_BOUNCE_SPEED));
            }
            else if(pose == ChickenPose::chase || pose == ChickenPose::flee)
            {
                if(look.mEntity->getSkeleton()->hasAnimation("Run"))
                {
                    // The run clip leans forward and spreads the wings, only the hops are added
                    lift = RUN_HOP_LIFT * std::fabs(std::sin(t * RUN_HOP_SPEED));
                    roll = RUN_ROLL_DEGREES * std::sin(t * RUN_ROLL_SPEED);
                }
                else
                {
                    // Head forward, wings out, quick hops
                    pitch = RUN_PITCH_DEGREES;
                    stretch = Ogre::Vector3(1.0f + RUN_WING_STRETCH * std::fabs(std::sin(t * RUN_WING_SPEED)), 1.0f, 1.0f);
                    lift = RUN_HOP_LIFT * std::fabs(std::sin(t * RUN_HOP_SPEED));
                    roll = RUN_ROLL_DEGREES * std::sin(t * RUN_ROLL_SPEED);
                }
            }
            else if(pose == ChickenPose::crow)
            {
                const Ogre::Real strength = std::min(1.0f, p * CROW_RAMP);
                if(look.mEntity->getSkeleton()->hasAnimation("Crow"))
                {
                    // The crow clip throws the head back and beats the wings, the body only rises a little
                    lift = CROW_LIFT * strength;
                }
                else
                {
                    // Head thrown back, the body stretches, the wings beat
                    pitch = CROW_PITCH_DEGREES * strength;
                    stretch = Ogre::Vector3(1.0f + CROW_WING_STRETCH * std::fabs(std::sin(t * CROW_WING_SPEED)) * strength, 1.0f, 1.0f + CROW_BODY_STRETCH * strength);
                    lift = CROW_LIFT * strength;
                }
            }
            else if(pose == ChickenPose::perch)
                pitch = PERCH_PITCH_DEGREES;
            else if(pose == ChickenPose::guard)
            {
                // Puffed up and flapping, with a lunge of the head at the creature in front of him, then back
                const Ogre::Real puff = GUARD_PUFF + GUARD_PUFF_WOBBLE * std::sin(t * GUARD_PUFF_SPEED);
                stretch = Ogre::Vector3(puff + GUARD_WING_STRETCH * std::fabs(std::sin(t * GUARD_WING_SPEED)), puff, puff);
                pitch = GUARD_PITCH_DEGREES + GUARD_LUNGE_DEGREES * std::max(0.0f, std::sin(t * GUARD_LUNGE_SPEED));
            }
            else if(pose == ChickenPose::fight)
            {
                // Puffed up, wings beating, pecking and lunging at the other rooster, little hops
                const Ogre::Real puff = FIGHT_PUFF + FIGHT_PUFF_WOBBLE * std::sin(t * FIGHT_PUFF_SPEED);
                stretch = Ogre::Vector3(puff + FIGHT_WING_STRETCH * std::fabs(std::sin(t * FIGHT_WING_SPEED)), puff, puff);
                pitch = FIGHT_PITCH_DEGREES + FIGHT_PECK_DEGREES * std::sin(t * FIGHT_PECK_SPEED);
                roll = FIGHT_ROLL_DEGREES * std::sin(t * FIGHT_ROLL_SPEED);
                lift = FIGHT_HOP_LIFT * std::fabs(std::sin(t * FIGHT_HOP_SPEED));
            }
            else if(pose == ChickenPose::protest)
            {
                // Held in the hand: puffed up, wings beating, kicking and rolling about
                const Ogre::Real puff = PROTEST_PUFF + PROTEST_PUFF_WOBBLE * std::sin(t * PROTEST_PUFF_SPEED);
                stretch = Ogre::Vector3(puff + PROTEST_WING_STRETCH * std::fabs(std::sin(t * PROTEST_WING_SPEED)), puff, puff);
                pitch = PROTEST_PITCH_DEGREES;
                roll = PROTEST_ROLL_DEGREES * std::sin(t * PROTEST_ROLL_SPEED);
                lift = PROTEST_HOP_LIFT * std::fabs(std::sin(t * PROTEST_HOP_SPEED));
            }
            else if(pose == ChickenPose::scratch)
            {
                // A hen scratches the ground with quick strokes, then pecks
                pitch = SCRATCH_PITCH_DEGREES + SCRATCH_STROKE_DEGREES * std::sin(p * SCRATCH_STROKE_SPEED);
                lift = SCRATCH_LIFT * std::fabs(std::sin(p * SCRATCH_STROKE_SPEED));
            }
            else if(pose == ChickenPose::flutter)
            {
                // The flutter clip lifts the hen and beats the wings, without it she is stretched and lifted here
                if(!look.mEntity->getSkeleton()->hasAnimation("Flutter"))
                {
                    // Flaps up for a moment with the wings out and settles down again
                    const Ogre::Real rise = std::sin(std::min(1.0f, p * FLUTTER_RISE_SPEED) * pi);
                    lift = FLUTTER_LIFT * rise;
                    stretch = Ogre::Vector3(1.0f + FLUTTER_WING_STRETCH * std::fabs(std::sin(p * FLUTTER_WING_SPEED)) * rise, 1.0f, 1.0f);
                    pitch = FLUTTER_PITCH_DEGREES * rise;
                }
            }
            else if(pose == ChickenPose::lay)
            {
                // The lay clip sits down, fluffs up and stands up, without it the body is squashed here
                if(look.mEntity->getSkeleton()->hasAnimation("Lay"))
                {
                    // Nothing to add
                }
                else if(p < LAY_SIT_SECONDS)
                    stretch = Ogre::Vector3(LAY_SIT_STRETCH_X, LAY_SIT_STRETCH_Y, LAY_SIT_STRETCH_Z + LAY_SIT_FLUFF * std::sin(p * LAY_SIT_FLUFF_SPEED));
                else
                    stretch = Ogre::Vector3(1.0f, 1.0f, LAY_STAND_STRETCH_Z);
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
                    stretch = Ogre::Vector3(1.0f + MOUNT_HEN_CROUCH_BULGE * look.mMountCrouch,
                        1.0f + MOUNT_HEN_CROUCH_BULGE * look.mMountCrouch,
                        1.0f - (1.0f - MOUNT_HEN_CROUCH) * look.mMountCrouch);
                    roll = CACKLE_ROLL_DEGREES * MOUNT_HEN_QUIVER * look.mMountCrouch * std::sin(p * CACKLE_ROLL_SPEED);
                }
                else
                {
                    roll = CACKLE_ROLL_DEGREES * std::sin(p * CACKLE_ROLL_SPEED);
                    lift = CACKLE_HOP_LIFT * std::fabs(std::sin(p * CACKLE_HOP_SPEED));
                }
            }
            else if(pose == ChickenPose::mount)
            {
                // A hen that becomes visible later still resolves to the server-selected partner.
                if(look.mMountPartner == nullptr && !chicken->getMountHenName().empty())
                {
                    for(std::map<ChickenEntity*, ChickenLook>::iterator other = mChickenLooks.begin(); other != mChickenLooks.end(); ++other)
                    {
                        if(other->first->getName() == chicken->getMountHenName())
                            look.mMountPartner = other->first;
                    }
                }
                // He climbs on the back of the hen he caught, treads and beats his wings there, and climbs down
                std::map<ChickenEntity*, ChickenLook>::iterator hen = (look.mMountPartner != nullptr) ?
                    mChickenLooks.find(look.mMountPartner) : mChickenLooks.end();
                const Ogre::Real climb = std::max(MOUNT_CLIMB_MIN_SECONDS, MOUNT_CLIMB_SECONDS);
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
                pitch = MOUNT_PITCH_DEGREES * on;
                stretch = Ogre::Vector3(1.0f + MOUNT_WING_STRETCH * std::fabs(std::sin(p * MOUNT_WING_SPEED)) * sitting,
                    1.0f, 1.0f);
                lift = MOUNT_BOUNCE_LIFT * std::fabs(std::sin(p * MOUNT_BOUNCE_SPEED)) * sitting;
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
                        // His head goes over her head: the way from his head to hers in the frame of his node
                        look.mEntity->_updateAnimation();
                        hen->second.mEntity->_updateAnimation();
                        const Ogre::Vector3 henHead = hen->second.mEntity->getSkeleton()->getBone("Head")->_getDerivedPosition();
                        const Ogre::Vector3 roosterHead = look.mEntity->getSkeleton()->getBone("Head")->_getDerivedPosition();
                        const Ogre::Vector3 targetHead = parent->convertWorldToLocalPosition(
                            hen->second.mNode->convertLocalToWorldPosition(henHead));
                        const Ogre::Quaternion lean = Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X);
                        const Ogre::Vector3 headOffset = lean * (roosterHead * kindScale(kind) * stretch);
                        shift = (targetHead - headOffset) * on;
                        shift.z = 0.0f;
                    }
                }
                if((look.mFeatherBursts == 0) && (p > climb) && (look.mMountPartner != nullptr) &&
                   chickenClipVisible(chicken, "Tread") && chickenClipVisible(look.mMountPartner, "Duck"))
                {
                    // He has landed: the feathers of both fly
                    look.mFeatherBursts = 1;
                    Ogre::SceneNode* parent = look.mNode->getParentSceneNode();
                    if(parent != nullptr)
                        createChickenFeatherEffect(parent->convertLocalToWorldPosition(shift + Ogre::Vector3(0.0f, 0.0f, lift + FEATHER_HEIGHT_MOUNT_LANDING)),
                            featherSystem(kind));
                    createChickenFeatherEffect(look.mMountPartner->getPosition() + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_MOUNT_HEN),
                        featherSystem(ChickenKind::hen));
                }
                else if((look.mFeatherBursts == 1) && (p > MOUNT_SECOND_BURST_SECONDS) &&
                    (look.mMountPartner != nullptr) && chickenClipVisible(chicken, "Tread") &&
                    chickenClipVisible(look.mMountPartner, "Duck"))
                {
                    look.mFeatherBursts = 2;
                    createChickenFeatherEffect(look.mMountPartner->getPosition() + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_MOUNT),
                        featherSystem(ChickenKind::hen));
                }
            }
            else if(pose == ChickenPose::emerge)
            {
                // Pops out of the coop
                const Ogre::Real grow = std::min(1.0f, p * EMERGE_GROW_SPEED);
                stretch = Ogre::Vector3::UNIT_SCALE * (EMERGE_START_SCALE + EMERGE_START_SCALE * grow);
                lift = EMERGE_LIFT * std::sin(std::min(1.0f, p * EMERGE_GROW_SPEED) * pi);
            }
            else if(moving)
            {
                // Walking: a small bob, chicks hop more
                const Ogre::Real speed = (kind == ChickenKind::chick) ? WALK_CHICK_BOB_SPEED : WALK_BOB_SPEED;
                lift = ((kind == ChickenKind::chick) ? WALK_CHICK_BOB_LIFT : WALK_BOB_LIFT) * std::fabs(std::sin(t * speed));
                pitch = WALK_BOB_PITCH_DEGREES * std::sin(t * speed);
            }
            else if(kind == ChickenKind::chick)
            {
                // A chick peeps now and then with a little hop
                const Ogre::Real peep = std::max(0.0f, std::sin(t * PEEP_SPEED));
                lift = PEEP_HOP_LIFT * std::pow(peep, PEEP_SHARPNESS);
            }
        }

        // A hen that flutters up shakes out a few feathers while the clip is seen
        if((kind == ChickenKind::hen) && (pose == ChickenPose::flutter) && (look.mFeatherBursts == 0) &&
           chickenClipVisible(chicken, "Flutter"))
        {
            look.mFeatherBursts = 1;
            createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_FLUTTER), featherSystem(kind));
        }

        // The door of the coop swings and the feathers fly when the animal walks through it
        if((look.mEmergeFeathersPending || look.mCoopDoorReplayPending) && moving)
        {
            look.mCoopExitWalking = true;
            for(std::map<BuildingObject*, CoopDecor>::iterator coop = mCoopDecors.begin(); coop != mCoopDecors.end(); ++coop)
            {
                const Ogre::Vector3 door = coop->first->getPosition() + Ogre::Vector3(COOP_DOOR_OFFSET, 0.0f, 0.0f);
                if(chicken->getPosition().squaredDistance(door) > COOP_DOOR_REACH_SQUARED)
                    continue;
                if(look.mCoopDoorReplayPending && chicken->getPosition().x >= door.x - COOP_DOOR_OPEN_MARGIN)
                {
                    look.mCoopDoorReplayPending = false;
                    if(coop->second.mDoor != nullptr)
                    {
                        coop->second.mDoor->setTimePosition(0.0f);
                        coop->second.mDoor->setEnabled(true);
                    }
                }
                if(look.mEmergeFeathersPending && chicken->getPosition().x >= door.x &&
                   chickenClipVisible(chicken, EntityAnimation::walk_anim))
                {
                    look.mEmergeFeathersPending = false;
                    createChickenFeatherEffect(chicken->getPosition() + Ogre::Vector3(0.0f, 0.0f, FEATHER_HEIGHT_EMERGE), featherSystem(kind));
                }
                if(!look.mEmergeFeathersPending && !look.mCoopDoorReplayPending)
                    break;
            }
        }

        if(look.mCoopExitWalking && !moving)
        {
            look.mEmergeFeathersPending = false;
            look.mCoopDoorReplayPending = false;
            look.mCoopExitWalking = false;
        }

        // The duck of a hen under a rooster is set again by him in every frame
        look.mMountCrouch = 0.0f;
        look.mNode->setPosition(shift.x, shift.y, lift);
        look.mNode->setScale(Ogre::Vector3::UNIT_SCALE * kindScale(kind) * stretch);
        look.mNode->setOrientation(Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X) *
            Ogre::Quaternion(Ogre::Degree(roll), Ogre::Vector3::UNIT_Y));
    }

    // Coops: a shaking door. The straw nests of the hatchery are drawn from
    // the places the server sent (see updateNestFields).
    mCoopDecorTimer += timeSinceLastFrame;
    const bool check = mCoopDecorTimer >= COOP_DECOR_CHECK_SECONDS;
    if(check)
        mCoopDecorTimer = 0.0f;
    for(std::map<BuildingObject*, CoopDecor>::iterator it = mCoopDecors.begin(); it != mCoopDecors.end(); ++it)
    {
        BuildingObject* coop = it->first;
        CoopDecor& decor = it->second;
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
            const Ogre::Real swing = COOP_DOOR_SWING_STRENGTH * decor.mShake * std::sin(decor.mShake * COOP_DOOR_SWING_SPEED);
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

//! \brief Removes the nest entities and nodes of one hatchery.
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
