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

namespace
{
struct PortraitScene
{
    Ogre::SceneManager* scene = Ogre::Root::getSingleton().createSceneManager("DefaultSceneManager");
    std::vector<Ogre::MaterialPtr> materials;

    ~PortraitScene()
    {
        Ogre::Root::getSingleton().destroySceneManager(scene);
        for(const auto& material : materials)
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
    unsigned short src, dest;
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
    const float height = std::max(size.z * 0.65f, size.x * 0.35f);
    Ogre::Vector3 center = bounds.getCenter();
    center.z = bounds.getMinimum().z + size.z * 0.72f;
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
                    center.z += size.z * 0.05f;
                    break;
                }
            }
        }
    }

    Ogre::Camera* camera = scene->createCamera("PortraitCamera");
    camera->setNearClipDistance(0.01f);
    camera->setFarClipDistance(std::max(10.0f, size.length() * 4.0f));
    camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
    camera->setOrthoWindow(height * 0.75f, height);
    Ogre::SceneNode* cameraNode = scene->getRootSceneNode()->createChildSceneNode();
    cameraNode->setFixedYawAxis(true, Ogre::Vector3::UNIT_Z);
    cameraNode->attachObject(camera);
    cameraNode->setPosition(center + Ogre::Vector3(0, -size.length() * 2.0f, size.z * 0.1f));
    cameraNode->lookAt(center, Ogre::Node::TS_WORLD);

    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(textureName, "General",
        Ogre::TEX_TYPE_2D, 192, 256, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_RENDERTARGET);
    try
    {
        Ogre::RenderTexture* target = texture->getBuffer()->getRenderTarget();
        target->setAutoUpdated(false);
        Ogre::Viewport* viewport = target->addViewport(camera);
        viewport->setOverlaysEnabled(false);
        viewport->setShadowsEnabled(false);
        viewport->setBackgroundColour(Ogre::ColourValue(0.025f, 0.018f, 0.015f));
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
        image.setArea(CEGUI::Rectf(0.0f, 0.0f, 192.0f, 256.0f));
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
