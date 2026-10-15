/*
 *  Copyright (C) 2011-2017  OpenDungeons Team
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

#include "entities/Creature.h"
#include "entities/CreatureProgression.h"

#include "ODApplication.h"
#include "creatureaction/CreatureAction.h"
#include "creatureaction/CreatureActionClaimGroundTile.h"
#include "creatureaction/CreatureActionClaimWallTile.h"
#include "creatureaction/CreatureActionDigTile.h"
#include "creatureaction/CreatureActionFight.h"
#include "creatureaction/CreatureActionFindHome.h"
#include "creatureaction/CreatureActionFlee.h"
#include "creatureaction/CreatureActionGetFee.h"
#include "creatureaction/CreatureActionGoCallToWar.h"
#include "creatureaction/CreatureActionGoDefendHeart.h"
#include "creatureaction/CreatureActionGrabEntity.h"
#include "creatureaction/CreatureActionLeaveDungeon.h"
#include "creatureaction/CreatureActionParkToTile.h"
#include "creatureaction/CreatureActionReloadTrap.h"
#include "creatureaction/CreatureActionPossessed.h"
#include "creatureaction/CreatureActionSearchEntityToCarry.h"
#include "creatureaction/CreatureActionSearchFood.h"
#include "creatureaction/CreatureActionSearchGroundTileToClaim.h"
#include "creatureaction/CreatureActionSearchJob.h"
#include "creatureaction/CreatureActionSearchTileToDig.h"
#include "creatureaction/CreatureActionSearchWallTileToClaim.h"
#include "creatureaction/CreatureActionSleep.h"
#include "creatureaction/CreatureActionStealFreeGold.h"
#include "creatureaction/CreatureActionUseRoom.h"
#include "creatureaction/CreatureActionWalkToTile.h"
#include "creaturebehaviour/CreatureBehaviour.h"
#include "creatureeffect/CreatureEffect.h"
#include "creatureeffect/CreatureEffectDigTile.h"
#include "creatureeffect/CreatureEffectManager.h"
#include "creatureeffect/CreatureEffectSlap.h"
#include "creaturemood/CreatureMood.h"
#include "creaturemood/CreatureMoodManager.h"
#include "creatureskill/CreatureSkill.h"


#include "entities/ChickenEntity.h"
#include "entities/CreatureDefinition.h"
#include "entities/CreatureMoodValues.h"
#include "entities/GameEntityType.h"
#include "entities/Tile.h"
#include "entities/TreasuryObject.h"
#include "entities/Weapon.h"



#include "game/Player.h"
#include "game/Skill.h"
#include "game/SkillType.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/SandboxMode.h"
#include "gamemap/LevelScript.h"
#include "gamemap/LevelScriptRunner.h"
#include "gamemap/Pathfinding.h"
#include "gamemap/RoomObjectNavigation.h"
#include "giftboxes/GiftBoxSkill.h"

#include "modes/GameEditorModeConsole.h"

#include "network/ODClient.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "render/CreatureOverlayStatus.h"
#include "render/RenderManager.h"
#include "rooms/RoomCrypt.h"
#include "rooms/RoomDormitory.h"
#include "rooms/RoomPrison.h"
#include "rooms/RoomArena.h"
#include "sound/SoundEffectsManager.h"
#include "spells/Spell.h"
#include "spells/SpellType.h"
#include "traps/Trap.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/MakeUnique.h"
#include "utils/Random.h"

#include <CEGUI/Event.h>
#include <CEGUI/System.h>
#include <CEGUI/UDim.h>
#include <CEGUI/Vector.h>
#include <CEGUI/WindowManager.h>
#include <CEGUI/Window.h>
#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/widgets/PushButton.h>

#include <OgreQuaternion.h>
#include <Ogre.h>
#include <Ogre.h>

#include <cmath>
#include <algorithm>



static const Ogre::Real CANNON_MISSILE_HEIGHT = 0.3;

//! \brief Casino mood points lost per second when the room config has no CasinoMoodDecay
static const double DEFAULT_CASINO_MOOD_DECAY_PER_SECOND = 3.0;
//! \brief Largest casino mood (positive or negative) when the room config has no CasinoMoodMax
static const double DEFAULT_CASINO_MOOD_MAX = 1500.0;
//! \brief Pit mood points lost per second when the room config has no PitMoodDecay
static const double DEFAULT_PIT_MOOD_DECAY_PER_SECOND = 3.0;
//! \brief Largest pit mood (positive or negative) when the room config has no PitMoodMax
static const double DEFAULT_PIT_MOOD_MAX = 1500.0;
//! \brief Radius in tiles of the hated company check when the room config has no HatedCompanyRadius
static const double DEFAULT_HATED_COMPANY_RADIUS = 5.0;
//! \brief Squared distance in tiles (3 tiles) up to which a creature in a possession group stays where it is instead of walking to the leader
static const int POSSESSION_GROUP_MAX_SQUARED_DISTANCE = 9;
//! \brief Distance in tiles below which a target is too close for the aim direction check of a possession skill
static const Ogre::Real POSSESSION_AIM_MIN_DISTANCE = 0.5;
//! \brief Squared length below which a possession move direction counts as no movement
static const Ogre::Real POSSESSION_MIN_DIRECTION_SQUARED_LENGTH = 0.0001f;
//! \brief Squared length below which a possession aim direction counts as no aim
static const Ogre::Real POSSESSION_MIN_AIM_SQUARED_LENGTH = 0.0001f;
//! \brief Number of closest targets for which a path is tried when a creature looks for a destination
static const uint32_t MAX_PATH_CHECK_TARGET_TRIES = 5;

// Target selection by combat class: the distance to an enemy support creature is multiplied by this
// factor for blitzers and flankers, so they prefer it over closer enemies
static const double COMBAT_CLASS_SUPPORT_TARGET_FACTOR = 0.5;

//! \brief Returns the factor applied to the squared distance of a potential target. Lower means preferred.
//! Blockers and support creatures attack the nearest enemy. Blitzers and flankers prefer enemy support creatures.
static double getTargetDistanceFactor(const Creature& attacker, const GameEntity& target)
{
    if(target.getObjectType() != GameEntityType::creature)
        return 1.0;

    const CreatureDefinition::CombatClass attackerClass = attacker.getDefinition()->getCombatClass();
    if((attackerClass != CreatureDefinition::CombatBlitzer) && (attackerClass != CreatureDefinition::CombatFlanker))
        return 1.0;

    const Creature& targetCreature = static_cast<const Creature&>(target);
    if(targetCreature.getDefinition()->isWorker())
        return 1.0;

    if(targetCreature.getDefinition()->getCombatClass() == CreatureDefinition::CombatSupport)
        return COMBAT_CLASS_SUPPORT_TARGET_FACTOR;

    return 1.0;
}

const int32_t Creature::NB_TURNS_BEFORE_CHECKING_TASK = 15;
const uint32_t Creature::NB_OVERLAY_HEALTH_VALUES = 8;

CreatureParticleEffect::CreatureParticleEffect(Creature& creature, const std::string& name, const std::string& script, int32_t nbTurnsEffect,
        CreatureEffect* effect) :
    EntityParticleEffect(name, script, nbTurnsEffect),
    mEffect(effect),
    mCreature(creature)
{
    if(mEffect == nullptr)
    {
        OD_LOG_ERR("null effect on creature=" + mCreature.getName() + ", name=" + name + ", script=" + script);
        return;
    }

    mEffect->startEffect(mCreature);
}

CreatureParticleEffect::~CreatureParticleEffect()
{
    if(mEffect != nullptr)
    {
        // We don't call release here as the
        // creature object is already partially destroyed.
        // mEffect->releaseEffect(mCreature);
        delete mEffect;
    }
    
}

Creature::Creature(GameMap* gameMap, const CreatureDefinition* definition, Seat* seat, Ogre::Vector3 position) :

    MovableGameEntity        (gameMap),
    parkingBit               (false),
    parkedBit                (false),
    mPhysicalDefense         (3.0),
    mMagicalDefense          (1.5),
    mElementDefense          (0.0),
    mModifierStrength        (1.0),
    mWeaponL                 (nullptr),
    mWeaponR                 (nullptr),
    mHomeTile                (nullptr),
    mDefinition              (definition),
    mHasVisualDebuggingEntities (false),
    mWakefulness             (100.0),
    mHunger                  (0.0),
    mLevel                   (1),
    mHp                      (10.0),
    mMaxHP                   (10.0),
    mExp                     (0.0),
    mGroundSpeed             (1.0),
    mWaterSpeed              (0.0),
    mLavaSpeed               (0.0),
    mDigRate                 (0.0),
    mClaimRate               (0.0),
    mDeathCounter            (0),
    mChampionTurns           (0),
    mJobCooldown             (0),
    mGoldFee                 (0),
    mGoldCarried             (0),
    mSkillTypeDropDeath      (SkillType::nullSkillType),
    mWeaponDropDeath         ("none"),
    mStatsWindow             (nullptr),
    mNbTurnsWithoutBattle    (0),
    mCasinoMood              (0.0),
    mCarriedEntity           (nullptr),
    mMoodCooldownTurns       (0),
    mMoodValue               (gameMap->isServerGameMap() ? CreatureMoodLevel::Neutral : CreatureMoodLevel::Unknown),
    mMoodPoints              (0),
    mPrayerRelief            (0),
    mNbTurnFurious           (-1),
    mOverlayHealthValue      (0),
    mOverlayMoodValue        (CreatureMoodValues::Nothing),
    mOverlayStatus           (nullptr),
    mNeedFireRefresh         (false),
    mDropCooldown            (0),
    mSpeedModifier           (1.0),
    mKoTurnCounter           (0),
    mSeatPrison              (nullptr),
    mNbTurnsTorture          (0),
    mNbTurnsPrison           (0),
    mActiveSlapsCount        (0),
    mIsHexenHen              (false),
    mHexenHenMeshShown       (false),
    mNbTurnsInHand           (0),
    mIsInHand                (false),
    mNbTurnsOutOfWork        (0),
    mNbTurnsTortureMood      (0),
    mNbTurnsRested           (0),
    mTorturedThisTurn        (false),
    mRestedThisTurn          (false),
    mNbTurnsHatedCompany     (0),
    mPitMood                 (0.0)
{
    //TODO: This should be set in initialiser list in parent classes
    setSeat(seat);
    mPosition = position;
    setMeshName(definition->getMeshName());
    setName(getGameMap()->nextUniqueNameCreature(definition->getClassName()));

    mMaxHP = mDefinition->getMinHp();
    setHP(mMaxHP);

    mGroundSpeed = mDefinition->getMoveSpeedGround();
    mWaterSpeed = mDefinition->getMoveSpeedWater();
    mLavaSpeed = mDefinition->getMoveSpeedLava();

    mDigRate = mDefinition->getDigRate();
    mClaimRate = mDefinition->getClaimRate();

    // Fighting stats
    mPhysicalDefense = mDefinition->getPhysicalDefense();
    mMagicalDefense = mDefinition->getMagicalDefense();
    mElementDefense = mDefinition->getElementDefense();

    if(mDefinition->getWeaponSpawnL().compare("none") != 0)
        mWeaponL = gameMap->getWeapon(mDefinition->getWeaponSpawnL());

    if(mDefinition->getWeaponSpawnR().compare("none") != 0)
        mWeaponR = gameMap->getWeapon(mDefinition->getWeaponSpawnR());

    setupDefinition(*gameMap, *ConfigManager::getSingleton().getCreatureDefinitionDefaultWorker());

    if(!getIsOnServerMap())
    {
        registerObserver(GameEditorModeConsole::getSingleton());
    }
    
}

Creature::Creature(GameMap* gameMap) :
    MovableGameEntity        (gameMap),
    parkingBit               (false),
    parkedBit                (false),
    mPhysicalDefense         (3.0),
    mMagicalDefense          (1.5),
    mElementDefense          (0.0),
    mModifierStrength        (1.0),
    mWeaponL                 (nullptr),
    mWeaponR                 (nullptr),
    mHomeTile                (nullptr),
    mDefinition              (nullptr),
    mHasVisualDebuggingEntities (false),
    mWakefulness             (100.0),
    mHunger                  (0.0),
    mLevel                   (1),
    mHp                      (10.0),
    mMaxHP                   (10.0),
    mExp                     (0.0),
    mGroundSpeed             (1.0),
    mWaterSpeed              (0.0),
    mLavaSpeed               (0.0),
    mDigRate                 (0.0),
    mClaimRate               (0.0),
    mDeathCounter            (0),
    mChampionTurns           (0),
    mJobCooldown             (0),
    mGoldFee                 (0),
    mGoldCarried             (0),
    mSkillTypeDropDeath      (SkillType::nullSkillType),
    mWeaponDropDeath         ("none"),
    mStatsWindow             (nullptr),
    mNbTurnsWithoutBattle    (0),
    mCasinoMood              (0.0),
    mCarriedEntity           (nullptr),
    mMoodCooldownTurns       (0),
    mMoodValue               (gameMap->isServerGameMap() ? CreatureMoodLevel::Neutral : CreatureMoodLevel::Unknown),
    mMoodPoints              (0),
    mPrayerRelief            (0),
    mNbTurnFurious           (-1),
    mOverlayHealthValue      (0),
    mOverlayMoodValue        (0),
    mOverlayStatus           (nullptr),
    mNeedFireRefresh         (false),
    mDropCooldown            (0),
    mSpeedModifier           (1.0),
    mKoTurnCounter           (0),
    mSeatPrison              (nullptr),
    mNbTurnsTorture          (0),
    mNbTurnsPrison           (0),
    mActiveSlapsCount        (0),
    mIsHexenHen              (false),
    mHexenHenMeshShown       (false),
    mNbTurnsInHand           (0),
    mIsInHand                (false),
    mNbTurnsOutOfWork        (0),
    mNbTurnsTortureMood      (0),
    mNbTurnsRested           (0),
    mTorturedThisTurn        (false),
    mRestedThisTurn          (false),
    mNbTurnsHatedCompany     (0),
    mPitMood                 (0.0)
{
    if(!getIsOnServerMap())
    {
        registerObserver(GameEditorModeConsole::getSingleton());
    }
}

Creature::~Creature()
{
    // The client remembers the creature the mouse is hovering in a bare pointer, so that
    // it can restore its ambient once the mouse leaves it. Nothing used to clear that
    // pointer when the creature itself went away, so a creature dying while highlighted
    // left it dangling and the next mouse move dereferenced freed memory.
    if(getIsOnServerMap())
        return;

    // May already be gone when the game map is torn down at shutdown.
    InputManager* inputManager = InputManager::getSingletonPtr();
    if(inputManager != nullptr && inputManager->mHighlightedCreature == this)
        inputManager->mHighlightedCreature = nullptr;
}

void Creature::createMeshLocal(NodeType nt)
{
    MovableGameEntity::createMeshLocal(nt);
    if(!getIsOnServerMap())
    {
        mHexenHenMeshShown = isHexenHen();
        RenderManager::getSingleton().rrCreateCreature(this);

        // By default, we set the creature in idle state
        RenderManager::getSingleton().rrSetObjectAnimationState(this, EntityAnimation::idle_anim, true);
    }

    createMeshWeapons();
}

void Creature::destroyMeshLocal(NodeType nt)
{
    destroyMeshWeapons();
    MovableGameEntity::destroyMeshLocal();
    if(getIsOnServerMap())
        return;

    destroyStatsWindow();
    RenderManager::getSingleton().rrDestroyCreature(this);
}

void Creature::createMeshWeapons()
{
    if(getIsOnServerMap())
        return;

    if(mHexenHenMeshShown)
        return;

    if(mWeaponL != nullptr)
        RenderManager::getSingleton().rrCreateWeapon(this, mWeaponL, "L");

    if(mWeaponR != nullptr)
        RenderManager::getSingleton().rrCreateWeapon(this, mWeaponR, "R");
}

void Creature::destroyMeshWeapons()
{
    if(getIsOnServerMap())
        return;

    if(mHexenHenMeshShown)
        return;

    if(mWeaponL != nullptr)
        RenderManager::getSingleton().rrDestroyWeapon(this, mWeaponL, "L");

    if(mWeaponR != nullptr)
        RenderManager::getSingleton().rrDestroyWeapon(this, mWeaponR, "R");
}

GameEntityType Creature::getObjectType() const
{
    return GameEntityType::creature;
}

void Creature::addToGameMap(GameMap* gameMap)
{
    getGameMap()->addCreature(this);
    getGameMap()->addAnimatedObject(this);
    getGameMap()->addClientUpkeepEntity(this);

    if(!getIsOnServerMap())
        return;

    getGameMap()->addActiveObject(this);
}

void Creature::removeFromGameMap(GameMap* gameMap)
{
    fireEntityRemoveFromGameMap();
    removeEntityFromPositionTile();
    getGameMap()->removeCreature(this);
    getGameMap()->removeAnimatedObject(this);
    getGameMap()->removeClientUpkeepEntity(this);

    if(!getIsOnServerMap())
        return;

    // If the creature has a homeTile where it sleeps, its bed needs to be destroyed.
    if (getHomeTile() != nullptr)
    {
        RoomDormitory* home = static_cast<RoomDormitory*>(getHomeTile()->getCoveringBuilding());
        home->releaseTileForSleeping(getHomeTile(), this);
    }

    fireRemoveEntityToSeatsWithVision();
    getGameMap()->removeActiveObject(this);
}

std::string Creature::getCreatureStreamFormat()
{
    std::string format = MovableGameEntity::getMovableGameEntityStreamFormat();
    if(!format.empty())
        format += "\t";

    format += "ClassName\tLevel\tCurrentXP\tCurrentHP\tCurrentWakefulness"
            "\tCurrentHunger\tGoldToDeposit\tLeftWeapon\tRightWeapon\tCarriedSkill\tCarriedWeapon"
            "\tNbCreatureEffects\tN*CreatureEffects";

    return format;
}

void Creature::exportToStream(std::ostream& os) const
{
    MovableGameEntity::exportToStream(os);
    os << mDefinition->getClassName() << "\t";
    os << getLevel() << "\t" << mExp << "\t";
    if(getHP() < mMaxHP)
        os << getHP();
    else
        os << "max";
    os << "\t" << mWakefulness << "\t" << mHunger << "\t" << mGoldCarried;

    // Check creature weapons
    if(mWeaponL != nullptr)
        os << "\t" << mWeaponL->getName();
    else
        os << "\tnone";

    if(mWeaponR != nullptr)
        os << "\t" << mWeaponR->getName();
    else
        os << "\tnone";

    os << "\t" << Skills::toString(mSkillTypeDropDeath);

    os << "\t" << mWeaponDropDeath;

    uint32_t nbCreatureEffects = 0;
    for(EntityParticleEffect* effect : mEntityParticleEffects)
    {
        if(effect->getEntityParticleEffectType() == EntityParticleEffectType::creature)
            ++nbCreatureEffects;
    }
    os << "\t" << nbCreatureEffects;
    for(EntityParticleEffect* effect : mEntityParticleEffects)
    {
        // We only save creature particle effects. The other are expected to be re-created
        // automatically (for example if it is a permanent effect for a creature)
        if(effect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
            continue;

        CreatureParticleEffect* creatureParticleEffect = static_cast<CreatureParticleEffect*>(effect);
        os << "\t";
        CreatureEffectManager::write(*creatureParticleEffect->mEffect, os);
    }
}

bool Creature::importFromStream(std::istream& is)
{
    // Beware: A generic class name might be used here so we shouldn't use mDefinition
    // here as it is not set yet (for example, default worker will be available only after
    // seat lobby configuration)
    if(!MovableGameEntity::importFromStream(is))
        return false;
    std::string tempString;

    if(!(is >> mDefinitionString))
        return false;
    if(!(is >> mLevel))
        return false;
    if(!(is >> mExp))
        return false;
    if(!(is >> mHpString))
        return false;
    if(!(is >> mWakefulness))
        return false;
    if(!(is >> mHunger))
        return false;
    if(!(is >> mGoldCarried))
        return false;
    if(!(is >> tempString))
        return false;
    if(tempString != "none")
    {
        mWeaponL = getGameMap()->getWeapon(tempString);
        if(mWeaponL == nullptr)
        {
            OD_LOG_ERR("Unknown weapon name=" + tempString);
        }
    }

    if(!(is >> tempString))
        return false;
    if(tempString != "none")
    {
        mWeaponR = getGameMap()->getWeapon(tempString);
        if(mWeaponR == nullptr)
        {
            OD_LOG_ERR("Unknown weapon name=" + tempString);
        }
    }

    if(!(is >> tempString))
        return false;
    mSkillTypeDropDeath = Skills::fromString(tempString);

    if(!(is >> mWeaponDropDeath))
        return false;

    mLevel = std::min(MAX_LEVEL, mLevel);

    uint32_t nbEffects;
    if(!(is >> nbEffects))
        return false;
    while(nbEffects > 0)
    {
        --nbEffects;
        CreatureEffect* effect = CreatureEffectManager::load(is);
        if(effect == nullptr)
            continue;

        addCreatureEffect(effect);
    }

    return true;
}

void Creature::buildStats()
{
    // Get the base value
    mMaxHP = mDefinition->getMinHp();
    mDigRate = mDefinition->getDigRate();
    mClaimRate = mDefinition->getClaimRate();
    mGroundSpeed = mDefinition->getMoveSpeedGround();
    mWaterSpeed = mDefinition->getMoveSpeedWater();
    mLavaSpeed  = mDefinition->getMoveSpeedLava();

    mPhysicalDefense = mDefinition->getPhysicalDefense();
    mMagicalDefense = mDefinition->getMagicalDefense();
    mElementDefense = mDefinition->getElementDefense();

    // Improve the stats to the current level
    double multiplier = mLevel - 1;
    if (multiplier <= 0.0)
        return;

    mMaxHP = CreatureProgression::stat(mMaxHP, mDefinition->getHpPerLevel(), mLevel);
    mDigRate += mDefinition->getDigRatePerLevel() * multiplier;
    mClaimRate += mDefinition->getClaimRatePerLevel() * multiplier;
    mGroundSpeed += mDefinition->getGroundSpeedPerLevel() * multiplier;
    mWaterSpeed += mDefinition->getWaterSpeedPerLevel() * multiplier;
    mLavaSpeed += mDefinition->getLavaSpeedPerLevel() * multiplier;

    mPhysicalDefense = CreatureProgression::stat(mPhysicalDefense, mDefinition->getPhysicalDefPerLevel(), mLevel);
    mMagicalDefense = CreatureProgression::stat(mMagicalDefense, mDefinition->getMagicalDefPerLevel(), mLevel);
    mElementDefense = CreatureProgression::stat(mElementDefense, mDefinition->getElementDefPerLevel(), mLevel);
}

Creature* Creature::getCreatureFromStream(GameMap* gameMap, std::istream& is)
{
    Creature* creature = new Creature(gameMap);
    if(!creature->importFromStream(is))
    {
        delete creature;
        return nullptr;
    }
    return creature;
}

Creature* Creature::getCreatureFromPacket(GameMap* gameMap, ODPacket& is)
{
    Creature* creature = new Creature(gameMap);
    creature->importFromPacket(is);
    return creature;
}

void Creature::exportToPacket(ODPacket& os, const Seat* seat) const
{
    MovableGameEntity::exportToPacket(os, seat);
    const std::string& className = mDefinition->getClassName();
    os << className;
    os << mLevel;
    os << mExp;

    os << mHp;
    os << mMaxHP;

    os << mDigRate;
    os << mClaimRate;
    os << mWakefulness;
    os << mHunger;

    os << mGroundSpeed;
    os << mWaterSpeed;
    os << mLavaSpeed;

    os << mPhysicalDefense;
    os << mMagicalDefense;
    os << mElementDefense;
    os << mOverlayHealthValue;

    // Only allied players should see creature mood (except some states)
    uint32_t moodValue = 0;
    if(seat->isAlliedSeat(getSeat()))
        moodValue = mOverlayMoodValue;
    else if(mSeatPrison != nullptr)
    {
        if(mSeatPrison->isAlliedSeat(seat))
            moodValue = mOverlayMoodValue & CreatureMoodValues::MoodPrisonFiltersPrisonAllies;
        else
            moodValue = mOverlayMoodValue & CreatureMoodValues::MoodPrisonFiltersAllPlayers;
    }

    os << moodValue;
    os << mSpeedModifier;

    if(mWeaponL != nullptr)
        os << mWeaponL->getName();
    else
        os << "none";

    if(mWeaponR != nullptr)
        os << mWeaponR->getName();
    else
        os << "none";

    os << isHexenHen();
    exportMoodToPacket(os, seat);
    exportActivityToPacket(os, seat);
    exportProgressToPacket(os, seat);
}

void Creature::importFromPacket(ODPacket& is)
{
    MovableGameEntity::importFromPacket(is);
    std::string tempString;

    OD_ASSERT_TRUE(is >> mDefinitionString);

    OD_ASSERT_TRUE(is >> mLevel);
    OD_ASSERT_TRUE(is >> mExp);

    OD_ASSERT_TRUE(is >> mHp);
    OD_ASSERT_TRUE(is >> mMaxHP);

    OD_ASSERT_TRUE(is >> mDigRate);
    OD_ASSERT_TRUE(is >> mClaimRate);
    OD_ASSERT_TRUE(is >> mWakefulness);
    OD_ASSERT_TRUE(is >> mHunger);

    OD_ASSERT_TRUE(is >> mGroundSpeed);
    OD_ASSERT_TRUE(is >> mWaterSpeed);
    OD_ASSERT_TRUE(is >> mLavaSpeed);

    OD_ASSERT_TRUE(is >> mPhysicalDefense);
    OD_ASSERT_TRUE(is >> mMagicalDefense);
    OD_ASSERT_TRUE(is >> mElementDefense);

    OD_ASSERT_TRUE(is >> mOverlayHealthValue);
    OD_ASSERT_TRUE(is >> mOverlayMoodValue);
    OD_ASSERT_TRUE(is >> mSpeedModifier);

    OD_ASSERT_TRUE(is >> tempString);
    if(tempString != "none")
    {
        mWeaponL = getGameMap()->getWeapon(tempString);
        if(mWeaponL == nullptr)
        {
            OD_LOG_ERR("Unknown weapon name=" + tempString);
        }
    }

    OD_ASSERT_TRUE(is >> tempString);
    if(tempString != "none")
    {
        mWeaponR = getGameMap()->getWeapon(tempString);
        if(mWeaponR == nullptr)
        {
            OD_LOG_ERR("Unknown weapon name=" + tempString);
        }
    }

    OD_ASSERT_TRUE(is >> mIsHexenHen);
    importMoodFromPacket(is);
    importActivityFromPacket(is);
    importProgressFromPacket(is);
    setupDefinition(*getGameMap(), *ConfigManager::getSingleton().getCreatureDefinitionDefaultWorker());
}

void Creature::setPosition(const Ogre::Vector3& v, GameMap *gameMap )
{
    
    MovableGameEntity::setPosition(v);
    if(mCarriedEntity != nullptr)
        mCarriedEntity->notifyCarryMove(v);

    if(!getIsOnServerMap()){

    
        InputManager& inputManager = InputManager::getSingleton();

        // getPositionTile() returns null whenever the creature sits outside the map,
        // which happens while it is held in the keeper hand or carried around.
        Tile* positionTile = getPositionTile();

        if(positionTile != nullptr &&
           positionTile->getX() == inputManager.mXPos &&
           positionTile->getY() == inputManager.mYPos)
        {

            Creature* closestCreature = positionTile->getClosestCreature(inputManager.mCreatureTypeForOutliner );
            if(closestCreature != nullptr)
            {
                if(closestCreature != inputManager.mHighlightedCreature)
                {
                    if(inputManager.mHighlightedCreature != nullptr)
                    {
                        inputManager.mHighlightedCreature->normalizeAmbient();
                    }
                    inputManager.mHighlightedCreature = closestCreature;
                    closestCreature->maxAmbient();
                }
            }
        }
        else if(inputManager.mHighlightedCreature == this)
        {
            inputManager.mHighlightedCreature->normalizeAmbient();
            inputManager.mHighlightedCreature = nullptr;        

        }
    }
}

void Creature::setHP(double nHP)
{
    if (nHP > mMaxHP)
        mHp = mMaxHP;
    else
        mHp = nHP;

    computeCreatureOverlayHealthValue();
}

void Creature::heal(double hp)
{
    mHp = std::min(mHp + hp, mMaxHP);

    computeCreatureOverlayHealthValue();
}

bool Creature::isAlive() const
{
    if(!getIsOnServerMap())
        return mOverlayHealthValue < (NB_OVERLAY_HEALTH_VALUES - 1);

    return mHp > 0.0;
}

void Creature::update(Ogre::Real timeSinceLastFrame)
{
    Tile* previousPositionTile = getPositionTile();
    // Update movements, direction, ...
    MovableGameEntity::update(timeSinceLastFrame);

    // Update the visual debugging entities
    //if we are standing in a different tile than we were last turn
    if (mHasVisualDebuggingEntities &&
        getIsOnServerMap() &&
        (getPositionTile() != previousPositionTile))
    {
        computeVisualDebugEntities();
    }

    if(getOverlayStatus() != nullptr)
    {
        getOverlayStatus()->update(timeSinceLastFrame);
    }
}

void Creature::computeVisibleTiles()
{
    // dead Creatures do not give vision
    if (getHP() <= 0.0)
        return;

    // KO Creatures do not give vision
    if (isKo())
        return;

    // creatures in jail do not give vision
    if (mSeatPrison != nullptr)
        return;

    if (!getIsOnMap())
        return;

    // Look at the surrounding area
    updateTilesInSight();
    for(Tile* tile : mVisibleTiles)
        tile->notifyVision(getSeat());
}

void Creature::setLevel(unsigned int level)
{
    // Reset XP once the level has been acquired.
    mLevel = std::max(1u, std::min(MAX_LEVEL, level));
    mExp = 0.0;

    const double previousMaxHP = mMaxHP;
    buildStats();
    if(mHp > 0.0 && previousMaxHP > 0.0)
        mHp = std::min(mMaxHP, mHp * mMaxHP / previousMaxHP);

    mNeedFireRefresh = true;
}

void Creature::dropCarriedEquipment()
{
    fireCreatureSound(CreatureSound::Die);
    clearActionQueue();
    clearDestinations(EntityAnimation::die_anim, false, false);

    // We drop what we are carrying
    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + getName() + ", position=" + Helper::toString(getPosition()));
        return;
    }

    if(mGoldCarried > 0)
    {
        TreasuryObject* obj = new TreasuryObject(getGameMap(), mGoldCarried);
        obj->addToGameMap();
        Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(myTile->getX()),
                                    static_cast<Ogre::Real>(myTile->getY()), 0.0f);
        obj->createMesh();
        obj->setPosition(spawnPosition);
        mGoldCarried = 0;
    }

    if(mSkillTypeDropDeath != SkillType::nullSkillType)
    {
        GiftBoxSkill* skillEntity = new GiftBoxSkill(getGameMap(),
            "DroppedBy" + getName(), mSkillTypeDropDeath);
        skillEntity->addToGameMap();
        Ogre::Vector3 spawnPosition(static_cast<Ogre::Real>(myTile->getX()),
                                    static_cast<Ogre::Real>(myTile->getY()), 0.0f);
        skillEntity->createMesh();
        skillEntity->setPosition(spawnPosition);
        mSkillTypeDropDeath = SkillType::nullSkillType;
    }

    // TODO: drop weapon when available
}

void Creature::doUpkeep()
{
    // A creature that cannot be controlled anymore is given back to the AI
    if(isPossessed() && (!isAlive() || isKo() || !getIsOnMap()))
        endPossession();

    // If the creature is in jail, we check if it is still standing on it (if not picked up). If
    // not, it is free
    if((mSeatPrison != nullptr) &&
       getIsOnMap())
    {
        Tile* myTile = getPositionTile();
        if(myTile == nullptr)
        {
            OD_LOG_ERR("name=" + getName() + ", position=" + Helper::toString(getPosition()));
            return;
        }

        // We check if the creature is in containment
        Room* roomPrison = myTile->getCoveringRoom();
        if((roomPrison == nullptr) ||
           (!roomPrison->isInContainment(*this)))
        {
            // it is not standing on a jail. It is free
            mSeatPrison = nullptr;
            mNeedFireRefresh = true;
        }
    }

    // The creature may be killed while temporary KO
    if((mKoTurnCounter != 0) && !isAlive())
        mKoTurnCounter = 0;

    // If the creature is KO to death or dead, we remove its particle effects
    if(!mEntityParticleEffects.empty() &&
       ((mKoTurnCounter < 0) || !isAlive()))
    {
        for(EntityParticleEffect* effect : mEntityParticleEffects)
        {
            delete effect;
        }
        mEntityParticleEffects.clear();
    }

    // We apply creature effects if any
    for(auto it =  mEntityParticleEffects.begin(); it != mEntityParticleEffects.end();)
    {
        EntityParticleEffect* entityEffect = *it;
        if(entityEffect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
        {
            if(entityEffect->mNbTurnsEffect < 0)
            {
                ++it;
                continue;
            }

            if(entityEffect->mNbTurnsEffect > 0)
            {
                --entityEffect->mNbTurnsEffect;
                ++it;
                continue;
            }

            delete entityEffect;
            it = mEntityParticleEffects.erase(it);
            continue;
        }

        CreatureParticleEffect* effect = static_cast<CreatureParticleEffect*>(*it);
        if(effect->mEffect->upkeepEffect(*this))
        {
            ++it;
            continue;
        }

        delete effect;
        it = mEntityParticleEffects.erase(it);
    }

    // The creature resents being held in the hand for long periods. The resentment fades once dropped
    if(mIsInHand)
        ++mNbTurnsInHand;
    else if(mNbTurnsInHand > 0)
        --mNbTurnsInHand;

    // Torture weighs on the mood while it lasts, sleeping in the lair relieves it. Both fade afterwards
    if(mTorturedThisTurn)
        ++mNbTurnsTortureMood;
    else if(mNbTurnsTortureMood > 0)
        --mNbTurnsTortureMood;

    if(mRestedThisTurn)
        ++mNbTurnsRested;
    else if(mNbTurnsRested > 0)
        --mNbTurnsRested;

    // Staying near a creature of the opposite alignment annoys, the annoyance fades afterwards
    if(isHatedCompanyNear())
        ++mNbTurnsHatedCompany;
    else if(mNbTurnsHatedCompany > 0)
        --mNbTurnsHatedCompany;

    mTorturedThisTurn = false;
    mRestedThisTurn = false;

    // if creature is not on map (picked up or being carried), we do nothing
    if(!getIsOnMap())
        return;

    // The champion costs mana and leaves when the owner cannot pay it
    if(getDefinition()->isChampion() && handleChampionUpkeep())
        return;
    // A creature that is working is not frustrated anymore
    if(isActionInList(CreatureActionType::useRoom))
        mNbTurnsOutOfWork = 0;

    // If the creature is temporary KO, it should do nothing
    if(mKoTurnCounter > 0)
    {
        --mKoTurnCounter;
        if(mKoTurnCounter > 0)
            return;

        if(!getGameMap()->isInEditorMode())
            setAnimationState(EntityAnimation::getup_anim, false,
                Ogre::Vector3::ZERO, false);
        computeCreatureOverlayMoodValue();
        return;
    }

    if(mKoTurnCounter < 0)
    {

        if(getIsOnMap())
        {
            Tile* myTile = getPositionTile();
            if(myTile == nullptr)
            {
                OD_LOG_ERR("name=" + getName() + ", position=" + Helper::toString(getPosition()));
                return;
            }
            Room* myRoom = myTile->getCoveringRoom();
            if( myRoom!=nullptr && myRoom->getType() == RoomType::arena)
            {
                
                RoomArena* myRoomArena = static_cast<RoomArena*>(myRoom);
                Ogre::Vector3 dest = myRoomArena->getGateTile()->getPosition();
                setPosition(dest);

                for(Seat* seat : mSeatsWithVisionNotified)
                {
                    if(seat->getPlayer() == nullptr)
                        continue;
                    if(!seat->getPlayer()->getIsHuman())
                        continue;

                    ServerNotification* serverNotification = new ServerNotification(
                        ServerNotificationType::entityTeleported, seat->getPlayer());
                    serverNotification->mPacket << getName() << dest;
                    ODServer::getSingleton().queueServerNotification(serverNotification);
                }
            }  
            
        }

        
        // If the counter reaches 0, the creature is dead
        ++mKoTurnCounter;
        if(mKoTurnCounter < 0)
            return;

        mHp = 0;
        computeCreatureOverlayHealthValue();
        computeCreatureOverlayMoodValue();
    }

    // Handle creature death
    if (!isAlive())
    {
        // Let the creature lay dead on the ground for a few turns before removing it from the GameMap.
        if (mDeathCounter == 0 )
        {
            OD_LOG_INF("Creature=" + getName() + " RIP");

            dropCarriedEquipment();
        }
        else if ((getDefinition()->isWorker() && mDeathCounter == 1) || mDeathCounter >= ConfigManager::getSingleton().getCreatureDeathCounter())
        {
            // Remove the creature from the game map and into the deletion queue, it will be deleted
            // when it is safe, i.e. all other pointers to it have been wiped from the program.
            removeFromGameMap();
            deleteYourself();
        }

        ++mDeathCounter;
        return;
    }

    // If the creature is in jail, it should not auto heal or do anything
    if(mSeatPrison != nullptr)
    {
        Tile* myTile = getPositionTile();
        if(myTile == nullptr)
        {
            OD_LOG_ERR("name=" + getName() + ", position=" + Helper::toString(getPosition()));
            return;
        }

        // If the creature is in a containment room, it should use it
        Room* roomPrison = myTile->getCoveringRoom();
        if((roomPrison != nullptr) && (decreaseJobCooldown()))
            roomPrison->useRoom(*this, true);

        return;
    }

    // If we are not standing somewhere on the map, do nothing.
    if (getPositionTile() == nullptr)
    {
        OD_LOG_ERR("creature=" + getName() + " not on map position=" + Helper::toString(getPosition()));
        return;
    }

    // Check to see if we have earned enough experience to level up.
    checkLevelUp();


    // Heal.
    mHp += mDefinition->getHpHealPerTurn();
    if (mHp > getMaxHp())
        mHp = getMaxHp();

    computeCreatureOverlayHealthValue();

    // Rogue creatures are not affected by wakefulness/hunger
    if(!getSeat()->isRogueSeat())
    {
        decreaseWakefulness(mDefinition->getWakefulnessLostPerTurn());

        increaseHunger(mDefinition->getHungerGrowthPerTurn());
    }

    mVisibleEnemyObjects         = getVisibleEnemyObjects();
    mVisibleAlliedObjects        = getVisibleAlliedObjects();
    mReachableAlliedObjects      = getReachableAttackableObjects(mVisibleAlliedObjects);

    // The relief from praying fades
    if(mPrayerRelief > 0)
    {
        mPrayerRelief -= ConfigManager::getSingleton().getRoomConfigInt32("TemplePrayerReliefDecayPerTurn");
        if(mPrayerRelief < 0)
            mPrayerRelief = 0;
    }

    // Check if we should compute mood
    if(mMoodCooldownTurns > 0)
    {
        --mMoodCooldownTurns;
    }
    // Rogue creatures do not have mood
    else if(!getSeat()->isRogueSeat())
    {
        computeMood();
        computeCreatureOverlayMoodValue();
        mMoodCooldownTurns = Random::Int(0, 5);
    }

    if(mMoodValue < CreatureMoodLevel::Furious)
    {
        mNbTurnFurious = -1;
    }
    else
    {
        // If the creature is furious for too long, it will become rogue
        if(mNbTurnFurious < 0)
            mNbTurnFurious = 0;
        else
            ++mNbTurnFurious;

        if(mNbTurnFurious >= ConfigManager::getSingleton().getNbTurnsFuriousMax())
        {
            // We couldn't leave the dungeon in time, we become rogue
            fireChatMsgBecameRogue();

            Seat* rogueSeat = getGameMap()->getSeatRogue();
            changeSeat(rogueSeat);
        }
    }

    ++mNbTurnsWithoutBattle;

    // The pit mood fades towards 0
    if(mPitMood != 0.0)
    {
        double decay = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("PitMoodDecay", DEFAULT_PIT_MOOD_DECAY_PER_SECOND)
            / ODApplication::turnsPerSecond;
        if(mPitMood > 0.0)
            mPitMood = std::max(0.0, mPitMood - decay);
        else
            mPitMood = std::min(0.0, mPitMood + decay);
    }

    // The casino mood fades towards 0
    if(mCasinoMood != 0.0)
    {
        double decay = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("CasinoMoodDecay", DEFAULT_CASINO_MOOD_DECAY_PER_SECOND)
            / ODApplication::turnsPerSecond;
        if(mCasinoMood > 0.0)
            mCasinoMood = std::max(0.0, mCasinoMood - decay);
        else
            mCasinoMood = std::min(0.0, mCasinoMood + decay);
    }

    // A frozen creature can neither move nor fight
    if(isFrozen())
    {
        if(!mActions.empty())
        {
            clearActionQueue();
            clearDestinations(EntityAnimation::idle_anim, true, true);
        }
        else if(isMoving())
        {
            clearDestinations(EntityAnimation::idle_anim, true, true);
        }
        return;
    }

    // A chicken cannot fight, use skills or work. It only wanders around
    if(isHexenHen())
    {
        handleHexenHenUpkeep();
        return;
    }

    // If a player controls the creature, its other actions are paused. Only the possessed
    // action runs and the movement comes from the player input
    if(isPossessed())
    {
        if(mActions.empty() || (mActions.back()->getType() != CreatureActionType::possessed))
            pushAction(Utils::make_unique<CreatureActionPossessed>(*this));

        // The creature skills keep recovering while the player controls the creature
        for(CreatureSkillData& skillData : mSkillData)
        {
            if(skillData.mWarmup > 0)
                --skillData.mWarmup;

            if(skillData.mCooldown > 0)
                --skillData.mCooldown;
        }

        std::function<bool()> possessedFunc = mActions.back()->action();
        possessedFunc();
        return;
    }

    bool isWarmUp = false;
    if(mAttackRecoveryTurns > 0)
    {
        --mAttackRecoveryTurns;
        mNeedFireRefresh = true;
    }
    // We use creature skills if we can
    for(CreatureSkillData& skillData : mSkillData)
    {
        if(skillData.mWarmup > 0)
        {
            --skillData.mWarmup;
            isWarmUp = true;
        }

        if(skillData.mCooldown > 0)
        {
            --skillData.mCooldown;
            continue;
        }

        if(!skillData.mSkill->canBeUsedBy(this))
            continue;

        if(!skillData.mSkill->tryUseSupport(*getGameMap(), this))
            continue;

        skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();
        skillData.mWarmup = skillData.mSkill->getWarmupNbTurns();

        if(skillData.mWarmup > 0)
            isWarmUp = true;
    }

    // If a warmup is active, we do nothing
    if(isWarmUp)
        return;

    decidePrioritaryAction();
    handleHeartDefence();

    // The loopback variable allows creatures to begin processing a new
    // action immediately after some other action happens.
    bool loopBack = false;
    unsigned int loops = 0;

    mActionTry.clear();

    do
    {
        ++loops;
        loopBack = false;

        if (mActions.empty())
        {
            loopBack = handleIdleAction();
            OD_LOG_DBG("creature=" + getName() + " action queue empty, defaulting to idle, result=" + (loopBack?"1":"0"));
        }
        else
        {
            CreatureAction* act = mActions.back().get();
            // We save the action type here because the action may be removed after calling
            // the action function
            CreatureActionType actType = act->getType();
            std::function<bool()> func = act->action();
            loopBack = func();
            OD_LOG_DBG("creature=" + getName() + " trying action=" + CreatureAction::toString(actType) + ", result=" + std::string(loopBack?"1":"0"));
        }
    } while (loopBack && loops < 20);

    if(!mActions.empty())
        mActions.back().get()->increaseNbTurnActive();

    for(std::unique_ptr<CreatureAction>& creatureAction : mActions)
        creatureAction.get()->increaseNbTurn();

    if(loops >= 20)
    {
        OD_LOG_INF("> 20 loops in Creature::doUpkeep name:" + getName() +
                " seat id: " + Helper::toString(getSeat()->getId()) + ". Breaking out..");
    }
}

void Creature::decidePrioritaryAction()
{
    for(const CreatureBehaviour* behaviour : getDefinition()->getCreatureBehaviours())
    {
        if(!behaviour->processBehaviour(*this))
            return;
    }
}

void Creature::handleHeartDefence()
{
    if(!getDefinition()->isHeartDefenceRunner())
        return;

    Seat* seat = getSeat();
    if(seat == nullptr || !seat->getHeartDefenceActive())
        return;

    if(isActionInList(CreatureActionType::goDefendHeart))
        return;

    // Existing combat and retreat decisions take priority over gathering at the defence point.
    if(isActionInList(CreatureActionType::fight) || isActionInList(CreatureActionType::flee))
        return;

    // The heart defence is on: drop the current job and run to the defence point
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionGoDefendHeart>(*this));
}

bool Creature::handleIdleAction()
{
    setAnimationState(EntityAnimation::idle_anim);

    if (mDefinition->isWorker())
    {
        // A trap of the keeper that used up its shots is armed again before the other jobs
        if(!hasActionBeenTried(CreatureActionType::reloadTrap) && getSeat()->getPlayer()->isWorkerReloadShareOpen() &&
           CreatureActionReloadTrap::tryStart(*this))
            return true;

        // Decide what to do
        std::vector<CreatureActionType> workerActions = getSeat()->getPlayer()->getWorkerPreferredActions(*this);
        for(CreatureActionType actionType : workerActions)
        {
            if(hasActionBeenTried(actionType))
                continue;

            switch(actionType)
            {
                case CreatureActionType::searchEntityToCarry:
                    pushAction(Utils::make_unique<CreatureActionSearchEntityToCarry>(*this, false));
                    return true;
                case CreatureActionType::searchGroundTileToClaim:
                    pushAction(Utils::make_unique<CreatureActionSearchGroundTileToClaim>(*this, false));
                    return true;
                case CreatureActionType::searchTileToDig:
                    pushAction(Utils::make_unique<CreatureActionSearchTileToDig>(*this, false));
                    return true;
                case CreatureActionType::searchWallTileToClaim:
                    pushAction(Utils::make_unique<CreatureActionSearchWallTileToClaim>(*this, false));
                    return true;
                default:
                    OD_LOG_ERR("name=" + getName() + ", unexpected worker action=" + CreatureAction::toString(actionType));
                    break;
            }
        }
    }

    // A standing order of the level script (go to a point, attack a dungeon, wait, ...) comes first
    if(LevelScriptRunner::doCreatureOrder(*this))
        return false;

    // The champion has no needs. It charges at the enemies and waits when there is none
    if(mDefinition->isChampion())
    {
        handleChampionIdle();
        return false;
    }

    // A creature in the group of a possessed creature follows it. Fights are handled by the
    // prioritary actions as usual
    if(isInPossessionGroup() && followPossessionLeader())
        return false;

    // We check if we are looking for our fee
    if(!mDefinition->isWorker() &&
       !hasActionBeenTried(CreatureActionType::getFee) &&
       (mGoldFee > 0))
    {
        pushAction(Utils::make_unique<CreatureActionGetFee>(*this));
        return true;
    }

    // We check if there is a go to war spell reachable
    std::vector<Spell*> callToWars = getGameMap()->getSpellsBySeatAndType(getSeat(), SpellType::callToWar);
    if(!callToWars.empty())
    {
        std::vector<Spell*> reachableCallToWars;
        for(Spell* callToWar : callToWars)
        {
            if(!callToWar->getIsOnMap())
                continue;

            Tile* callToWarTile = callToWar->getPositionTile();
            if(callToWarTile == nullptr)
                continue;

            if (!getGameMap()->pathExists(this, getPositionTile(), callToWarTile))
                continue;

            reachableCallToWars.push_back(callToWar);
        }

        if(!reachableCallToWars.empty())
        {
            // We go there
            uint32_t index = Random::Uint(0,reachableCallToWars.size()-1);
            Spell* callToWar = reachableCallToWars[index];
            Tile* callToWarTile = callToWar->getPositionTile();
            std::list<Tile*> tempPath = getGameMap()->path(this, callToWarTile);
            // If we are 5 tiles from the call to war, we don't go there
            if(tempPath.size() >= 5)
            {
                std::vector<Ogre::Vector2> path;
                tileToVector2(tempPath, path, true, 0.0);
                setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
                pushAction(Utils::make_unique<CreatureActionGoCallToWar>(*this));
                return false;
            }
        }
    }

    // Check to see if we have found a "home" tile where we can sleep. Even if we are not sleepy,
    // we want to have a bed
    if (!mDefinition->isWorker() &&
        !hasActionBeenTried(CreatureActionType::findHome) &&
        (mHomeTile == nullptr) &&
        (Random::Double(0.0, 1.0) < 0.5))
    {
        pushAction(Utils::make_unique<CreatureActionFindHome>(*this, false));
        return true;
    }

    // If we are sleepy, we go to sleep
    if (!mDefinition->isWorker() &&
        !hasActionBeenTried(CreatureActionType::sleep) &&
        (mHomeTile != nullptr) &&
        (Random::Double(20.0, 30.0) > mWakefulness))
    {
        pushAction(Utils::make_unique<CreatureActionSleep>(*this));
        return true;
    }

    // If we are hungry, we go to eat
    if (!mDefinition->isWorker() &&
        !hasActionBeenTried(CreatureActionType::searchFood) &&
        (Random::Double(70.0, 80.0) < mHunger))
    {
        pushAction(Utils::make_unique<CreatureActionSearchFood>(*this, false));
        return true;
    }

    // We try to steal some gold if there is some on the ground. Only creatures
    // with a StealGold amount in their definition (thieves) do that
    if (!mDefinition->isWorker() &&
        (mDefinition->getStealGold() > 0) &&
        !hasActionBeenTried(CreatureActionType::stealFreeGold) &&
        (Random::Uint(0, 10) > 8))
    {
        pushAction(Utils::make_unique<CreatureActionStealFreeGold>(*this));
        return true;
    }

    // Otherwise, we try to work
    if (!mDefinition->isWorker() &&
        !hasActionBeenTried(CreatureActionType::searchJob) &&
        (Random::Double(0.0, 1.0) < 0.4))
    {
        pushAction(Utils::make_unique<CreatureActionSearchJob>(*this, false));
        return true;
    }

    // Any creature.

    // Workers should move around randomly at large jumps.  Non-workers either wander short distances or follow workers.
    Tile* tileDest = nullptr;
    // Define reachable tiles from the tiles within radius
    std::vector<Tile*> reachableTiles;
    for (Tile* tile: mTilesWithinSightRadius)
    {
        if (getGameMap()->pathExists(this, getPositionTile(), tile))
            reachableTiles.push_back(tile);
    }

    if (!mDefinition->isWorker())
    {
        // Non-workers only.

        // Check to see if we want to try to follow a worker around or if we want to try to explore.
        double r = Random::Double(0.0, 1.0);
        if (r < 0.7)
        {
            bool workerFound = false;
            // Try to find a worker to follow around.
            for (unsigned int i = 0; !workerFound && i < mReachableAlliedObjects.size(); ++i)
            {
                // Check to see if we found a worker.
                if (mReachableAlliedObjects[i]->getObjectType() == GameEntityType::creature
                    && static_cast<Creature*>(mReachableAlliedObjects[i])->mDefinition->isWorker())
                {
                    // We found a worker so find a tile near the worker to walk to.  See if the worker is digging.
                    Tile* tempTile = mReachableAlliedObjects[i]->getCoveredTile(0);
                    if (static_cast<Creature*>(mReachableAlliedObjects[i])->isActionInList(CreatureActionType::digTile))
                    {
                        // Worker is digging, get near it since it could expose enemies.
                        int x = static_cast<int>(static_cast<double>(tempTile->getX()) + 3.0
                                * Random::gaussianRandomDouble());
                        int y = static_cast<int>(static_cast<double>(tempTile->getY()) + 3.0
                                * Random::gaussianRandomDouble());
                        tileDest = getGameMap()->getTile(x, y);
                    }
                    else
                    {
                        // Worker is not digging, wander a bit farther around the worker.
                        int x = static_cast<int>(static_cast<double>(tempTile->getX()) + 8.0
                                * Random::gaussianRandomDouble());
                        int y = static_cast<int>(static_cast<double>(tempTile->getY()) + 8.0
                                * Random::gaussianRandomDouble());
                        tileDest = getGameMap()->getTile(x, y);
                    }
                    workerFound = true;
                }

                // If there are no workers around, choose tiles far away to "roam" the dungeon.
                if (!workerFound)
                {
                    if (!reachableTiles.empty())
                    {
                        tileDest = reachableTiles[static_cast<unsigned int>(Random::Double(0.6, 0.8)
                                                                           * (reachableTiles.size() - 1))];
                    }
                }
            }
        }
        else
        {
            // Randomly choose a tile near where we are standing to walk to.
            if (!reachableTiles.empty())
            {
                unsigned int tileIndex = static_cast<unsigned int>(reachableTiles.size()
                                                                   * Random::Double(0.1, 0.3));
                tileDest = reachableTiles[tileIndex];
            }
        }
    }
    else
    {
        // Workers only.

        // Choose a tile far away from our current position to wander to.
        if (!reachableTiles.empty())
        {
            tileDest = reachableTiles[Random::Uint(reachableTiles.size() / 2,
                                                   reachableTiles.size() - 1)];
        }
    }

    if(setDestination(tileDest))
        return false;

    // Retry failed wandering next turn, not repeatedly in this upkeep.
    return false;
}

bool Creature::searchBestTargetInList(const std::vector<GameEntity*>& listObjects, const std::vector<Tile*>& tilesFilter, GameEntity*& attackedEntity,
        Tile*& attackedTile, Tile*& positionTile, CreatureSkillData*& creatureSkillData)
{
    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + getName() + ", position=" + Helper::toString(getPosition()));
        return false;
    }

    GameEntity* entityFlee = nullptr;
    // Closest creature
    GameEntity* entityAttack = nullptr;
    Tile* tileAttack = nullptr;
    CreatureSkillData* skillData = nullptr;
    Tile* tilePosition = nullptr;
    // Distance to the target, divided by its threat factor and weighted by its combat class (see below)
    double closestDistWeighted = -1.0;
    double ownThreat = getThreat();
    // We try to attack creatures first
    for(GameEntity* entity : listObjects)
    {
        // Invisible creatures cannot be seen by the enemy and are not targeted
        if((entity->getObjectType() == GameEntityType::creature) &&
           static_cast<Creature*>(entity)->isInvisible())
        {
            continue;
        }

        // Strong enemy creatures are targeted first: the more threatening a creature is compared to us,
        // the closer it appears to be. Other entities are not weighted.
        double threatFactor = 1.0;
        if((entity->getObjectType() == GameEntityType::creature) && (ownThreat > 0.0))
        {
            threatFactor = static_cast<Creature*>(entity)->getThreat() / ownThreat;
            threatFactor = std::max(0.5, std::min(2.0, threatFactor));
        }

        GameEntity* entityAttackCheck = nullptr;
        Tile* tileAttackCheck = nullptr;
        CreatureSkillData* skillDataCheck = nullptr;
        int closestDistCheck = -1;
        // We check the closest tile of this entity
        std::vector<Tile*> coveredTiles = entity->getCoveredTiles();
        for(Tile* tile : coveredTiles)
        {
            if(std::find(mVisibleTiles.begin(), mVisibleTiles.end(), tile) == mVisibleTiles.end())
                continue;

            int dist = static_cast<int>(Pathfinding::squaredDistanceTile(*tile, *myTile) / threatFactor);
            if((closestDistCheck != -1) && (dist >= closestDistCheck))
                continue;

            // We found a tile closer
            // Note that we don't break because if this entity is on more than 1 tile,
            // we want to attack the closest tile
            closestDistCheck = dist;
            entityAttackCheck = entity;
            tileAttackCheck = tile;
        }

        if((entityAttackCheck == nullptr) || (tileAttackCheck == nullptr))
            continue;

        // We check if this entity is closer than the other one (if any), weighted by the combat class
        double closestDistCheckWeighted = static_cast<double>(closestDistCheck) * getTargetDistanceFactor(*this, *entityAttackCheck);
        if((closestDistWeighted >= 0.0) && (closestDistCheckWeighted >= closestDistWeighted))
            continue;

        // We check if we are supposed to flee from this entity
        if((entityFlee == nullptr) && entityAttackCheck->isDangerous(this, closestDistCheck))
            entityFlee = entityAttackCheck;

        // If we found a suitable enemy, we check if we can attack it
        double skillRangeMax = 0.0;
        for(CreatureSkillData& skillDataTmp : mSkillData)
        {
            if(skillDataTmp.mCooldown > 0)
                continue;

            if(!skillDataTmp.mSkill->canBeUsedBy(this))
                continue;

            double skillRange = skillDataTmp.mSkill->getRangeMax(this, entityAttackCheck);
            if(skillRange <= 0)
                continue;
            if(skillRange < skillRangeMax)
                continue;

            skillRangeMax = skillRange;
            skillDataCheck = &skillDataTmp;
        }
        if(skillRangeMax <= 0)
            continue;

        // Check if we can attack from our position
        int rangeTarget = Pathfinding::squaredDistanceTile(*tileAttackCheck, *myTile);
        if(rangeTarget <= (skillRangeMax * skillRangeMax))
        {
             // We can attack
             tilePosition = myTile;
             entityAttack = entityAttackCheck;
             tileAttack = tileAttackCheck;
             skillData = skillDataCheck;
             closestDistWeighted = closestDistCheckWeighted;
             continue;
        }

        // We check if we can attack from somewhere. To do that, we check
        // from the target point of view if there is a tile with visibility within range
        int skillRangeMaxInt = static_cast<int>(skillRangeMax);
        int skillRangeMaxIntSquared = skillRangeMaxInt * skillRangeMaxInt;
        int bestScoreAttack = -1;
        std::vector<Tile*> tiles;
        if(tilesFilter.empty())
            tiles = getGameMap()->visibleTiles(tileAttackCheck->getX(), tileAttackCheck->getY(), skillRangeMaxInt);
        else
        {
            float radiusSquared = skillRangeMaxInt * skillRangeMaxInt;
            for(Tile* tile : tilesFilter)
            {
                float dist = Pathfinding::squaredDistanceTile(*tileAttackCheck, *tile);
                if(dist > radiusSquared)
                    continue;

                tiles.push_back(tile);
            }
        }
        for(Tile* tile : tiles)
        {
            if(tile->isFullTile())
                continue;

            if(!getGameMap()->pathExists(this, myTile, tile))
                continue;

            int distFoeTmp = Pathfinding::squaredDistanceTile(*tile, *tileAttackCheck);
            int distAttackTmp = Pathfinding::squaredDistanceTile(*tile, *myTile);
            // We compute a score for each tile. We will choose the best one. Note that we try to be as close as possible
            // from the fightIdleDist but by walking the less possible. We need to find a compromise
            int scoreAttack = std::abs(skillRangeMaxIntSquared - distFoeTmp) * 2 + distAttackTmp;
            // Support creatures keep their distance, flankers prefer to get behind the target
            CreatureDefinition::CombatClass combatClass = getDefinition()->getCombatClass();
            if((combatClass == CreatureDefinition::CombatSupport) && (distFoeTmp < skillRangeMaxIntSquared))
                scoreAttack += (skillRangeMaxIntSquared - distFoeTmp) * 2;
            else if(combatClass == CreatureDefinition::CombatFlanker)
            {
                int behindTarget = (tile->getX() - tileAttackCheck->getX()) * (myTile->getX() - tileAttackCheck->getX()) +
                    (tile->getY() - tileAttackCheck->getY()) * (myTile->getY() - tileAttackCheck->getY());
                if(behindTarget < 0)
                    scoreAttack /= 2;
            }
            if((bestScoreAttack != -1) && (bestScoreAttack <= scoreAttack))
                continue;

            // We found a better target
            bestScoreAttack = scoreAttack;
            tilePosition = tile;
            entityAttack = entityAttackCheck;
            tileAttack = tileAttackCheck;
            skillData = skillDataCheck;
            closestDistWeighted = closestDistCheckWeighted;
            // We don't break because there might be a better spot
        }
    }

    // If there is a dangerous entity and we cannot attack, we should try to get away
    if((entityFlee != nullptr) && (skillData == nullptr))
    {
        // Let's try to run to the closest spot at the distance closest to the fight idle distance
        Tile* tileEntityFlee = entityFlee->getPositionTile();
        if(tileEntityFlee == nullptr)
        {
            OD_LOG_ERR("entity=" + entityFlee->getName() + ", position=" + Helper::toString(entityFlee->getPosition()));
            return false;
        }

        int bestScoreFlee = -1;
        int32_t fightIdleDist = getDefinition()->getFightIdleDist();
        Tile* fleeTile = nullptr;
        std::vector<Tile*> tiles;
        if(tilesFilter.empty())
            tiles = getGameMap()->visibleTiles(tileEntityFlee->getX(), tileEntityFlee->getY(), fightIdleDist);
        else
        {
            float radiusSquared = fightIdleDist * fightIdleDist;
            for(Tile* tile : tilesFilter)
            {
                float dist = Pathfinding::squaredDistanceTile(*tileEntityFlee, *tile);
                if(dist > radiusSquared)
                    continue;

                tiles.push_back(tile);
            }
        }
        int32_t fightIdleDistSquared = fightIdleDist * fightIdleDist;
        for(Tile* tile : tiles)
        {
            if(tile->isFullTile())
                continue;

            if(!getGameMap()->pathExists(this, myTile, tile))
                continue;

            int distFoeTmp = Pathfinding::squaredDistanceTile(*tile, *tileEntityFlee);
            int fleeDistTmp = Pathfinding::squaredDistanceTile(*tile, *myTile);
            // We compute a score for each tile. We will choose the best one. Note that we try to be as close as possible
            // from the fightIdleDist but by walking the less possible. We need to find a compromise
            int scoreFlee = std::abs(fightIdleDistSquared - distFoeTmp) * 2 + fleeDistTmp;
            if((bestScoreFlee != -1) && (bestScoreFlee <= scoreFlee))
                continue;

            bestScoreFlee = scoreFlee;
            fleeTile = tile;
        }

        if(fleeTile == nullptr)
            return false;

        attackedEntity = nullptr;
        attackedTile = nullptr;
        positionTile = fleeTile;
    }
    else if ((entityAttack == nullptr) ||
        (tileAttack == nullptr) ||
        (tilePosition == nullptr))
    {
        // We couldn't find an entity to attack
        return false;
    }
    else
    {
        attackedEntity = entityAttack;
        attackedTile = tileAttack;
        creatureSkillData = skillData;
        positionTile = tilePosition;
    }

    return true;
}

void Creature::engageAlliedNaturalEnemy(Creature& attackerCreature)
{
    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
    {
        OD_LOG_ERR("name=" + getName() + ", pos=" + Helper::toString(getPosition()));
        return;
    }

    // If we are already fighting, do nothing
    if(isActionInList(CreatureActionType::fight))
        return;

    // When fighting a natural enemy, we always fight to death
    // We want to notify the player that his creatures are fighting
    fightCreature(attackerCreature, false, true);
}

double Creature::getMoveSpeed() const
{
    return getMoveSpeed(getPositionTile());
}

double Creature::getMoveSpeed(Tile* tile) const
{
    if(tile == nullptr)
    {
        OD_LOG_ERR("creature=" + getName());
        return 1.0;
    }

    if(getIsOnServerMap())
    {
        // Check if the covering building allows this creature to go through
        if(tile->getCoveringBuilding() != nullptr)
            return tile->getCoveringBuilding()->getCreatureSpeed(this, tile);
        else
            return tile->getCreatureSpeedDefault(this);
    }
    else
    {
        if(tile->getHasBridge())
            return getMoveSpeedGround();
        else
            return tile->getCreatureSpeedDefault(this);
    }
}

double Creature::getPhysicalDefense() const
{
    double defense = mPhysicalDefense;
    if (mWeaponL != nullptr)
        defense += mWeaponL->getPhysicalDefense();
    if (mWeaponR != nullptr)
        defense += mWeaponR->getPhysicalDefense();

    return defense;
}

double Creature::getMagicalDefense() const
{
    double defense = mMagicalDefense;
    if (mWeaponL != nullptr)
        defense += mWeaponL->getMagicalDefense();
    if (mWeaponR != nullptr)
        defense += mWeaponR->getMagicalDefense();

    return defense;
}

double Creature::getElementDefense() const
{
    double defense = mElementDefense;
    if (mWeaponL != nullptr)
        defense += mWeaponL->getElementDefense();
    if (mWeaponR != nullptr)
        defense += mWeaponR->getElementDefense();

    return defense;
}

void Creature::checkLevelUp()
{
    while(getLevel() < MAX_LEVEL)
    {
        const double newXP = mDefinition->getXPNeededWhenLevel(getLevel());
        if(!std::isfinite(newXP) || newXP <= 0.0)
        {
            OD_LOG_ERR("creature=" + getName() + ", newXP=" + Helper::toString(newXP));
            return;
        }
        if(mExp < newXP)
            return;

        const double remainingXP = mExp - newXP;
        setLevel(mLevel + 1);
        mExp = remainingXP;
    }
    mExp = 0.0;
}

void Creature::exportToPacketForUpdate(ODPacket& os, Seat* seat) 
{
    MovableGameEntity::exportToPacketForUpdate(os, seat);

    int seatId = getSeat()->getId();
    os << mLevel;
    os << seatId;
    os << mOverlayHealthValue;

    // Only allied players should see creature mood (except some states)
    uint32_t moodValue = 0;
    if(seat->isAlliedSeat(getSeat()))
        moodValue = mOverlayMoodValue;
    else if(mSeatPrison != nullptr)
    {
        if(mSeatPrison->isAlliedSeat(seat))
            moodValue = mOverlayMoodValue & CreatureMoodValues::MoodPrisonFiltersPrisonAllies;
        else
            moodValue = mOverlayMoodValue & CreatureMoodValues::MoodPrisonFiltersAllPlayers;
    }

    os << moodValue;
    os << mGroundSpeed;
    os << mWaterSpeed;
    os << mLavaSpeed;
    os << mSpeedModifier;

    int seatPrisonId = -1;
    if(mSeatPrison != nullptr)
        seatPrisonId = mSeatPrison->getId();

    os << seatPrisonId;
    os << isHexenHen();
    exportMoodToPacket(os, seat);
    exportActivityToPacket(os, seat);
    exportProgressToPacket(os, seat);
}

void Creature::updateFromPacket(ODPacket& is)
{
    MovableGameEntity::updateFromPacket(is);

    int seatId;
    OD_ASSERT_TRUE(is >> mLevel);
    OD_ASSERT_TRUE(is >> seatId);
    OD_ASSERT_TRUE(is >> mOverlayHealthValue);
    OD_ASSERT_TRUE(is >> mOverlayMoodValue);
    OD_ASSERT_TRUE(is >> mGroundSpeed);
    OD_ASSERT_TRUE(is >> mWaterSpeed);
    OD_ASSERT_TRUE(is >> mLavaSpeed);
    OD_ASSERT_TRUE(is >> mSpeedModifier);

    // We do not scale the creature if it is picked up (because it is already not at its normal size). It will be
    // resized anyway when dropped
    if(getIsOnMap())
        RenderManager::getSingleton().rrScaleCreature(*this);

    if(getSeat()->getId() != seatId)
    {
        Seat* seat = getGameMap()->getSeatById(seatId);
        if(seat == nullptr)
        {
            OD_LOG_ERR("Creature " + getName() + ", wrong seatId=" + Helper::toString(seatId));
        }
        else
        {
            setSeat(seat);
        }
    }

    OD_ASSERT_TRUE(is >> seatId);
    if(seatId == -1)
        mSeatPrison = nullptr;
    else
    {
        mSeatPrison = getGameMap()->getSeatById(seatId);
        if(mSeatPrison == nullptr)
        {
            OD_LOG_ERR("Creature " + getName() + ", wrong seatId=" + Helper::toString(seatId));
        }
    }

    OD_ASSERT_TRUE(is >> mIsHexenHen);
    updateHexenHenMesh();

    importMoodFromPacket(is);
    importActivityFromPacket(is);
    importProgressFromPacket(is);
}

double Creature::getExperienceProgress() const
{
    if(!getIsOnServerMap())
        return mExperienceProgress;
    if(mLevel >= MAX_LEVEL)
        return 1.0;
    const double needed = mDefinition->getXPNeededWhenLevel(mLevel);
    return needed > 0.0 ? std::max(0.0, std::min(1.0, mExp / needed)) : 0.0;
}

void Creature::exportProgressToPacket(ODPacket& os, const Seat* seat) const
{
    if(!ODServer::getSingleton().supportsCreatureProgress(seat->getPlayer()))
        return;
    os << getExperienceProgress() << mAttackRecoveryTurns << mAttackRecoveryDuration
       << mAttackRecoverySerial;
}

void Creature::importProgressFromPacket(ODPacket& is)
{
    mHasProgressInformation = false;
    if(!ODClient::getSingleton().supportsCreatureProgress())
        return;
    double experience;
    uint32_t remaining, duration, serial;
    OD_ASSERT_TRUE(is >> experience >> remaining >> duration >> serial);
    if(!std::isfinite(experience) || experience < 0.0 || experience > 1.0 || remaining > duration)
    {
        OD_LOG_ERR("Invalid creature progress for " + getName());
        return;
    }
    mExperienceProgress = experience;
    mAttackRecoveryTurns = remaining;
    mAttackRecoveryDuration = duration;
    mAttackRecoverySerial = serial;
    mHasProgressInformation = true;
}

void Creature::exportMoodToPacket(ODPacket& os, const Seat* seat) const
{
    if(!ODServer::getSingleton().supportsCreatureMood(seat->getPlayer()))
        return;

    int32_t mood = static_cast<int32_t>(seat->isAlliedSeat(getSeat()) ?
        mMoodValue : CreatureMoodLevel::Unknown);
    os << mood;
}

void Creature::importMoodFromPacket(ODPacket& is)
{
    mMoodValue = CreatureMoodLevel::Unknown;
    if(!ODClient::getSingleton().supportsCreatureMood())
        return;

    int32_t mood = static_cast<int32_t>(CreatureMoodLevel::Unknown);
    OD_ASSERT_TRUE(is >> mood);
    if(mood < static_cast<int32_t>(CreatureMoodLevel::Unknown) ||
       mood > static_cast<int32_t>(CreatureMoodLevel::Furious))
    {
        OD_LOG_ERR("Invalid creature mood=" + Helper::toString(mood));
        return;
    }
    mMoodValue = static_cast<CreatureMoodLevel>(mood);
}

CreatureActivity Creature::getActivity() const
{
    CreatureActivity activity;
    if(!getIsOnMap() || !isAlive() || isKo())
        return activity;

    if(!getIsOnServerMap())
        return mActivity;

    activity.known = true;
    if(!mActions.empty())
        activity.action = mActions.back()->getType();

    for(std::vector<std::unique_ptr<CreatureAction>>::const_reverse_iterator it = mActions.rbegin(); it != mActions.rend(); ++it)
    {
        const CreatureActionType type = (*it)->getType();
        if(activity.task == CreatureActionType::nb && type != CreatureActionType::walkToTile &&
           type != CreatureActionType::parkToTile)
            activity.task = type;

        if(type != CreatureActionType::useRoom)
            continue;

        const Room* room = static_cast<const CreatureActionUseRoom*>(it->get())->getRoom();
        if(room != nullptr)
        {
            activity.assignedRoom = room->getType();
            const Tile* tile = getPositionTile();
            activity.inAssignedRoom = tile != nullptr && tile->getCoveringRoom() == room;
        }
        break;
    }
    return activity;
}

void Creature::exportActivityToPacket(ODPacket& os, const Seat* seat) const
{
    if(!ODServer::getSingleton().supportsCreatureActivity(seat->getPlayer()))
        return;

    const CreatureActivity activity = seat->isAlliedSeat(getSeat()) ? getActivity() : CreatureActivity();
    os << activity.known;
    if(!activity.known)
        return;

    os << static_cast<int32_t>(activity.action) << static_cast<int32_t>(activity.task)
       << static_cast<int32_t>(activity.assignedRoom) << activity.inAssignedRoom;
}

void Creature::importActivityFromPacket(ODPacket& is)
{
    mActivity = CreatureActivity();
    if(!ODClient::getSingleton().supportsCreatureActivity())
        return;

    CreatureActivity activity;
    if(!(is >> activity.known))
    {
        OD_LOG_ERR("Missing creature activity for " + getName());
        return;
    }
    if(!activity.known)
        return;

    int32_t action, task, room;
    if(!(is >> action >> task >> room >> activity.inAssignedRoom))
    {
        OD_LOG_ERR("Incomplete creature activity for " + getName());
        return;
    }
    if(action < 0 || action > static_cast<int32_t>(CreatureActionType::nb) ||
       task < 0 || task > static_cast<int32_t>(CreatureActionType::nb) ||
       room < 0 || room >= static_cast<int32_t>(RoomType::nbRooms))
    {
        OD_LOG_ERR("Invalid creature activity for " + getName());
        return;
    }
    activity.action = static_cast<CreatureActionType>(action);
    activity.task = static_cast<CreatureActionType>(task);
    activity.assignedRoom = static_cast<RoomType>(room);
    mActivity = activity;
}

void Creature::updateTilesInSight()
{
    Tile* posTile = getPositionTile();
    if (posTile == nullptr)
        return;

    // The tiles with sight radius without constraints
    mTilesWithinSightRadius = getGameMap()->circularRegion(posTile->getX(), posTile->getY(), mDefinition->getSightRadius());

    // Only the tiles the creature can "see".
    mVisibleTiles = getGameMap()->visibleTiles(posTile->getX(), posTile->getY(), mDefinition->getSightRadius());
}

std::vector<GameEntity*> Creature::getVisibleEnemyObjects()
{
    return getVisibleForce(getSeat(), true);
}

std::vector<GameEntity*> Creature::getReachableAttackableObjects(const std::vector<GameEntity*>& objectsToCheck)
{
    std::vector<GameEntity*> tempVector;
    Tile* myTile = getPositionTile();

    // Loop over the vector of objects we are supposed to check.
    for (unsigned int i = 0; i < objectsToCheck.size(); ++i)
    {
        // Try to find a valid path from the tile this creature is in to the nearest tile where the current target object is.
        GameEntity* entity = objectsToCheck[i];
        // We only consider alive objects
        if(entity->getHP(nullptr) <= 0)
            continue;

        Tile* objectTile = entity->getCoveredTile(0);
        if (getGameMap()->pathExists(this, myTile, objectTile))
            tempVector.push_back(objectsToCheck[i]);
    }

    return tempVector;
}

std::vector<GameEntity*> Creature::getCreaturesFromList(const std::vector<GameEntity*> &objectsToCheck, bool workersOnly)
{
    std::vector<GameEntity*> tempVector;

    // Loop over the vector of objects we are supposed to check.
    for (std::vector<GameEntity*>::const_iterator it = objectsToCheck.begin(); it != objectsToCheck.end(); ++it)
    {
        // Try to find a valid path from the tile this creature is in to the nearest tile where the current target object is.
        GameEntity* entity = *it;
        // We only consider alive objects
        if(entity->getObjectType() != GameEntityType::creature)
            continue;

        if(workersOnly && !static_cast<Creature*>(entity)->getDefinition()->isWorker())
            continue;

        tempVector.push_back(entity);
    }

    return tempVector;
}

std::vector<GameEntity*> Creature::getVisibleAlliedObjects()
{
    return getVisibleForce(getSeat(), false);
}

std::vector<GameEntity*> Creature::getVisibleForce(Seat* seat, bool invert)
{
    return getGameMap()->getVisibleForce(mVisibleTiles, seat, invert);
}

void Creature::computeVisualDebugEntities()
{
    if(!getIsOnServerMap())
        return;

    mHasVisualDebuggingEntities = true;

    updateTilesInSight();

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::refreshCreatureVisDebug, nullptr);

    const std::string& name = getName();
    serverNotification->mPacket << name;
    serverNotification->mPacket << true;
    if(getIsOnMap())
    {
        uint32_t nbTiles = mVisibleTiles.size();
        serverNotification->mPacket << nbTiles;

        for (Tile* tile : mVisibleTiles)
            getGameMap()->tileToPacket(serverNotification->mPacket, tile);
    }
    else
    {
        uint32_t nbTiles = 0;
        serverNotification->mPacket << nbTiles;
    }

    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::refreshVisualDebugEntities(const std::vector<Tile*>& tiles)
{
    if(getIsOnServerMap())
        return;

    mHasVisualDebuggingEntities = true;

    for (Tile* tile : tiles)
    {
        // We check if the visual debug is already on this tile
        if(std::find(mVisualDebugEntityTiles.begin(), mVisualDebugEntityTiles.end(), tile) != mVisualDebugEntityTiles.end())
            continue;

        RenderManager::getSingleton().rrCreateCreatureVisualDebug(this, tile);

        mVisualDebugEntityTiles.push_back(tile);
    }

    // now, we check if visual debug should be removed from a tile
    for (std::vector<Tile*>::iterator it = mVisualDebugEntityTiles.begin(); it != mVisualDebugEntityTiles.end();)
    {
        Tile* tile = *it;
        if(std::find(tiles.begin(), tiles.end(), tile) != tiles.end())
        {
            ++it;
            continue;
        }

        it = mVisualDebugEntityTiles.erase(it);

        RenderManager::getSingleton().rrDestroyCreatureVisualDebug(this, tile);
    }
}

void Creature::stopComputeVisualDebugEntities()
{
    if(!getIsOnServerMap())
        return;

    mHasVisualDebuggingEntities = false;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::refreshCreatureVisDebug, nullptr);
    const std::string& name = getName();
    serverNotification->mPacket << name;
    serverNotification->mPacket << false;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::destroyVisualDebugEntities()
{
    if(getIsOnServerMap())
        return;

    mHasVisualDebuggingEntities = false;

    for (Tile* tile : mVisualDebugEntityTiles)
    {
        if (tile == nullptr)
            continue;

        RenderManager::getSingleton().rrDestroyCreatureVisualDebug(this, tile);
    }
    mVisualDebugEntityTiles.clear();
}

std::vector<Tile*> Creature::getCoveredTiles()
{
    std::vector<Tile*> tempVector;
    tempVector.push_back(getPositionTile());
    return tempVector;
}

Tile* Creature::getCoveredTile(int index)
{
    if(index > 0)
    {
        OD_LOG_ERR("name=" + getName() + ", index=" + Helper::toString(index));
        return nullptr;
    }

    return getPositionTile();
}

uint32_t Creature::numCoveredTiles() const
{
    if(getPositionTile() == nullptr)
        return 0;

    return 1;
}

bool Creature::CloseStatsWindow(const CEGUI::EventArgs& /*e*/)
{
    destroyStatsWindow();
    return true;
}

