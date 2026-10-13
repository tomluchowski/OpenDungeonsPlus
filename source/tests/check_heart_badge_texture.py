"""Update the heart badge with the real CEGUI Ogre renderer and read back what the badge image shows.

The heart ring in the top left corner did not move when the heart was hit. The badge image
(OpenDungeonsIcons/ManaBadge) refers to the CEGUI texture "ManaBadge"; a window caches its geometry
together with the Ogre texture it had when it was drawn (CEGUI's OgreGeometryBuffer::appendGeometry
stores getOgreTexture() in the batch). OgreTexture::loadFromMemory makes a new Ogre texture on every call,
so the redrawn ring went into a texture the badge never showed again.

This check links OgreMain and the CEGUI Ogre renderer, opens a hidden window, bootstraps CEGUI, creates
the badge texture the way Gui.cpp does, and then:
- reproduces the cause: loadFromMemory replaces the Ogre texture;
- runs the real Gui::updateHeartBadge and drawBadgePixels, cut out of Gui.cpp by signature, and checks
  that the Ogre texture stays the same and that its pixels, read back from the texture, are the ring of
  the new health (and differ from the full ring).
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
deps = Path(os.environ.get('OD_DEPS_ROOT', str(Path.home() / 'od-deps')))
gui = (repo / 'source/render/Gui.cpp').read_text()
rules = repo / 'source/game/HeartHealthRing.h'


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


update = function(gui, 'void Gui::updateHeartBadge(').replace('void Gui::updateHeartBadge(', 'void updateHeartBadge(')
badge_code = ('const int BADGE_SIZE = 128;\n' + function(gui, 'bool isInBadgeSymbol(') + '\n'
              + function(gui, 'void drawBadgePixels(') + '\n')

probe = r'''
#include <iostream>
#include <string>
#include <vector>
#include <Ogre.h>
#include <CEGUI/CEGUI.h>
#include <CEGUI/RendererModules/Ogre/Renderer.h>
#include <CEGUI/RendererModules/Ogre/Texture.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include "RULES_HEADER"

BADGE_CODE
UPDATE

int gChecks = 0, gFailures = 0;
void check(bool ok, const std::string& msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}

std::vector<unsigned char> readBack(CEGUI::Texture& texture)
{
    Ogre::TexturePtr ogreTexture = static_cast<CEGUI::OgreTexture&>(texture).getOgreTexture();
    std::vector<unsigned char> pixels(BADGE_SIZE * BADGE_SIZE * 4);
    Ogre::PixelBox box(BADGE_SIZE, BADGE_SIZE, 1, ogreTexture->getFormat(), pixels.data());
    ogreTexture->getBuffer()->blitToMemory(box);
    return pixels;
}

int main(int argc, char** argv)
{
    try
    {
    const std::string prefix = argv[1];
    Ogre::LogManager* logs = new Ogre::LogManager();
    logs->createLog("badge-Ogre.log", true, false, false);
    Ogre::Root* root = new Ogre::Root("", "", "");
    root->loadPlugin(prefix + "/bin/RenderSystem_GL3Plus");
    root->setRenderSystem(root->getAvailableRenderers().front());
    root->initialise(false);
    Ogre::NameValuePairList options;
    options["hidden"] = "true";
    Ogre::RenderWindow* window = root->createRenderWindow("Badge", 256, 256, false, &options);
    // As the game: the shader generator supplies the GPU programs the GL3+ renderer needs
    Ogre::ResourceGroupManager& resources = Ogre::ResourceGroupManager::getSingleton();
    resources.addResourceLocation(prefix + "/Media/Main", "FileSystem", "OgreInternal");
    resources.addResourceLocation(prefix + "/Media/RTShaderLib/GLSL", "FileSystem", "General");
    resources.initialiseAllResourceGroups();
    Ogre::RTShader::ShaderGenerator::initialize();
    Ogre::RTShader::ShaderGenerator* shaders = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    OgreBites::SGTechniqueResolverListener listener(shaders);
    Ogre::MaterialManager::getSingleton().addListener(&listener);
    Ogre::SceneManager* scene = root->createSceneManager("DefaultSceneManager");
    shaders->addSceneManager(scene);
    Ogre::MaterialManager::getSingleton().setActiveScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
    Ogre::Camera* camera = scene->createCamera("Camera");
    scene->getRootSceneNode()->createChildSceneNode()->attachObject(camera);
    window->addViewport(camera);
    CEGUI::OgreRenderer& renderer = CEGUI::OgreRenderer::bootstrapSystem(*window);

    // As Gui.cpp at start: the full ring, loaded once
    std::vector<unsigned char> full;
    drawBadgePixels(full, 0, 1.0f, false);
    CEGUI::Texture& badge = renderer.createTexture("ManaBadge");
    badge.loadFromMemory(full.data(), CEGUI::Sizef(BADGE_SIZE, BADGE_SIZE), CEGUI::Texture::PF_RGBA);
    const Ogre::TexturePtr shown = static_cast<CEGUI::OgreTexture&>(badge).getOgreTexture();
    check(readBack(badge) == full, "the full ring is in the badge texture");

    // The cause: loading again makes a new Ogre texture; a window that cached the first one keeps showing it
    {
        CEGUI::Texture& probe = renderer.createTexture("Probe");
        probe.loadFromMemory(full.data(), CEGUI::Sizef(BADGE_SIZE, BADGE_SIZE), CEGUI::Texture::PF_RGBA);
        const Ogre::Texture* first = static_cast<CEGUI::OgreTexture&>(probe).getOgreTexture().get();
        probe.loadFromMemory(full.data(), CEGUI::Sizef(BADGE_SIZE, BADGE_SIZE), CEGUI::Texture::PF_RGBA);
        const Ogre::Texture* second = static_cast<CEGUI::OgreTexture&>(probe).getOgreTexture().get();
        check(first != second, "reproduced: loadFromMemory replaces the Ogre texture the badge window had cached");
        renderer.destroyTexture("Probe");
    }

    // The production update after a hit: the same Ogre texture now holds the smaller ring
    updateHeartBadge(0.4f, true);
    std::vector<unsigned char> expected;
    drawBadgePixels(expected, 0, 0.4f, true);
    check(static_cast<CEGUI::OgreTexture&>(badge).getOgreTexture().get() == shown.get(),
        "updateHeartBadge keeps the Ogre texture the badge window shows");
    const std::vector<unsigned char> after = readBack(badge);
    check(after == expected, "the shown texture holds the ring of the new health");
    check(after != full, "the shown ring is no longer the full ring");
    updateHeartBadge(0.0f, false);
    std::vector<unsigned char> empty;
    drawBadgePixels(empty, 0, 0.0f, false);
    check(readBack(badge) == empty && static_cast<CEGUI::OgreTexture&>(badge).getOgreTexture().get() == shown.get(),
        "a destroyed heart empties the shown ring");

    CEGUI::OgreRenderer::destroySystem();
    Ogre::MaterialManager::getSingleton().removeListener(&listener);
    shaders->removeSceneManager(scene);
    Ogre::RTShader::ShaderGenerator::destroy();
    // The Ogre root is not deleted: its teardown after the CEGUI renderer crashes in this bare setup,
    // and the process ends here anyway
    }
    catch(const std::exception& e)
    {
        std::cout << "EXCEPTION " << e.what() << '\n';
        return 1;
    }
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''
# Style of the fixture itself (the extracted production code is compiled as it is)
assert re.search(r'\bauto\b', probe) is None and '[&]' not in probe
probe = probe.replace('RULES_HEADER', rules.as_posix()).replace('BADGE_CODE', badge_code).replace('UPDATE', update)
assert '.loadFromMemory(' not in update and '.blitFromMemory(' in update, 'the update must not replace the texture the badge shows'
print('STATIC OK: updateHeartBadge writes into the existing texture')
for path in (Path(__file__), repo / 'source/render/Gui.cpp'):
    assert not [b for b in path.read_bytes() if b < 32 and b not in (9, 10, 13)], path

with tempfile.TemporaryDirectory(prefix='odp-heart-badge-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    include = deps / 'install/include'
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/W1', '/I', str(include / 'OGRE'),
                    '/I', str(include / 'OGRE/RTShaderSystem'), '/I', str(include / 'cegui-0'), 'check.cpp', '/Fecheck.exe', '/link',
                    '/LIBPATH:' + str(deps / 'install/lib'), 'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib', 'CEGUIBase-0.lib',
                    'CEGUIOgreRenderer-0.lib'],
                   cwd=work, check=True)
    environment = dict(os.environ)
    environment['PATH'] = str(deps / 'install/bin') + os.pathsep + environment.get('PATH', '')
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / 'check.exe'), str(deps / 'install').replace('\\', '/')], cwd=work, check=True,
                           env=environment)
            break
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)
