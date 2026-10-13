/*!
 *  \file   RenderManager.h
 *  \date   26 March 2011
 *  \author oln
 *  \brief  handles the render requests
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

#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include <set>

#include <string>
#include <map>
#include <OgreSingleton.h>
#include <OgreMath.h>
#include <OgreSkeleton.h>
#include <OgreDefaultDebugDrawer.h>
#include <cstdint>
#include "entities/GameEntity.h"
#include "rooms/HatcheryNestField.h"
#include <OgreVector2.h>

class DraggableTileContainer;
class GameMap;
class Building;
class BuildingObject;
class ChickenEntity;
class Seat;
class Tile;
class Room;
class GameEntity;
class MovableGameEntity;
class MapLight;
class Creature;
class Player;
class RenderedMovableEntity;
class RockLava;
class Weapon;

namespace Ogre
{
class AnimationState;
class OverlaySystem;
class SceneManager;
class SceneNode;
class ParticleSystem;

namespace RTShader {
    class ShaderGenerator;
}
} //End namespace Ogre


class RenderManager: public Ogre::Singleton<RenderManager>, Ogre::RenderTargetListener
{
public:
    RenderManager(Ogre::OverlaySystem* overlaySystem);
    ~RenderManager();

    Ogre::Light* mHandLight;
    
    static const uint8_t OD_RENDER_QUEUE_ID_GUI;

    inline Ogre::SceneManager* getSceneManager() const
    { return mSceneManager; }

    //! \brief Loop through the render requests in the queue and process them
    void updateRenderAnimations(Ogre::Real timeSinceLastFrame);

    void setPosition(Ogre::Camera* obj, const Ogre::Vector3& vec);
    
    void setOrientation(Ogre::Camera* obj, const Ogre::Quaternion& q);

    Ogre::Vector3 getPosition(Ogre::Camera* obj);

    Ogre::Quaternion getOrientation(Ogre::Camera* obj);

    // Called before a render is called to the render target
    void preRenderTargetUpdate(const Ogre::RenderTargetEvent& evt);
    void setup();
    void handleSchemeNotFound(Ogre::MaterialPtr material);
    
    //! \brief Initialize the renderer when a new game (Game or Editor) is launched
    void initGameRenderer(GameMap* gameMap);
    void stopGameRenderer(GameMap*);

    //! \brief starts the compositor compositorName.
    void triggerCompositor(const std::string& compositorName);

    //! \brief setup the scene
    void createScene(Ogre::Viewport*);

    void setViewport(Ogre::Viewport* viewport)
    { mViewport = viewport; }

    //! \brief Sets/Updates the overall world lighting value with given factor.
    void setWorldAmbientLightingFactor(float lightFactor);
    void setDynamicShadowsEnabled(bool enabled);

    //! \brief Set the entity's opacity
    void setEntityOpacity(Ogre::Entity* ent, float opacity);

    //! Beware this should be called on client side only (not from the server thread)
    //! moveCursor allows to move the cursor on GUI
    //!  moveWorldCoords sends the world coords where the map light is
    void moveCursor(float relX, float relY);
    Ogre::FloatRect getHandCursorBounds(float relX, float relY) const;
    void moveWorldCoords(Ogre::Real x, Ogre::Real y);
    void entitySlapped();

    static const Ogre::Real BLENDER_UNITS_PER_OGRE_UNIT;
    static const Ogre::Real KEEPER_HAND_WORLD_Z;
    static const Ogre::Real DRAGGABLE_NODE_HEIGHT;
    
    //! Debug function to be used for dev only. Beware, it should not be called from the server thread
    static std::string consoleListAnimationsForMesh(const std::string& meshName);

    //! Functions needed to create Fog texture dynamically ( in the program run )

    void saveTexture(Ogre::TexturePtr texture, const std::string& filename);
    Ogre::TexturePtr createPerlinTexture();
    Ogre::TexturePtr createAlphaChannelForTexture(Ogre::TexturePtr m_texture);
    Ogre::TexturePtr copyTexture(Ogre::TexturePtr oldTexture);
    void setupFogMaterial(Ogre::TexturePtr myTexture);
    void cleanUp();
    
    //Render request functions
    void rrRefreshTile(Tile& tile, GameMap& draggableTileContainer, const Player& localPlayer, NodeType nt = NodeType::MTILES_NODE);
    void rrCreateTile(Tile& tile, GameMap& dtc, const Player& localPlayer, NodeType nt = NodeType::MTILES_NODE);
    void rrDestroyTile(Tile& tile, NodeType nt = NodeType::MTILES_NODE);
    void rrTemporalMarkTile(Tile* curTile);
    void rrDetachEntity(GameEntity* curEntity, bool really_do = true);
    void rrAttachEntity(GameEntity* curEntity);
    void rrCreateRenderedMovableEntity(RenderedMovableEntity* curRenderedMovableEntity, NodeType nt = NodeType::MTILES_NODE);
    void rrDestroyRenderedMovableEntity(RenderedMovableEntity* curRenderedMovableEntity, NodeType nt = NodeType::MTILES_NODE);
    void rrUpdateEntityOpacity(RenderedMovableEntity* entity);
    void rrCreateCreature(Creature* curCreature);
    void rrDestroyCreature(Creature* curCreature);
    void rrOrientEntityToward(MovableGameEntity* gameEntity, const Ogre::Vector3& direction);
    void rrPitchAroundAxis(RenderedMovableEntity* gameEntity, Ogre::Degree dd);
    void rrScaleCreature(Creature& creature);
    void rrCreateWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand);
    void rrDestroyWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand);
    void rrCreateMapLight(MapLight* curMapLight, bool displayVisual);
    void rrDestroyMapLight(MapLight* curMapLight);
    void rrDestroyMapLightVisualIndicator(MapLight* curMapLight);
    void rrPickUpEntity(GameEntity* curEntity, Player* localPlayer);
    void rrDropHand(GameEntity* curEntity, Player* localPlayer);
    void rrRotateHand(Player* localPlayer);
    void rrEnableHeldCreatureDisplay(bool enabled, Player* localPlayer);
    bool isKeeperHandVisible() const { return mHandKeeperHandVisibility == 0; }
    void rrAddOutliner(Creature* creature);
    void rrRemoveOutliner(Creature* creature);
    void rrIncreaseAmbient(Creature* creature);
    void rrNormalizeAmbient(Creature* creature);
    void rrCreateCreatureVisualDebug(Creature* curCreature, Tile* curTile);
    void rrDestroyCreatureVisualDebug(Creature* curCreature, Tile* curTile);
    void rrCreateSeatVisionVisualDebug(int seatId, Tile* tile);
    void rrDestroySeatVisionVisualDebug(int seatId, Tile* tile);
    void rrSetObjectAnimationState(MovableGameEntity* curAnimatedObject, const std::string& animation, bool loop);
    void rrMoveEntity(GameEntity* entity, const Ogre::Vector3& position);
    void rrMoveMapLightFlicker(MapLight* mapLight, const Ogre::Vector3& position);
    void rrCarryEntity(Creature* carrier, GameEntity* carried);
    void rrReleaseCarriedEntity(Creature* carrier, GameEntity* carried);
    Ogre::ParticleSystem* rrEntityAddParticleEffect(GameEntity* entity, const std::string& particleName,
        const std::string& particleScript);
    void rrEntityRemoveParticleEffect(GameEntity* entity, Ogre::ParticleSystem* particleSystem);
    void rrToggleHandSelectorVisibility();
    void rrSetHandPose(bool pointing, bool digging, bool building = false);
    void rrPlayBuildAnimation();
    bool rrPlayIdleHandAnimation();
    bool rrIsIdleHandAnimationPlaying() const;
    void rrCancelIdleHandAnimation();
    void rrPlayDigAnimation();
    void rrDrawTilePreview(const std::vector<Tile*>& tiles, const Ogre::ColourValue& colour, bool construction = false);
    void rrCreateRoomConstructionEffect(const std::vector<Tile*>& tiles);

    //! \brief Creates a free-standing particle effect that stays until rrDestroyFreeParticleEffect().
    //! \param colour If not null, the colour of every emitter of the script is set to it.
    void rrCreateFreeParticleEffect(const std::string& effectName, const std::string& particleScript,
        const Ogre::Vector3& position, const Ogre::ColourValue* colour);
    void rrMoveFreeParticleEffect(const std::string& effectName, const Ogre::Vector3& position);
    void rrDestroyFreeParticleEffect(const std::string& effectName);
    //! Client-only copy of the dungeon heart for the defeat sequence (the server has already removed the
    //! real heart object), placed like the real one: DungeonTempleObject mesh on the floor, no turn, scale 1.
    //! Its materials are clones, so that its core can glow without changing other hearts.
    void rrCreateDefeatHeart(const Ogre::Vector3& position);
    //! Moves and scales the copy; glow 0 is its normal look, 1 the hottest red
    void rrUpdateDefeatHeart(const Ogre::Vector3& position, Ogre::Real scale, Ogre::Real glow);
    void rrDestroyDefeatHeart();
    //! Creates count rubble pieces of the burst heart (a small procedural shard mesh with the heart's
    //! own shell material), one scene node each, hidden until they are first moved
    void rrCreateDefeatRubble(size_t count);
    //! Places rubble piece index; rotation holds the degrees around the x, y and z axes
    void rrMoveDefeatRubblePiece(size_t index, const Ogre::Vector3& position, const Ogre::Vector3& rotation,
        const Ogre::Vector3& scale);
    void rrDestroyDefeatRubble();
    void rrCreateCreatureCombatImpact(Creature* creature, bool weaponClash,
        bool bodyDamage, const Ogre::Vector3& attackerPosition);
    //! \brief Gives the meal animation of a creature the chicken to eat: shows a copy of it at the given position and hides the real one.
    void rrSetFeedingChicken(Creature* creature, MovableGameEntity* chicken,
        const Ogre::Vector3& position);

    //! \brief Hatchery animals: scale, tint and procedural motion by kind and pose
    void rrCreateChickenLook(ChickenEntity* chicken);
    //! \brief Gives the entity back to the node of the animal and removes the look (accessories, fight links).
    void rrDestroyChickenLook(ChickenEntity* chicken);
    //! \brief The kind of the animal changed (chick grown up): new scale and tint.
    void rrUpdateChickenLook(ChickenEntity* chicken);
    //! \brief The egg hatched: shell pieces fly.
    void rrChickenHatched(ChickenEntity* chicken);
    //! \brief An egg was trampled at the position: shell pieces and yolk fly.
    void rrEggTrampled(const Ogre::Vector3& position);
    //! \brief The server-selected rooster/hen pair of the mounting pose.
    void rrChickenMount(ChickenEntity* rooster, ChickenEntity* hen);
    //! \brief Two roosters fight (phase as in ServerNotificationType::chickenFight): they are partners of each other
    //! until the fight is over.
    void rrChickenFight(ChickenEntity* first, ChickenEntity* second, uint32_t phase);
    //! \brief The server set a pose (see ChickenPose.h), an empty pose is the normal walking and idling.
    void rrSetChickenPose(ChickenEntity* chicken, const std::string& pose);
    //! \brief The server sent the nest places and the places of the loose feathers of a hatchery (by room name); the
    //! nests are drawn at the places. An empty list of nests means that the hatchery is gone.
    void rrSetHatcheryNests(const std::string& roomName, const std::vector<HatcheryNestField::Place>& places,
        const std::vector<HatcheryNestField::Place>& feathers);
    //! \brief Loose feathers next to a coop of the hatchery (the straw nests lie scattered over the hatchery, see updateNestFields)
    void rrCreateCoopDecor(BuildingObject* coop);
    //! \brief Removes the nest and the feathers next to the coop.
    void rrDestroyCoopDecor(BuildingObject* coop);
    //! \brief Makes the procedural meshes of the hatchery (egg in straw, comb, tail, feathers) if needed
    void rrEnsureChickenMesh(const std::string& meshName);

    //! \brief Toggles the creatures text overlay
    void rrSetCreaturesTextOverlay(GameMap& gameMap, bool value);

    //! \brief Toggles the creatures text overlay
    void rrTemporaryDisplayCreaturesTextOverlay(Creature* creature, Ogre::Real timeToDisplay);

    std::string rrBuildSkullFlagMaterial(const std::string& materialNameBase,
        const Ogre::ColourValue& color);

    //! \brief Does requested stuff for rendering in the minimap. Each time the minimap is rendered, this function will be
    //! called once before rendering is done with postRender = false and once when it is rendered with postRender = true.
    //! That allows to hide stuff that we don't want to display in the minimap
    void rrMinimapRendering(bool postRender, bool keepWorldLighting = false);

    Ogre::Light* addPointLightMenu(const std::string& name, const Ogre::Vector3& pos,
        const Ogre::ColourValue& diffuse, const Ogre::ColourValue& specular, Ogre::Real attenuationRange,
        Ogre::Real attenuationConstant, Ogre::Real attenuationLinear, Ogre::Real attenuationQuadratic);
    void removePointLightMenu(Ogre::Light* light);
    Ogre::Entity* addEntityMenu(const std::string& meshName, const std::string& entityName,
        const Ogre::Vector3& pos);
    void removeEntityMenu(Ogre::Entity* ent);
    Ogre::AnimationState* setMenuEntityAnimation(const std::string& entityName, const std::string& animation, bool loop);
    //! \brief Called to update the given animation with the given time. Returns true if animation ended and
    //! false otherwise
    bool updateMenuEntityAnimation(Ogre::AnimationState* animState, Ogre::Real timeSinceLastFrame);
    //! \brief Returns the scene node related to the given entity name
    Ogre::SceneNode* getMenuEntityNode(const std::string& entityName);
    const Ogre::Vector3& getMenuEntityPosition(Ogre::SceneNode* node);
    void updateMenuEntityPosition(Ogre::SceneNode* node, const Ogre::Vector3& pos);
    void orientMenuEntityPosition(Ogre::SceneNode* node, const Ogre::Vector3& direction);
    Ogre::ParticleSystem* addEntityParticleEffectMenu(Ogre::SceneNode* node,
        const std::string& particleName, const std::string& particleScript);
    void removeEntityParticleEffectMenu(Ogre::SceneNode* node,
        Ogre::ParticleSystem* particleSystem);
    Ogre::ParticleSystem* addEntityParticleEffectBoneMenu(const std::string& entityName,
        const std::string& boneName, const std::string& particleName, const std::string& particleScript);
    void removeEntityParticleEffectBoneMenu(const std::string& entityName,
        Ogre::ParticleSystem* particleSystem);
    Ogre::Quaternion getNodeOrientation(Ogre::SceneNode* node);
    Ogre::Viewport* getViewport(){return mViewport;}
    void setProgressiveNodeOrientation(Ogre::SceneNode* node, Ogre::Real progress,
        const Ogre::Quaternion& angleSrc, const Ogre::Quaternion& angleDest);
    void setScaleMenuEntity(Ogre::SceneNode* node, const Ogre::Vector3& absSize);
    const Ogre::Vector3& getMenuEntityScale(Ogre::SceneNode* node);

    Ogre::RenderTarget* mRenderTarget;
    bool m_ZPrePassEnabled;
    
private:
    Ogre::InstanceManager* mInstanceManagerDirt;
    Ogre::InstanceManager* mInstanceManagerCloud;
        
    Ogre::DefaultDebugDrawer ddd;

    // Ogre::TexturePtr perlinFog;
    
    //! \brief Correctly places entities in hand next to the keeper hand
    void changeRenderQueueRecursive(Ogre::SceneNode* node, uint8_t renderQueueId);


    template<typename Manager> bool removeIfExists(std::string, std::string);
    //! \brief Correctly places entities in hand next to the keeper hand
    void rrOrderHand(Player* localPlayer);
    void rrUpdateHeldCreature();

    //! \brief Colorize the material with the corresponding team id color.
    //! \note If the material (wall tiles only) is marked for digging, a yellow color is added
    //! to the given color.
    //! \returns The new material name according to the current colorization.
    std::string colourizeMaterial(const std::string& materialName, const Seat* seat, bool markedForDigging, bool playerHasVision);

    //! \brief Colorize an entity with the team corresponding color.
    //! \Note: if the entity is marked for digging (wall tiles only), then a yellow color
    //! is added to the current colorization.
    void colourizeEntity(Ogre::Entity* ent, const Seat* seat, bool markedForDigging, bool playerHasVision);

    //! \brief Makes the material be transparent with the given opacity (0.0f - 1.0f)
    //! \returns The new material name according to the current opacity.
    std::string setMaterialOpacity(const std::string& materialName, float opacity);

    //! \brief Disables all animations of the given entity and starts the given one
    Ogre::AnimationState* setEntityAnimation(Ogre::Entity* ent, const std::string& animation, bool loop);

    //! \brief The main scene manager reference. Don't delete it.
    Ogre::SceneManager* mSceneManager;

    //! \brief Reference to the Ogre sub scene nodes. Don't delete them.
    Ogre::SceneNode* mDraggableSceneNode;
    Ogre::SceneNode* mTileSceneNode;
    Ogre::SceneNode* mRoomSceneNode;
    Ogre::SceneNode* mCreatureSceneNode;
    Ogre::SceneNode* mLightSceneNode;
    Ogre::SceneNode* mMainMenuSceneNode;

    Ogre::AnimationState* mHandAnimationState;
    std::string mHandPose = "Idle";
    Ogre::ManualObject* mHandPickaxe = nullptr;
    Ogre::Entity* mHandHammer = nullptr;
    Ogre::ManualObject* mHandIdleProp = nullptr;
    Ogre::Vector3 mHammerStrikePoint = Ogre::Vector3::ZERO;
    Ogre::ManualObject* mTilePreview = nullptr;

    struct CreatureCombatImpactEffect
    {
        Creature* mCreature;
        Ogre::SceneNode* mNode;
        Ogre::ParticleSystem* mParticleSystem;
        Ogre::Real mRemainingTime;
    };
    std::vector<CreatureCombatImpactEffect> mCreatureCombatImpactEffects;

    struct CreatureCombatReaction
    {
        Creature* mCreature;
        Ogre::Entity* mEntity;
        Ogre::AnimationState* mAnimation;
        Ogre::SkeletonAnimationBlendMode mPreviousBlendMode;
    };
    std::vector<CreatureCombatReaction> mCreatureCombatReactions;
    uint64_t mCreatureCombatEffectNumber = 0;
    std::map<Creature*, uint32_t> mCreatureAttackVariants;

    enum class CreatureFeedingStyle
    {
        peck,
        lunge,
        heavy,
        humanoid,
        magical,
        coil
    };

    struct CreatureFeedingBone
    {
        Ogre::Bone* mBone;
        Ogre::Vector3 mPosition;
        Ogre::Quaternion mOrientation;
        bool mWasManual;
        Ogre::Bone* mDriver = nullptr;
        Ogre::Vector3 mScale = Ogre::Vector3::UNIT_SCALE;
    };
    struct CreatureFeedingLimb
    {
        Ogre::Bone* mUpper;
        Ogre::Bone* mLower;
        Ogre::Bone* mTip;
        Ogre::Vector3 mTipOffset;
        Ogre::Vector3 mRestTip;
        Ogre::Vector3 mGripOffset = Ogre::Vector3::ZERO;
    };
    struct CreatureFeedingAnimation
    {
        Creature* mCreature;
        Ogre::SceneNode* mNode;
        Ogre::Entity* mEntity;
        Ogre::Vector3 mBasePosition;
        Ogre::Quaternion mBaseOrientation;
        Ogre::Vector3 mBaseScale;
        Ogre::Real mElapsed;
        CreatureFeedingStyle mStyle;
        Ogre::AnimationState* mAnimation;
        Ogre::SceneNode* mChickenNode;
        Ogre::Entity* mChickenEntity;
        Ogre::Vector3 mChickenStart;
        Ogre::Vector3 mChickenScale;
        Ogre::Bone* mHead;
        unsigned int mFeatherBursts;
        std::vector<CreatureFeedingBone> mReachBones;
        std::vector<Ogre::Bone*> mRoots;
        Ogre::Bone* mSpine = nullptr;
        CreatureFeedingLimb mArms[2] = {};
        CreatureFeedingLimb mLegs[2] = {};
        //! The eaten animal is a rooster: its feathers have the colours of the rooster
        bool mRoosterFeathers = false;
        Ogre::Bone* mJaw = nullptr;
        Ogre::Real mMouthForward = 0.0f;
        Ogre::Quaternion mChickenOrientation = Ogre::Quaternion::IDENTITY;
    };
    std::vector<CreatureFeedingAnimation> mCreatureFeedingAnimations;

    struct ChickenFeatherEffect
    {
        Ogre::SceneNode* mNode;
        Ogre::ParticleSystem* mParticleSystem;
        Ogre::Real mRemainingTime;
    };
    std::vector<ChickenFeatherEffect> mChickenFeatherEffects;
    uint64_t mChickenFeatherEffectNumber = 0;

    //! \brief Per animal state of the look: the child node that carries scale and motion, the pose and its timers,
    //! the rooster accessories and the fight link.
    struct ChickenLook
    {
        Ogre::SceneNode* mNode;
        Ogre::Entity* mEntity;
        std::vector<Ogre::Entity*> mAccessories;
        std::string mPose;
        Ogre::Real mTime;
        Ogre::Real mPoseTime;
        Ogre::Real mPhase;
        int mFeatherBursts;
        //! The feathers of the coop exit are still to fly (they fly when the animal walks through the door)
        bool mEmergeFeathersPending;
        //! The door of the coop still has to swing again when the animal reaches it
        bool mCoopDoorReplayPending;
        //! The animal walks out of the coop (the flags above end when it stops)
        bool mCoopExitWalking;
        //! The rooster this one fights (set by the server event), the first of the two makes the feather clouds
        ChickenEntity* mFightPartner;
        bool mFightLeader;
        Ogre::Real mFightTimer;
        //! The hen the rooster climbs on while he mounts (set when the pose starts, null when none is near)
        ChickenEntity* mMountPartner;
        //! How far the hen is ducked down under the rooster (0 to 1), set by the rooster every frame
        Ogre::Real mMountCrouch;
        //! The clip phase of the rooster while he mounts: 1 climbing on, 2 treading, 3 climbing down (0 when not mounting)
        int mMountPhase;
        //! The egg lies in a nest of a coop: its own straw is hidden, the nest has straw
        bool mNestEgg;
    };
    std::map<ChickenEntity*, ChickenLook> mChickenLooks;

    //! \brief Nest and loose feathers next to a coop, with the timer of the door shake.
    struct CoopDecor
    {
        Ogre::SceneNode* mNode;
        Ogre::Real mShake;
        //! The door clip of the coop mesh, null for the old coop mesh (the coop shakes then)
        Ogre::AnimationState* mDoor;
    };
    std::map<BuildingObject*, CoopDecor> mCoopDecors;
    //! The nest places the server sent, by room name (the clients compute nothing). mVersion counts the messages.
    struct ServerNests
    {
        ServerNests() :
            mVersion(0)
        {}

        std::vector<HatcheryNestField::Place> mPlaces;
        std::vector<HatcheryNestField::Place> mFeathers;
        uint32_t mVersion;
    };
    std::map<std::string, ServerNests> mServerNests;
    //! The straw nests of one hatchery (one entity each, at the places the server sent). mKey is the version of
    //! the places they were made for.
    struct NestField
    {
        std::vector<Ogre::SceneNode*> mNodes;
        std::vector<Ogre::Entity*> mEntities;
        uint32_t mKey;
    };
    std::map<std::string, NestField> mNestFields;
    //! Draws the server-provided nests by room name (a client has no Room objects of its own). A nest shows while
    //! its tile is visible to the local player and still belongs to a hatchery.
    void updateNestFields();
    //! \brief Removes the nest entities and nodes of one hatchery.
    void destroyNestField(NestField& field);
    //! Set while a game is shown
    GameMap* mGameMap = nullptr;
    Ogre::Real mCoopDecorTimer = 0.0f;
    uint64_t mChickenLookNumber = 0;

    struct CreatureSleepAnimation
    {
        Creature* mCreature;
        Ogre::Entity* mEntity;
        Ogre::SceneNode* mNode;
        Ogre::Vector3 mBaseScale;
        Ogre::AnimationState* mAnimation;
        Ogre::Real mElapsed;
        bool mNativeEntry;
        Ogre::Vector3 mBasePosition, mRestPosition;
        Ogre::Quaternion mBaseOrientation, mRestOrientation;
    };
    std::vector<CreatureSleepAnimation> mCreatureSleepAnimations;
    std::set<Creature*> mSteppingCreatures;

    struct CreatureDropAnimation
    {
        Creature* mCreature;
        Ogre::SceneNode* mNode;
        Ogre::Vector3 mStart;
        Ogre::Vector3 mEnd;
        Ogre::Quaternion mStartOrientation;
        Ogre::Quaternion mLieOrientation;
        Ogre::Vector3 mLiePosition;
        Ogre::Real mElapsed;
        bool mLieOnGround;
        bool mUseFallbackLie;
    };
    std::vector<CreatureDropAnimation> mCreatureDropAnimations;

    struct CreatureGroundPose
    {
        Creature* mCreature;
        Ogre::SceneNode* mNode;
        Ogre::Quaternion mStandingOrientation;
        Ogre::Real mStandingZ;
    };
    std::vector<CreatureGroundPose> mCreatureGroundPoses;

    struct CreatureGetUpAnimation
    {
        Creature* mCreature;
        Ogre::SceneNode* mNode;
        Ogre::AnimationState* mAnimationState;
        Ogre::Quaternion mStartOrientation;
        Ogre::Quaternion mEndOrientation;
        Ogre::Vector3 mStartPosition;
        Ogre::Vector3 mEndPosition;
        Ogre::Real mElapsed;
        bool mUseFallback;
    };
    std::vector<CreatureGetUpAnimation> mCreatureGetUpAnimations;

    void cancelCreatureDropAnimation(Creature* creature);
    void cancelCreatureGetUpAnimation(Creature* creature);
    void startCreatureGetUpAnimation(Creature* creature);
    void restoreCreatureGroundPose(Creature* creature);
    void setCreatureDropGroundAnimation(Creature* creature);
    struct RoomConstructionEffect
    {
        std::string mNodeName;
        std::string mParticleName;
        Ogre::Real mRemainingTime;
    };
    std::vector<RoomConstructionEffect> mRoomConstructionEffects;
    uint64_t mRoomConstructionEffectNumber = 0;

    void clearCreatureCombatEffects(Creature* creature = nullptr);
    //! \brief Starts the meal animation of a creature that eats a chicken and picks the style for its mesh.
    void startCreatureFeedingAnimation(Creature* creature, Ogre::Entity* entity);
    //! \brief Looks up the arm, leg, head and spine bones of the feeding creature and stores their rest pose.
    void prepareCreatureFeedingReach(CreatureFeedingAnimation& feeding);
    //! \brief Poses the body of the feeding creature for one frame of the meal.
    //! \param progress Progress of the whole meal from 0 to 1.
    //! \param reach How far the arms reach for the chicken (0 to 1).
    //! \param lift How far the chicken is lifted from the ground to the hold position (0 to 1); the crouch shrinks with it.
    //! \param release How far the hands let go of the chicken (0 to 1).
    //! \param dip How far the head dips towards the chicken during a bite (0 to 1).
    //! \param biteLean Extra forward lean of the spine in degrees during a bite.
    //! \return The middle between the two hands where the chicken is gripped.
    Ogre::Vector3 updateCreatureFeedingReach(CreatureFeedingAnimation& feeding, Ogre::Real progress,
        Ogre::Real reach, Ogre::Real lift, Ogre::Real release, Ogre::Real dip, Ogre::Real biteLean);
    //! \brief Gets the position of the mouth of the feeding creature in the space of its node.
    //! \param height Height of the creature mesh.
    //! \param chickenHeight Height of the chicken, used to keep the chicken in front of the jaws.
    //! \param withoutHands True when the chicken is taken into the mouth instead of held in the hands.
    Ogre::Vector3 getCreatureFeedingMouth(const CreatureFeedingAnimation& feeding, Ogre::Real height,
        Ogre::Real chickenHeight, bool withoutHands) const;
    void cancelCreatureFeedingAnimation(Creature* creature = nullptr);
    //! \brief Starts a short feather (or egg shell) particle effect at the position.
    //! \param position World position of the effect.
    //! \param particleName Name of the particle template to use.
    void createChickenFeatherEffect(const Ogre::Vector3& position, const std::string& particleName = "ChickenFeathers");
    //! True when the animal is in the scene, in the view of the camera and plays the clip (feathers only fly for that).
    bool chickenClipVisible(ChickenEntity* chicken, const std::string& clip) const;
    //! \brief Moves every animal look (pose motion, fight clouds) and the coop decor for the elapsed time.
    //! \param timeSinceLastFrame Seconds since the last frame.
    void updateChickenLooks(Ogre::Real timeSinceLastFrame);
    //! \brief Sets scale, tint and rooster accessories of the animal for its current kind.
    void applyChickenKindLook(ChickenEntity* chicken);
    //! \brief Forgets all animal looks and coop decors without touching the scene.
    void clearChickenLooks();
    void clearChickenFeatherEffects();
    void startCreatureSleepAnimation(Creature* creature, Ogre::Entity* entity);
    void fitCreatureToBed(CreatureSleepAnimation& sleeping);
    void cancelCreatureSleepAnimation(Creature* creature = nullptr);
    void updateCreatureStep(Creature* creature);
    void cancelCreatureStep(Creature* creature = nullptr);
    void clearRoomConstructionEffects();


    Ogre::TexturePtr m_texture;

    Ogre::Viewport* mViewport;
    Ogre::RTShader::ShaderGenerator* mShaderGenerator;

    //! For the keeper hand
    Ogre::SceneNode* mHandKeeperNode;
    Ogre::SceneNode* mHeldCreatureGrip = nullptr;
    Ogre::SceneNode* mHeldCreatureStorage = nullptr;
    bool mHeldCreatureDisplayEnabled = false;
    Ogre::SceneNode* mDummyNode;
    Ogre::SceneNode* mHandLightNode;
    Ogre::SceneNode* mHandLightNode2;
    Ogre::Camera* mShadowCam;
    Ogre::Radian mCurrentFOVy;
    Ogre::Real mCurrentAspectRatio;
    Ogre::Real mFactorWidth;
    Ogre::Real mFactorHeight;

    // As a workaround for some issues, we create dummy entities too small to be seen
    // and attach them to the keeper hand. This vector allows to keep a track and delete
    // them/recreate when loading a new game
    std::vector<Ogre::SceneNode*> mDummyEntities;

    //! \brief True if the creatures are currently displaying their text overlay
    bool mCreatureTextOverlayDisplayed;

    //! Bit array to allow to display tile hand (= 0) or not (!= 0)
    uint32_t mHandKeeperHandVisibility;
};

#endif // RENDERMANAGER_H