void Creature::createStatsWindow()
{
    if (mStatsWindow != nullptr)
        return;

    ClientNotification *clientNotification = new ClientNotification(
        ClientNotificationType::askCreatureInfos);
    std::string name = getName();
    clientNotification->mPacket << name << true;
    ODClient::getSingleton().queueClientNotification(clientNotification);

    CEGUI::WindowManager* wmgr = CEGUI::WindowManager::getSingletonPtr();
    CEGUI::Window* rootWindow = CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();

    mStatsWindow = wmgr->createWindow("OD/FrameWindow", std::string("CreatureStatsWindows_") + getName());
    mStatsWindow->setPosition(CEGUI::UVector2(CEGUI::UDim(0.3, 0), CEGUI::UDim(0.3, 0)));
    mStatsWindow->setSize(CEGUI::USize(CEGUI::UDim(0, 380), CEGUI::UDim(0, 400)));

    CEGUI::Window* textWindow = wmgr->createWindow("OD/StaticText", "TextDisplay");
    textWindow->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05, 0), CEGUI::UDim(0.1, 0)));
    textWindow->setSize(CEGUI::USize(CEGUI::UDim(0.9, 0), CEGUI::UDim(0.85, 0)));
    textWindow->setProperty("FrameEnabled", "False");
    textWindow->setProperty("BackgroundEnabled", "False");

    // We want to close the window when the cross is clicked
    mStatsWindow->subscribeEvent(CEGUI::FrameWindow::EventCloseClicked,
        CEGUI::Event::Subscriber(&Creature::CloseStatsWindow, this));

    // Set the window title
    mStatsWindow->setText(getName() + " (" + getDefinition()->getClassName() + ")");

    mStatsWindow->addChild(textWindow);
    rootWindow->addChild(mStatsWindow);
    mStatsWindow->show();

    updateStatsWindow("Loading...");
}

