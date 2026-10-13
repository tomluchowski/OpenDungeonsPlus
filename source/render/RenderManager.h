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
#include "render/TreasuryCreatureRules.h"
#include "render/TreasuryGoldBatch.h"

class DraggableTileContainer;
class GameMap;
class Building;
class Seat;
class Tile;
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
    //! \brief The "Treasury detail" option changed: every pile, floor gold heap and thief sack is drawn again for the
    //! new setting (the piles and heaps a few per frame, see updateTreasuryRebuild).
    void rrTreasuryDetailChanged();
    void rrCreateCreature(Creature* curCreature);
    void rrDestroyCreature(Creature* curCreature);
    //! Shows, resizes or removes the sack of a thief according to the gold it carries (as sent by the server)
    void rrRefreshCreatureGoldSack(Creature* creature);
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
    //! A worker poured gold onto the treasury tile (x, y): coins fall onto the top of the pile
    void rrTreasuryDeposit(GameMap* gameMap, int x, int y);
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
    void rrSetFeedingChicken(Creature* creature, MovableGameEntity* chicken,
        const Ogre::Vector3& position);

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

    //! \brief State of a creature that lies down in a bed or sleeps in it.
    struct CreatureSleepAnimation
    {
        Creature* mCreature;
        Ogre::Entity* mEntity;
        Ogre::SceneNode* mNode;
        Ogre::Vector3 mBaseScale;
        Ogre::AnimationState* mAnimation;
        //! Seconds since the creature started to lie down.
        Ogre::Real mElapsed;
        //! True if the mesh has its own sleep entry animation, false if one is generated.
        bool mNativeEntry;
        //! Position and orientation of the node before sleeping and when lying in the bed.
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

    //! The kinds of treasury effect; each has a budget per room of its own
    enum class TreasuryEffectKind
    {
        splash,
        ambient
    };
    struct TreasuryEffect
    {
        std::string mName;
        Ogre::Real mRemaining;
        const void* mRoomKey;
        TreasuryEffectKind mKind;
    };
    std::vector<TreasuryEffect> mTreasuryEffects;
    TreasuryCreatureRules::SplashBudget mTreasurySplashBudget;
    //! Sparkles, sliding coins and rolling coins on rich piles have a budget per room of their own
    TreasuryCreatureRules::SplashBudget mTreasuryAmbientBudget;
    Ogre::Real mTreasuryAmbientTimer = 0.0f;
    size_t mTreasuryAmbientCursor = 0;
    //! Where a creature last splashed coins, to space the splashes along its way
    std::map<Creature*, Ogre::Vector2> mTreasuryLastSplash;
    int mTreasuryEffectNumber = 0;

    //! A worker pouring its gold out on top of a pile (procedural climb and tilt of the render node)
    struct TreasuryPour
    {
        Creature* mCreature;
        Ogre::Real mElapsed;
        int mLevel;
        float mRise;
        Ogre::Quaternion mTilt;
        //! Skeleton clips of the worker (climb and pour); null when the mesh has none (procedural motion)
        Ogre::Entity* mEntity;
        Ogre::AnimationState* mClip;
        int mPhase;
    };
    std::vector<TreasuryPour> mTreasuryPours;

    //! The gold piles of this client with the node type they were created for and whether they use the reduced
    //! mesh because they are far from the camera. The level of detail switches piles by creating their mesh again;
    //! a change of the treasury detail option does the same for all of them.
    struct TreasuryPileInfo
    {
        NodeType mNodeType;
        bool mFar;
    };
    std::map<RenderedMovableEntity*, TreasuryPileInfo> mTreasuryPiles;
    //! While a pile is created again for the level of detail: 1 reduced, 0 full (-1 when not, the distance decides)
    int mTreasuryPileFarOverride = -1;
    Ogre::Real mTreasuryLodTimer = 0.0f;
    //! Piles (true) and floor gold heaps (false) waiting to be drawn again after the detail option changed
    std::vector<std::pair<RenderedMovableEntity*, bool> > mTreasuryRebuildQueue;
    size_t mTreasuryRebuildIndex = 0;
    //! The piles of a treasury are drawn as one batch per room and patch of tiles
    TreasuryGoldBatch mTreasuryBatch;

    //! A pile that grows or sinks: its node settles to the new height over a short time
    struct TreasuryPileSettle
    {
        std::string mEntityName;
        Ogre::SceneNode* mNode;
        Ogre::Real mElapsed;
        float mFrom;
        bool mTaken;
        //! True when the taken pile does not dip as a whole but shows a local dent (see TreasuryPileDent)
        bool mLocalDent;
    };
    std::vector<TreasuryPileSettle> mTreasuryPileSettles;
    //! The dent where gold was taken: while it lasts a dynamic copy of the pile is drawn in place of its entity
    //! (the entity is detached from its node and does not take part in the room batch), and given back afterwards
    struct TreasuryPileDent
    {
        std::string mEntityName;
        std::string mOgreName;
        Ogre::SceneNode* mNode;
        Ogre::Entity* mEntity;
        Ogre::ManualObject* mObject;
        std::string mMeshName;
        float mU;
        float mV;
        Ogre::Real mElapsed;
    };
    std::vector<TreasuryPileDent> mTreasuryPileDents;
    int mTreasuryDentNumber = 0;
    //! An object standing in the gold of a treasury, drawn partly buried: only its node is lifted by mCurrent
    struct TreasuryBuriedObject
    {
        float mHeight;
        float mCurrent;
        float mTarget;
    };
    std::map<RenderedMovableEntity*, TreasuryBuriedObject> mTreasuryBuriedObjects;

    //! A thief carrying gold shows a sack of coins, its size follows the amount the server sends
    struct TreasuryThiefSack
    {
        Creature* mCreature;
        std::string mSackName;
        std::string mSackMesh;
    };
    std::vector<TreasuryThiefSack> mTreasuryThiefSacks;
    //! Set while a game is shown; the sacks of the thieves look up the creatures of the game there
    GameMap* mGameMap = nullptr;

    void clearCreatureCombatEffects(Creature* creature = nullptr);
    void startCreatureFeedingAnimation(Creature* creature, Ogre::Entity* entity);
    void prepareCreatureFeedingReach(CreatureFeedingAnimation& feeding);
    Ogre::Vector3 updateCreatureFeedingReach(CreatureFeedingAnimation& feeding, Ogre::Real progress);
    void cancelCreatureFeedingAnimation(Creature* creature = nullptr);
    void createChickenFeatherEffect(const Ogre::Vector3& position);
    void clearChickenFeatherEffects();
    //! \brief Starts the movement of the creature into its bed.
    void startCreatureSleepAnimation(Creature* creature, Ogre::Entity* entity);
    //! \brief Finds the nearest bed and computes where the creature has to lie on it.
    void fitCreatureToBed(CreatureSleepAnimation& sleeping);
    //! \brief Ends the sleep animation of the given creature, or of all creatures if null,
    //! and restores position, orientation and scale.
    void cancelCreatureSleepAnimation(Creature* creature = nullptr);
    void updateCreatureStep(Creature* creature);
    void cancelCreatureStep(Creature* creature = nullptr);
    void clearRoomConstructionEffects();
    //! \brief The creatures on the tile follow the gold of its pile (a pile was created or removed there).
    void refreshCreaturesOnTile(Tile* tile);
    //! \brief A creature walked on deep gold: splashes coins and clinks after every step of the given length.
    void treasuryCreatureStep(Creature* creature, const Ogre::Vector3& position, float surfaceHeight, int level);
    //! \brief The worker standing on the tile climbs the pile and tips its gold out (only what is in view).
    void startTreasuryPour(Tile* tile, int level);
    //! \brief Moves the pouring workers (clips or procedural climb and tilt).
    void updateTreasuryPours(Ogre::Real timeSinceLastFrame);
    //! \brief Ends the pour of the creature (of every creature without a creature) and straightens it up.
    void cancelTreasuryPour(Creature* creature);
    //! \brief The extra height of the pouring worker above the gold of its tile (0 when it does not pour).
    float getTreasuryPourRise(Creature* creature) const;
    //! \brief Starts an effect of the given kind at the position, when it is in view and its room still has a place
    //! in the budget of the kind. Returns whether the effect was started.
    bool createTreasuryEffect(const void* roomKey, const std::string& script, const Ogre::Vector3& position,
        TreasuryEffectKind kind = TreasuryEffectKind::splash);
    //! \brief Ends the effects whose time is over and gives their places back to the budget of their room.
    void updateTreasuryEffects(Ogre::Real timeSinceLastFrame);
    //! \brief Now and then starts sliding coins or a sparkle on a rich pile in view.
    void updateTreasuryAmbient(Ogre::Real timeSinceLastFrame);
    //! \brief A pile of a tile was created with another level than before: it settles to its new height, and a taken
    //! pile gets a dent and throws coins.
    void startTreasuryPileChange(Ogre::SceneNode* node, const std::string& entityName, Tile* tile, int oldLevel,
        int newLevel, Ogre::Entity* entity, const std::string& pileMeshName);
    //! \brief Lets the local dents of the piles that lose gold form and fill up again.
    void updateTreasuryDents(Ogre::Real timeSinceLastFrame);
    //! \brief Ends the dent of the pile: its entity is drawn again, the dynamic copy is destroyed.
    void finishTreasuryDent(const std::string& entityName);
    //! \brief Lets the piles that grow or sink settle to their new height.
    void updateTreasuryPileSettles(Ogre::Real timeSinceLastFrame);
    //! \brief Ends the settling of the pile at once (the entity of the pile goes away).
    void cancelTreasuryPileSettle(const std::string& entityName);
    //! \brief True while the pile of that entity settles to a new height.
    bool isTreasuryPileSettling(const std::string& entityName) const;
    //! \brief Lists an object (or gold) standing in the gold of a treasury; it is drawn partly buried.
    void registerBuriedObject(RenderedMovableEntity* entity, float objectHeight);
    //! \brief The pile of the tile changed: the objects standing on it follow its new height.
    void refreshBuriedObjectsOnTile(Tile* tile);
    //! \brief Lets the objects standing in the gold sink or rise smoothly to their depth.
    void updateTreasuryBuriedObjects(Ogre::Real timeSinceLastFrame);
    //! \brief The height by which the node of the object is lifted so that it stands partly buried in the gold of
    //! its tile (0 when it is not listed or is carried). With settleAtOnce the object takes that height at once.
    float getBuriedLift(RenderedMovableEntity* entity, bool settleAtOnce);
    //! \brief Distance of a point to the camera in tiles (0 without a camera).
    float getTreasuryCameraDistance(const Ogre::Vector3& position) const;
    //! \brief Switches piles between the full and the reduced mesh by their distance to the camera.
    void updateTreasuryLod(Ogre::Real timeSinceLastFrame);
    //! \brief Draws a few of the piles queued by rrTreasuryDetailChanged again.
    void updateTreasuryRebuild();
    //! \brief Removes the sack that a thief carries on its back (nothing happens when it has none).
    void removeTreasuryThiefSack(Creature* creature);
    //! \brief Forgets the effects, the piles, the buried objects and the sacks and destroys the draw batches (the
    //! scene is cleared or destroyed).
    void clearTreasuryEffects();


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
