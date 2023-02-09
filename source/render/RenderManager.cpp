/*!
 *  \file   RenderManager.cpp
 *  \date   26 March 2001
 *  \author oln, paul424
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

#include "render/RenderManager.h"

#include "entities/Creature.h"
#include "entities/CreatureDefinition.h"
#include "entities/GameEntity.h"
#include "entities/MapLight.h"
#include "entities/MovableGameEntity.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/RockLava.h"
#include "entities/Tile.h"
#include "entities/Weapon.h"
#include "game/Player.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/TileSet.h"
#include "modes/ModeManager.h"
#include "render/CreatureOverlayStatus.h"
#include "render/DebugDrawer.h"
#include "render/MovableTextOverlay.h"
#include "render/ODFrameListener.h"
#include "rooms/Room.h"
#include "utils/ConfigManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "utils/ResourceManager.h"


#include <OgreBone.h>
#include <OgreCamera.h>
#include <OgreCompositorManager.h>
#include <OgreEntity.h>
#include <OgreMaterialManager.h>
#include <OgreMesh.h>
#include <OgreMovableObject.h>
#include <OgreOverlayContainer.h>
#include <OgreParticleSystem.h>
#include <OgrePrerequisites.h>
#include <OgreQuaternion.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreShadowCameraSetupLiSPSM.h>
#include <OgreSkeleton.h>
#include <OgreSkeletonInstance.h>
#include <OgreSubEntity.h>
#include <OgreSubMesh.h>
#include <OgreRoot.h>
#include <OgreTechnique.h>
#include <OgreViewport.h>
#include <Overlay/OgreOverlay.h>
#include <Overlay/OgreOverlayManager.h>
#include <Overlay/OgreOverlaySystem.h>
#include <RTShaderSystem/OgreShaderGenerator.h>

#include <sstream>
#include <string>

template<> RenderManager* Ogre::Singleton<RenderManager>::msSingleton = nullptr;

const uint8_t RenderManager::OD_RENDER_QUEUE_ID_GUI = 101;

const Ogre::Real RenderManager::BLENDER_UNITS_PER_OGRE_UNIT = 10.0f;

const Ogre::Real KEEPER_HAND_POS_Z = 20.0;
const Ogre::Real RenderManager::KEEPER_HAND_WORLD_Z = KEEPER_HAND_POS_Z / RenderManager::BLENDER_UNITS_PER_OGRE_UNIT;

const Ogre::Real KEEPER_HAND_CREATURE_PICKED_OFFSET = 0.05f;
const Ogre::Real KEEPER_HAND_CREATURE_PICKED_SCALE = 0.05f;

const Ogre::ColourValue BASE_AMBIENT_VALUE = Ogre::ColourValue(0.3f, 0.3f, 0.3f);

const Ogre::Real RenderManager::DRAGGABLE_NODE_HEIGHT = 3.0f;


RenderManager::RenderManager(Ogre::OverlaySystem* overlaySystem) :
    mRenderTarget(nullptr),
    mHandLight(nullptr),
    mHandAnimationState(nullptr),
    mViewport(nullptr),
    mShaderGenerator(nullptr),
    mHandKeeperNode(nullptr),
    mDummyNode(nullptr),
    mHandLightNode(nullptr),
    mShadowCam(nullptr),
    mCurrentFOVy(0.0f),
    mFactorWidth(0.0f),
    mFactorHeight(0.0f),
    mCreatureTextOverlayDisplayed(false),
    mHandKeeperHandVisibility(0),
    m_ZPrePassEnabled(false)
{
  
    
    mSceneManager = Ogre::Root::getSingleton().createSceneManager("DefaultSceneManager", "SceneManager");
    if(ConfigManager::getSingleton().getAudioValue(Config::SHADOWS)=="Yes")
    {
        mSceneManager->setShadowTechnique(Ogre::ShadowTechnique::SHADOWTYPE_TEXTURE_ADDITIVE);
        // mSceneManager->setShadowCameraSetup(Ogre::LiSPSMShadowCameraSetup::create());
        // mSceneManager->setShadowTextureConfig(0,1024,1024,Ogre::PixelFormat::PF_R32G32B32A32_UINT,0);
        // mSceneManager->setShadowFarDistance(100.0);
        // mSceneManager->setShadowDirectionalLightExtrusionDistance(500.0);
        // mSceneManager->setShadowTextureSelfShadow(true);
        // donno if the below should be here -- paul424 :
        auto myIter = Ogre::MaterialManager::getSingleton().getResourceIterator();
        while(myIter.hasMoreElements())
        {
            auto myPointer = myIter.peekNextValue();
            Ogre::SharedPtr<Ogre::Material> myCastPointer = std::dynamic_pointer_cast<Ogre::Material> (myPointer);
            Ogre::Technique* technique;
            technique = myCastPointer->getTechnique(0);

            if( technique->getPass(technique->getNumPasses() - 1)->hasFragmentProgram())
            {
                if(technique->getPass(technique->getNumPasses() - 1)->getFragmentProgramParameters()->hasNamedParameters())
                {
                    const Ogre::GpuNamedConstants& gnc = technique->getPass(technique->getNumPasses() - 1)->getFragmentProgramParameters()->getConstantDefinitions();
                    auto it = gnc.map.find("shadowingEnabled");
                    if(it!=  gnc.map.end())
                        technique->getPass(technique->getNumPasses() - 1)->getFragmentProgramParameters()->setNamedConstant("shadowingEnabled",true);
                }
            }
            myIter.getNext();
        }  
        
    }
    ddd.setStatic(true);
    mSceneManager->addListener(&ddd);
    mSceneManager->addRenderQueueListener(overlaySystem);
    mDraggableSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Draggable_scene_node");
    mShaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    mShaderGenerator->addSceneManager(mSceneManager);

    mCreatureSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Creature_scene_node");
    mTileSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Tile_scene_node");
    mRoomSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Room_scene_node");
    mLightSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("Light_scene_node");
    mMainMenuSceneNode = mSceneManager->getRootSceneNode()->createChildSceneNode("MainMenu_scene_node");
    mDraggableSceneNode->setPosition(0.0,0.0,DRAGGABLE_NODE_HEIGHT);


}

RenderManager::~RenderManager()
{
    delete DebugDrawer::getSingletonPtr();
}

void RenderManager::initGameRenderer(GameMap* gameMap)
{
    OD_ASSERT_TRUE(gameMap);
    OD_ASSERT_TRUE(mHandKeeperNode);
    mCreatureTextOverlayDisplayed = false;

    // Create the light which follows the single tile selection mesh
    if(mHandLight == nullptr)
    {
        mHandLight = mSceneManager->createLight("MouseLight");
        mHandLight->setType(Ogre::Light::LT_POINT);
        // mHandLight->setDirection(0,0.77,-0.77);
        mHandLight->setDiffuseColour(Ogre::ColourValue(0.65f, 0.65f, 0.45f));
        mHandLight->setSpecularColour(Ogre::ColourValue(0.65f, 0.65f, 0.45f));

        // // the value borrowed from https://wiki.ogre3d.org/-Point+Light+Attenuation
        mHandLight->setAttenuation(50, 1.0, 0.09, 0.032);
        
        
        
        if(mHandLightNode == nullptr)
        {
            mHandLightNode = mLightSceneNode->createChildSceneNode(); //mSceneManager->createSceneNode();
        }
        
        mHandLightNode->attachObject(mHandLight);
    }
    // dirty hack to make the hovering gold tile be height agnostic, please look Gold.material file
    Ogre::MaterialPtr liftedGold = Ogre::MaterialManager::getSingleton().getByName("Gold")->clone("LiftedGold");
    liftedGold->getTechnique(0)->getPass(liftedGold->getTechnique(0)->getNumPasses() - 1)->getVertexProgramParameters()->setNamedConstant("height",DRAGGABLE_NODE_HEIGHT);
    
    //Add a too small to be visible dummy dirt tile to the hand node
    //so that there will always be a dirt tile "visible"
    //This is an ugly workaround for issue where destroying some entities messes
    //up the lighing for some of the rtshader materials.
    const std::string& defaultTileMesh = gameMap->getMeshForDefaultTile();
    if(!mSceneManager->hasEntity(defaultTileMesh + "_dummyEnt"))
    {
        Ogre::SceneNode* dummyNode = mHandKeeperNode->createChildSceneNode(defaultTileMesh + "_dummyNode");
        dummyNode->setScale(Ogre::Vector3(0.00000001f, 0.00000001f, 0.00000001f));
        Ogre::Entity* dummyEnt = mSceneManager->createEntity(defaultTileMesh + "_dummyEnt", defaultTileMesh);
        dummyEnt->setLightMask(0);
        dummyEnt->setCastShadows(false);
        dummyNode->attachObject(dummyEnt);
        mDummyEntities.push_back(dummyNode);
    }

    // We load every creature class and attach them to the keeper hand.
    // That's an ugly workaround to avoid a crash that occurs when CEGUI refreshes
    // ogre open gl renderer after removing some creatures
    // Note that from what I've seen, loading only one creature like "Troll.mesh" should be
    // enough. However, it doesn't work with other creatures (like "Kobold.mesh"). Since we
    // don't really know why, it is safer to load every creature
    for(uint32_t i = 0; i < gameMap->numClassDescriptions(); ++i)
    {
        const CreatureDefinition* def = gameMap->getClassDescription(i);
        if(!def)
        {
            OD_LOG_WRN("Class description not found!");
            continue;
        }
        // We check if the mesh is already loaded. If not, we load it
        if(mSceneManager->hasEntity(def->getClassName() + "_dummyEnt"))
            continue;

        Ogre::SceneNode* dummyNode = mHandKeeperNode->createChildSceneNode(def->getClassName() + "_dummyNode");
        dummyNode->setScale(Ogre::Vector3(0.00000001f, 0.00000001f, 0.00000001f));
        Ogre::Entity* dummyEnt = mSceneManager->createEntity(def->getClassName() + "_dummyEnt", def->getMeshName());
        dummyEnt->setLightMask(0);
        dummyEnt->setCastShadows(false);
        dummyNode->attachObject(dummyEnt);
        mDummyEntities.push_back(dummyNode);
    }

    // Create the RTT texture
    m_texture = Ogre::TextureManager::getSingleton().createManual("smokeTexture", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,Ogre::uint(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getViewport()->getActualWidth()), Ogre::uint(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getViewport()->getActualHeight()), 0, Ogre::PF_FLOAT32_RGBA, Ogre::TU_RENDERTARGET, 0);

    // Setup the render target and its listener
    mRenderTarget = m_texture->getBuffer()->getRenderTarget();
    ODFrameListener::getSingleton().getCameraManager()->createCamera("RenderToTexture",ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getNearClipDistance(),ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getFarClipDistance());
    ODFrameListener::getSingleton().getCameraManager()->createCameraNode("RenderToTexture");    
    Ogre::Viewport* tmpViewport = mRenderTarget->addViewport(ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture"));
    tmpViewport->setBackgroundColour(Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f));
    tmpViewport->setClearEveryFrame(true);
    tmpViewport->setOverlaysEnabled(false);
    tmpViewport->setShadowsEnabled(false);
    mRenderTarget->addListener(this);
    mRenderTarget->setAutoUpdated(false);

    // Update the render target (for a correct first frame)
    // preRenderTargetUpdate:
    std::vector<bool> mTemporaryWasVisible;
    for (int i = 0; i < gameMap->getCreatures().size(); i++)
    {
        CreatureOverlayStatus* tmp = gameMap->getCreatures()[i]->getOverlayStatus();
        mTemporaryWasVisible.push_back(tmp->getMovableTextOverlay()->isVisible());
        tmp->getMovableTextOverlay()->setVisible(false);
    }
    mRenderTarget->update();
    // postRenderTargetUpdate:
    int tmpItr = 0;
    for (int i = 0; i < gameMap->getCreatures().size() ; i++)
    {
        CreatureOverlayStatus* tmp = gameMap->getCreatures()[i]->getOverlayStatus();
        tmp->getMovableTextOverlay()->setVisible(mTemporaryWasVisible[tmpItr++]);
    }        
    mTemporaryWasVisible.clear();    
    
    Ogre::MaterialPtr mSmokeMaterial = Ogre::MaterialManager::getSingleton().getByName("Examples/Smoke");
    mSmokeMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(1)->setTexture(m_texture);
    
    
}

void RenderManager::setup()
{
    OD_LOG_INF("TEST from setup()");
    // Loop through all materials and handle when a scheme is not found
    Ogre::ResourceManager::ResourceMapIterator tmpResourceIterator = Ogre::MaterialManager::getSingleton().getResourceIterator();
    while (tmpResourceIterator.hasMoreElements())
    {
        Ogre::ResourcePtr tmpResourcePtr = tmpResourceIterator.getNext();

	const Ogre::String tmpResourceType = tmpResourcePtr->getCreator()->getResourceType();
	if (tmpResourceType == "Material")
	{
            Ogre::MaterialPtr tmpMaterialPtr = Ogre::static_pointer_cast<Ogre::Material>(tmpResourcePtr);
    // Create a new technique in the material
            Ogre::Technique* tmpTechnique = tmpMaterialPtr->getTechnique(0);
            tmpTechnique->setSchemeName("Normal");
            if (tmpMaterialPtr)
                handleSchemeNotFound(tmpMaterialPtr);
	}
    }
}

// Handles a material when the scheme was not found
void RenderManager::handleSchemeNotFound(Ogre::MaterialPtr material)
{
    // Check if we already have the scheme technique
    if (material->getTechnique("ZPrePassScheme"))
        // Return the function, we are done
        return;
    
    // Filter certain specific materials to never get a new technique
    Ogre::String tmpMaterialName = material->getName();


    if ( material->isTransparent() || tmpMaterialName.find("Background")!=Ogre::String::npos || tmpMaterialName.find("CreatureOverlay")!=Ogre::String::npos || tmpMaterialName.find("Examples/Smoke")!=Ogre::String::npos )
	return;

    // Create a new technique in the material
    Ogre::Technique* tmpTechnique = material->createTechnique();
    tmpTechnique->setName("ZPrePassScheme");

    // Set the scheme name of the technique
    tmpTechnique->setSchemeName("ZPrePassScheme");

    // Create a new pass in the technique
    Ogre::Pass* tmpPass = tmpTechnique->createPass();

    // Point the pass to the technique of the z-prepass materials pass
    Ogre::MaterialPtr tmpZPrePassMaterial = Ogre::MaterialManager::getSingleton().getByName("ZPrePass");
    *tmpPass = *tmpZPrePassMaterial->getTechnique(0)->getPass(0);
    
}


void RenderManager::setPosition(Ogre::Camera* obj, const Ogre::Vector3& vec)
{
	Ogre::Node* tmpNode = obj->getParentNode();
	tmpNode->setPosition(vec);
}

void RenderManager::setOrientation(Ogre::Camera* obj, const Ogre::Quaternion& q)
{
	Ogre::Node* tmpNode = obj->getParentNode();
	tmpNode->setOrientation(q);
}

Ogre::Vector3 RenderManager::getPosition(Ogre::Camera* obj)
{
	return obj->getDerivedPosition();
	//Node* tmpNode = obj->getParentNode();
	//return tmpNode->_getDerivedPosition();
}

Ogre::Quaternion RenderManager::getOrientation(Ogre::Camera* obj)
{
	return obj->getDerivedOrientation();
	//Node* tmpNode = obj->getParentNode();
	//return tmpNode->_getDerivedOrientation();
}

// Called before a render is called to the render target
void RenderManager::preRenderTargetUpdate(const Ogre::RenderTargetEvent& evt)
{
  
    static Ogre::Overlay* tmpOverlay = NULL;
    static Ogre::MaterialPtr tmpMaterial;

    if(!m_ZPrePassEnabled)
        return;    
    
    if (ODFrameListener::getSingleton().getModeManager()->getInputManager().mKeyboard->getKeyboard()->isModifierDown(OIS::Keyboard::Modifier::CapsLock))
    {
  
        
        if (!tmpOverlay)
        {
            tmpMaterial = Ogre::MaterialManager::getSingleton().getByName("DebugZPrePass");
            tmpOverlay = Ogre::OverlayManager::getSingleton().create("blablabla");
            tmpOverlay->setZOrder(5);
            tmpOverlay->show();

            Ogre::OverlayContainer* tmpOverlayContainer = (Ogre::OverlayContainer*)Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", "testnameasd");
            tmpOverlayContainer->setMetricsMode(Ogre::GMM_RELATIVE);
            tmpOverlayContainer->setPosition(0, 0);
            tmpOverlayContainer->setDimensions(1, 1);
            tmpOverlayContainer->setMaterialName(tmpMaterial->getName());
            tmpOverlayContainer->show();
            tmpOverlay->add2D(tmpOverlayContainer);
        }
        tmpMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTexture(m_texture);
        tmpOverlay->show();
    }
    else
    {
        if (tmpOverlay)
            tmpOverlay->hide();
    }



    // Update the camera
    setPosition(ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture"), getPosition(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()));
    setOrientation(ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture"), getOrientation(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()));
    ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture")->setFOVy(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getFOVy());
    ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture")->setAspectRatio(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getAspectRatio());
    ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture")->setNearClipDistance(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getNearClipDistance());
    ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture")->setFarClipDistance(ODFrameListener::getSingleton().getCameraManager()->getActiveCamera()->getFarClipDistance());
    ODFrameListener::getSingleton().getCameraManager()->getCamera("RenderToTexture")->getViewport()->setMaterialScheme("ZPrePassScheme");
    
}


void RenderManager::stopGameRenderer(GameMap*)
{
    // We do not remove the entities from mDummyEntities as it is a workaround avoiding a crash and removing
    // them can cause the crash to happen

    // Remove the light following the keeper hand
    if(mHandLight != nullptr)
    {
        mSceneManager->destroyLight(mHandLight);
        mHandLight = nullptr;
    }
    if(Ogre::MaterialManager::getSingleton().resourceExists("LiftedGold","Graphics"))
        Ogre::MaterialManager::getSingleton().remove("LiftedGold","Graphics");
    if(Ogre::TextureManager::getSingleton().resourceExists("smokeTexture", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME))    
        Ogre::TextureManager::getSingleton().remove("smokeTexture", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if(ODFrameListener::getSingleton().getCameraManager()->hasCamera("RenderToTexture"))
        ODFrameListener::getSingleton().getCameraManager()->destroyCamera("RenderToTexture");
    if(ODFrameListener::getSingleton().getCameraManager()->hasCameraNode("RenderToTexture"))
    ODFrameListener::getSingleton().getCameraManager()->destroyCameraNode("RenderToTexture");    
}

void RenderManager::triggerCompositor(const std::string& compositorName)
{
    Ogre::CompositorManager::getSingleton().setCompositorEnabled(mViewport, compositorName.c_str(), true);
}

void RenderManager::createScene(Ogre::Viewport* nViewport)
{
    OD_LOG_INF("Creating scene...");
    
    mViewport = nViewport;

    
    //Set up the shader generator
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
        ResourceManager::getSingleton().getShaderCachePath(), "FileSystem", "Graphics");

    mViewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);

    // Sets the overall world lighting.
    mSceneManager->setAmbientLight(BASE_AMBIENT_VALUE);

    Ogre::ParticleSystem::setDefaultNonVisibleUpdateTimeout(5);

    // Create the nodes that will follow the mouse pointer.
    Ogre::Entity* keeperHandEnt = mSceneManager->createEntity("keeperHandEnt", "Keeperhand.mesh");
    keeperHandEnt->setLightMask(0);
    keeperHandEnt->setCastShadows(false);
    mHandAnimationState = keeperHandEnt->getAnimationState("Idle");
    mHandAnimationState->setTimePosition(0);
    mHandAnimationState->setLoop(true);
    mHandAnimationState->setEnabled(true);
    // Note that we need to render something on OD_RENDER_QUEUE_ID_GUI otherwise, Ogre
    // will not call the render function with queue id = OD_RENDER_QUEUE_ID_GUI and the
    // GUI will not be displayed
    keeperHandEnt->setRenderQueueGroup(OD_RENDER_QUEUE_ID_GUI);

    Ogre::OverlayManager& overlayManager = Ogre::OverlayManager::getSingleton();
    Ogre::Overlay* handKeeperOverlay = overlayManager.create(keeperHandEnt->getName() + "_Ov");
    mHandKeeperNode = mSceneManager->createSceneNode(keeperHandEnt->getName() + "_node");
    mHandKeeperNode->attachObject(keeperHandEnt);
    mHandKeeperNode->setScale(Ogre::Vector3::UNIT_SCALE * KEEPER_HAND_POS_Z);
    mHandKeeperNode->setPosition(0.0f, 0.0f, -KEEPER_HAND_POS_Z);
    handKeeperOverlay->add3D(mHandKeeperNode);
    handKeeperOverlay->show();

    mHandKeeperNode->setVisible(mHandKeeperHandVisibility == 0);
    new DebugDrawer(mSceneManager, 0.1f);

}

void RenderManager::setWorldAmbientLightingFactor(float lightFactor)
{
    Ogre::ColourValue factoredLighting(BASE_AMBIENT_VALUE * lightFactor);
    mSceneManager->setAmbientLight(factoredLighting);
}

Ogre::Light* RenderManager::addPointLightMenu(const std::string& name, const Ogre::Vector3& pos,
        const Ogre::ColourValue& diffuse, const Ogre::ColourValue& specular, Ogre::Real attenuationRange,
        Ogre::Real attenuationConstant, Ogre::Real attenuationLinear, Ogre::Real attenuationQuadratic)
{
    if(mSceneManager->hasLight(name))
    {
        OD_LOG_ERR("There is already a light=" + name);
        return nullptr;
    }

    Ogre::Light* light = mSceneManager->createLight(name);
    Ogre::SceneNode* sn =  mSceneManager->getRootSceneNode()->createChildSceneNode("PointLightMenuSceneNode");
    sn->attachObject(light);
    light->setType(Ogre::Light::LT_POINT);
    light->setDiffuseColour(diffuse);
    light->setSpecularColour(specular);
    light->setAttenuation(attenuationRange, attenuationConstant, attenuationLinear, attenuationQuadratic);
    mSceneManager->getRootSceneNode()->createChildSceneNode(pos)->attachObject(light);
    return light;
}

void RenderManager::removePointLightMenu(Ogre::Light* light)
{
    mSceneManager->destroyLight(light);
    mSceneManager->getRootSceneNode()->removeAndDestroyChild("PointLightMenuSceneNode");
}

Ogre::Entity* RenderManager::addEntityMenu(const std::string& meshName, const std::string& entityName,
        const Ogre::Vector3& pos)
{
    if(mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is already an entity=" + entityName);
        return nullptr;
    }

    if(!Ogre::MeshManager::getSingleton().resourceExists(meshName,"Graphics"))
        Ogre::MeshManager::getSingleton().load(meshName,"Graphics");

    Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshName,"Graphics");

    unsigned short src, dest;
    if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    {
        meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    } 

    Ogre::Entity* ent = mSceneManager->createEntity(entityName, meshPtr);
    
    Ogre::SceneNode* node = mMainMenuSceneNode->createChildSceneNode(ent->getName() + "_node");
    node->attachObject(ent);
    node->setPosition(pos);

    return ent;
}

void RenderManager::removeEntityMenu(Ogre::Entity* ent)
{
    Ogre::SceneNode* entNode = mSceneManager->getSceneNode(ent->getName() + "_node");
    entNode->detachObject(ent);
    mMainMenuSceneNode->removeChild(entNode);
    mSceneManager->destroySceneNode(entNode->getName());
    mSceneManager->destroyEntity(ent);
}

Ogre::AnimationState* RenderManager::setMenuEntityAnimation(const std::string& entityName, const std::string& animation, bool loop)
{
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is no entity=" + entityName);
        return nullptr;
    }

    Ogre::Entity* ent = mSceneManager->getEntity(entityName);
    if(!ent->hasAnimationState(animation))
    {
        OD_LOG_ERR("Entity=" + ent->getName() + ", has no animation=" + animation);
        return nullptr;
    }

    return setEntityAnimation(ent, animation, loop);
}

bool RenderManager::updateMenuEntityAnimation(Ogre::AnimationState* animState, Ogre::Real timeSinceLastFrame)
{
    animState->addTime(timeSinceLastFrame);
    return animState->hasEnded();
}

Ogre::SceneNode* RenderManager::getMenuEntityNode(const std::string& entityName)
{
    std::string nodeName = entityName + "_node";
    if(!mSceneManager->hasSceneNode(nodeName))
    {
        OD_LOG_ERR("No node for entityName=" + entityName + ", node name=" + nodeName);
        return nullptr;
    }

    Ogre::SceneNode* node = mSceneManager->getSceneNode(nodeName);
    return node;
}

const Ogre::Vector3& RenderManager::getMenuEntityPosition(Ogre::SceneNode* node)
{
    return node->getPosition();
}

void RenderManager::updateMenuEntityPosition(Ogre::SceneNode* node, const Ogre::Vector3& pos)
{
    node->setPosition(pos);
}

void RenderManager::orientMenuEntityPosition(Ogre::SceneNode* node, const Ogre::Vector3& direction)
{
    Ogre::Vector3 tempVector = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;

    // Work around 180 degree quaternion rotation quirk
    if ((1.0f + tempVector.dotProduct(direction)) < 0.0001f)
    {
        node->roll(Ogre::Degree(180));
    }
    else
    {
        node->rotate(tempVector.getRotationTo(direction));
    }
}

Ogre::ParticleSystem* RenderManager::addEntityParticleEffectMenu(Ogre::SceneNode* node,
        const std::string& particleName, const std::string& particleScript)
{
    Ogre::ParticleSystem* particleSystem = mSceneManager->createParticleSystem(particleName, particleScript);
    node->attachObject(particleSystem);
    return particleSystem;
}

void RenderManager::removeEntityParticleEffectMenu(Ogre::SceneNode* node,
        Ogre::ParticleSystem* particleSystem)
{
    node->detachObject(particleSystem);
    mSceneManager->destroyParticleSystem(particleSystem);
}

Ogre::ParticleSystem* RenderManager::addEntityParticleEffectBoneMenu(const std::string& entityName,
        const std::string& boneName, const std::string& particleName, const std::string& particleScript)
{
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("Cannot find entityName=" + entityName);
        return nullptr;
    }

    Ogre::ParticleSystem* particleSystem = mSceneManager->createParticleSystem(particleName, particleScript);

    Ogre::Entity* ent = mSceneManager->getEntity(entityName);
    ent->attachObjectToBone(boneName, particleSystem);

    return particleSystem;
}

void RenderManager::removeEntityParticleEffectBoneMenu(const std::string& entityName,
        Ogre::ParticleSystem* particleSystem)
{
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("Cannot find entityName=" + entityName);
        return;
    }

    Ogre::Entity* ent = mSceneManager->getEntity(entityName);
    ent->detachObjectFromBone(particleSystem);
    mSceneManager->destroyParticleSystem(particleSystem);
}

Ogre::Quaternion RenderManager::getNodeOrientation(Ogre::SceneNode* node)
{
    return node->getOrientation();
}

void RenderManager::setProgressiveNodeOrientation(Ogre::SceneNode* node, Ogre::Real progress,
        const Ogre::Quaternion& angleSrc, const Ogre::Quaternion& angleDest)
{
    node->setOrientation(Ogre::Quaternion::Slerp(progress, angleSrc, angleDest, true));
}

void RenderManager::setScaleMenuEntity(Ogre::SceneNode* node, const Ogre::Vector3& absSize)
{
    node->setScale(absSize);
}

const Ogre::Vector3& RenderManager::getMenuEntityScale(Ogre::SceneNode* node)
{
    return node->getScale();
}

void RenderManager::updateRenderAnimations(Ogre::Real timeSinceLastFrame)
{
    if(mHandAnimationState != nullptr)
    {
        mHandAnimationState->addTime(timeSinceLastFrame);
        if(mHandAnimationState->hasEnded())
        {
            Ogre::Entity* ent = mSceneManager->getEntity("keeperHandEnt");
            mHandAnimationState = setEntityAnimation(ent, "Idle", true);
        }
    }
}

void RenderManager::rrRefreshTile(const Tile& tile, GameMap& draggableTileContainer, const Player& localPlayer, NodeType nt)
{
    if (tile.getEntityNode() == nullptr)
        return;

    std::string tileName = tile.getOgreNamePrefix() + tile.getName();
    std::string meshName;

    // We only mark vision on ground tiles (except lava and water)
    bool vision = true;
    switch(tile.getTileVisual())
    {
        case TileVisual::claimedGround:
        case TileVisual::dirtGround:
        case TileVisual::goldGround:
        case TileVisual::rockGround:
            vision = tile.getLocalPlayerHasVision();
            break;
        default:
            break;
    }
    bool isMarked = tile.getMarkedForDigging(&localPlayer);
    
    const TileSetValue& tileSetValue = draggableTileContainer.getMeshForTile(&tile);

    // We compute the mesh
    meshName = tileSetValue.getMeshName();

    Ogre::Entity* tileMeshEnt = nullptr;
    const std::string tileMeshName = tileName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_tileMesh";
    if(mSceneManager->hasEntity(tileMeshName))
    {
        tileMeshEnt = mSceneManager->getEntity(tileMeshName);
        if(tileMeshEnt->getMesh()->getName().compare(meshName) != 0)
        {
            // Unlink and delete the old mesh
            mSceneManager->getSceneNode(tileMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node")->detachObject(tileMeshEnt);
            mSceneManager->destroyEntity(tileMeshEnt);
            tileMeshEnt = nullptr;
        }
    }

    Ogre::SceneNode* tileMeshNode = nullptr;
    std::string tileMeshNodeName = tileMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node";
    if(mSceneManager->hasSceneNode(tileMeshNodeName))
        tileMeshNode = mSceneManager->getSceneNode(tileMeshNodeName);

    if((tileMeshEnt == nullptr) && !meshName.empty())
    {


        if(!Ogre::MeshManager::getSingleton().resourceExists(meshName,"Graphics"))
            Ogre::MeshManager::getSingleton().load(meshName,"Graphics");
        Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshName,"Graphics");
        unsigned short src, dest;
        if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
        {
            meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
        } 



        
        tileMeshEnt = mSceneManager->createEntity(tileMeshName, meshPtr);
        // If the node does not exist, we create it
        if(tileMeshNode == nullptr)
            tileMeshNode = tile.getEntityNode()->createChildSceneNode(tileMeshNodeName);
        // Link the tile mesh back to the relevant scene node so OGRE will render it
        tileMeshNode->attachObject(tileMeshEnt);
    }
    // We rescale and set the orientation that may have changed
    if(tileMeshNode != nullptr)
    {
        tileMeshNode->resetOrientation();

        // We rotate depending on the tileset
        Ogre::Quaternion q;
        if(tileSetValue.getRotationX() != 0.0f)
            q = q * Ogre::Quaternion(Ogre::Degree(tileSetValue.getRotationX()), Ogre::Vector3::UNIT_X);

        if(tileSetValue.getRotationY() != 0.0f)
            q = q * Ogre::Quaternion(Ogre::Degree(tileSetValue.getRotationY()), Ogre::Vector3::UNIT_Y);

        if(tileSetValue.getRotationZ() != 0.0f)
            q = q * Ogre::Quaternion(Ogre::Degree(tileSetValue.getRotationZ()), Ogre::Vector3::UNIT_Z);

        if(q != Ogre::Quaternion::IDENTITY)
            tileMeshNode->rotate(q);
    }

    if(tileMeshEnt != nullptr)
    {
        tileMeshEnt->setCastShadows(false);
        // We replace the material if required by the tileset
        if(!tileSetValue.getMaterialName().empty())
            tileMeshEnt->setMaterialName(tileSetValue.getMaterialName());


        // dirty hack to make the hovering gold tile be height agnostic, please look Gold.material file
        if(tileSetValue.getMaterialName() == "Gold" && nt == NodeType::MDTC_NODE)
            tileMeshEnt->setMaterialName("LiftedGold");
        Seat* seatColor = nullptr;
        if(tile.shouldColorTileMesh())
            seatColor = tile.getSeat();

        colourizeEntity(tileMeshEnt, seatColor, isMarked, vision);
    }


    if (tile.getTileVisual() == TileVisual::waterGround || tile.getTileVisual() == TileVisual::lavaGround){
        if(tile.getHasBridge())
        {
            // We display the custom mesh if there is one
            const std::string bridgeMeshName = tileName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_bridgeMesh";
            meshName = tile.getMeshName();
            Ogre::Entity* customMeshEnt = nullptr;
            if(mSceneManager->hasEntity(bridgeMeshName))
            {
                customMeshEnt = mSceneManager->getEntity(bridgeMeshName);
            }

            if((customMeshEnt == nullptr))
            {
                // If the node does not exist, we create it

                if(!Ogre::MeshManager::getSingleton().resourceExists(meshName,"Graphics"))
                    Ogre::MeshManager::getSingleton().load(meshName,"Graphics");
    

                Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshName,"Graphics");
                unsigned short src, dest;
    
                if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
                {
                    meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
                }               
                std::string customMeshNodeName = bridgeMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node";
                Ogre::SceneNode* customMeshNode;
                if(!mSceneManager->hasSceneNode(customMeshNodeName))
                    customMeshNode = tile.getEntityNode()->createChildSceneNode(customMeshNodeName);
                else
                    customMeshNode = mSceneManager->getSceneNode(customMeshNodeName);

                customMeshEnt = mSceneManager->createEntity(bridgeMeshName, meshPtr);

                customMeshNode->attachObject(customMeshEnt);
                customMeshNode->resetOrientation();


            }

            if(customMeshEnt != nullptr)
            {
                Seat* seatColor = nullptr;
                if(tile.shouldColorCustomMesh())
                    seatColor = tile.getSeat();

                colourizeEntity(customMeshEnt, seatColor, isMarked, vision);
            }

            tile.getEntityNode()->setPosition(static_cast<Ogre::Real>(tile.getX()), static_cast<Ogre::Real>(tile.getY()),static_cast<Ogre::Real>(tile.getZ()) );        

        }
        else
        {
            // We display the custom mesh if there is one
            const std::string bridgeMeshName = tileName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_bridgeMesh";
            meshName = tile.getMeshName();
            Ogre::Entity* customMeshEnt = nullptr;
            if(mSceneManager->hasEntity(bridgeMeshName))
            {
                customMeshEnt = mSceneManager->getEntity(bridgeMeshName);

                // Unlink and delete the old mesh
                mSceneManager->getSceneNode(bridgeMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node")->detachObject(customMeshEnt);
                mSceneManager->destroyEntity(customMeshEnt);
                
            }
        }
        
    }
    


    
}

void RenderManager::rrCreateTile(Tile& tile, GameMap& dtc, const Player& localPlayer, NodeType nt)
{
    std::string tileName = tile.getOgreNamePrefix() + tile.getName();
    Ogre::SceneNode* node ;   
    if(nt == NodeType::MTILES_NODE)
        node = mTileSceneNode->createChildSceneNode(tileName + "_node");
    else // if(nt == NodeType::MDTC_NODE)
        node = mDraggableSceneNode->createChildSceneNode(tileName + "_dtc_node");
    tile.setParentSceneNode(node->getParentSceneNode());
    tile.setEntityNode(node);
    node->setPosition(static_cast<Ogre::Real>(tile.getX()), static_cast<Ogre::Real>(tile.getY()),static_cast<Ogre::Real>(tile.getZ()) );

    rrRefreshTile(tile, dtc, localPlayer,nt);
}

void RenderManager::rrDestroyTile(Tile& tile, NodeType nt)
{
    if (tile.getEntityNode() == nullptr)
        return;

    std::string tileName = tile.getOgreNamePrefix() + tile.getName();
    std::string selectorName = tileName + "_selection_indicator";
    if(mSceneManager->hasEntity(selectorName))
    {
        Ogre::SceneNode* selectorNode = mSceneManager->getSceneNode(selectorName + "Node");
        Ogre::Entity* selectorEnt = mSceneManager->getEntity(selectorName);
        tile.getEntityNode()->removeChild(selectorNode);
        selectorNode->detachObject(selectorEnt);
        mSceneManager->destroySceneNode(selectorNode);
        mSceneManager->destroyEntity(selectorEnt);
    }

    const std::string tileMeshName = tileName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_tileMesh";
    if(mSceneManager->hasSceneNode(tileMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node"))
    {
        Ogre::SceneNode* tileMeshNode = mSceneManager->getSceneNode(tileMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node");
        if(mSceneManager->hasEntity(tileMeshName))
        {
            Ogre::Entity* ent = mSceneManager->getEntity(tileMeshName);
            tileMeshNode->detachObject(ent);
            mSceneManager->destroyEntity(ent);
        }
        tile.getEntityNode()->removeChild(tileMeshNode);
        mSceneManager->destroySceneNode(tileMeshNode);
    }

    const std::string customMeshName = tileName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_customMesh";
    if(mSceneManager->hasSceneNode(customMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node"))
    {
        Ogre::SceneNode* customMeshNode = mSceneManager->getSceneNode(customMeshName + (static_cast<bool>(nt) ?  "" : "_dtc" ) + "_node");
        if(mSceneManager->hasEntity(customMeshName))
        {
            Ogre::Entity* ent = mSceneManager->getEntity(customMeshName);
            customMeshNode->detachObject(ent);
            mSceneManager->destroyEntity(ent);
        }
        tile.getEntityNode()->removeChild(customMeshNode);
        mSceneManager->destroySceneNode(customMeshNode);
    }

    mSceneManager->destroySceneNode(tile.getEntityNode());
    tile.setParentSceneNode(nullptr);
    tile.setEntityNode(nullptr);
}

void RenderManager::rrTemporalMarkTile(Tile* curTile)
{
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();
    Ogre::Entity* ent;

    bool bb = curTile->getSelected();

    std::string tileName = curTile->getOgreNamePrefix() + curTile->getName();
    std::string selectorName = tileName + "_selection_indicator";
    if (mSceneMgr->hasEntity(selectorName))
    {
        ent = mSceneMgr->getEntity(selectorName);
    }
    else
    {
        std::string tileNodeName = tileName + "_node";
        ent = mSceneMgr->createEntity(selectorName, "SquareSelector.mesh");
        ent->setLightMask(0);
        ent->setCastShadows(false);
        Ogre::SceneNode* tileNode = mSceneManager->getSceneNode(tileNodeName);
        Ogre::SceneNode* selectorNode = tileNode->createChildSceneNode(selectorName + "Node");
        selectorNode->setInheritScale(false);
        selectorNode->attachObject(ent);
    }

    ent->setVisible(bb);
}

void RenderManager::rrDetachEntity(GameEntity* entity)
{
    //TODO : reorganize the way culling starts for gameEntities
    // so the nullptr check wouldn't be necessery
    // this is ad hoc solution:  
    Ogre::SceneNode* entityNode = entity->getEntityNode();
    entity->getParentSceneNode()->removeChild(entityNode);    

}

void RenderManager::rrAttachEntity(GameEntity* entity)
{
    //TODO : reorganize the way culling starts for gameEntities
    // so the nullptr check wouldn't be necessery
    // this is ad hoc solution:      
    Ogre::SceneNode* entityNode = entity->getEntityNode();
    entity->getParentSceneNode()->addChild(entityNode);
}

void RenderManager::rrCreateRenderedMovableEntity(RenderedMovableEntity* renderedMovableEntity, NodeType nt)
{
    std::string meshName = renderedMovableEntity->getMeshName();
    
    std::string tempString = renderedMovableEntity->getOgreNamePrefix() + renderedMovableEntity->getName() + (static_cast<bool>(nt) ?  "" : "_dtc" );

    Ogre::SceneNode* node;

    if(nt == NodeType::MTILES_NODE)
        node = mRoomSceneNode->createChildSceneNode(tempString + "_node");    
    else
        node = mDraggableSceneNode->createChildSceneNode(tempString + "_node");
    node->setPosition(renderedMovableEntity->getPosition());
    node->roll(Ogre::Degree(renderedMovableEntity->getRotationAngle()));


    Ogre::Entity* ent = nullptr;
    if(!meshName.empty())
    {


        if(!Ogre::MeshManager::getSingleton().resourceExists(meshName + ".mesh","Graphics"))
            Ogre::MeshManager::getSingleton().load(meshName + ".mesh","Graphics");
    

        Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshName + ".mesh","Graphics");
        unsigned short src, dest;
    
        if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
        {
            meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
        } 

        
        ent = mSceneManager->createEntity(tempString, meshPtr);
        node->attachObject(ent);
    }

    renderedMovableEntity->setParentSceneNode(node->getParentSceneNode());
    renderedMovableEntity->setEntityNode(node);

    // If it is required, we hide the tile
    if((renderedMovableEntity->getHideCoveredTile()) &&
       (renderedMovableEntity->getOpacity() >= 1.0))
    {
        Tile* posTile = renderedMovableEntity->getPositionTile();
        if(posTile == nullptr)
            return;

        std::string tileName = posTile->getOgreNamePrefix() + posTile->getName();
        if (!mSceneManager->hasEntity(tileName))
            return;

        Ogre::Entity* entity = mSceneManager->getEntity(tileName);
        entity->setVisible(false);
    }

    if ((ent != nullptr) && (renderedMovableEntity->getOpacity() < 1.0f))
        setEntityOpacity(ent, renderedMovableEntity->getOpacity());
}


void RenderManager::rrDestroyRenderedMovableEntity(RenderedMovableEntity* curRenderedMovableEntity, NodeType nt)
{
    std::string tempString = curRenderedMovableEntity->getOgreNamePrefix()
                             + curRenderedMovableEntity->getName()+  (static_cast<bool>(nt) ?  "" : "_dtc" );
    Ogre::SceneNode* node = curRenderedMovableEntity->getEntityNode();
    if(mSceneManager->hasEntity(tempString))
    {
        Ogre::Entity* ent = mSceneManager->getEntity(tempString);
        node->detachObject(ent);
        mSceneManager->destroyEntity(ent);
    }
    mSceneManager->destroySceneNode(node);
    curRenderedMovableEntity->setParentSceneNode(nullptr);
    curRenderedMovableEntity->setEntityNode(nullptr);

    // If it was hidden, we display the tile
    if(curRenderedMovableEntity->getHideCoveredTile())
    {
        Tile* posTile = curRenderedMovableEntity->getPositionTile();
        if(posTile == nullptr)
            return;

        std::string tileName = posTile->getOgreNamePrefix() + posTile->getName();
        if (!mSceneManager->hasEntity(tileName))
            return;

        Ogre::Entity* entity = mSceneManager->getEntity(tileName);
        if (posTile->getCoveringBuilding() != nullptr)
            entity->setVisible(posTile->getCoveringBuilding()->shouldDisplayGroundTile());
        else
            entity->setVisible(true);
    }



    
}



void RenderManager::rrUpdateEntityOpacity(RenderedMovableEntity* entity)
{
    std::string entStr = entity->getOgreNamePrefix() + entity->getName();
    Ogre::Entity* ogreEnt = mSceneManager->hasEntity(entStr) ? mSceneManager->getEntity(entStr) : nullptr;
    if (ogreEnt == nullptr)
    {
        OD_LOG_INF("Update opacity: Couldn't find entity: " + entStr);
        return;
    }

    setEntityOpacity(ogreEnt, entity->getOpacity());

    // We add the tile if it is required and the opacity is 1. Otherwise, we show it (in case the trap gets deactivated)
    bool tileVisible = (!entity->getHideCoveredTile() || (entity->getOpacity() < 1.0f));
    Tile* posTile = entity->getPositionTile();
    if(posTile != nullptr)
    {
        std::string tileName = posTile->getOgreNamePrefix() + posTile->getName();
        if (mSceneManager->hasEntity(tileName))
        {
            Ogre::Entity* ogreEntity = mSceneManager->getEntity(tileName);
            ogreEntity->setVisible(tileVisible);
        }
    }
}

void RenderManager::rrCreateCreature(Creature* curCreature)
{
    const std::string& meshName = curCreature->getDefinition()->getMeshName();

    // Load the mesh for the creature


    if(!Ogre::MeshManager::getSingleton().resourceExists(meshName,"Graphics"))
        Ogre::MeshManager::getSingleton().load(meshName,"Graphics");
    

    Ogre::MeshPtr meshPtr = Ogre::MeshManager::getSingleton().getByName(meshName,"Graphics");
    unsigned short src, dest;
    
    if (!meshPtr->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
    {
        meshPtr->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }    
    std::string creatureName = curCreature->getOgreNamePrefix() + curCreature->getName();
    Ogre::Entity* ent = mSceneManager->createEntity(creatureName, meshPtr);


    Ogre::SceneNode* node = mCreatureSceneNode->createChildSceneNode(creatureName + "_node");
    curCreature->setEntityNode(node);
    node->setPosition(curCreature->getPosition());
    node->attachObject(ent);
    curCreature->setParentSceneNode(node->getParentSceneNode());

    Ogre::Camera* cam = mViewport->getCamera();
    CreatureOverlayStatus* creatureOverlay = new CreatureOverlayStatus(curCreature, ent, cam);
    curCreature->setOverlayStatus(creatureOverlay);

    creatureOverlay->displayHealthOverlay(mCreatureTextOverlayDisplayed ? -1.0 : 0.0);

    curCreature->showOutliner();
}

void RenderManager::rrDestroyCreature(Creature* curCreature)
{
    if(curCreature->getOverlayStatus() != nullptr)
    {
        delete curCreature->getOverlayStatus();
        curCreature->setOverlayStatus(nullptr);
    }

    std::string creatureName = curCreature->getOgreNamePrefix() + curCreature->getName();
    if (mSceneManager->hasEntity(creatureName))
    {
        Ogre::SceneNode* creatureNode = curCreature->getEntityNode();
        Ogre::Entity* ent = mSceneManager->getEntity(creatureName);
        creatureNode->detachObject(ent);
        mCreatureSceneNode->removeChild(creatureNode);
        curCreature->setParentSceneNode(nullptr);
        curCreature->setEntityNode(nullptr);
        mSceneManager->destroyEntity(ent);
        mSceneManager->destroySceneNode(creatureNode->getName());
    }
}

void RenderManager::rrOrientEntityToward(MovableGameEntity* gameEntity, const Ogre::Vector3& direction)
{
    Ogre::SceneNode* node = mSceneManager->getSceneNode(gameEntity->getOgreNamePrefix() + gameEntity->getName() + "_node");
    Ogre::Vector3 tempVector = node->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Y;

    // Work around 180 degree quaternion rotation quirk
    if ((1.0f + tempVector.dotProduct(direction)) < 0.0001f)
    {
        node->roll(Ogre::Degree(180));
    }
    else
    {
        node->rotate(tempVector.getRotationTo(direction));
    }
}

void RenderManager::rrScaleCreature(Creature& creature)
{
    if(creature.getEntityNode() == nullptr)
    {
        OD_LOG_ERR("creature=" + creature.getName());
        return;
    }

    Ogre::Real scaleFactor = static_cast<Ogre::Real>(
        1.0 + 0.02 * static_cast<double>(creature.getLevel()));
    creature.getEntityNode()->setScale(Ogre::Vector3::UNIT_SCALE * scaleFactor);
}

void RenderManager::rrCreateWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand)
{
    Ogre::Entity* ent = mSceneManager->getEntity(curCreature->getOgreNamePrefix() + curCreature->getName());
    std::string weaponName = curWeapon->getOgreNamePrefix() + hand;
    if(!ent->getSkeleton()->hasBone(weaponName))
    {
        OD_LOG_WRN("Tried to add weapons to entity \"" + ent->getName() + " \" using model \"" +
                              ent->getMesh()->getName() + "\" that is missing the required bone \"" +
                              curWeapon->getOgreNamePrefix() + hand + "\"");
        return;
    }
    Ogre::Bone* weaponBone = ent->getSkeleton()->getBone(
                                curWeapon->getOgreNamePrefix() + hand);
    Ogre::Entity* weaponEntity = mSceneManager->createEntity(curWeapon->getOgreNamePrefix()
                                + hand + "_" + curCreature->getName(),
                                curWeapon->getMeshName());

    // Rotate by -90 degrees around the x-axis from the bone's rotation.
    Ogre::Quaternion rotationQuaternion;
    rotationQuaternion.FromAngleAxis(Ogre::Degree(-90.0), Ogre::Vector3(1.0,
                                    0.0, 0.0));

    ent->attachObjectToBone(weaponBone->getName(), weaponEntity,
                            rotationQuaternion);
}

void RenderManager::rrDestroyWeapon(Creature* curCreature, const Weapon* curWeapon, const std::string& hand)
{
    std::string weaponEntityName = curWeapon->getOgreNamePrefix() + hand + "_" + curCreature->getName();
    if(mSceneManager->hasEntity(weaponEntityName))
    {
        Ogre::Entity* weaponEntity = mSceneManager->getEntity(weaponEntityName);
        weaponEntity->detachFromParent();
        mSceneManager->destroyEntity(weaponEntity);
    }
}

void RenderManager::rrCreateMapLight(MapLight* curMapLight, bool displayVisual)
{
    // Create the light and attach it to the lightSceneNode.
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    Ogre::Light* light = mSceneManager->createLight(mapLightName + "_light");
    light->setDiffuseColour(curMapLight->getDiffuseColor());
    light->setSpecularColour(curMapLight->getSpecularColor());
    light->setAttenuation(curMapLight->getAttenuationRange(),
                          curMapLight->getAttenuationConstant(),
                          curMapLight->getAttenuationLinear(),
                          curMapLight->getAttenuationQuadratic());

    // Create the base node that the "flicker_node" and the mesh attach to.
    Ogre::SceneNode* mapLightNode = mLightSceneNode->createChildSceneNode(mapLightName + "_node");
    curMapLight->setEntityNode(mapLightNode);
    curMapLight->setParentSceneNode(mapLightNode->getParentSceneNode());
    mapLightNode->setPosition(curMapLight->getPosition());

    if (displayVisual)
    {
        // Create the MapLightIndicator mesh so the light can be drug around in the map editor.
        Ogre::Entity* lightEntity = mSceneManager->createEntity(mapLightName, "Lamp.mesh");
        mapLightNode->attachObject(lightEntity);
    }

    // Create the "flicker_node" which moves around randomly relative to
    // the base node.  This node carries the light itself.
    Ogre::SceneNode* flickerNode = mapLightNode->createChildSceneNode(mapLightName + "_flicker_node");
    flickerNode->attachObject(light);
    curMapLight->setFlickerNode(flickerNode);
}

void RenderManager::rrDestroyMapLight(MapLight* curMapLight)
{
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    if (mSceneManager->hasLight(mapLightName + "_light"))
    {
        Ogre::Light* light = mSceneManager->getLight(mapLightName + "_light");
        Ogre::SceneNode* lightNode = mSceneManager->getSceneNode(mapLightName + "_node");
        Ogre::SceneNode* lightFlickerNode = mSceneManager->getSceneNode(mapLightName
                                            + "_flicker_node");
        lightFlickerNode->detachObject(light);
        mLightSceneNode->removeChild(lightNode);
        mSceneManager->destroyLight(light);

        if (mSceneManager->hasEntity(mapLightName))
        {
            Ogre::Entity* mapLightIndicatorEntity = mSceneManager->getEntity(
                mapLightName);
            lightNode->detachObject(mapLightIndicatorEntity);
        }
        mSceneManager->destroySceneNode(lightFlickerNode->getName());
        mSceneManager->destroySceneNode(lightNode->getName());
    }
}

void RenderManager::rrDestroyMapLightVisualIndicator(MapLight* curMapLight)
{
    std::string mapLightName = curMapLight->getOgreNamePrefix() + curMapLight->getName();
    if (mSceneManager->hasLight(mapLightName + "_light"))
    {
        Ogre::SceneNode* mapLightNode = mSceneManager->getSceneNode(mapLightName + "_node");
        std::string mapLightIndicatorName = mapLightName;
        if (mSceneManager->hasEntity(mapLightIndicatorName))
        {
            Ogre::Entity* mapLightIndicatorEntity = mSceneManager->getEntity(mapLightIndicatorName);
            mapLightNode->detachObject(mapLightIndicatorEntity);
            mSceneManager->destroyEntity(mapLightIndicatorEntity);
            //NOTE: This line throws an error complaining 'scene node not found' that should not be happening.
            //mSceneManager->destroySceneNode(node->getName());
        }
    }
}

void RenderManager::rrPickUpEntity(GameEntity* curEntity, Player* localPlayer)
{
    Ogre::Entity* ent = mSceneManager->getEntity("keeperHandEnt");
    if(ent->hasAnimationState("Pickup"))
        mHandAnimationState = setEntityAnimation(ent, "Pickup", false);

    // Detach the entity from its scene node
    Ogre::SceneNode* curEntityNode = curEntity->getEntityNode();
    curEntity->setParentNodeDetachFlags(
        EntityParentNodeAttach::DETACH_PICKEDUP, true);

    // We make sure the creature will be rendered over the scene by adding it to the same render queue as the keeper hand (and
    // by clearing the depth buffer in ODFrameListener)
    changeRenderQueueRecursive(curEntityNode, OD_RENDER_QUEUE_ID_GUI);

    // Attach the creature to the hand scene node
    mHandKeeperNode->addChild(curEntityNode);
    // When something is picked up, we do a relative scale to allow to see bigger
    // things... bigger
    curEntityNode->scale(Ogre::Vector3::UNIT_SCALE * KEEPER_HAND_CREATURE_PICKED_SCALE);

    rrOrderHand(localPlayer);
}

void RenderManager::rrDropHand(GameEntity* curEntity, Player* localPlayer)
{
    Ogre::Entity* ent = mSceneManager->getEntity("keeperHandEnt");
    if(ent->hasAnimationState("Drop"))
        mHandAnimationState = setEntityAnimation(ent, "Drop", false);

    // Detach the entity from the "hand" scene node
    Ogre::SceneNode* curEntityNode = curEntity->getEntityNode();
    mHandKeeperNode->removeChild(curEntityNode);

    // We put the creature back to the default render queue
    changeRenderQueueRecursive(curEntityNode, Ogre::RenderQueueGroupID::RENDER_QUEUE_MAIN);

    // Attach the creature from the creature scene node
    curEntity->setParentNodeDetachFlags(
        EntityParentNodeAttach::DETACH_PICKEDUP, false);
    Ogre::Vector3 position = curEntity->getPosition();
    curEntityNode->setPosition(position);
    if(curEntity->resizeMeshAfterDrop())
        curEntityNode->scale(Ogre::Vector3::UNIT_SCALE / KEEPER_HAND_CREATURE_PICKED_SCALE);

    rrOrderHand(localPlayer);
}

void RenderManager::rrOrderHand(Player* localPlayer)
{
    // Move the other creatures in the player's hand to make room for the one just picked up.
    int i = 0;
    const std::vector<GameEntity*>& objectsInHand = localPlayer->getObjectsInHand();
    for (GameEntity* tmpEntity : objectsInHand)
    {
        Ogre::Vector3 pos;
        pos.x = static_cast<Ogre::Real>(i % 6 + 1) * KEEPER_HAND_CREATURE_PICKED_OFFSET;
        pos.y = static_cast<Ogre::Real>(i / 6) * KEEPER_HAND_CREATURE_PICKED_OFFSET;
        pos.z = 0;
        tmpEntity->getEntityNode()->setPosition(pos);
        ++i;
    }
}

void RenderManager::rrRotateHand(Player* localPlayer)
{
    // Loop over the creatures in our hand and redraw each of them in their new location.
    int i = 0;
    const std::vector<GameEntity*>& objectsInHand = localPlayer->getObjectsInHand();
    for (GameEntity* tmpEntity : objectsInHand)
    {
        Ogre::SceneNode* tmpEntityNode = mSceneManager->getSceneNode(tmpEntity->getOgreNamePrefix() + tmpEntity->getName() + "_node");
        tmpEntityNode->setPosition(static_cast<Ogre::Real>(i % 6 + 1), static_cast<Ogre::Real>(i / 6), static_cast<Ogre::Real>(0.0));
        ++i;
    }
}

void RenderManager::rrPitchAroundAxis(RenderedMovableEntity* renderedmovableGameEntity, Ogre::Degree dd)
{

    if(renderedmovableGameEntity->getEntityNode() == nullptr)
    {
        OD_LOG_ERR("Entity do not have node=" + renderedmovableGameEntity->getName());
        return;
    }

    Ogre::SceneNode* node = renderedmovableGameEntity->getEntityNode();
    node->pitch(dd);



}


void RenderManager::rrCreateCreatureVisualDebug(Creature* curCreature, Tile* curTile)
{
    if (curTile != nullptr && curCreature != nullptr)
    {
        std::stringstream tempSS;
        tempSS << "Vision_indicator_" << curCreature->getName() << "_"
            << curTile->getX() << "_" << curTile->getY();

        Ogre::Entity* visIndicatorEntity = mSceneManager->createEntity(tempSS.str(),
                                           "Cre_vision_indicator.mesh");
        Ogre::SceneNode* visIndicatorNode = mCreatureSceneNode->createChildSceneNode(tempSS.str()
                                            + "_node");
        visIndicatorNode->attachObject(visIndicatorEntity);
        visIndicatorNode->setPosition(Ogre::Vector3(static_cast<Ogre::Real>(curTile->getX()),
                                                    static_cast<Ogre::Real>(curTile->getY()),
                                                    static_cast<Ogre::Real>(0)));
    }
}

void RenderManager::rrDestroyCreatureVisualDebug(Creature* curCreature, Tile* curTile)
{
    std::stringstream tempSS;
    tempSS << "Vision_indicator_" << curCreature->getName() << "_"
        << curTile->getX() << "_" << curTile->getY();
    if (mSceneManager->hasEntity(tempSS.str()))
    {
        Ogre::Entity* visIndicatorEntity = mSceneManager->getEntity(tempSS.str());
        Ogre::SceneNode* visIndicatorNode = mSceneManager->getSceneNode(tempSS.str() + "_node");
        
        visIndicatorNode->detachAllObjects();
        mSceneManager->destroyEntity(visIndicatorEntity);
        mSceneManager->destroySceneNode(visIndicatorNode);
    }
}

void RenderManager::rrCreateSeatVisionVisualDebug(int seatId, Tile* tile)
{
    if (tile != nullptr)
    {
        std::stringstream tempSS;
        tempSS << "Seat_Vision_indicator" << seatId << "_"
            << tile->getX() << "_" << tile->getY();

        Ogre::Entity* visIndicatorEntity = mSceneManager->createEntity(tempSS.str(),
                                           "Cre_vision_indicator.mesh");
        Ogre::SceneNode* visIndicatorNode = mCreatureSceneNode->createChildSceneNode(tempSS.str()
                                            + "_node");
        
        visIndicatorNode->attachObject(visIndicatorEntity);
        visIndicatorNode->setPosition(Ogre::Vector3(static_cast<Ogre::Real>(tile->getX()),
                                                    static_cast<Ogre::Real>(tile->getY()),
                                                    static_cast<Ogre::Real>(0)));
    }
}

void RenderManager::rrDestroySeatVisionVisualDebug(int seatId, Tile* tile)
{
    std::stringstream tempSS;
    tempSS << "Seat_Vision_indicator" << seatId << "_"
        << tile->getX() << "_" << tile->getY();
    if (mSceneManager->hasEntity(tempSS.str()))
    {
        Ogre::Entity* visIndicatorEntity = mSceneManager->getEntity(tempSS.str());
        Ogre::SceneNode* visIndicatorNode = mSceneManager->getSceneNode(tempSS.str() + "_node");

        visIndicatorNode->detachAllObjects();
        mSceneManager->destroyEntity(visIndicatorEntity);
        mSceneManager->destroySceneNode(visIndicatorNode);
    }
}

void RenderManager::rrSetObjectAnimationState(MovableGameEntity* curAnimatedObject, const std::string& animation, bool loop)
{
    std::string objectName = curAnimatedObject->getOgreNamePrefix()
        + curAnimatedObject->getName();
    if (!mSceneManager->hasEntity(objectName))
        return;

    Ogre::Entity* objectEntity = mSceneManager->getEntity(objectName);

    // Can't animate entities without skeleton
    if (!objectEntity->hasSkeleton())
        return;

    std::string anim = animation;

    // Handle the case where this entity does not have the requested animation.
    while (!objectEntity->getSkeleton()->hasAnimation(anim))
    {
        // Try to change the unexisting animation to a close existing one.
        if (anim == EntityAnimation::sleep_anim)
        {
            anim = EntityAnimation::die_anim;
            continue;
        }
        else if (anim == EntityAnimation::die_anim)
        {
            anim = EntityAnimation::idle_anim;
            break;
        }

        if (anim == EntityAnimation::flee_anim)
        {
            anim = EntityAnimation::walk_anim;
        }
        else if (anim == EntityAnimation::dig_anim || anim == EntityAnimation::claim_anim)
        {
            anim = EntityAnimation::attack_anim;
        }
        else
        {
            anim = EntityAnimation::idle_anim;
            break;
        }
    }

    if (!objectEntity->getSkeleton()->hasAnimation(anim))
        return;

    Ogre::AnimationState* animState = setEntityAnimation(objectEntity, anim, loop);
    curAnimatedObject->setAnimationState(animState);
}
void RenderManager::rrMoveEntity(GameEntity* entity, const Ogre::Vector3& position)
{
    if(entity->getEntityNode() == nullptr)
    {
        OD_LOG_ERR("Entity do not have node=" + entity->getName());
        return;
    }
         
    entity->getEntityNode()->setPosition(position);
}

void RenderManager::rrMoveMapLightFlicker(MapLight* mapLight, const Ogre::Vector3& position)
{
    if(mapLight->getFlickerNode() == nullptr)
    {
        OD_LOG_ERR("MapLight do not have flicker=" + mapLight->getName());
        return;
    }

    mapLight->getFlickerNode()->setPosition(position);
}

Ogre::ParticleSystem* RenderManager::rrEntityAddParticleEffect(GameEntity* entity, const std::string& particleName,
    const std::string& particleScript)
{
    Ogre::SceneNode* node = entity->getEntityNode();
    if(particleScript.empty())
        return nullptr;

    Ogre::ParticleSystem* particleSystem = mSceneManager->createParticleSystem(particleName, particleScript);

    node->attachObject(particleSystem);

    return particleSystem;
}

void RenderManager::rrEntityRemoveParticleEffect(GameEntity* entity, Ogre::ParticleSystem* particleSystem)
{
    if(particleSystem == nullptr)
        return;

    Ogre::SceneNode* node = entity->getEntityNode();
    node->detachObject(particleSystem);
    mSceneManager->destroyParticleSystem(particleSystem);
}

std::string RenderManager::consoleListAnimationsForMesh(const std::string& meshName)
{
    if(!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(meshName + ".mesh"))
        return "\nmesh not available for " + meshName;

    std::string name = meshName + "consoleListAnimationsForMesh";
    Ogre::Entity* objectEntity = msSingleton->mSceneManager->createEntity(name, meshName + ".mesh");
    if (!objectEntity->hasSkeleton())
        return "\nNo skeleton for " + meshName;

    std::string ret;
    Ogre::AnimationStateSet* animationSet = objectEntity->getAllAnimationStates();
    if(animationSet != nullptr)
    {
        for(Ogre::AnimationStateIterator asi =
            animationSet->getAnimationStateIterator(); asi.hasMoreElements(); asi.moveNext())
        {
            std::string animName = asi.peekNextValue()->getAnimationName();
            ret += "\nAnimation: " + animName;
        }
    }

#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    // For Ogre < 1.10 we still need to use Skeleton::BoneIterator
    Ogre::Skeleton::BoneIterator boneIterator = objectEntity->getSkeleton()->getBoneIterator();
    while (boneIterator.hasMoreElements())
    {
        std::string boneName = boneIterator.getNext()->getName();
        ret += "\nBone: " + boneName;
    }
#else
    auto bones = objectEntity->getSkeleton()->getBones();
    for(const auto b : bones)
    {
        if(b)
        {
            std::string boneName = b->getName();
            ret += "\nBone: " + boneName;
        }
    }
#endif
    msSingleton->mSceneManager->destroyEntity(objectEntity);
    return ret;
}

void RenderManager::colourizeEntity(Ogre::Entity *ent, const Seat* seat, bool markedForDigging, bool playerHasVision)
{
    // Colorize the the textures
    // Loop over the sub entities in the mesh
    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);

        std::string materialName = tempSubEntity->getMaterialName();
        // If the material name have been modified, we restore the original name
        std::size_t index = materialName.find("##");
        if(index != std::string::npos)
            materialName = materialName.substr(0, index);

        materialName = colourizeMaterial(materialName, seat, markedForDigging, playerHasVision);
        tempSubEntity->setMaterialName(materialName);
    }
}

std::string RenderManager::colourizeMaterial(const std::string& materialName, const Seat* seat, bool markedForDigging, bool playerHasVision)
{
    if (seat == nullptr && !markedForDigging && playerHasVision)
        return materialName;

    std::stringstream tempSS;

    tempSS.str("");

    tempSS << materialName ; // << "##";

    // Create the material name.
    if(seat != nullptr)
        tempSS << "Color_" << seat->getColorId() << "_" ;
    else
        tempSS << "Color_null_" ;

    if (markedForDigging)
        tempSS << "dig_";
    else if(!playerHasVision)
        tempSS << "novision_";

    Ogre::MaterialPtr requestedMaterial = Ogre::MaterialManager::getSingleton().getByName(tempSS.str());

    // std::cout << "\nCloning material:  " << tempSS.str();

    // If this texture has been copied and colourized, we can return
#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    if (!requestedMaterial.isNull())
#else
    if (requestedMaterial)
#endif
        return tempSS.str();

    // If not yet, then do so

    // Check to see if we find a seat with the requested color, if not then just use the original, uncolored material.
    if (seat == nullptr && !markedForDigging && playerHasVision)
        return materialName;

    Ogre::MaterialPtr oldMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName);
    
    //std::cout << "\nMaterial does not exist, creating a new one.";
    
    Ogre::MaterialPtr newMaterial = oldMaterial->clone(tempSS.str());
    bool cloned = mShaderGenerator->cloneShaderBasedTechniques(*oldMaterial,
                                                               *newMaterial);
    if(!cloned)
    {
        OD_LOG_ERR("Failed to clone rtss for material: " + materialName);
    }

    // Loop over the techniques for the new material
    for (unsigned int j = 0; j < newMaterial->getNumTechniques(); ++j)
    {
        Ogre::Technique* technique = newMaterial->getTechnique(j);
        Ogre::String techniqueName = technique->getName();
        if (technique->getNumPasses() == 0 || techniqueName.find("ZPrePassScheme") != Ogre::String::npos)
            continue;

        if (markedForDigging)
        {
            // Color the material with yellow on the latest pass
            // so we're sure to see the taint.
            Ogre::ColourValue color(1.0, 1.0, 0.0, 1.0);
            for (uint16_t i = 0; i < technique->getNumPasses(); ++i)
            {
                Ogre::Pass* pass = technique->getPass(i);
                pass->setSpecular(color);
                pass->setAmbient(color);
                pass->setDiffuse(color);
                pass->setEmissive(color);
            }
        }
        else if(!playerHasVision)
        {
            // Color the material with dark color on the latest pass
            // so we're sure to see the taint.
            Ogre::Pass* pass = technique->getPass(0);
            Ogre::ColourValue color(0.2, 0.2, 0.2, 1.0);
            pass->setSpecular(color);
            pass->setAmbient(color);
            pass->setDiffuse(color);
            pass->setEmissive(color);            
        }
        if (seat != nullptr)
        {
            // Color the material with the Seat's color.
            Ogre::ColourValue color = seat->getColorValue();
            color.a = 1.0;
            technique->getPass(technique->getNumPasses() -1 )->getFragmentProgramParameters()->setNamedConstant("seatColor", color) ;

            

        }
    }

    return tempSS.str();
}


void RenderManager::rrAddOutliner(Creature* creature)
{
    std::string entityName  = creature->getOgreNamePrefix() +  creature->getName();
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is no entity=" + entityName);
        return ;
    }


    Ogre::Entity* ent = mSceneManager->getEntity(entityName);
    
    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {

        std::stringstream ss("");
        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);
        ss << tempSubEntity->getMaterialName();
        ss << "##_Outliner";



        Ogre::ColourValue cc = creature->getSeat()->getColorValue();        
        ss << "_" << cc;
        std::string materialName = tempSubEntity->getMaterialName();
        if(materialName.find("##_Outliner") != std::string::npos)
            continue;
        OD_LOG_INF("searching for: " +  ss.str() );
        Ogre::MaterialPtr  myOutliner = Ogre::MaterialManager::getSingletonPtr()->getByName(ss.str(),"Graphics");
        if (!myOutliner)
        {
            OD_LOG_INF("couldn't find: " +  ss.str() );

            //myOutliner = Ogre::MaterialManager::getSingletonPtr()->create("myOutliner","Graphics");               
            myOutliner= Ogre::MaterialManager::getSingletonPtr()->getByName(materialName, "Graphics")->clone(ss.str(),"Graphics");
            OD_LOG_INF("cloning......");
            OD_LOG_INF(myOutliner->getName());
            OD_LOG_INF("the number of techniques  is " + Helper::toString(myOutliner->getNumTechniques()));
            Ogre::Pass* myPass1 = myOutliner->getTechnique(myOutliner->getNumTechniques() - 1)->createPass();
            OD_LOG_INF("the number of passes is " + Helper::toString(myOutliner->getTechnique(myOutliner->getNumTechniques() - 1)->getNumPasses()));
            myOutliner->getTechnique(myOutliner->getNumTechniques() - 1)->getPass( myOutliner->getTechnique(myOutliner->getNumTechniques() - 1)->getNumPasses()-1)->setDepthBias(-4, -32);
        
            Ogre::HighLevelGpuProgramManager& mgr =  Ogre::HighLevelGpuProgramManager::getSingleton();
            Ogre::HighLevelGpuProgramPtr vertex_program;

            if(!mgr.resourceExists("Enlarge","Graphics"))
            {
                vertex_program = mgr.createProgram("Enlarge", "Graphics","glsl" ,Ogre::GpuProgramType::GPT_VERTEX_PROGRAM);

                vertex_program->setSourceFile("Enlarge.vert");
            }
            else
            {
                vertex_program = mgr.getByName("Enlarge","Graphics");
            }

            myPass1->setVertexProgram("Enlarge","Graphics");

            Ogre::Vector3 center = mSceneManager->getEntity(entityName)->getMesh()->getBounds().getCenter();
         
            myPass1->getVertexProgramParameters()->setNamedConstant("center", center);
            myPass1->getVertexProgramParameters()->setNamedAutoConstant("worldViewProj", Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);

            Ogre::HighLevelGpuProgramPtr fragment_program;
            if(!mgr.resourceExists("Custom_color","Graphics"))
            {
                fragment_program = mgr.createProgram ("Custom_color", "Graphics","glsl" ,Ogre::GpuProgramType::GPT_FRAGMENT_PROGRAM);
                fragment_program->setSourceFile("Custom_color.frag");                                     
            }
            else
                fragment_program = mgr.getByName("Custom_color","Graphics");
            myPass1->setFragmentProgram("Custom_color","Graphics");
            myPass1->getFragmentProgramParameters()->setNamedConstant("color",cc);
            myPass1->getFragmentProgramParameters()->setNamedConstant("ambient",cc);
        }
          
        tempSubEntity->setMaterial(myOutliner);
    }  
}


void RenderManager::rrRemoveOutliner(Creature* creature)
{
    std::string entityName  = creature->getOgreNamePrefix() +  creature->getName();
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is no entity=" + entityName);
        return ;
    }


    Ogre::Entity* ent = mSceneManager->getEntity(entityName);



    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {

        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);        
        std::string materialName = tempSubEntity->getMaterialName();
        if(materialName.find("##_Outliner") == std::string::npos)
        {
            OD_LOG_ERR("Called rrRemoveOutliner on creature, which has currently no Outliner=: " + entityName + "with material " + materialName);
            return ;
        }    
        size_t pp = materialName.find("##_Outliner");
        materialName.erase(materialName.begin() + pp, materialName.end());
        Ogre::MaterialPtr  myMaterialPtr = Ogre::MaterialManager::getSingletonPtr()->getByName(materialName,"Graphics");
        if(!myMaterialPtr)
            OD_LOG_ERR("Couldn't find the orignal material, that is the material " + materialName + " in rrRemoveOutliner ");
        else
            tempSubEntity->setMaterial(myMaterialPtr);
    }
}


void RenderManager::rrIncreaseAmbient(Creature* creature)
{
    std::string entityName  = creature->getOgreNamePrefix() +  creature->getName();
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is no entity=" + entityName);
        return ;
    }

    Ogre::Entity* ent = mSceneManager->getEntity(entityName);
    
    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);
        std::string materialName =  tempSubEntity->getMaterialName();
        if(materialName.find("##_Brighter") != std::string::npos)
            continue;
        
        std::stringstream ss("");
        ss << tempSubEntity->getMaterialName();
        ss << "##_Brighter";
        OD_LOG_INF("searching for: " +  ss.str() );
        Ogre::MaterialPtr  myBrighter = Ogre::MaterialManager::getSingletonPtr()->getByName(ss.str(),"Graphics");
        if (!myBrighter)
        {
            OD_LOG_INF("couldn't find: " +  ss.str() );

            //myBrighter = Ogre::MaterialManager::getSingletonPtr()->create("myBrighter","Graphics");               
            myBrighter= Ogre::MaterialManager::getSingletonPtr()->getByName(materialName, "Graphics")->clone(ss.str(),"Graphics");
            OD_LOG_INF("cloning......");
            OD_LOG_INF(myBrighter->getName());
            OD_LOG_INF("the number of techniques  is " + Helper::toString(myBrighter->getNumTechniques()));
            OD_LOG_INF("the number of passes is " + Helper::toString(myBrighter->getTechnique(myBrighter->getNumTechniques() - 1)->getNumPasses()));
            for(int ii =0 ; ii < myBrighter->getTechnique(myBrighter->getNumTechniques() - 1)->getNumPasses(); ++ii)
            {
                myBrighter->getTechnique(myBrighter->getNumTechniques() - 1)->getPass(ii)->getFragmentProgramParameters()->setNamedConstant("ambient",Ogre::ColourValue(8.0,8.0,8.0));
                // cv = cv * 8.0;
                // myBrighter->getTechnique(myBrighter->getNumTechniques() - 1)->getPass(ii)->setAmbient(cv);
            }


        }
          
        tempSubEntity->setMaterial(myBrighter);
    } 
    

}


void RenderManager::rrNormalizeAmbient(Creature* creature)
{
    std::string entityName  = creature->getOgreNamePrefix() +  creature->getName();
    if(!mSceneManager->hasEntity(entityName))
    {
        OD_LOG_ERR("There is no entity=" + entityName);
        return ;
    }


    Ogre::Entity* ent = mSceneManager->getEntity(entityName);



    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {

        Ogre::SubEntity *tempSubEntity = ent->getSubEntity(i);        
        std::string materialName = tempSubEntity->getMaterialName();
        if(materialName.find("##_Brighter") == std::string::npos)
        {
            OD_LOG_ERR("Called rrRemoveBrighter on creature, which has currently no Brighter=: " + entityName + "with material " + materialName);
            return ;
        }    
        size_t pp = materialName.find("##_Brighter");
        materialName.erase(materialName.begin() + pp, materialName.end());
        Ogre::MaterialPtr  myMaterialPtr = Ogre::MaterialManager::getSingletonPtr()->getByName(materialName,"Graphics");
        if(!myMaterialPtr)
            OD_LOG_ERR("Couldn't find the orignal material, that is the material " + materialName + " in rrRemoveBrighter ");
        else
            tempSubEntity->setMaterial(myMaterialPtr);
    }
}

void RenderManager::rrCarryEntity(Creature* carrier, GameEntity* carried)
{
    Ogre::Entity* carrierEnt = mSceneManager->getEntity(carrier->getOgreNamePrefix() + carrier->getName());
    Ogre::Entity* carriedEnt = mSceneManager->getEntity(carried->getOgreNamePrefix() + carried->getName());
    Ogre::SceneNode* carrierNode = mSceneManager->getSceneNode(carrierEnt->getName() + "_node");
    Ogre::SceneNode* carriedNode = mSceneManager->getSceneNode(carriedEnt->getName() + "_node");
    carried->setParentNodeDetachFlags(
        EntityParentNodeAttach::DETACH_CARRIED, true);
    carriedNode->setInheritScale(false);
    carrierNode->addChild(carriedNode);
    // We want the carried object to be at half tile (z = 0.5)
    carriedNode->setPosition(Ogre::Vector3(0, 0, 0.5));
}

void RenderManager::rrReleaseCarriedEntity(Creature* carrier, GameEntity* carried)
{
    Ogre::Entity* carrierEnt = mSceneManager->getEntity(carrier->getOgreNamePrefix() + carrier->getName());
    Ogre::Entity* carriedEnt = mSceneManager->getEntity(carried->getOgreNamePrefix() + carried->getName());
    Ogre::SceneNode* carrierNode = mSceneManager->getSceneNode(carrierEnt->getName() + "_node");
    Ogre::SceneNode* carriedNode = mSceneManager->getSceneNode(carriedEnt->getName() + "_node");
    carrierNode->removeChild(carriedNode);
    carried->setParentNodeDetachFlags(
        EntityParentNodeAttach::DETACH_CARRIED, false);
    carriedNode->setInheritScale(true);
}

void RenderManager::rrSetCreaturesTextOverlay(GameMap& gameMap, bool value)
{
    mCreatureTextOverlayDisplayed = value;
    for(Creature* creature : gameMap.getCreatures())
        creature->getOverlayStatus()->displayHealthOverlay(mCreatureTextOverlayDisplayed ? -1.0 : 0.0);
}

void RenderManager::rrTemporaryDisplayCreaturesTextOverlay(Creature* creature, Ogre::Real timeToDisplay)
{
    creature->getOverlayStatus()->displayHealthOverlay(timeToDisplay);
}

void RenderManager::rrToggleHandSelectorVisibility()
{
    if((mHandKeeperHandVisibility & 0x01) == 0)
        mHandKeeperHandVisibility |= 0x01;
    else
        mHandKeeperHandVisibility &= ~0x01;

    mHandKeeperNode->setVisible(mHandKeeperHandVisibility == 0);
}

void RenderManager::setEntityOpacity(Ogre::Entity* ent, float opacity)
{
    for (unsigned int i = 0; i < ent->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity* subEntity = ent->getSubEntity(i);
        subEntity->setMaterialName(setMaterialOpacity(subEntity->getMaterialName(), opacity));
    }
}

std::string RenderManager::setMaterialOpacity(const std::string& materialName, float opacity)
{
    if (opacity < 0.0f || opacity > 1.0f)
        return materialName;

    std::stringstream newMaterialName;
    newMaterialName.str("");

    // Check whether the material name has alreay got an _alpha_ suffix and remove it.
    size_t alphaPos = materialName.find("_alpha_");
    // Create the material name accordingly.
    if (alphaPos == std::string::npos)
        newMaterialName << materialName;
    else
        newMaterialName << materialName.substr(0, alphaPos);

    // Only precise the opactiy when its useful, otherwise give the original material name.
    if (opacity != 1.0f)
        newMaterialName << "_alpha_" << static_cast<int>(opacity * 255.0f);

    Ogre::MaterialPtr requestedMaterial = Ogre::MaterialManager::getSingleton().getByName(newMaterialName.str());

    // If this texture has been copied and colourized, we can return
#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    if (!requestedMaterial.isNull())
#else
    if (requestedMaterial)
#endif
        return newMaterialName.str();

    // If not yet, then do so
    Ogre::MaterialPtr oldMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName);
    //std::cout << "\nMaterial does not exist, creating a new one.";
    Ogre::MaterialPtr newMaterial = oldMaterial->clone(newMaterialName.str());
    bool cloned = mShaderGenerator->cloneShaderBasedTechniques(*oldMaterial,
                                                               *newMaterial);
    if(!cloned)
    {
        OD_LOG_ERR("Failed to clone rtss for material: " + materialName);
    }

    // Loop over the techniques for the new material
    for (auto i = 0; i < newMaterial->getNumTechniques(); ++i)
    {
        Ogre::Technique* technique = newMaterial->getTechnique(i);
        for(auto j = 0; j < technique->getNumPasses(); ++j)
        {
            // Set alpha value for all passes
            Ogre::Pass* pass = technique->getPass(j);
            Ogre::ColourValue color = pass->getEmissive();
            color.a = opacity;
            pass->setEmissive(color);

            color = pass->getSpecular();
            color.a = opacity;
            pass->setSpecular(color);

            color = pass->getAmbient();
            color.a = opacity;
            pass->setAmbient(color);

            color = pass->getDiffuse();
            color.a = opacity;
            pass->setDiffuse(color);

            if (opacity < 1.0f)
            {
                pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                pass->setDepthWriteEnabled(false);
            }
            else
            {
                // Use sane default, but this should never happen...
                pass->setSceneBlending(Ogre::SBT_MODULATE);
                pass->setDepthWriteEnabled(true);
            }
        }
    }

    return newMaterialName.str();
}

void RenderManager::moveCursor(float relX, float relY)
{
    Ogre::Camera* cam = mViewport->getCamera();
    if(cam->getFOVy() != mCurrentFOVy)
    {
        mCurrentFOVy = cam->getFOVy();
        Ogre::Radian angle = cam->getFOVy() * 0.5f;
        Ogre::Real tan = Ogre::Math::Tan(angle);
        Ogre::Real shortestSize = KEEPER_HAND_POS_Z * tan * 2.0f;
        Ogre::Real width = mViewport->getActualWidth();
        Ogre::Real height = mViewport->getActualHeight();
        if(width > height)
        {
            mFactorHeight = shortestSize;
            mFactorWidth = shortestSize * width / height;
        }
        else
        {
            mFactorWidth = shortestSize;
            mFactorHeight = shortestSize * height / width;
        }
    }

    mHandKeeperNode->setPosition(mFactorWidth * (relX - 0.5f), mFactorHeight * (0.5f - relY), -KEEPER_HAND_POS_Z);
}

void RenderManager::moveWorldCoords(Ogre::Real x, Ogre::Real y)
{
    if(mHandLightNode != nullptr)
    {
        mHandLightNode->setPosition(x, y,  KEEPER_HAND_WORLD_Z);
    }
}

void RenderManager::entitySlapped()
{
    Ogre::Entity* ent = mSceneManager->getEntity("keeperHandEnt");
    if(ent->hasAnimationState("Slap"))
        mHandAnimationState = setEntityAnimation(ent, "Slap", false);
}

std::string RenderManager::rrBuildSkullFlagMaterial(const std::string& materialNameBase,
        const Ogre::ColourValue& color)
{
    std::string materialNameToUse = materialNameBase + "_" + Helper::toString(color);

    Ogre::MaterialPtr requestedMaterial = Ogre::MaterialPtr(Ogre::MaterialManager::getSingleton().getByName(materialNameToUse));

    // If this texture has been copied and colourized, we can return
#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    if (!requestedMaterial.isNull())
#else
    if (requestedMaterial)
#endif
        return materialNameToUse;

    Ogre::MaterialPtr oldMaterial = Ogre::MaterialManager::getSingleton().getByName(materialNameBase);

    Ogre::MaterialPtr newMaterial = oldMaterial->clone(materialNameToUse);
    if (!mShaderGenerator->cloneShaderBasedTechniques(*oldMaterial,
                                                      *newMaterial)) {
        OD_LOG_ERR("Failed to clone rtss for material: " + materialNameBase);
    }

    for (unsigned short j = 0; j < newMaterial->getNumTechniques(); ++j)
    {
        Ogre::Technique* technique = newMaterial->getTechnique(j);
        Ogre::String techniqueName = technique->getName();
        if (technique->getNumPasses() == 0 || techniqueName.find("ZPrePassScheme") != Ogre::String::npos)
            continue;

        for (uint16_t i = 0; i < technique->getNumPasses(); ++i)
        {
            Ogre::Pass* pass = technique->getPass(i);
            pass->getTextureUnitState(0)->setColourOperationEx(Ogre::LayerBlendOperationEx::LBX_MODULATE,
                Ogre::LayerBlendSource::LBS_TEXTURE, Ogre::LayerBlendSource::LBS_MANUAL, Ogre::ColourValue(),
                color);
        }
    }

    return materialNameToUse;
}

void RenderManager::rrMinimapRendering(bool postRender)
{
    if(mHandLight != nullptr)
        mHandLight->setVisible(postRender);

    mLightSceneNode->setVisible(postRender);
}

void RenderManager::changeRenderQueueRecursive(Ogre::SceneNode* node, uint8_t renderQueueId)
{
    for(unsigned short i = 0; i < node->numAttachedObjects(); ++i)
    {
        Ogre::MovableObject* obj = node->getAttachedObject(i);
        obj->setRenderQueueGroup(renderQueueId);
    }

    for(unsigned short i = 0; i < node->numChildren(); ++i)
    {
        Ogre::SceneNode* childNode = dynamic_cast<Ogre::SceneNode *>(node->getChild(i));
        if(childNode == nullptr)
            continue;

        changeRenderQueueRecursive(childNode, renderQueueId);
    }
}

Ogre::AnimationState* RenderManager::setEntityAnimation(Ogre::Entity* ent, const std::string& animation, bool loop)
{
    Ogre::AnimationStateSet* animationSet = ent->getAllAnimationStates();
    if(animationSet == nullptr)
        return nullptr;

    Ogre::AnimationState* animState = nullptr;

    for(Ogre::AnimationStateIterator asi =
        animationSet->getAnimationStateIterator(); asi.hasMoreElements(); asi.moveNext())
    {
        Ogre::AnimationState* as = asi.peekNextValue();
        if(as->getAnimationName() == animation)
        {
            // We we are not currently playing the animation or if we
            // are not looped, we start the animation from the beginning
            if(!as->getEnabled() || !loop)
                as->setTimePosition(0);

            as->setLoop(loop);
            as->setEnabled(true);
            animState = as;
            continue;
        }
        as->setEnabled(false);
    }

    return animState;
}