void Creature::destroyStatsWindow()
{
    if (mStatsWindow != nullptr)
    {
        ClientNotification *clientNotification = new ClientNotification(
            ClientNotificationType::askCreatureInfos);
        std::string name = getName();
        clientNotification->mPacket << name << false;
        ODClient::getSingleton().queueClientNotification(clientNotification);

        mStatsWindow->destroy();
        mStatsWindow = nullptr;
    }
}

void Creature::updateStatsWindow(const std::string& txt)
{
    if (mStatsWindow == nullptr)
        return;

    CEGUI::Window* textWindow = mStatsWindow->getChild("TextDisplay");
    textWindow->setText(txt);
}

std::string Creature::getStatsText()
{
    // The creatures are not refreshed at each turn so this information is relevant in the server
    // GameMap only
    const std::string formatTitleOn = "[font='MedievalSharp-12'][colour='FFF2C860']";
    const std::string formatTitleOff = "[font='MedievalSharp-10'][colour='FFE8DCC0']";

    std::stringstream tempSS;
    tempSS << formatTitleOn << "Characteristics" << formatTitleOff << std::endl;
    tempSS << "Level: " << getLevel() << std::endl;
    tempSS << "Experience: " << mExp << std::endl;
    tempSS << "HP: " << getHP() << " / " << mMaxHP << std::endl;
    tempSS << "Gold: " << mGoldCarried << std::endl;
    if (!getDefinition()->isWorker())
    {
        tempSS << "Wakefulness: " << mWakefulness << std::endl;
        tempSS << "Hunger: " << mHunger << std::endl;
    }
    tempSS << "Move speed (G/W/L): " << getMoveSpeedGround() << " / "
        << getMoveSpeedWater() << " / " << getMoveSpeedLava() << std::endl;
    tempSS << "Weapons:" << std::endl;
    if(mWeaponL == nullptr)
        tempSS << " - Left: none" << std::endl;
    else
        tempSS << " - Left: " << mWeaponL->getName() << " | Damage (P/M/E): " << mWeaponL->getPhysicalDamage()
               << " / " << mWeaponL->getMagicalDamage() << " / " << mWeaponL->getElementDamage() << std::endl;
    if(mWeaponR == nullptr)
        tempSS << " - Right: none" << std::endl;
    else
        tempSS << " - Right: " << mWeaponR->getName() << " | Damage (P/M/E): " << mWeaponR->getPhysicalDamage()
               << " / " << mWeaponR->getMagicalDamage() << " / " << mWeaponR->getElementDamage() << std::endl;
    tempSS << "Defense (P/M/E): " << getPhysicalDefense() << " / " << getMagicalDefense() << " / " << getElementDefense() << std::endl;
    if (getDefinition()->isWorker())
    {
        tempSS << "Dig rate: " << getDigRate() << std::endl;
        tempSS << "Dance rate: " << mClaimRate << std::endl;
    }

    tempSS << formatTitleOn << "\nDebugging information" << formatTitleOff << std::endl;
    tempSS << "Seat and team IDs: " << getSeat()->getId() << " / " << getSeat()->getTeamId() << std::endl;
    tempSS << "Position: " << Helper::toString(getPosition()) << std::endl;
    tempSS << "Actions:";
    for(const std::unique_ptr<CreatureAction>& ca : mActions)
    {
        tempSS << " " << CreatureAction::toString(ca.get()->getType());
    }
    tempSS << std::endl;   
    tempSS << "Destinations:";
    for(const Ogre::Vector2& dest : mWalkQueue)
    {
        tempSS << " " << Helper::toString(dest);
    }
    tempSS << std::endl;
    tempSS << "Mood: " << CreatureMood::toString(mMoodValue) << std::endl;
    tempSS << "Mood points: " << Helper::toString(mMoodPoints) << std::endl;
    return tempSS.str();
}

