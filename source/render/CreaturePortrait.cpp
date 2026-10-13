/*
 *  Copyright (C) 2026 OpenDungeons Team
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/CreaturePortrait.h"

#include <Ogre.h>
#include <OgreBone.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreRenderTexture.h>

#include <CEGUI/BasicImage.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/System.h>
#include <CEGUI/Texture.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
//! Visible portrait height as a fraction of the mesh height, or of the mesh width if larger.
const float PORTRAIT_HEIGHT_PER_MESH_HEIGHT = 0.65f;
const float PORTRAIT_HEIGHT_PER_MESH_WIDTH = 0.35f;
//! Height of the camera focus point inside the mesh bounds, as a fraction of the mesh height.
const float PORTRAIT_FOCUS_HEIGHT_RATIO = 0.72f;
//! Focus offset above a head bone, as a fraction of the mesh height.
const float PORTRAIT_HEAD_OFFSET_RATIO = 0.05f;
//! Camera distance as a multiple of the mesh diagonal, and its height offset as a fraction of the mesh height.
const float PORTRAIT_CAMERA_DISTANCE_FACTOR = 2.0f;
const float PORTRAIT_CAMERA_HEIGHT_RATIO = 0.1f;
//! Far clip distance as a multiple of the mesh diagonal, with a lower limit.
const float PORTRAIT_FAR_CLIP_FACTOR = 4.0f;
const float PORTRAIT_FAR_CLIP_MINIMUM = 10.0f;
//! Vertical position of the square hand icon crop inside the portrait texture
//! (0 = top, 1 = bottom of the free space), for illustrated and rendered portraits.
const float HAND_ICON_ILLUSTRATED_TOP_RATIO = 0.25f;
const float HAND_ICON_RENDERED_TOP_RATIO = 0.5f;

//! Owns the temporary scene manager and material copies of one portrait render.
struct PortraitScene
{
    Ogre::SceneManager* scene = Ogre::Root::getSingleton().createSceneManager("DefaultSceneManager");
    std::vector<Ogre::MaterialPtr> materials;

    ~PortraitScene()
    {
        Ogre::Root::getSingleton().destroySceneManager(scene);
        for(const Ogre::MaterialPtr& material : materials)
            Ogre::MaterialManager::getSingleton().remove(material->getHandle());
    }
};
}

Ogre::TexturePtr createCreaturePortrait(const std::string& meshName, const std::string& textureName)
{
    PortraitScene portrait;
    Ogre::SceneManager* scene = portrait.scene;
    scene->setAmbientLight(Ogre::ColourValue(0.6f, 0.6f, 0.6f));
    Ogre::Light* light = scene->createLight();
    light->setType(Ogre::Light::LT_DIRECTIONAL);
    light->setDiffuseColour(0.8f, 0.8f, 0.8f);
    light->setSpecularColour(0.1f, 0.1f, 0.1f);
    Ogre::SceneNode* lightNode = scene->getRootSceneNode()->createChildSceneNode();
    lightNode->attachObject(light);
    lightNode->setDirection(0.4f, 1.0f, -0.6f);

    Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().load(meshName, "Graphics");
    unsigned short src = 0;
    unsigned short dest = 0;
    if(!mesh->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
        mesh->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    Ogre::Entity* entity = scene->createEntity(mesh);
    scene->getRootSceneNode()->createChildSceneNode()->attachObject(entity);
    if(entity->hasAnimationState("Idle"))
        entity->getAnimationState("Idle")->setEnabled(true);
    entity->_updateAnimation();

    // World shadow settings mutate shared material parameters. Only portrait
    // copies may override them, and they are released after this one render.
    for(unsigned int i = 0; i < entity->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity* subEntity = entity->getSubEntity(i);
        Ogre::MaterialPtr material = subEntity->getMaterial()->clone(textureName + "/" + std::to_string(i));
        portrait.materials.push_back(material);
        for(unsigned short t = 0; t < material->getNumTechniques(); ++t)
        {
            Ogre::Technique* technique = material->getTechnique(t);
            for(unsigned short p = 0; p < technique->getNumPasses(); ++p)
            {
                Ogre::Pass* pass = technique->getPass(p);
                if(!pass->hasFragmentProgram())
                    continue;
                Ogre::GpuProgramParametersSharedPtr parameters = pass->getFragmentProgramParameters();
                if(parameters->hasNamedParameters() &&
                    parameters->getConstantDefinitions().map.count("shadowingEnabled") != 0)
                    parameters->setNamedConstant("shadowingEnabled", false);
            }
        }
        subEntity->setMaterial(material);
    }

    const Ogre::AxisAlignedBox& bounds = mesh->getBounds();
    const Ogre::Vector3 size = bounds.getSize();
    const float height = std::max(size.z * PORTRAIT_HEIGHT_PER_MESH_HEIGHT, size.x * PORTRAIT_HEIGHT_PER_MESH_WIDTH);
    Ogre::Vector3 center = bounds.getCenter();
    center.z = bounds.getMinimum().z + size.z * PORTRAIT_FOCUS_HEIGHT_RATIO;
    if(size.z < size.y)
    {
        center = bounds.getCenter();
        if(entity->hasSkeleton())
        {
            for(unsigned short i = 0; i < entity->getSkeleton()->getNumBones(); ++i)
            {
                Ogre::Bone* bone = entity->getSkeleton()->getBone(i);
                std::string name = bone->getName();
                Ogre::StringUtil::toLowerCase(name);
                if(name == "head" || Ogre::StringUtil::endsWith(name, "_head"))
                {
                    center = bone->_getDerivedPosition();
                    center.z += size.z * PORTRAIT_HEAD_OFFSET_RATIO;
                    break;
                }
            }
        }
    }

    Ogre::Camera* camera = scene->createCamera("PortraitCamera");
    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(std::max(PORTRAIT_FAR_CLIP_MINIMUM, size.length() * PORTRAIT_FAR_CLIP_FACTOR));
    camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
    camera->setOrthoWindow(height * 0.5f, height);
    Ogre::SceneNode* cameraNode = scene->getRootSceneNode()->createChildSceneNode();
    cameraNode->setFixedYawAxis(true, Ogre::Vector3::UNIT_Z);
    cameraNode->attachObject(camera);
    cameraNode->setPosition(center + Ogre::Vector3(0, -size.length() * PORTRAIT_CAMERA_DISTANCE_FACTOR, size.z * PORTRAIT_CAMERA_HEIGHT_RATIO));
    cameraNode->lookAt(center, Ogre::Node::TS_WORLD);

    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(textureName, "General",
        Ogre::TEX_TYPE_2D, CREATURE_PORTRAIT_WIDTH, CREATURE_PORTRAIT_HEIGHT, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);
    try
    {
        Ogre::RenderTexture* target = texture->getBuffer()->getRenderTarget();
        target->setAutoUpdated(false);
        Ogre::Viewport* viewport = target->addViewport(camera);
        viewport->setOverlaysEnabled(false);
        viewport->setShadowsEnabled(false);
        viewport->setBackgroundColour(Ogre::ColourValue(0.025f, 0.018f, 0.015f));
        // A preceding GUI draw can leave its scissor rectangle active on this target.
        Ogre::Root::getSingleton().getRenderSystem()->setScissorTest(false);
        target->update();
        target->removeAllViewports();
    }
    catch(...)
    {
        Ogre::TextureManager::getSingleton().remove(texture->getHandle());
        throw;
    }
    return texture;
}

const CEGUI::Image& getCreaturePortraitImage(const std::string& meshName)
{
    const std::string name = "CreaturePortrait/" + meshName;
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    if(images.isDefined(name))
        return images.get(name);

    Ogre::TexturePtr texture = createCreaturePortrait(meshName, name);
    CEGUI::OgreRenderer& renderer = static_cast<CEGUI::OgreRenderer&>(
        *CEGUI::System::getSingleton().getRenderer());
    try
    {
        CEGUI::Texture& guiTexture = renderer.createTexture(name, texture, true);
        CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
        image.setTexture(&guiTexture);
        image.setArea(CEGUI::Rectf(0.0f, 0.0f,
            static_cast<float>(CREATURE_PORTRAIT_WIDTH), static_cast<float>(CREATURE_PORTRAIT_HEIGHT)));
        image.setAutoScaled(CEGUI::ASM_Disabled);
        return image;
    }
    catch(...)
    {
        if(images.isDefined(name))
            images.destroy(name);
        if(renderer.isTextureDefined(name))
            renderer.destroyTexture(name);
        else
            Ogre::TextureManager::getSingleton().remove(texture->getHandle());
        throw;
    }
}

const CEGUI::Image& getCreaturePanelPortraitImage(const std::string& meshName)
{
    const std::string name = "IllustratedCreaturePortrait/" + meshName;
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    if(images.isDefined(name))
        return images.get(name);

    const std::string filename = "portrait-" + meshName + ".png";
    if(!Ogre::ResourceGroupManager::getSingleton().resourceExists("Graphics", filename))
        return getCreaturePortraitImage(meshName);

    CEGUI::OgreRenderer& renderer = static_cast<CEGUI::OgreRenderer&>(
        *CEGUI::System::getSingleton().getRenderer());
    try
    {
        CEGUI::Texture& texture = renderer.createTexture(name, filename, "Graphics");
        const CEGUI::Sizef size = texture.getOriginalDataSize();
        CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
        image.setTexture(&texture);
        image.setArea(CEGUI::Rectf(0.0f, 0.0f, size.d_width, size.d_height));
        image.setAutoScaled(CEGUI::ASM_Disabled);
        return image;
    }
    catch(...)
    {
        if(images.isDefined(name))
            images.destroy(name);
        if(renderer.isTextureDefined(name))
            renderer.destroyTexture(name);
        throw;
    }
}

const CEGUI::Image& getCreatureHandIconImage(const std::string& meshName)
{
    const std::string name = "CreatureHandIcon/" + meshName;
    CEGUI::ImageManager& images = CEGUI::ImageManager::getSingleton();
    if(images.isDefined(name))
        return images.get(name);

    getCreaturePanelPortraitImage(meshName);
    CEGUI::Renderer& renderer = *CEGUI::System::getSingleton().getRenderer();
    const bool illustrated = renderer.isTextureDefined("IllustratedCreaturePortrait/" + meshName);
    const std::string portraitName = illustrated ?
        "IllustratedCreaturePortrait/" + meshName : "CreaturePortrait/" + meshName;
    CEGUI::Texture& texture = renderer.getTexture(portraitName);
    const CEGUI::Sizef size = texture.getOriginalDataSize();
    const float side = std::min(size.d_width, size.d_height);
    const float left = (size.d_width - side) * 0.5f;
    const float topRatio = illustrated ? HAND_ICON_ILLUSTRATED_TOP_RATIO : HAND_ICON_RENDERED_TOP_RATIO;
    const float top = (size.d_height - side) * topRatio;
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(images.create("BasicImage", name));
    image.setTexture(&texture);
    image.setArea(CEGUI::Rectf(left, top, left + side, top + side));
    image.setAutoScaled(CEGUI::ASM_Disabled);
    return image;
}
