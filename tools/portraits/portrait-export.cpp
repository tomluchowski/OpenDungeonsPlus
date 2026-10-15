/*
 * Copyright (C) 2026 OpenDungeons Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/CreaturePortrait.h"
#include <Ogre.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreRenderTexture.h>
#include <CEGUI/BasicImage.h>
#include <CEGUI/RenderTarget.h>
#include <CEGUI/GeometryBuffer.h>
#include <CEGUI/System.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/RendererModules/Ogre/Texture.h>
#include <CEGUI/RendererModules/Ogre/ResourceProvider.h>
#include <CEGUI/RendererModules/Ogre/ImageCodec.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

int main(int argc, char** argv)
{
    if(argc != 4)
    {
        std::cerr << "Usage: portrait-export <project-root> <dependency-prefix> <output-directory>\n";
        return 2;
    }
    const std::string project = argv[1];
    const std::string dependencies = argv[2];
    const std::string output = argv[3];
    try
    {
        Ogre::Root root("", "", output + "/portrait-export-Ogre.log");
        root.loadPlugin(dependencies + "/bin/RenderSystem_GL3Plus.dll");
        root.loadPlugin(dependencies + "/bin/Codec_STBI.dll");
        Ogre::RenderSystem* renderer = root.getAvailableRenderers().front();
        root.setRenderSystem(renderer);
        renderer->setConfigOption("Full Screen", "No");
        renderer->setConfigOption("Debug Layer", "Off");
        root.initialise(false);
        Ogre::NameValuePairList options;
        options["hidden"] = "true";
        options["FSAA"] = "0";
        Ogre::RenderWindow* window = root.createRenderWindow("PortraitAssetPreview", 192, 384, false, &options);
        window->setAutoUpdated(false);
        Ogre::ResourceGroupManager& resources = Ogre::ResourceGroupManager::getSingleton();
        for(const char* directory : {"/models", "/materials/scripts/Creatures", "/materials/textures", "/shaders"})
            resources.addResourceLocation(project + directory, "FileSystem", "Graphics");
        resources.addResourceLocation(dependencies + "/Media/Main", "FileSystem", "OgreInternal");
        resources.addResourceLocation(dependencies + "/Media/Main", "FileSystem", "Graphics");
        resources.addResourceLocation(dependencies + "/Media/RTShaderLib/GLSL", "FileSystem", "Graphics");
        resources.initialiseAllResourceGroups();
        resources.addResourceLocation(project + "/materials/scripts", "FileSystem", "Graphics");
        Ogre::MaterialManager::getSingleton().parseScript(resources.openResource("ReflMetal.material", "Graphics"), "Graphics");

        Ogre::RTShader::ShaderGenerator::initialize();
        Ogre::RTShader::ShaderGenerator* shaderGenerator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        OgreBites::SGTechniqueResolverListener listener(shaderGenerator);
        Ogre::MaterialManager::getSingleton().addListener(&listener);
        Ogre::SceneManager* mainScene = root.createSceneManager("DefaultSceneManager");
        shaderGenerator->addSceneManager(mainScene);
        Ogre::Camera* mainCamera = mainScene->createCamera("MainCamera");
        mainScene->getRootSceneNode()->createChildSceneNode()->attachObject(mainCamera);
        window->addViewport(mainCamera)->setBackgroundColour(Ogre::ColourValue::Black);
        Ogre::MaterialManager::getSingleton().setActiveScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        CEGUI::OgreRenderer& guiRenderer = CEGUI::OgreRenderer::create(*window);
        CEGUI::OgreResourceProvider& provider = CEGUI::OgreRenderer::createOgreResourceProvider();
        CEGUI::OgreImageCodec& codec = CEGUI::OgreRenderer::createOgreImageCodec();
        CEGUI::System::create(guiRenderer, &provider, nullptr, &codec, nullptr, "", (output + "/portrait-export-CEGUI.log").c_str());
        guiRenderer.setFrameControlExecutionEnabled(false);
        std::vector<std::pair<Ogre::GpuProgramParametersSharedPtr, size_t>> shadowParameters;
        Ogre::ResourceManager::ResourceMapIterator materials = Ogre::MaterialManager::getSingleton().getResourceIterator();
        while(materials.hasMoreElements())
        {
            std::shared_ptr<Ogre::Material> material = std::static_pointer_cast<Ogre::Material>(materials.getNext());
            for(unsigned short t = 0; t < material->getNumTechniques(); ++t)
                for(unsigned short p = 0; p < material->getTechnique(t)->getNumPasses(); ++p)
                {
                    Ogre::Pass* pass = material->getTechnique(t)->getPass(p);
                    if(!pass->hasFragmentProgram()) continue;
                    Ogre::GpuProgramParametersSharedPtr parameters = pass->getFragmentProgramParameters();
                    if(!parameters->hasNamedParameters()) continue;
                    Ogre::GpuConstantDefinitionMap::const_iterator found = parameters->getConstantDefinitions().map.find("shadowingEnabled");
                    if(found == parameters->getConstantDefinitions().map.end()) continue;
                    parameters->setNamedConstant("shadowingEnabled", 1);
                    shadowParameters.emplace_back(parameters, found->second.physicalIndex);
                }
        }
        std::ifstream config(project + "/config/creatures.cfg");
        if(!config) throw std::runtime_error("Cannot read creature configuration");
        std::string line;
        std::set<std::string> meshes;
        while(std::getline(config, line))
        {
            std::istringstream tokens(line);
            std::string field, value;
            if(tokens >> field >> value && field == "MeshName") meshes.insert(value);
        }
        if(meshes.empty()) throw std::runtime_error("No creature meshes found");
        for(const std::string& meshName : meshes)
        {
            const CEGUI::Image& image = getCreaturePortraitImage(meshName);
            if(&image != &getCreaturePortraitImage(meshName)) throw std::runtime_error("Portrait cache miss");
            const CEGUI::OgreTexture& guiTexture = static_cast<const CEGUI::OgreTexture&>(guiRenderer.getTexture("CreaturePortrait/" + meshName));
            Ogre::TexturePtr texture = guiTexture.getOgreTexture();
            Ogre::RenderTexture* target = texture->getBuffer()->getRenderTarget();
            target->writeContentsToFile(output + "/portrait-" + meshName + ".png");
            const uint32_t width = texture->getWidth();
            const uint32_t height = texture->getHeight();
            std::vector<unsigned char> pixels(width * height * 4);
            Ogre::PixelBox box(width, height, 1, Ogre::PF_BYTE_RGBA, pixels.data());
            texture->getBuffer()->blitToMemory(box);
            unsigned int coloured = 0;
            for(size_t p = 0; p < pixels.size(); p += 4)
                if(pixels[p] > 20 || pixels[p + 1] > 20 || pixels[p + 2] > 20) ++coloured;
            if(coloured < width * height / 100) throw std::runtime_error("Empty portrait: " + meshName);
            if(target->getNumViewports() != 0) throw std::runtime_error("Retained portrait viewport");
            if(meshName == "Kobold.mesh" || meshName == "Orc.mesh")
            {
                window->update(false);
                CEGUI::GeometryBuffer& buffer = guiRenderer.createGeometryBuffer();
                buffer.setClippingRegion(CEGUI::Rectf(0, 0, 192, 384));
                image.render(buffer, CEGUI::Rectf(0, 0, 192, 384), nullptr, CEGUI::ColourRect(0xFFFFFFFF));
                guiRenderer.beginRendering();
                CEGUI::RenderTarget& guiTarget = guiRenderer.getDefaultRenderTarget();
                guiTarget.activate();
                guiTarget.draw(buffer);
                guiTarget.deactivate();
                guiRenderer.endRendering();
                window->writeContentsToFile(output + "/portrait-gui-" + meshName + ".png");
                window->swapBuffers();
                guiRenderer.destroyGeometryBuffer(buffer);
            }
            std::cout << meshName << " visiblePixels=" << coloured << std::endl;
            for(const std::pair<Ogre::GpuProgramParametersSharedPtr, size_t>& value : shadowParameters)
                if(value.first->getIntPointer(value.second)[0] != 1)
                    throw std::runtime_error("World shadow parameter changed");
            Ogre::SceneManagerEnumerator::SceneManagerIterator scenes = root.getSceneManagerIterator();
            while(scenes.hasMoreElements())
                if(scenes.getNext() != mainScene) throw std::runtime_error("Retained portrait scene");
            Ogre::ResourceManager::ResourceMapIterator remainingMaterials = Ogre::MaterialManager::getSingleton().getResourceIterator();
            while(remainingMaterials.hasMoreElements())
                if(remainingMaterials.getNext()->getName().find("CreaturePortrait/") == 0)
                    throw std::runtime_error("Retained portrait material copy");
        }
        CEGUI::OgreRenderer::destroySystem();
        for(const std::string& meshName : meshes)
            if(Ogre::TextureManager::getSingleton().resourceExists("CreaturePortrait/" + meshName, "General"))
                throw std::runtime_error("Retained portrait texture after GUI shutdown");
        shaderGenerator->removeSceneManager(mainScene);
        root.destroySceneManager(mainScene);
        Ogre::MaterialManager::getSingleton().removeListener(&listener);
        Ogre::RTShader::ShaderGenerator::destroy();
        std::cout << "PORTRAITS=" << meshes.size() << std::endl;
    }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