double Creature::getPitDamageFactor(GameEntity* attacker)
{
    // Fights between creatures inside an arena only hurt a fraction of normal combat
    if((attacker == nullptr) || (attacker->getObjectType() != GameEntityType::creature))
        return 1.0;

    Tile* tileVictim = getPositionTile();
    Tile* tileAttacker = attacker->getPositionTile();
    if((tileVictim == nullptr) || (tileAttacker == nullptr) ||
       (tileVictim->getCoveringRoom() == nullptr) || (tileAttacker->getCoveringRoom() == nullptr))
        return 1.0;

    if((tileVictim->getCoveringRoom()->getType() != RoomType::arena) ||
       (tileAttacker->getCoveringRoom()->getType() != RoomType::arena))
        return 1.0;

    return ConfigManager::getSingleton().getRoomConfigDouble("ArenaDamageTakenPercent");
}

namespace
{
//! \brief Counts an event of the creature for the conditions of the level script (server only)
//! \param creature The creature the event happened to
//! \param eventName The name of the event, for example "killed" or "slapped"
void recordScriptEvent(const Creature& creature, const std::string& eventName)
{
    if(!creature.getIsOnServerMap())
        return;

    creature.getGameMap()->getLevelScript().recordEvent(creature.getName(), eventName);
}
} // namespace

double Creature::takeDamage(GameEntity* attacker, double absoluteDamage, double physicalDamage, double magicalDamage, double elementDamage,
        Tile *tileTakingDamage, bool ko)
{
    bool wasAlive = isAlive();
    mNbTurnsWithoutBattle = 0;
    // The champion cannot be hurt
    if(getDefinition()->isChampion())
        return 0.0;
    physicalDamage = std::max(physicalDamage - getPhysicalDefense(), 0.0);
    magicalDamage = std::max(magicalDamage - getMagicalDefense(), 0.0);
    elementDamage = std::max(elementDamage - getElementDefense(), 0.0);
    double totalDamage = (absoluteDamage + physicalDamage + magicalDamage + elementDamage) * getPitDamageFactor(attacker);
    double damageDone = std::min(mHp, totalDamage);
    mHp -= damageDone;
    if(wasAlive && (damageDone > 0.0))
        recordScriptEvent(*this, "attacked");

    if(mHp <= 0)
    {
        // A possessed creature is not knocked out, it dies and the keeper loses mana
        if(isPossessed())
        {
            if(wasAlive && (getSeat() != nullptr))
            {
                double manaLoss = ConfigManager::getSingleton().getSpellConfigDouble("PossessDeathManaLoss");
                getSeat()->addMana(-manaLoss);
            }
        }
        // If the attacking entity is a creature and its seat is configured to KO creatures
        // instead of killing, we KO
        else if(ko && !getDefinition()->isWorker())
        {
            mHp = 1.0;
            recordScriptEvent(*this, "incapacitated");
            mKoTurnCounter = -ConfigManager::getSingleton().getNbTurnsKoCreatureAttacked();
            OD_LOG_INF("creature=" + getName() + " has been KO by " + attacker->getName());
            dropCarriedEquipment();
        }
    }

    computeCreatureOverlayHealthValue();
    computeCreatureOverlayMoodValue();

    if(!isAlive())
    {
        // The killing blow counts once for the debriefing (a KO does not get here)
        if(wasAlive && (attacker != nullptr) && (attacker->getSeat() != nullptr))
            attacker->getSeat()->recordCreatureKill(getSeat());
        if(wasAlive && (getSeat() != nullptr))
            ++getSeat()->getStatistics().mCreaturesLost;
        if(wasAlive)
        {
            recordScriptEvent(*this, "killed");
            recordScriptEvent(*this, "incapacitated");
        }
        fireEntityDead();
    }

    if(!getIsOnServerMap())
        return damageDone;

    Player* player = getGameMap()->getPlayerBySeat(getSeat());
    if (player == nullptr)
        return damageDone;

    // If we are a worker attacked by a worker, we fight. Otherwise, we flee (if it is a fighter, a trap,
    // or whatever)
    if(!getDefinition()->isWorker())
        return damageDone;

    bool shouldFlee = true;
    if((attacker != nullptr) &&
       (attacker->getObjectType() == GameEntityType::creature))
    {
        Creature* creatureAttacking = static_cast<Creature*>(attacker);
        if(creatureAttacking->getDefinition()->isWorker())
        {
            // We do not flee because of this attack
            shouldFlee = false;
        }
    }

    if(shouldFlee)
    {
        flee();
        return damageDone;
    }
    return damageDone;
}

void Creature::receiveExp(double experience)
{
    if (!std::isfinite(experience) || experience < 0 || mLevel >= MAX_LEVEL)
        return;

    mExp += experience;
    mNeedFireRefresh = true;
}

void Creature::useAttack(CreatureSkillData& skillData, GameEntity& entityAttack,
        Tile& tileAttack, bool ko, bool notifyPlayerIfHit)
{
    // Keep ranged skills visually distinct, including shots at adjacent targets.
    const Ogre::Vector3& pos = getPosition();
    const Ogre::Vector3 target = entityAttack.getObjectType() == GameEntityType::creature ?
        entityAttack.getPosition() : Ogre::Vector3(tileAttack.getX(), tileAttack.getY(), 0);
    Ogre::Vector3 walkDirection(target.x - pos.x, target.y - pos.y, 0);
    walkDirection.normalise();
    const bool ranged = skillData.mSkill->getRangeMax(this, &entityAttack) > 1.0;
    setAnimationState(ranged ? EntityAnimation::ranged_attack_anim :
        EntityAnimation::combat_attack_anim, false, walkDirection, true);
    fireCreatureSound(CreatureSound::Attack);
    setNbTurnsWithoutBattle(0);

    // Calculate how much damage we do.
    Tile* myTile = getPositionTile();
    float range = Pathfinding::distanceTile(*myTile, tileAttack);

    // We use the skill
    skillData.mSkill->tryUseFight(*getGameMap(), this, range,
        &entityAttack, &tileAttack, ko, notifyPlayerIfHit);
    skillData.mWarmup = skillData.mSkill->getWarmupNbTurns();
    skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();

    // Both timers count down together; either can postpone the next attack.
    mAttackRecoveryDuration = std::max(skillData.mWarmup, skillData.mCooldown);
    mAttackRecoveryTurns = mAttackRecoveryDuration;
    ++mAttackRecoverySerial;
    mNeedFireRefresh = true;

    // Fighting is tiring
    decreaseWakefulness(0.5);
    // but gives experience
    receiveExp(1.5);
}

bool Creature::isActionInList(CreatureActionType action) const
{
    for (const std::unique_ptr<CreatureAction>& ca : mActions)
    {
        if (ca.get()->getType() == action)
            return true;
    }
    return false;
}

void Creature::clearActionQueue()
{
    mActions.clear();
}

bool Creature::hasActionBeenTried(CreatureActionType actionType) const
{
    if(std::find(mActionTry.begin(), mActionTry.end(), actionType) == mActionTry.end())
        return false;

    return true;
}

void Creature::pushAction(std::unique_ptr<CreatureAction>&& action)
{
    CreatureActionType actionType = action.get()->getType();
    if(std::find(mActionTry.begin(), mActionTry.end(), actionType) == mActionTry.end())
    {
        mActionTry.push_back(actionType);
    }

    mActions.emplace_back(std::move(action));
}

void Creature::popAction()
{
    if(mActions.empty())
    {
        OD_LOG_ERR("name=" + getName() + ", trying to pop empty action list");
        return;
    }

    mActions.pop_back();
}

bool Creature::isSandboxHeroFor(const Seat* seat) const
{
    if(!getGameMap()->isSandbox())
        return false;

    if(seat == nullptr)
        return false;

    if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
        return false;

    return SandboxMode::isHeroSeat(getSeat()) && !SandboxMode::isHeroSeat(seat);
}

bool Creature::tryPickup(Seat* seat)
{
    if(!getIsOnMap())
        return false;

    // Cannot pick up dead creatures
    if (!getGameMap()->isInEditorMode() && !isAlive())
        return false;

    if(!getGameMap()->isInEditorMode() && (mSeatPrison == nullptr) && !getSeat()->canOwnedCreatureBePickedUpBy(seat) &&
       !isSandboxHeroFor(seat))
    {
        return false;
    }

    if(!getGameMap()->isInEditorMode() && (mSeatPrison != nullptr) && !mSeatPrison->canOwnedCreatureBePickedUpBy(seat))
        return false;

    // KO creatures cannot be picked up
    if(isKo())
        return false;

    // A creature controlled by a player cannot be picked up
    if(isPossessed())
        return false;

    // The champion cannot be held in the hand
    if(getDefinition()->isChampion())
        return false;

    return true;
}

void Creature::pickup()
{
    // Stop the creature walking and set it off the map to prevent the AI from running on it.
    removeEntityFromPositionTile();
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    mActivity = CreatureActivity();

    if(!getIsOnServerMap())
        return;

    if(getHasVisualDebuggingEntities())
        computeVisualDebugEntities();

    mIsInHand = true;
    if(getSeat() != nullptr)
        ++getSeat()->getStatistics().mCreaturesPickedUp;

    recordScriptEvent(*this, "pickedup");

    fireCreatureSound(CreatureSound::Pickup);
}

int32_t Creature::getNbRecentSlaps(int32_t nbTurns) const
{
    int64_t turnNumber = getGameMap()->getTurnNumber();
    int32_t nbSlaps = 0;
    for(int64_t slapTurn : mSlapTurns)
    {
        if(turnNumber - slapTurn <= nbTurns)
            ++nbSlaps;
    }

    return nbSlaps;
}

bool Creature::canGoThroughTile(Tile* tile) const
{
    if(tile == nullptr)
        return false;

    return getMoveSpeed(tile) > 0.0;
}

bool Creature::tryDrop(Seat* seat, Tile* tile)
{
    // check whether the tile is a ground tile ...
    if(tile->isFullTile())
        return false;

    // In editor mode, we allow creatures to be dropped anywhere they can walk
    if(getGameMap()->isInEditorMode() && canGoThroughTile(tile))
        return true;

    // we cannot drop a creature on a tile we don't see
    if(!seat->hasVisionOnTile(tile))
        return false;

    // In the sandbox, the heroes taken from the toolbox can be dropped on any ground the seat sees
    if(isSandboxHeroFor(seat) && canGoThroughTile(tile))
        return true;

    // If it is a worker, he can be dropped on dirt
    if (getDefinition()->isWorker() && (tile->getTileVisual() == TileVisual::dirtGround || tile->getTileVisual() == TileVisual::goldGround))
        return true;


    if((tile != nullptr) &&
       (tile->getCoveringRoom() != nullptr))
    {
        Room* room = tile->getCoveringRoom();
        if(room->getType() == RoomType::arena)
            return room->hasOpenCreatureSpot(this);
    }

    
    // Every creature can be dropped on allied claimed tiles
    if(tile->isClaimedForSeat(seat))
        return true;



    return false;
}

void Creature::drop(const Ogre::Vector3& v)
{
    setPosition(v);
    if(!getIsOnServerMap())
    {
        mDropCooldown = 2;
        updateHexenHenMesh();
        return;
    }

    if(getHasVisualDebuggingEntities())
        computeVisualDebugEntities();

    fireCreatureSound(CreatureSound::Drop);

    mIsInHand = false;
    if(getSeat() != nullptr)
        ++getSeat()->getStatistics().mCreaturesDropped;

    // The creature is temporary KO
    mKoTurnCounter = mDefinition->getTurnsStunDropped();
    computeCreatureOverlayMoodValue();

    // Action queue should be empty but it shouldn't hurt
    clearActionQueue();

    // In editor mode, we do not check for forced actions
    if(getGameMap()->isInEditorMode())
        return;

    if(mDefinition->isWorker())
    {
        // If a worker is dropped, he will search in the tile he is and in the 4 neighboor tiles.
        // 1 - If the tile he is in a treasury and he is carrying gold, he should deposit it
        // 2 - if one of the 4 neighboor tiles is marked, he will dig
        // 3 - if there is a carryable entity where it is dropped, it should try to carry it
        // 4 - if the the tile he is in is not claimed and one of the neigbboor tiles is claimed, he will claim
        // 5 - if the the tile he is in is claimed and one of the neigbboor tiles is not claimed, he will claim
        // 6 - If the tile he is in is claimed and one of the neigbboor tiles is a not claimed wall, he will claim
        Tile* position = getPositionTile();
        Seat* seat = getSeat();
        Tile* tileMarkedDig = nullptr;
        Tile* tileMarkedDigPos = nullptr;
        Tile* tileToClaim = nullptr;
        Tile* tileWallNotClaimed = nullptr;
        for (Tile* tile : position->getAllNeighbors())
        {
            if(tileMarkedDig == nullptr &&
                tile->getMarkedForDigging(getGameMap()->getPlayerBySeat(seat))
                )
            {
                // Check if there is room for digging
                std::vector<Tile*> tiles;
                tile->canWorkerDig(*this, tiles);
                // We search for the closest neighbor tile (may be not the position
                // tile if the player drops several workers at the same tile)
                float distBest = -1;
                for (Tile* neigh : tiles)
                {
                    float dist = Pathfinding::squaredDistanceTile(*position, *neigh);
                    if((distBest != -1) && (distBest <= dist))
                        continue;

                    distBest = dist;
                    tileMarkedDig = tile;
                    tileMarkedDigPos = neigh;
                }
            }
            else if(tileToClaim == nullptr &&
                tile->isClaimedForSeat(seat) &&
                position->isGroundClaimable(seat)
                )
            {
                tileToClaim = position;
            }
            else if(tileToClaim == nullptr &&
                position->isClaimedForSeat(seat) &&
                tile->isGroundClaimable(seat)
                )
            {
                tileToClaim = tile;
            }
            else if(tileWallNotClaimed == nullptr &&
                position->isClaimedForSeat(seat) &&
                tile->isWallClaimable(seat)
                )
            {
                tileWallNotClaimed = tile;
            }
        }

        // We try to deposit gold if we are on a room while carrying gold
        if((mGoldCarried > 0) && (mDigRate > 0.0) &&
           (position->getCoveringRoom() != nullptr))
        {
            int deposited = position->getCoveringRoom()->depositGold(mGoldCarried, position);
            if(deposited > 0)
            {
                mGoldCarried -= deposited;
                return;
            }
        }

        std::vector<GameEntity*> carryable;
        position->fillWithCarryableEntities(this, carryable);

        // Now, we can decide
        if((tileMarkedDig != nullptr) && (tileMarkedDigPos != nullptr) && (mDigRate > 0.0))
        {
            pushAction(Utils::make_unique<CreatureActionSearchTileToDig>(*this, true));
            pushAction(Utils::make_unique<CreatureActionDigTile>(*this, *tileMarkedDig, *tileMarkedDigPos));
            return;
        }

        if(!carryable.empty())
        {
            // We look for the most important entity to carry
            GameEntity* entityToCarry = carryable[0];
            for(GameEntity* entity : carryable)
            {
                if(entity->getEntityCarryType(this) <= entityToCarry->getEntityCarryType(this))
                    continue;

                entityToCarry = entity;
            }

            pushAction(Utils::make_unique<CreatureActionGrabEntity>(*this, *entityToCarry));
            return;
        }

        if((tileToClaim != nullptr) && (mClaimRate > 0.0))
        {
            pushAction(Utils::make_unique<CreatureActionSearchGroundTileToClaim>(*this, true));
            pushAction(Utils::make_unique<CreatureActionClaimGroundTile>(*this, *tileToClaim));
            return;
        }

        if((tileWallNotClaimed != nullptr) && (mClaimRate > 0.0))
        {
            pushAction(Utils::make_unique<CreatureActionSearchWallTileToClaim>(*this, true));
            pushAction(Utils::make_unique<CreatureActionClaimWallTile>(*this, *tileWallNotClaimed));
            return;
        }

        // We couldn't find why we were dropped here. Let's behave as usual
        return;
    }

    // Fighters
    // If we are dropped on a tile with a building, we notify it so that it
    // can do some special actions
    Tile* tile = getPositionTile();
    if((tile != nullptr) &&
       (tile->getCoveringBuilding() != nullptr))
    {
        Building* building = tile->getCoveringBuilding();
        building->creatureDropped(*this);
        return;
    }
}

bool Creature::resizeMeshAfterDrop()
{
    RenderManager::getSingleton().rrScaleCreature(*this);
    return false;
}


bool Creature::parkToWallTile(Tile* wallTile, Tile* nTile)
{
    if(nTile == nullptr || wallTile == nullptr )
        return false;

    Tile *posTile = getPositionTile();
    if(posTile == nullptr)
        return false;

    Ogre::Vector2 parkingPoint;
    parkingPoint = (wallTile->getPosition2d() - nTile->getPosition2d())*0.4 + nTile->getPosition2d() ;

    
    std::list<Tile*> result = getGameMap()->path(this, nTile);

    std::vector<Ogre::Vector2> path;
    tileToVector2(result, path, true, 0.0);
    
    path.push_back(parkingPoint);
    
    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path,false);

    pushAction(Utils::make_unique<CreatureActionParkToTile>(*this));    
    return true;
}

bool Creature::setDestination(Tile* tile)
{
    if(tile == nullptr)
        return false;

    Tile *posTile = getPositionTile();
    if(posTile == nullptr)
        return false;

    std::list<Tile*> result = getGameMap()->path(this, tile);

    std::vector<Ogre::Vector2> path;
    tileToVector2(result, path, true, 0.0);
    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path,true);
    if(!isMoving() && posTile != tile)
        return false;
    pushAction(Utils::make_unique<CreatureActionWalkToTile>(*this));
    return true;
}

bool Creature::wanderRandomly(const std::string& animationState)
{
    // We pick randomly a visible tile far away (at the end of visible tiles)
    if(mTilesWithinSightRadius.empty())
        return false;

    // Add reachable tiles only before searching for one of them
    std::vector<Tile*> reachableTiles;
    for (Tile* tile: mTilesWithinSightRadius)
    {
        if (getGameMap()->pathExists(this, getPositionTile(), tile))
            reachableTiles.push_back(tile);
    }

    if (reachableTiles.empty())
        return false;

    Tile* tileDestination = reachableTiles[Random::Uint(0, reachableTiles.size() - 1)];
    setDestination(tileDestination);
    return false;
}

bool Creature::isAttackable(Tile* tile, Seat* seat) const
{
    if(mHp <= 0.0)
        return false;

    // KO Creature to death creatures are not a threat and cannot be attacked. However, temporary KO can be
    if(mKoTurnCounter < 0)
        return false;

    // Creatures in prison are not a treat and cannot be attacked
    if(mSeatPrison != nullptr)
        return false;

    return true;
}

EntityCarryType Creature::getEntityCarryType(Creature* carrier)
{
    // Workers cannot be carried to crypt/prison
    if(getDefinition()->isWorker())
        return EntityCarryType::notCarryable;

    // KO to death entities can be carried
    if(mKoTurnCounter < 0)
        return EntityCarryType::koCreature;

    // Dead creatures are carryable
    if(getHP() <= 0.0)
        return EntityCarryType::corpse;

    return EntityCarryType::notCarryable;
}

void Creature::notifyEntityCarryOn(Creature* carrier)
{
    removeEntityFromPositionTile();
}

void Creature::notifyEntityCarryOff(const Ogre::Vector3& position)
{
    mPosition = position;
    addEntityToPositionTile();
}

void Creature::carryEntity(GameEntity* carriedEntity)
{
    if(!getIsOnServerMap())
        return;

    OD_ASSERT_TRUE(carriedEntity != nullptr);
    OD_ASSERT_TRUE(mCarriedEntity == nullptr);
    mCarriedEntity = nullptr;
    if(carriedEntity == nullptr)
        return;

    // We remove the carried entity from the clients gamemaps as well as the carrier
    // and we send the carrier creation message (that will embed the carried)
    carriedEntity->fireRemoveEntityToSeatsWithVision();
    // We only notify seats that already had vision. We copy the seats with vision list
    // because fireRemoveEntityToSeatsWithVision will empty it.
    std::vector<Seat*> seatsWithVision = mSeatsWithVisionNotified;
    // We remove ourself and send the creation
    fireRemoveEntityToSeatsWithVision();
    mCarriedEntity = carriedEntity;
    notifySeatsWithVision(seatsWithVision);
}

void Creature::releaseCarriedEntity()
{
    if(!getIsOnServerMap())
        return;

    GameEntity* carriedEntity = mCarriedEntity;
    mCarriedEntity = nullptr;
    if(carriedEntity == nullptr)
    {
        OD_LOG_ERR("name=" + getName());
        return;
    }

    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr)
            continue;
        if(!seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::releaseCarriedEntity, seat->getPlayer());
        serverNotification->mPacket << getName() << carriedEntity->getObjectType();
        serverNotification->mPacket << carriedEntity->getName();
        serverNotification->mPacket << mPosition;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

bool Creature::canSlap(Seat* seat)
{
    Tile* tile = getPositionTile();
    if(tile == nullptr)
    {
        OD_LOG_ERR("entityName=" + getName());
        return false;
    }

    if(mDropCooldown > 0)
        return false;

    if(getGameMap()->isInEditorMode())
        return true;

    if(getHP() <= 0.0)
        return false;

    // A creature controlled by a player cannot be slapped
    if(isPossessed())
        return false;

    // If the creature is in prison, it can be slapped by the jail owner only
    if(mSeatPrison != nullptr)
        return (mSeatPrison == seat);

    // Only the owning player can slap a creature
    if(getSeat() != seat)
        return false;

    return true;
}

void Creature::slap()
{
    if(!getIsOnServerMap())
        return;

    fireCreatureSound(CreatureSound::Slap);

    // In editor mode, we remove the creature
    if(getGameMap()->isInEditorMode())
    {
        removeFromGameMap();
        deleteYourself();
        return;
    }

    // A slap sends the champion away
    if(getDefinition()->isChampion())
    {
        dismissChampion();
        return;
    }

    if(getSeat() != nullptr)
        ++getSeat()->getStatistics().mCreaturesSlapped;

    recordScriptEvent(*this, "slapped");

    CreatureEffectSlap* effect = new CreatureEffectSlap(
        ConfigManager::getSingleton().getSlapEffectDuration(), "");
    addCreatureEffect(effect);

    // We remember the slap to compute the mood. Only the latest ones are kept
    mSlapTurns.push_back(getGameMap()->getTurnNumber());
    if(mSlapTurns.size() > 10)
        mSlapTurns.erase(mSlapTurns.begin());

    mHp -= mMaxHP * ConfigManager::getSingleton().getSlapDamagePercent() / 100.0;
    computeCreatureOverlayHealthValue();
}

void Creature::fireAddEntity(Seat* seat, bool async, NodeType nt )
{
    if(async)
    {
        ServerNotification serverNotification(
            ServerNotificationType::addEntity, seat->getPlayer());
        serverNotification.mPacket << nt;          
        exportHeadersToPacket(serverNotification.mPacket);
        exportToPacket(serverNotification.mPacket, seat);
        ODServer::getSingleton().sendAsyncMsg(serverNotification);

        if(mCarriedEntity != nullptr)
        {
            OD_LOG_ERR("Trying to fire add creature in async mode name=" + getName() + " while carrying " + mCarriedEntity->getName());
        }
        return;
    }

    ServerNotification* serverNotification = new ServerNotification(
        ServerNotificationType::addEntity, seat->getPlayer());
    serverNotification->mPacket << nt;      
    exportHeadersToPacket(serverNotification->mPacket);
    exportToPacket(serverNotification->mPacket, seat);
    ODServer::getSingleton().queueServerNotification(serverNotification);

    if(mCarriedEntity != nullptr)
    {
        mCarriedEntity->addSeatWithVision(seat, false);

        serverNotification = new ServerNotification(
            ServerNotificationType::carryEntity, seat->getPlayer());
        serverNotification->mPacket << getName() << mCarriedEntity->getObjectType();
        serverNotification->mPacket << mCarriedEntity->getName();
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Creature::fireRemoveEntity(Seat* seat,NodeType nt)
{
    // If we are carrying an entity, we release it first, then we can remove it and us
    if(mCarriedEntity != nullptr)
    {
        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::releaseCarriedEntity, seat->getPlayer());
        serverNotification->mPacket << getName() << mCarriedEntity->getObjectType();
        serverNotification->mPacket << mCarriedEntity->getName();
        serverNotification->mPacket << mPosition;
        ODServer::getSingleton().queueServerNotification(serverNotification);

        mCarriedEntity->removeSeatWithVision(seat);
    }

    const std::string& name = getName();
    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::removeEntity, seat->getPlayer());
    GameEntityType type = getObjectType();
    serverNotification->mPacket << type;
    serverNotification->mPacket << name;
    serverNotification->mPacket << NodeType::MTILES_NODE;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireCreatureRefreshIfNeeded()
{
    const CreatureActivity activity = getActivity();
    if(!(mActivity == activity))
    {
        mActivity = activity;
        mNeedFireRefresh = true;
    }

    if(!mNeedFireRefresh)
        return;

    mNeedFireRefresh = false;
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr)
            continue;
        if(!seat->getPlayer()->getIsHuman())
            continue;

        const std::string& name = getName();
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::entitiesRefresh, seat->getPlayer());
        uint32_t nbCreature = 1;
        serverNotification->mPacket << nbCreature;
        serverNotification->mPacket << GameEntityType::creature;
        serverNotification->mPacket << name;
        exportToPacketForUpdate(serverNotification->mPacket, seat);
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Creature::fireChatMsgTookFee(int goldTaken)
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg;
    // We don't display the same message if we have taken all our fee or only a part of it
    if(getGoldFee() <= 0)
        msg = getName() + " took its fee: " + Helper::toString(goldTaken);
    else
        msg = getName() + " took " + Helper::toString(goldTaken) + " from its fee";

    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireChatMsgLeftDungeon()
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg = getName() + " left your dungeon";
    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireChatMsgLeavingDungeon()
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg = getName() + " is leaving your dungeon";
    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireChatMsgBecameRogue()
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg = getName() + " is not under your control anymore !";
    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireChatMsgUnhappy()
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg = getName() + " is unhappy !";
    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::fireChatMsgFurious()
{
    if(getSeat()->getPlayer() == nullptr)
        return;
    if(!getSeat()->getPlayer()->getIsHuman())
        return;
    if(getSeat()->getPlayer()->getHasLost())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::chatServer, getSeat()->getPlayer());
    std::string msg = getName() + " is furious !";
    serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::setupDefinition(GameMap& dtc, const CreatureDefinition& defaultWorkerCreatureDefinition)
{
    bool setHpToStrHp = false;
    if(mDefinition == nullptr)
    {
        // If the classname corresponds to the default worker CreatureDefinition, we use
        // the dedicated class. The correct one will be set after the seat is initialized
        if(!mDefinitionString.empty() &&  mDefinitionString.compare(ConfigManager::DefaultWorkerCreatureDefinition) != 0)
        {
            mDefinition = dtc.getClassDescription(mDefinitionString);
        }
        else
        {
            // If we are in editor mode, we take the default worker class. Otherwise, we take
            // the default worker from the seat faction
            if(dtc.isInEditorMode() || !mSeat)
                mDefinition = &defaultWorkerCreatureDefinition;
            else
                mDefinition = getSeat()->getWorkerClassToSpawn();
        }

        if(mDefinition == nullptr)
        {
            OD_LOG_ERR("Definition=" + mDefinitionString);
            return;
        }

        if(getIsOnServerMap())
        {
            setHpToStrHp = true;

            // name
            if (getName().compare("autoname") == 0)
            {
                std::string name = getGameMap()->nextUniqueNameCreature(mDefinition->getClassName());
                setName(name);
            }
        }
    }

    if(getIsOnServerMap())
    {
        for(const CreatureSkill* skill : mDefinition->getCreatureSkills())
        {
            CreatureSkillData skillData(skill, skill->getCooldownNbTurns(), 0);
            mSkillData.push_back(skillData);
        }
    }

    buildStats();

    // Now, the max hp is known. If needed, we set it
    if(setHpToStrHp)
    {
        if(mHpString.compare("max") == 0)
            mHp = mMaxHP;
        else
            mHp = Helper::toDouble(mHpString);

        computeCreatureOverlayHealthValue();
    }
}

void Creature::fireCreatureSound(CreatureSound sound)
{
    Tile* posTile = getPositionTile();
    if(posTile == nullptr)
        return;

    std::string soundFamily;
    switch(sound)
    {
        case CreatureSound::Pickup:
            soundFamily = getDefinition()->getSoundFamilyPickup();
            break;
        case CreatureSound::Drop:
            soundFamily = getDefinition()->getSoundFamilyDrop();
            break;
        case CreatureSound::Attack:
            soundFamily = getDefinition()->getSoundFamilyAttack();
            break;
        case CreatureSound::Die:
            soundFamily = getDefinition()->getSoundFamilyDie();
            break;
        case CreatureSound::Slap:
            soundFamily = getDefinition()->getSoundFamilySlap();
            break;
        case CreatureSound::Dig:
            soundFamily = "Default/Dig";
            break;
        default:
            OD_LOG_ERR("Wrong CreatureSound value=" + Helper::toString(static_cast<uint32_t>(sound)));
            return;
    }

    std::string soundComplete = "Creatures/" + soundFamily;
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr)
            continue;
        if(!seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::playSpatialSound, seat->getPlayer());
        serverNotification->mPacket << soundComplete << posTile->getX() << posTile->getY();
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Creature::fireCombatImpact(bool weaponClash, bool bodyDamage,
    const Ogre::Vector3& attackerPosition)
{
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::creatureCombatImpact, seat->getPlayer());
        notification->mPacket << getName() << weaponClash << bodyDamage
            << attackerPosition;
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

void Creature::fireChickenFeeding(const std::string& chickenName,
    const Ogre::Vector3& chickenPosition)
{
    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr || !seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* notification = new ServerNotification(
            ServerNotificationType::creatureChickenFeeding, seat->getPlayer());
        notification->mPacket << getName() << chickenName << chickenPosition;
        ODServer::getSingleton().queueServerNotification(notification);
    }
}

void Creature::itsPayDay()
{
    // Rogue creatures do not have to be paid
    if(getSeat()->isRogueSeat())
        return;

    mGoldFee += mDefinition->getFee(getLevel());
}

void Creature::increaseHunger(double value)
{
    if(getSeat()->isRogueSeat())
        return;

    mHunger = std::min(100.0, mHunger + value);
}

void Creature::decreaseWakefulness(double value)
{
    if(getSeat()->isRogueSeat())
        return;

    mWakefulness = std::max(0.0, mWakefulness - value);
}

void Creature::addCasinoMood(double points)
{
    double maxPoints = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("CasinoMoodMax", DEFAULT_CASINO_MOOD_MAX);
    mCasinoMood = std::max(-maxPoints, std::min(maxPoints, mCasinoMood + points));
}

void Creature::addPitMood(double points)
{
    double maxPoints = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("PitMoodMax", DEFAULT_PIT_MOOD_MAX);
    mPitMood = std::max(-maxPoints, std::min(maxPoints, mPitMood + points));
}

bool Creature::isGoodAligned() const
{
    const std::vector<std::string>& heroClasses = ConfigManager::getSingleton().getFactionSpawnPool("Hero");
    return std::find(heroClasses.begin(), heroClasses.end(), mDefinition->getClassName()) != heroClasses.end();
}

bool Creature::isHatedCompanyNear() const
{
    // Only creatures that have moods can be annoyed
    if(mDefinition->getCreatureMoods().empty() || !getIsOnMap() || !isAlive())
        return false;

    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
        return false;

    bool isGood = isGoodAligned();
    double radius = ConfigManager::getSingleton().getRoomConfigDoubleOrDefault("HatedCompanyRadius", DEFAULT_HATED_COMPANY_RADIUS);
    double squaredRadius = radius * radius;
    const std::vector<Creature*>& creatures = getGameMap()->getCreatures();
    for(Creature* other : creatures)
    {
        if((other == this) || !other->isAlive() || !other->getIsOnMap())
            continue;

        // Workers have no alignment
        if(other->getDefinition()->isWorker())
            continue;

        if(!getSeat()->isAlliedSeat(other->getSeat()))
            continue;

        if(other->isGoodAligned() == isGood)
            continue;

        Tile* otherTile = other->getPositionTile();
        if(otherTile == nullptr)
            continue;

        if(Pathfinding::squaredDistanceTile(*myTile, *otherTile) <= squaredRadius)
            return true;
    }
    return false;
}

void Creature::addPrayerRelief(int32_t relief, int32_t maxRelief)
{
    mPrayerRelief = std::min(mPrayerRelief + relief, maxRelief);
}

void Creature::computeMood()
{
    mMoodPoints = CreatureMoodManager::computeCreatureMoodModifiers(*this);

    CreatureMoodLevel oldMoodValue = mMoodValue;
    mMoodValue = CreatureMoodManager::getCreatureMoodLevel(mMoodPoints);
    if(mMoodValue == oldMoodValue)
        return;

    mNeedFireRefresh = true;

    if((mMoodValue >= CreatureMoodLevel::Furious) &&
       (oldMoodValue < CreatureMoodLevel::Furious))
    {
        // We became unhappy
        fireChatMsgFurious();
    }
    else if((mMoodValue > CreatureMoodLevel::Neutral) &&
       (oldMoodValue <= CreatureMoodLevel::Neutral))
    {
        // We became unhappy
        fireChatMsgUnhappy();
    }
}

void Creature::computeCreatureOverlayHealthValue()
{
    if(!getIsOnServerMap())
        return;

    uint32_t value = 0;
    double hp = getHP();
    // Note that we make a special case for hp = 0 to avoid errors due to roundness
    if(hp <= 0)
    {
        value = NB_OVERLAY_HEALTH_VALUES - 1;
    }
    else
    {
        uint32_t nbSteps = NB_OVERLAY_HEALTH_VALUES - 2;
        double healthStep = getMaxHp() / static_cast<double>(nbSteps);
        double tmpHealth = getMaxHp();
        for(value = 0; value < nbSteps; ++value)
        {
            if(hp >= tmpHealth)
                break;

            tmpHealth -= healthStep;
        }
    }

    if(mOverlayHealthValue != value)
    {
        mOverlayHealthValue = value;
        mNeedFireRefresh = true;
    }
}

void Creature::computeCreatureOverlayMoodValue()
{
    if(!getIsOnServerMap())
        return;

    uint32_t value = 0;
    // The creature mood applies only if the creature is alive
    if(isAlive())
    {
        switch(mMoodValue)
        {
            case CreatureMoodLevel::Angry:
                value |= CreatureMoodValues::Angry;
                break;
            case CreatureMoodLevel::Furious:
                value |= CreatureMoodValues::Furious;
                break;
            default:
                break;
        }

        // We update the mood bit array according to actions in the list
        for (const std::unique_ptr<CreatureAction>& ca : mActions)
            value |= ca.get()->updateMoodModifier();

        if(mKoTurnCounter < 0)
            value |= CreatureMoodValues::KoDeath;
        else if(mKoTurnCounter > 0)
            value |= CreatureMoodValues::KoTemp;

        if(isHungry())
            value |= CreatureMoodValues::Hungry;

        if(isTired())
            value |= CreatureMoodValues::Tired;

        if(mSeatPrison != nullptr)
            value |= CreatureMoodValues::InJail;
    }

    if(mOverlayMoodValue != value)
    {
        mOverlayMoodValue = value;
        mNeedFireRefresh = true;
    }
}

void Creature::addCreatureEffect(CreatureEffect* effect)
{
    std::string effectName = nextParticleSystemsName();

    OD_LOG_INF("Added CreatureEffect name=" + effectName + " on creature=" + getName());

    CreatureParticleEffect* particleEffect = new CreatureParticleEffect(*this, effectName, effect->getParticleEffectScript(),
        effect->getNbTurnsEffect(), effect);
    mEntityParticleEffects.push_back(particleEffect);

    mNeedFireRefresh = true;
}

void Creature::addParticleEffect(const std::string& effectScript, uint32_t nbTurns)
{
    EntityParticleEffect* effect = new EntityParticleEffect(
        nextParticleSystemsName(), effectScript, nbTurns);
    mEntityParticleEffects.push_back(effect);
}

bool Creature::removeCreatureEffect(CreatureEffect* effectForDeletion)
{
    mNeedFireRefresh = false;
    for(auto it =  mEntityParticleEffects.begin(); it != mEntityParticleEffects.end(); ++it)
    {
        CreatureParticleEffect* effect = static_cast<CreatureParticleEffect*>(*it);
        if(effect->mEffect == effectForDeletion)
        {
            effect->mNbTurnsEffect = 0 ;
            effect->mEffect->mNbTurnsEffect = 0 ;
            mNeedFireRefresh = true;
            return true;
        }
        
    }
    return false;
}

bool Creature::isDefector() const
{
    for(const EntityParticleEffect* effect : mEntityParticleEffects)
    {
        if(effect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
            continue;

        const CreatureParticleEffect* creatureEffect = static_cast<const CreatureParticleEffect*>(effect);
        if((creatureEffect->mEffect->getEffectName() == "Defector") &&
           (creatureEffect->mEffect->getNbTurnsEffect() > 0))
        {
            return true;
        }
    }

    return false;
}

bool Creature::isHexenHen() const
{
    if(!getIsOnServerMap())
        return mIsHexenHen;

    for(const EntityParticleEffect* effect : mEntityParticleEffects)
    {
        if(effect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
            continue;

        const CreatureParticleEffect* creatureEffect = static_cast<const CreatureParticleEffect*>(effect);
        if((creatureEffect->mEffect->getEffectName() == "HexenHen") &&
           (creatureEffect->mEffect->getNbTurnsEffect() > 0))
        {
            return true;
        }
    }

    return false;
}

bool Creature::isFrozen() const
{
    if(!getIsOnServerMap())
        return false;

    for(const EntityParticleEffect* effect : mEntityParticleEffects)
    {
        if(effect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
            continue;

        const CreatureParticleEffect* creatureEffect = static_cast<const CreatureParticleEffect*>(effect);
        if((creatureEffect->mEffect->getEffectName() == "Frozen") &&
           (creatureEffect->mEffect->getNbTurnsEffect() > 0))
        {
            return true;
        }
    }

    return false;
}

bool Creature::isInvisible() const
{
    if(!getIsOnServerMap())
        return false;

    for(const EntityParticleEffect* effect : mEntityParticleEffects)
    {
        if(effect->getEntityParticleEffectType() != EntityParticleEffectType::creature)
            continue;

        const CreatureParticleEffect* creatureEffect = static_cast<const CreatureParticleEffect*>(effect);
        if((creatureEffect->mEffect->getEffectName() == "Invisible") &&
           (creatureEffect->mEffect->getNbTurnsEffect() > 0))
        {
            return true;
        }
    }

    return false;
}

const std::string& Creature::getCurrentMeshName() const
{
    static const std::string chickenMeshName = "Chicken.mesh";
    if(isHexenHen())
        return chickenMeshName;

    return getDefinition()->getMeshName();
}

void Creature::updateHexenHenMesh()
{
    if(getIsOnServerMap() || !isMeshExisting() || !getIsOnMap())
        return;

    if(mHexenHenMeshShown == mIsHexenHen)
        return;

    destroyMeshWeapons();
    mHexenHenMeshShown = mIsHexenHen;
    RenderManager::getSingleton().rrChangeCreatureMesh(this);
    createMeshWeapons();
    RenderManager::getSingleton().rrScaleCreature(*this);
}

void Creature::handleHexenHenUpkeep()
{
    if(!mActions.empty())
    {
        clearActionQueue();
        clearDestinations(EntityAnimation::idle_anim, true, true);
    }

    if(isMoving())
        return;

    if(Random::Int(0, 3) != 0)
        return;

    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
        return;

    Tile* destTile = getGameMap()->getTile(myTile->getX() + Random::Int(-2, 2), myTile->getY() + Random::Int(-2, 2));
    if((destTile == nullptr) || (destTile == myTile) || !canGoThroughTile(destTile))
        return;

    std::list<Tile*> tempPath = getGameMap()->path(this, destTile);
    if(tempPath.empty())
        return;

    std::vector<Ogre::Vector2> path;
    tileToVector2(tempPath, path, true, 0.0);
    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
}

bool Creature::isHurt() const
{
    //On server side, we test HP
    if(getIsOnServerMap())
        return getHP() < getMaxHp();

    // On client side, we test overlay value. 0 represents full health
    return mOverlayHealthValue > 0;
}

bool Creature::isKo() const
{
    if(getIsOnServerMap())
        return mKoTurnCounter != 0;

    // On client side, we test mood overlay value
    return (mOverlayMoodValue & CreatureMoodValues::KoDeathOrTemp) != 0;
}

bool Creature::isKoDeath() const
{
    if(getIsOnServerMap())
        return mKoTurnCounter < 0;

    // On client side, we test mood overlay value
    return (mOverlayMoodValue & CreatureMoodValues::KoDeath) != 0;
}

bool Creature::isKoTemp() const
{
    if(getIsOnServerMap())
        return mKoTurnCounter > 0;

    // On client side, we test mood overlay value
    return (mOverlayMoodValue & CreatureMoodValues::KoTemp) != 0;
}

bool Creature::isInPrison() const
{
    return mSeatPrison != nullptr;
}

void Creature::correctEntityMovePosition(Ogre::Vector2& position)
{
    static const double offset = 0.3;
    if(position.x > 0)
        position.x += Random::Double(-offset, offset);

    if(position.y > 0)
        position.y += Random::Double(-offset, offset);

    // if(position.z > 0)
    //     position.z += Random::Double(-offset, offset);
}

void Creature::checkWalkPathValid(bool includeWalkDistortion)
{
    bool stop = false;
    for(const Ogre::Vector2& dest : mWalkQueue)
    {
        Tile* tile = getGameMap()->getTile(Helper::round(dest.x), Helper::round(dest.y));
        if(tile == nullptr)
        {
            stop = true;
            break;
        }

        if(!canGoThroughTile(tile))
        {
            stop = true;
            break;
        }
    }

    if(!stop && getIsOnServerMap())
        stop = RoomObjectNavigation::blocked(*this,
            std::vector<Ogre::Vector2>(mWalkQueue.begin(), mWalkQueue.end()), includeWalkDistortion);

    if(!stop)
        return;

    // There is an unpassable tile in our way. We stop what we are doing
    clearDestinations(EntityAnimation::idle_anim, true, true);
}

void Creature::setJobCooldown(int val)
{
    // If the creature has been slapped, its cooldown is decreased
    if(hasSlapEffect())
        val = Helper::round(static_cast<float>(val) * 0.8f);

    mJobCooldown = val;
}

bool Creature::isTired() const
{
    if(getIsOnServerMap())
        return mWakefulness <= 20.0;

    return (mOverlayMoodValue & CreatureMoodValues::Tired) != 0;
}

bool Creature::isHungry() const
{
    if(getIsOnServerMap())
        return mHunger >= 80.0;

    return (mOverlayMoodValue & CreatureMoodValues::Hungry) != 0;
}

void Creature::resetKoTurns()
{
    mKoTurnCounter = 0;
    mNeedFireRefresh = true;
}

void Creature::stun(int32_t nbTurns)
{
    if(!isAlive() || (nbTurns <= 0) || (mKoTurnCounter < 0))
        return;

    mKoTurnCounter = std::max(mKoTurnCounter, nbTurns);
    computeCreatureOverlayMoodValue();
    clearActionQueue();
}

void Creature::knockOutToDeath()
{
    if(mKoTurnCounter < 0)
        return;

    mKoTurnCounter = -ConfigManager::getSingleton().getNbTurnsKoCreatureAttacked();
    OD_LOG_INF("creature=" + getName() + " has been knocked out");
    dropCarriedEquipment();
    computeCreatureOverlayMoodValue();
    mNeedFireRefresh = true;
}

void Creature::setInJail(Room* prison)
{
    if(prison == nullptr)
    {
        if(mSeatPrison == nullptr)
            return;

        mSeatPrison = nullptr;
        mNeedFireRefresh = true;
        return;
    }

    // Creature is set in prison
    if(mSeatPrison == prison->getSeat())
        return;

    mSeatPrison = prison->getSeat();
    mNeedFireRefresh = true;
}

double Creature::getThreat() const
{
    // Threat multiplier in percent for the levels 1 to 10. Above level 10 the last
    // step of the table (100 percent per level) is continued.
    static const double THREAT_PERCENT_BY_LEVEL[10] = {100.0, 125.0, 150.0, 175.0, 200.0, 225.0, 250.0, 300.0, 400.0, 500.0};
    unsigned int level = (mLevel < 1) ? 1 : mLevel;
    double percent;
    if(level <= 10)
        percent = THREAT_PERCENT_BY_LEVEL[level - 1];
    else
        percent = THREAT_PERCENT_BY_LEVEL[9] + 100.0 * static_cast<double>(level - 10);

    return mHp * percent / 100.0;
}

bool Creature::isDangerous(const Creature* creature, int distance) const
{
    if(getDefinition()->isWorker())
        return false;

    if(isHexenHen())
        return false;

    return true;
}

void Creature::clientUpkeep()
{
    MovableGameEntity::clientUpkeep();
    if(mDropCooldown > 0)
        --mDropCooldown;
}

void Creature::setMoveSpeedModifier(double modifier)
{
    mSpeedModifier = modifier;

    mGroundSpeed = mDefinition->getMoveSpeedGround();
    mWaterSpeed = mDefinition->getMoveSpeedWater();
    mLavaSpeed  = mDefinition->getMoveSpeedLava();

    double multiplier = mLevel - 1;
    if (multiplier > 0.0)
    {
        mGroundSpeed += mDefinition->getGroundSpeedPerLevel() * multiplier;
        mWaterSpeed += mDefinition->getWaterSpeedPerLevel() * multiplier;
        mLavaSpeed += mDefinition->getLavaSpeedPerLevel() * multiplier;
    }

    mGroundSpeed *= mSpeedModifier;
    mWaterSpeed *= mSpeedModifier;
    mLavaSpeed *= mSpeedModifier;
    mNeedFireRefresh = true;
}

void Creature::clearMoveSpeedModifier()
{
    setMoveSpeedModifier(1.0);
}

void Creature::setDefenseModifier(double phy, double mag, double ele)
{
    mPhysicalDefense = mDefinition->getPhysicalDefense();
    mMagicalDefense = mDefinition->getMagicalDefense();
    mElementDefense = mDefinition->getElementDefense();

    mPhysicalDefense += phy;
    mMagicalDefense += mag;
    mElementDefense += ele;

    // Improve the stats to the current level
    double multiplier = mLevel - 1;
    if (multiplier <= 0.0)
        return;

    mPhysicalDefense += mDefinition->getPhysicalDefPerLevel() * multiplier;
    mMagicalDefense += mDefinition->getMagicalDefPerLevel() * multiplier;
    mElementDefense += mDefinition->getElementDefPerLevel() * multiplier;

    mNeedFireRefresh = true;
}

void Creature::clearDefenseModifier()
{
    setDefenseModifier(0.0, 0.0, 0.0);
}

void Creature::setStrengthModifier(double modifier)
{
    mModifierStrength = modifier;
    // Since strength is not used on client side, no need to send it
}

void Creature::clearStrengthModifier()
{
    setStrengthModifier(1.0);
}

bool Creature::isWarmup() const
{
    for(const CreatureSkillData& skillData : mSkillData)
    {
        if(skillData.mWarmup > 0)
            return true;
    }

    return false;
}

void Creature::fight()
{
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    bool ko = getSeat()->getKoCreatures();
    pushAction(Utils::make_unique<CreatureActionFight>(*this, nullptr, ko, true));
}

void Creature::fightCreature(Creature& creature, bool ko, bool notifyPlayerIfHit)
{
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionFight>(*this, &creature, ko, notifyPlayerIfHit));
}

void Creature::flee()
{
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionFlee>(*this));
}

void Creature::stunForTurns(int32_t nbTurns)
{
    // Only living creatures that are not already KO can be stunned
    if(!isAlive() || (mKoTurnCounter != 0))
        return;

    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    mKoTurnCounter = nbTurns;
    computeCreatureOverlayMoodValue();
}

void Creature::fleeFromTile(Tile* fearTile, int32_t nbTurns)
{
    // Fear breaks the possession of the creature
    if(isPossessed())
        endPossession();

    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionFlee>(*this, fearTile, nbTurns));
}

void Creature::sleep()
{
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionSleep>(*this));
}

void Creature::leaveDungeon()
{
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    pushAction(Utils::make_unique<CreatureActionLeaveDungeon>(*this));
}

void Creature::changeSeat(Seat* newSeat)
{
    OD_LOG_INF("creature=" + getName() + " changes side from seatId=" + Helper::toString(getSeat()->getId()) + " to seatId=" + Helper::toString(newSeat->getId()));
    OD_ASSERT_TRUE_MSG(getSeat() != newSeat, "creature=" + getName() + ", seatId=" + Helper::toString(newSeat->getId()));
    // A neutral creature that joins a keeper is claimed
    if(getSeat()->isRogueSeat() && !newSeat->isRogueSeat())
        recordScriptEvent(*this, "claimed");

    setSeat(newSeat);
    mMoodValue = CreatureMoodLevel::Neutral;
    mMoodPoints = 0;
    mPrayerRelief = 0;
    mWakefulness = 100;
    mHunger = 0;
    mNbTurnsTorture = 0;
    mNbTurnsPrison = 0;
    mActiveSlapsCount = 0;
    mCasinoMood = 0.0;
    mNbTurnsInHand = 0;
    mIsInHand = false;
    mNbTurnsOutOfWork = 0;
    mNbTurnsTortureMood = 0;
    mNbTurnsRested = 0;
    mNbTurnsHatedCompany = 0;
    mPitMood = 0.0;
    mTorturedThisTurn = false;
    mRestedThisTurn = false;
    mSlapTurns.clear();
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
    mNeedFireRefresh = true;
    if (getHomeTile() != nullptr)
    {
        RoomDormitory* home = static_cast<RoomDormitory*>(getHomeTile()->getCoveringBuilding());
        home->releaseTileForSleeping(getHomeTile(), this);
    }
}

void Creature::stopWalking()
{
    if(parkingBit)
        parkedBit = true;
    MovableGameEntity::stopWalking();
}

void Creature::teleportTo(Tile* tile)
{
    if((tile == nullptr) || !getIsOnServerMap() || !getIsOnMap())
        return;

    clearDestinations(EntityAnimation::idle_anim, true, true);

    Ogre::Vector3 dest = tile->getPosition();
    setPosition(dest);

    for(Seat* seat : mSeatsWithVisionNotified)
    {
        if(seat->getPlayer() == nullptr)
            continue;
        if(!seat->getPlayer()->getIsHuman())
            continue;

        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::entityTeleported, seat->getPlayer());
        serverNotification->mPacket << getName() << dest;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }

    if(getHasVisualDebuggingEntities())
        computeVisualDebugEntities();
    mNeedFireRefresh = true;
}

Tile* Creature::getWalkDestinationTile() const
{
    if(mWalkQueue.empty())
        return nullptr;

    const Ogre::Vector2& destination = mWalkQueue.back();
    return getGameMap()->getTile(Helper::round(destination.x), Helper::round(destination.y));
}

bool Creature::takeCorpse()
{
    if(!getIsOnServerMap() || !getIsOnMap() || isAlive() || getDefinition()->isWorker())
        return false;

    // The counter is 0 until the death was handled (items dropped, owner told). Once the body is gone
    // (it is removed from the map when the counter is over), it cannot be raised
    uint32_t deathCounterMax = ConfigManager::getSingleton().getCreatureDeathCounter();
    if((mDeathCounter == 0) || (mDeathCounter >= deathCounterMax))
        return false;

    // The body is removed by the next upkeep
    mDeathCounter = deathCounterMax;
    return true;
}


void Creature::showOutliner()
{
    RenderManager::getSingleton().rrAddOutliner(this);
}


void Creature::removeOutliner()
{
    RenderManager::getSingleton().rrRemoveOutliner(this);
}


void Creature::maxAmbient()
{

    RenderManager::getSingleton().rrIncreaseAmbient(this);

}

void Creature::normalizeAmbient()
{

    RenderManager::getSingleton().rrNormalizeAmbient(this);

}

bool Creature::handleChampionUpkeep()
{
    ++mChampionTurns;

    // The cast price covers the first seconds (price divided by the drain per second). After that
    // the owner pays each turn the share of the drain per second and the champion leaves when the
    // mana cannot pay one second of it
    double price = ConfigManager::getSingleton().getSpellConfigDouble("SummonChampionPrice");
    double drainPerSecond = ConfigManager::getSingleton().getSpellConfigDouble("SummonChampionDrainPerSecond");
    if(drainPerSecond <= 0.0)
        return false;

    double freeSeconds = price / drainPerSecond;
    if(static_cast<double>(mChampionTurns) <= (freeSeconds * ODApplication::turnsPerSecond))
        return false;

    double drainPerTurn = drainPerSecond / ODApplication::turnsPerSecond;
    if((getSeat()->getMana() < drainPerSecond) || !getSeat()->takeMana(drainPerTurn))
    {
        dismissChampion();
        return true;
    }

    return false;
}

bool Creature::handleChampionIdle()
{
    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
        return false;

    // Enemy creatures first, the nearest reachable one. The dungeon hearts of the enemies come after
    std::vector<std::pair<int, Tile*>> targets;
    for(Creature* creature : getGameMap()->getCreatures())
    {
        if(creature->getSeat()->isAlliedSeat(getSeat()) || !creature->isAlive() || !creature->getIsOnMap() ||
           creature->isInPrison())
        {
            continue;
        }

        Tile* tile = creature->getPositionTile();
        if(tile == nullptr)
            continue;

        int distX = tile->getX() - myTile->getX();
        int distY = tile->getY() - myTile->getY();
        targets.push_back(std::make_pair(distX * distX + distY * distY, tile));
    }
    std::sort(targets.begin(), targets.end());

    std::vector<std::pair<int, Tile*>> heartTargets;
    for(Room* heart : getGameMap()->getRoomsByType(RoomType::dungeonTemple))
    {
        if(heart->getSeat()->isAlliedSeat(getSeat()))
            continue;

        std::vector<Tile*> heartTiles = heart->getCoveredTiles();
        if(heartTiles.empty())
            continue;

        int distX = heartTiles.front()->getX() - myTile->getX();
        int distY = heartTiles.front()->getY() - myTile->getY();
        heartTargets.push_back(std::make_pair(distX * distX + distY * distY, heartTiles.front()));
    }
    std::sort(heartTargets.begin(), heartTargets.end());
    targets.insert(targets.end(), heartTargets.begin(), heartTargets.end());

    // The path check is costly, so only the closest few targets are tried
    uint32_t nbTries = 0;
    for(const std::pair<int, Tile*>& target : targets)
    {
        if(nbTries >= MAX_PATH_CHECK_TARGET_TRIES)
            break;
        ++nbTries;

        if(!getGameMap()->pathExists(this, myTile, target.second))
            continue;

        if(setDestination(target.second))
            return true;
    }

    return false;
}

void Creature::dismissChampion()
{
    if(!getIsOnServerMap())
        return;

    OD_LOG_INF("The champion " + getName() + " leaves");
    if((getSeat()->getPlayer() != nullptr) && getSeat()->getPlayer()->getIsHuman() &&
       !getSeat()->getPlayer()->getHasLost())
    {
        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, getSeat()->getPlayer());
        std::string msg = "The champion leaves your dungeon";
        serverNotification->mPacket << msg << EventShortNoticeType::aboutCreatures;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }

    clearDestinations(EntityAnimation::idle_anim, true, true);
    removeFromGameMap();
    deleteYourself();
}

namespace
{
//! \brief Computes where the possessed creature can walk from the given position in the given
//! direction (unit vector). Returns false if it cannot move at all in this direction.
bool computePossessedDestination(const Creature& creature, const Ogre::Vector2& position,
    const Ogre::Vector2& direction, Ogre::Vector2& destination)
{
    const Ogre::Real stepLength = 0.25f;
    const Ogre::Real maxLength = 2.0f;
    destination = position;
    for(Ogre::Real length = stepLength; length <= maxLength; length += stepLength)
    {
        Ogre::Vector2 next = position + direction * length;
        Tile* nextTile = creature.getGameMap()->getTile(Helper::round(next.x), Helper::round(next.y));
        if(!creature.canGoThroughTile(nextTile))
            break;

        destination = next;
    }

    return (destination != position);
}
}

void Creature::startPossession(Player& player)
{
    mPossessor = &player;
    mPossessionTurns = 0;
    player.setPossessedCreatureName(getName());

    // The creature stops what it is doing. Its actions are kept and will go on after the possession
    clearDestinations(EntityAnimation::idle_anim, true, true);
    pushAction(Utils::make_unique<CreatureActionPossessed>(*this));

    // Nearby fighting creatures of the player follow the possessed one
    formPossessionGroup();

    if(!player.getIsHuman())
        return;

    ServerNotification* serverNotification = new ServerNotification(
        ServerNotificationType::possessionStart, &player);
    const std::string& name = getName();
    serverNotification->mPacket << name;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::endPossession()
{
    if(mPossessor == nullptr)
        return;

    Player* player = mPossessor;
    mPossessor = nullptr;
    player->setPossessedCreatureName(std::string());
    // Whether the creature fell (dead, knocked out or gone), so the client does not have to guess it
    const bool lost = !isAlive() || isKo() || !getIsOnMap();

    // The group does not follow anymore and goes back to its normal behaviour
    for(const std::string& memberName : mGroupMemberNames)
    {
        Creature* member = getGameMap()->getCreature(memberName);
        if((member != nullptr) && (member->mGroupLeaderName == getName()))
            member->leavePossessionGroup();
    }
    mGroupMemberNames.clear();

    for(std::vector<std::unique_ptr<CreatureAction>>::iterator it = mActions.begin(); it != mActions.end();)
    {
        if((*it)->getType() == CreatureActionType::possessed)
            it = mActions.erase(it);
        else
            ++it;
    }

    if(isAlive() && getIsOnMap())
        clearDestinations(EntityAnimation::idle_anim, true, true);

    if(!player->getIsHuman())
        return;

    ServerNotification* serverNotification = new ServerNotification(
        ServerNotificationType::possessionEnd, player);
    serverNotification->mPacket << lost;
    ODServer::getSingleton().queueServerNotification(serverNotification);
}

void Creature::formPossessionGroup()
{
    mGroupMemberNames.clear();

    uint32_t maxSize = ConfigManager::getSingleton().getSpellConfigUInt32("PossessGroupSize");
    int32_t radius = ConfigManager::getSingleton().getSpellConfigInt32("PossessGroupRadiusTiles");
    int32_t radiusSquared = radius * radius;
    Tile* myTile = getPositionTile();
    if((maxSize == 0) || (myTile == nullptr))
        return;

    std::vector<std::pair<int, Creature*>> candidates;
    for(Creature* creature : getGameMap()->getCreaturesBySeat(getSeat()))
    {
        if((creature == this) || creature->getDefinition()->isWorker() || !creature->isAlive() ||
           creature->isKo() || !creature->getIsOnMap() || creature->isInPrison() ||
           creature->isPossessed() || creature->isInPossessionGroup())
        {
            continue;
        }

        Tile* tile = creature->getPositionTile();
        if(tile == nullptr)
            continue;

        int distance = Pathfinding::squaredDistanceTile(*tile, *myTile);
        if(distance > radiusSquared)
            continue;

        candidates.push_back(std::make_pair(distance, creature));
    }

    std::sort(candidates.begin(), candidates.end());
    for(const std::pair<int, Creature*>& candidate : candidates)
    {
        if(mGroupMemberNames.size() >= maxSize)
            break;

        candidate.second->joinPossessionGroup(getName());
        mGroupMemberNames.push_back(candidate.second->getName());
    }
}

void Creature::joinPossessionGroup(const std::string& leaderName)
{
    mGroupLeaderName = leaderName;

    // The creature leaves what it was doing. The group behaviour is handled when it is idle
    clearDestinations(EntityAnimation::idle_anim, true, true);
    clearActionQueue();
}

void Creature::leavePossessionGroup()
{
    mGroupLeaderName.clear();
}

bool Creature::followPossessionLeader()
{
    Creature* leader = getGameMap()->getCreature(mGroupLeaderName);
    if((leader == nullptr) || !leader->isPossessed() || !leader->getIsOnMap())
    {
        leavePossessionGroup();
        return false;
    }

    Tile* myTile = getPositionTile();
    Tile* leaderTile = leader->getPositionTile();
    if((myTile == nullptr) || (leaderTile == nullptr))
        return true;

    // The creature stays around the leader. If it is further than 3 tiles, it walks to him
    if(Pathfinding::squaredDistanceTile(*myTile, *leaderTile) <= POSSESSION_GROUP_MAX_SQUARED_DISTANCE)
        return true;

    if(!getGameMap()->pathExists(this, myTile, leaderTile))
        return true;

    std::list<Tile*> tempPath = getGameMap()->path(this, leaderTile);
    // The group does not stand on the leader
    for(int i = 0; (i < 2) && (tempPath.size() > 1); ++i)
        tempPath.pop_back();

    std::vector<Ogre::Vector2> path;
    tileToVector2(tempPath, path, true, 0.0);
    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
    pushAction(Utils::make_unique<CreatureActionGoCallToWar>(*this));
    return true;
}

void Creature::possessedMove(const Ogre::Vector2& direction)
{
    if(!isPossessed() || !getIsOnMap() || isFrozen())
        return;

    Ogre::Vector2 position(mPosition.x, mPosition.y);
    Ogre::Vector2 destination = position;
    bool canMove = false;
    if(direction.squaredLength() > POSSESSION_MIN_DIRECTION_SQUARED_LENGTH)
    {
        Ogre::Vector2 dir = direction;
        dir.normalise();
        // If the creature is blocked, it tries to slide along the obstacle
        canMove = computePossessedDestination(*this, position, dir, destination);
        if(!canMove)
            canMove = computePossessedDestination(*this, position, Ogre::Vector2(dir.x, 0.0f), destination);
        if(!canMove)
            canMove = computePossessedDestination(*this, position, Ogre::Vector2(0.0f, dir.y), destination);
    }

    if(!canMove)
    {
        if(isMoving())
            clearDestinations(EntityAnimation::idle_anim, true, true);

        return;
    }

    std::vector<Ogre::Vector2> path;
    path.push_back(destination);
    setWalkPath(EntityAnimation::walk_anim, EntityAnimation::idle_anim, true, true, path, true);
}

namespace
{
//! \brief The attacks used with the left mouse button while possessing (melee and ranged).
//! The other skills of the creature are used with the number keys.
bool isPossessedBasicAttack(const CreatureSkill& skill)
{
    return (skill.getSkillName() == "Melee") || (skill.getSkillName() == "MissileLaunch");
}

//! \brief True if the skill is not cooling down or warming up and the creature can use it right now
//! \param creature The possessed creature that wants to use the skill.
//! \param skillData The state of the skill (cooldown and warmup turns left).
bool isPossessedSkillReady(const Creature& creature, const CreatureSkillData& skillData)
{
    return (skillData.mCooldown == 0) && (skillData.mWarmup == 0) &&
        skillData.mSkill->canBeUsedBy(&creature);
}
}

bool Creature::possessedFindTarget(const Ogre::Vector2& aim, const CreatureSkillData& skillData,
    GameEntity*& entityAttack, Tile*& tileAttack)
{
    entityAttack = nullptr;
    tileAttack = nullptr;
    Tile* myTile = getPositionTile();
    if(myTile == nullptr)
        return false;

    if(aim.squaredLength() < POSSESSION_MIN_AIM_SQUARED_LENGTH)
        return false;

    Ogre::Vector2 aimDir = aim;
    aimDir.normalise();

    // The target has to be in front of the creature (45 degrees to each side)
    const Ogre::Real minCos = 0.7f;
    bool bestIsCreature = false;
    Ogre::Real bestDist = 0.0f;
    for(GameEntity* entity : mVisibleEnemyObjects)
    {
        bool isCreature = (entity->getObjectType() == GameEntityType::creature);
        if(isCreature)
        {
            Creature* enemy = static_cast<Creature*>(entity);
            if(!enemy->isAlive())
                continue;

            // Workers attack workers only
            if(getDefinition()->isWorker() && !enemy->getDefinition()->isWorker())
                continue;
        }
        else if(getDefinition()->isWorker())
            continue;

        if(entity->getHP(nullptr) <= 0)
            continue;

        double skillRange = skillData.mSkill->getRangeMax(this, entity);
        if(skillRange <= 0.0)
            continue;

        for(Tile* tile : entity->getCoveredTiles())
        {
            if(!entity->isAttackable(tile, getSeat()))
                continue;

            // Same range check as in a normal fight
            int squaredDist = Pathfinding::squaredDistanceTile(*tile, *myTile);
            if(static_cast<double>(squaredDist) > (skillRange * skillRange))
                continue;

            Ogre::Vector2 toTile(tile->getX() - mPosition.x, tile->getY() - mPosition.y);
            Ogre::Real dist = toTile.length();
            if(dist > POSSESSION_AIM_MIN_DISTANCE)
            {
                toTile /= dist;
                if(toTile.dotProduct(aimDir) < minCos)
                    continue;
            }

            // Creatures are attacked before the other objects, then the closest one
            if((entityAttack != nullptr) && ((bestIsCreature && !isCreature) ||
               ((bestIsCreature == isCreature) && (dist >= bestDist))))
                continue;

            entityAttack = entity;
            tileAttack = tile;
            bestIsCreature = isCreature;
            bestDist = dist;
        }
    }

    return (entityAttack != nullptr);
}

void Creature::possessedAttack(const Ogre::Vector2& aim)
{
    if(!isPossessed() || !getIsOnMap() || !isAlive() || isKo() || isFrozen())
        return;

    // The melee attack is preferred. The ranged attack is used if no enemy is within melee range
    CreatureSkillData* bestSkill = nullptr;
    GameEntity* bestEntity = nullptr;
    Tile* bestTile = nullptr;
    double bestRange = 0.0;
    for(CreatureSkillData& skillData : mSkillData)
    {
        if(!isPossessedBasicAttack(*skillData.mSkill))
            continue;

        if(!isPossessedSkillReady(*this, skillData))
            continue;

        GameEntity* entity = nullptr;
        Tile* tile = nullptr;
        if(!possessedFindTarget(aim, skillData, entity, tile))
            continue;

        double range = skillData.mSkill->getRangeMax(this, entity);
        if((bestSkill != nullptr) && (range >= bestRange))
            continue;

        bestSkill = &skillData;
        bestEntity = entity;
        bestTile = tile;
        bestRange = range;
    }

    if(bestSkill == nullptr)
        return;

    useAttack(*bestSkill, *bestEntity, *bestTile, false, true);
}

void Creature::possessedUseSkill(uint32_t slot, const Ogre::Vector2& aim)
{
    if(!isPossessed() || !getIsOnMap() || !isAlive() || isKo() || isFrozen())
        return;

    uint32_t index = 0;
    for(CreatureSkillData& skillData : mSkillData)
    {
        if(isPossessedBasicAttack(*skillData.mSkill))
            continue;

        if(!skillData.mSkill->canBeUsedBy(this))
            continue;

        if(index != slot)
        {
            ++index;
            continue;
        }

        if(!isPossessedSkillReady(*this, skillData))
            return;

        // Skills without range (haste, heal, ...) are used on the creature itself
        if(skillData.mSkill->getRangeMax(this, this) <= 0.0)
        {
            if(!skillData.mSkill->tryUseSupport(*getGameMap(), this))
                return;

            skillData.mCooldown = skillData.mSkill->getCooldownNbTurns();
            skillData.mWarmup = skillData.mSkill->getWarmupNbTurns();
            return;
        }

        GameEntity* entity = nullptr;
        Tile* tile = nullptr;
        if(!possessedFindTarget(aim, skillData, entity, tile))
            return;

        useAttack(skillData, *entity, *tile, false, true);
        return;
    }
}
