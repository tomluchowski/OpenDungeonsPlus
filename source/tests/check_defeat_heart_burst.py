"""Run the heart burst of the defeat sequence with the real Ogre library in a hidden window and look at the pictures.

Two parts:
1. DefeatHeartBurst.h is compiled as it is and its pure functions are checked: shake, pulse and glow of the heart
   copy, the burst time, the rubble layout (count, bounds inside the 3 by 3 platform, determinism, finite values)
   and the settle animation of every piece.
2. The production driver (GameMode::updateDefeatSequence, startDefeatBurst, startDefeatSwirl, stopDefeatEffects,
   onDefeatSequenceFinished, hideInterfaceForDefeat, cutCameraToHeart) and the production render functions
   (RenderManager::rrCreateFreeParticleEffect ... rrDestroyDefeatRubble, CameraManager::resetCamera and its
   helpers) are cut out of the sources by signature and compiled with small mocks for CEGUI, the game map and
   the singletons. The scene is a floor of temple and claimed tiles with the game's ambient light; the models,
   materials and particle scripts come from the repository. A whole sequence runs on a simulated clock at 30
   frames per second; pictures are taken at chosen times and written to build/defeat-burst-preview/. The
   checks count changed, warm and green pixels against a picture of the empty floor, and check that nothing
   the sequence created is left in the scene or in the resource managers after the end and after an early stop
   (the destructor path).
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
deps = Path(os.environ.get('OD_DEPS_ROOT', str(Path.home() / 'od-deps')))
game_mode = (repo / 'source/modes/GameMode.cpp').read_text()
render_manager = (repo / 'source/render/RenderManager.cpp').read_text()
camera_manager = (repo / 'source/camera/CameraManager.cpp').read_text()
burst_header_path = repo / 'source/modes/DefeatHeartBurst.h'


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def between(text, first, last):
    start = text.index(first)
    return text[start:text.index(last, start)]


constants = '\n'.join(re.findall(r'^const (?:std::string|size_t) DEFEAT_\w+.*;$', game_mode, re.MULTILINE))
assert 'DEFEAT_BURST_EFFECT_NAMES' in constants and 'DEFEAT_SWIRL_EFFECT_NAME' in constants

pure = r'''
#include <cmath>
#include <iostream>
#include <vector>
#include "modes/DefeatHeartBurst.h"

int gChecks = 0, gFailures = 0;
void check(bool ok, const char* msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}
bool finite(float v) {return std::isfinite(v);}

int main()
{
    namespace S = DefeatHeartBurstSettings;
    namespace B = DefeatHeartBurst;
    check(B::isHeartShownAt(0.0f) && B::isHeartShownAt(0.79f) && !B::isHeartShownAt(0.8f) && !B::isHeartShownAt(-0.1f), "the copy of the heart is shown from 0 until the burst at 0.8 s");
    check(!B::isBurstDueAt(0.79f) && B::isBurstDueAt(0.8f) && B::isBurstDueAt(30.0f), "the burst is due from 0.8 s on");
    float maxShake = 0.0f, maxScale = 1.0f, minScale = 1.0f, maxGlow = 0.0f, minGlow = 1.0f, lateShake = 0.0f;
    for(int i = 0; i < 800; ++i)
    {
        const float t = 0.001f * static_cast<float>(i);
        const float shake = std::sqrt(B::heartShakeXAt(t) * B::heartShakeXAt(t) + B::heartShakeYAt(t) * B::heartShakeYAt(t));
        if(shake > maxShake) maxShake = shake;
        if(t > 0.6f && shake > lateShake) lateShake = shake;
        if(B::heartPulseScaleAt(t) > maxScale) maxScale = B::heartPulseScaleAt(t);
        if(B::heartPulseScaleAt(t) < minScale) minScale = B::heartPulseScaleAt(t);
        if(B::heartGlowAt(t) > maxGlow) maxGlow = B::heartGlowAt(t);
        if(B::heartGlowAt(t) < minGlow) minGlow = B::heartGlowAt(t);
    }
    check(maxShake <= S::SHAKE_AMPLITUDE * 1.4143f && lateShake > 0.5f * S::SHAKE_AMPLITUDE, "the heart shakes, more and more, never more than the amplitude");
    check(minScale >= 1.0f && maxScale <= 1.0f + S::PULSE_AMPLITUDE + 0.0001f && maxScale > 1.0f + 0.5f * S::PULSE_AMPLITUDE, "the pulse stays between 1 and 1 + amplitude and reaches it");
    check(minGlow >= 0.0f && maxGlow <= 1.0f && maxGlow > 0.8f && B::heartGlowAt(0.0f) == 0.0f, "the glow runs from 0 (normal look) and flickers up to nearly 1");
    check(B::heartShakeXAt(0.0f) == 0.0f && B::heartShakeXAt(0.8f) == 0.0f && B::heartPulseScaleAt(5.0f) == 1.0f && B::heartGlowAt(5.0f) == 0.0f, "no shake, pulse or glow at the start or after the burst");

    const std::vector<DefeatRubblePiece> pieces = B::buildRubbleLayout();
    const std::vector<DefeatRubblePiece> again = B::buildRubbleLayout();
    check(pieces.size() == S::RUBBLE_PIECES && pieces.size() >= 12, "the layout has the configured number of pieces");
    bool same = again.size() == pieces.size();
    bool allFinite = true, inside = true, heights = true, scales = true, delays = true, tilts = true;
    float farthest = 0.0f, nearest = 10.0f;
    for(size_t i = 0; i < pieces.size(); ++i)
    {
        const DefeatRubblePiece& p = pieces[i];
        same = same && p.restX == again[i].restX && p.restY == again[i].restY && p.turn == again[i].turn && p.scaleX == again[i].scaleX;
        const float values[15] = {p.startX, p.startY, p.startZ, p.restX, p.restY, p.restZ, p.tiltX, p.tiltY, p.turn, p.spin, p.arc, p.delay, p.scaleX, p.scaleY, p.scaleZ};
        for(int v = 0; v < 15; ++v) allFinite = allFinite && finite(values[v]);
        const float radius = std::sqrt(p.restX * p.restX + p.restY * p.restY);
        if(radius > farthest) farthest = radius;
        if(radius < nearest) nearest = radius;
        // The shard mesh reaches 0.36 from its centre, scaled by at most 1.4: the whole piece stays on the platform
        const float reach = 0.36f * (p.scaleX > p.scaleY ? p.scaleX : p.scaleY);
        inside = inside && radius <= S::RUBBLE_RADIUS && std::fabs(p.restX) + reach < 1.5f && std::fabs(p.restY) + reach < 1.5f;
        heights = heights && p.restZ >= 0.03f && p.restZ <= 0.3f && p.startZ >= 0.6f && p.startZ <= 1.6f && p.arc >= 0.2f && p.arc <= 0.6f;
        scales = scales && p.scaleX >= 0.7f && p.scaleX <= 1.4f && p.scaleY >= 0.6f && p.scaleY <= 1.2f && p.scaleZ >= 0.8f && p.scaleZ <= 1.6f;
        delays = delays && p.delay >= 0.0f && p.delay <= S::RUBBLE_MAX_DELAY;
        tilts = tilts && std::fabs(p.tiltX) <= 40.0f && std::fabs(p.tiltY) <= 40.0f && p.turn >= 0.0f && p.turn < 360.0f && std::fabs(p.spin) >= 120.0f && std::fabs(p.spin) <= 360.0f;
    }
    check(same, "the layout is the same every time (deterministic)");
    check(allFinite, "every value of the layout is finite");
    check(inside, "every piece rests inside the rubble radius and completely on the 3 by 3 platform");
    check(farthest > 0.6f && nearest < 0.25f, "the pile covers the middle and spreads over the platform centre");
    check(heights && scales && delays && tilts, "heights, sizes, delays and angles stay in their ranges");
    int neighbours = 0;
    for(size_t i = 0; i < pieces.size(); ++i)
        for(size_t j = i + 1; j < pieces.size(); ++j)
        {
            const float dx = pieces[i].restX - pieces[j].restX, dy = pieces[i].restY - pieces[j].restY;
            if(std::sqrt(dx * dx + dy * dy) < 0.05f) ++neighbours;
        }
    check(neighbours == 0, "no two pieces rest on the same spot");

    bool settled = true, waiting = true, bounded = true, monotonicProgress = true;
    for(size_t i = 0; i < pieces.size(); ++i)
    {
        const DefeatRubblePiece& p = pieces[i];
        const DefeatRubblePose atBurst = B::rubblePoseAt(p, 0.0f);
        waiting = waiting && atBurst.x == p.startX && atBurst.y == p.startY && atBurst.z == p.startZ;
        const DefeatRubblePose rest = B::rubblePoseAt(p, S::RUBBLE_SETTLE_DURATION);
        const DefeatRubblePose later = B::rubblePoseAt(p, 20.0f);
        settled = settled && rest.x == p.restX && rest.y == p.restY && std::fabs(rest.z - p.restZ) < 0.0001f && rest.tiltX == p.tiltX
            && rest.turn == p.turn && later.z == rest.z && later.x == rest.x;
        float last = -1.0f;
        for(int step = 0; step <= 100; ++step)
        {
            const float s = 0.012f * static_cast<float>(step);
            const DefeatRubblePose pose = B::rubblePoseAt(p, s);
            bounded = bounded && finite(pose.x) && finite(pose.z) && pose.z >= p.restZ - 0.0001f && pose.z <= p.startZ + p.arc + 0.0001f
                && std::fabs(pose.x) <= std::fabs(p.restX) + 0.0001f;
            const float progress = B::rubbleFallProgress(p, s);
            monotonicProgress = monotonicProgress && progress >= last && progress >= 0.0f && progress <= 1.0f;
            last = progress;
        }
    }
    check(waiting, "at the burst every piece is at its start in the heart's middle");
    check(settled, "every piece lies still at its rest position 1 s after the burst and stays there");
    check(bounded, "while flying a piece stays between its rest height and its start height plus its arc, never beyond its rest distance");
    check(monotonicProgress, "the fall progress only grows, from 0 to 1");
    std::cout << "PURE CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''

render = r'''
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <Ogre.h>
#include <OgreManualObject.h>
#include <RTShaderSystem/OgreShaderGenerator.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include "modes/DefeatSequence.h"
#include "modes/DefeatHeartBurst.h"

std::vector<std::string> gErrors;
std::vector<std::string> gInfos;
#define OD_LOG_ERR(x) gErrors.push_back(x)
#define OD_LOG_INF(x) gInfos.push_back(x)
namespace CullingType {const uint32_t SHOW_ALL = 0x03;}

class RenderManager
{
public:
    Ogre::SceneManager* mSceneManager = nullptr;
    static RenderManager& getSingleton() {static RenderManager manager;return manager;}
    template<typename Manager> bool removeIfExists(std::string, std::string);
    void rrCreateFreeParticleEffect(const std::string& effectName, const std::string& particleScript,
        const Ogre::Vector3& position, const Ogre::ColourValue* colour);
    void rrMoveFreeParticleEffect(const std::string& effectName, const Ogre::Vector3& position);
    void rrDestroyFreeParticleEffect(const std::string& effectName);
    void rrCreateDefeatHeart(const Ogre::Vector3& position);
    void rrUpdateDefeatHeart(const Ogre::Vector3& position, Ogre::Real scale, Ogre::Real glow);
    void rrDestroyDefeatHeart();
    void rrCreateDefeatRubble(size_t count);
    void rrMoveDefeatRubblePiece(size_t index, const Ogre::Vector3& position, const Ogre::Vector3& rotation,
        const Ogre::Vector3& scale);
    void rrDestroyDefeatRubble();
};
@@REMOVE@@
@@RENDER@@

class CameraManager
{
public:
    enum Direction {fullStop};
    CameraManager(Ogre::Camera* camera, Ogre::SceneNode* node) : mActiveCamera(camera), mActiveCameraNode(node) {}
    void move(Direction, double = 0.0) {}
    Ogre::SceneNode* getActiveCameraNode() const {return mActiveCameraNode;}
    Ogre::Camera* getActiveCamera() const {return mActiveCamera;}
    void resetCamera(const Ogre::Vector3& position, const Ogre::Vector3& rotation);
    Ogre::Vector3 getCameraViewTarget() const;
    Ogre::Vector3 getGroundOffset(Ogre::Real height) const;
    Ogre::Camera* mActiveCamera;
    Ogre::SceneNode* mActiveCameraNode;
};
@@RESET@@
@@TARGET@@
@@OFFSET@@

struct ODFrameListener
{
    CameraManager* mCameraManager;
    static ODFrameListener& getSingleton() {static ODFrameListener listener;return listener;}
    CameraManager* getCameraManager() {return mCameraManager;}
};

namespace CEGUI
{
struct Window
{
    bool visible;float alpha;std::string text;std::vector<Window*> children;
    Window():visible(true),alpha(-1.0f){}
    size_t getChildCount(){return children.size();}
    Window* getChildAtIdx(size_t i){return children[i];}
    bool isVisible(){return visible;}
    void hide(){visible=false;}
    void setAlpha(float a){alpha=a;}
    void setText(const std::string& t){text=t;}
};
}
struct Seat {Ogre::ColourValue colour;const Ogre::ColourValue& getColorValue() const {return colour;}};
struct GameMap
{
    Seat seat;
    Seat* getSeatById(int id){return id==1?&seat:nullptr;}
};
@@CONSTANTS@@

class GameMode
{
public:
    GameMode():mRootWindow(&mRoot),mGameMap(&mMap){}
    void cutCameraToHeart(const Ogre::Vector3& heartPosition);
    void updateDefeatSequence(std::chrono::steady_clock::time_point now);
    void startDefeatSwirl();
    void startDefeatBurst(float time);
    void stopDefeatEffects();
    void hideInterfaceForDefeat();
    void onDefeatSequenceFinished();
    void showDefeatDebriefing() {++mDebriefingShown;}
    int mDebriefingShown = 0;
    DefeatSequence mDefeatSequence;
    Ogre::Vector3 mDefeatHeartPosition;
    Ogre::Vector3 mDefeatSwirlDirection;
    bool mDefeatExplosionEffectActive = false;
    bool mDefeatSwirlEffectActive = false;
    bool mDefeatSwirlDone = false;
    bool mDefeatHeartShown = false;
    bool mDefeatBurstDone = false;
    bool mDefeatRubbleShown = false;
    std::vector<DefeatRubblePiece> mDefeatRubble;
    CEGUI::Window* mDefeatTint = nullptr;
    CEGUI::Window* mDefeatFade = nullptr;
    CEGUI::Window* mDefeatSubtitle = nullptr;
    CEGUI::Window* mDefeatCameraMarker = nullptr;
    CEGUI::Window* mDefeatDebriefing = nullptr;
    CEGUI::Window mRoot, mTint, mFade, mSubtitle, mMarker;
    CEGUI::Window* mRootWindow;
    GameMap mMap;
    GameMap* mGameMap;
};
@@CUT@@
@@UPDATE@@
@@SWIRL@@
@@BURST@@
@@STOP@@
@@HIDE@@
@@FINISHED@@

int gChecks = 0, gFailures = 0;
void check(bool ok, const std::string& msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}

const int WIDTH = 800;
const int HEIGHT = 500;
const float HEART_X = 58.0f;
const float HEART_Y = 102.0f;
Ogre::Root* gRoot = nullptr;
Ogre::RenderWindow* gWindow = nullptr;
std::string gOut;

std::vector<unsigned char> capture(const std::string& file)
{
    gWindow->update(false);
    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 4);
    gWindow->copyContentsToMemory(Ogre::PixelBox(WIDTH, HEIGHT, 1, Ogre::PF_BYTE_RGBA, pixels.data()), Ogre::RenderTarget::FB_BACK);
    Ogre::Image image;
    image.loadDynamicImage(pixels.data(), WIDTH, HEIGHT, 1, Ogre::PF_BYTE_RGBA, false);
    image.save(gOut + "/" + file);
    return pixels;
}

struct Counts {int changed; int warm; int green; long long redExcess;};

// Pixels that differ from the empty floor: all of them, warm bright ones (fire) and green ones (sparks, swirl)
Counts compare(const std::vector<unsigned char>& picture, const std::vector<unsigned char>& floor)
{
    Counts counts = {0, 0, 0, 0};
    for(size_t p = 0; p < picture.size(); p += 4)
    {
        const int r = picture[p], g = picture[p + 1], b = picture[p + 2];
        const int difference = std::abs(r - floor[p]) + std::abs(g - floor[p + 1]) + std::abs(b - floor[p + 2]);
        if(difference <= 30)
            continue;
        ++counts.changed;
        if(r >= 170 && r > b + 60 && g >= 60)
            ++counts.warm;
        if(g >= 110 && g > r + 30)
            ++counts.green;
        counts.redExcess += r - (g + b) / 2;
    }
    return counts;
}

std::string describe(const std::string& name, const Counts& c)
{
    return name + " changed=" + std::to_string(c.changed) + " warm=" + std::to_string(c.warm) + " green=" + std::to_string(c.green)
        + " redExcess=" + std::to_string(c.redExcess);
}

std::chrono::steady_clock::duration seconds(double value)
{
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(value));
}

bool hasDefeatObjects(Ogre::SceneManager* scene)
{
    const char* types[3] = {"Entity", "ParticleSystem", "ManualObject"};
    for(int t = 0; t < 3; ++t)
    {
        const Ogre::SceneManager::MovableObjectMap& objects = scene->getMovableObjects(types[t]);
        for(Ogre::SceneManager::MovableObjectMap::const_iterator it = objects.begin(); it != objects.end(); ++it)
            if(it->first.find("Defeat") != std::string::npos)
                return true;
    }
    Ogre::SceneNode* root = scene->getRootSceneNode();
    for(unsigned short i = 0; i < root->numChildren(); ++i)
        if(root->getChild(i)->getName().find("Defeat") != std::string::npos)
            return true;
    return false;
}

bool hasDefeatResources()
{
    const char* clones[3] = {"DefeatHeart_Stacheln", "DefeatHeart_Sphere", "DefeatHeart_Sphere2"};
    for(int i = 0; i < 3; ++i)
        if(Ogre::MaterialManager::getSingleton().resourceExists(clones[i], "Graphics"))
            return true;
    return Ogre::MeshManager::getSingleton().resourceExists("DefeatHeartShard", "Graphics");
}

void beginSequence(GameMode& mode, std::chrono::steady_clock::time_point start)
{
    // As GameMode::startDefeatSequence: windows, camera cut, heart copy, first update
    mode.mRoot.children.clear();
    mode.mRoot.children.push_back(&mode.mTint);mode.mRoot.children.push_back(&mode.mFade);
    mode.mRoot.children.push_back(&mode.mSubtitle);mode.mRoot.children.push_back(&mode.mMarker);
    mode.mDefeatTint = &mode.mTint;mode.mDefeatFade = &mode.mFade;mode.mDefeatSubtitle = &mode.mSubtitle;mode.mDefeatCameraMarker = &mode.mMarker;
    mode.mMap.seat.colour = Ogre::ColourValue(0.15f, 0.85f, 0.2f, 0.5f);
    mode.mDefeatSequence.start(1, static_cast<int32_t>(HEART_X), static_cast<int32_t>(HEART_Y), start);
    mode.mDefeatHeartPosition = Ogre::Vector3(HEART_X, HEART_Y, 0.0f);
    mode.cutCameraToHeart(mode.mDefeatHeartPosition);
    RenderManager::getSingleton().rrCreateDefeatHeart(mode.mDefeatHeartPosition);
    mode.mDefeatHeartShown = true;
    mode.updateDefeatSequence(start);
}

int main(int argc, char** argv)
{
    try
    {
    const std::string repo = argv[1];
    const std::string prefix = argv[2];
    gOut = argv[3];
    Ogre::LogManager* logs = new Ogre::LogManager();
    logs->createLog(gOut + "/heart-burst-Ogre.log", true, false, false);
    gRoot = new Ogre::Root("", "", "");
    gRoot->loadPlugin(prefix + "/bin/RenderSystem_GL3Plus");
    gRoot->loadPlugin(prefix + "/bin/Codec_STBI");
    gRoot->loadPlugin(prefix + "/bin/Plugin_ParticleFX");
    gRoot->setRenderSystem(gRoot->getAvailableRenderers().front());
    gRoot->initialise(false);
    Ogre::NameValuePairList options;
    options["hidden"] = "true";
    options["vsync"] = "false";
    gWindow = gRoot->createRenderWindow("DefeatHeartBurst", WIDTH, HEIGHT, false, &options);
    Ogre::ResourceGroupManager& groups = Ogre::ResourceGroupManager::getSingleton();
    groups.createResourceGroup("Graphics");
    const char* dirs[5] = {"models", "materials/textures", "materials/scripts", "shaders", "particles"};
    for(int i = 0; i < 5; ++i)
        groups.addResourceLocation(repo + "/" + dirs[i], "FileSystem", "Graphics", true);
    groups.addResourceLocation(prefix + "/Media/Main", "FileSystem", "OgreInternal");
    groups.addResourceLocation(prefix + "/Media/RTShaderLib/GLSL", "FileSystem", "OgreInternal");
    Ogre::RTShader::ShaderGenerator::initialize();
    Ogre::RTShader::ShaderGenerator* generator = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    groups.initialiseAllResourceGroups();
    OgreBites::SGTechniqueResolverListener listener(generator);
    Ogre::MaterialManager::getSingleton().addListener(&listener);

    const char* scripts[5] = {"HeartExplosion", "HeartExplosionFlash", "HeartExplosionSparks", "HeartExplosionSmoke", "DefeatSwirl"};
    for(int i = 0; i < 5; ++i)
    {
        Ogre::ParticleSystem* particleTemplate = Ogre::ParticleSystemManager::getSingleton().getTemplate(scripts[i]);
        check(particleTemplate != nullptr, std::string("the particle script is parsed: ") + scripts[i]);
        if(particleTemplate != nullptr)
            check(Ogre::MaterialManager::getSingleton().resourceExists(particleTemplate->getMaterialName(), "Graphics"),
                std::string("the material of the script exists: ") + scripts[i]);
    }

    Ogre::SceneManager* scene = gRoot->createSceneManager();
    generator->addSceneManager(scene);
    // As RenderManager: the game's ambient light and the non-visible timeout of particle systems
    scene->setAmbientLight(Ogre::ColourValue(0.3f, 0.3f, 0.3f));
    Ogre::ParticleSystem::setDefaultNonVisibleUpdateTimeout(5);
    RenderManager::getSingleton().mSceneManager = scene;
    Ogre::Light* light = scene->createLight("KeeperLight");
    light->setType(Ogre::Light::LT_POINT);
    light->setDiffuseColour(Ogre::ColourValue(0.65f, 0.65f, 0.45f));
    light->setAttenuation(500, 1.0f, 0.09f, 0.032f);
    scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(HEART_X, HEART_Y - 2.0f, 4.0f))->attachObject(light);
    for(int x = -5; x <= 5; ++x)
    {
        for(int y = -4; y <= 12; ++y)
        {
            const bool temple = std::abs(x) <= 1 && std::abs(y) <= 1;
            Ogre::Entity* tile = scene->createEntity(temple ? "DungeonTemple.mesh" : "Claimed_gd_1111.mesh");
            scene->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(HEART_X + x, HEART_Y + y, 0.0f))->attachObject(tile);
        }
    }
    // The game camera, as CameraManager::createCamera("RTS", 0.02, 300.0) and createCameraNode("RTS")
    Ogre::Camera* camera = scene->createCamera("RTS");
    camera->setNearClipDistance(0.02f);
    camera->setFarClipDistance(300.0f);
    Ogre::SceneNode* cameraNode = scene->getRootSceneNode()->createChildSceneNode("RTS_node");
    cameraNode->createChildSceneNode("RTS_node2")->attachObject(camera);
    Ogre::Viewport* viewport = gWindow->addViewport(camera);
    viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
    viewport->setBackgroundColour(Ogre::ColourValue(0.0f, 0.0f, 0.0f));
    camera->setAspectRatio(static_cast<Ogre::Real>(WIDTH) / static_cast<Ogre::Real>(HEIGHT));
    CameraManager cameraManager(camera, cameraNode);
    ODFrameListener::getSingleton().mCameraManager = &cameraManager;

    GameMode mode;
    mode.cutCameraToHeart(Ogre::Vector3(HEART_X, HEART_Y, 0.0f));
    gRoot->renderOneFrame(0.0f);
    const std::vector<unsigned char> floor = capture("00-floor.png");
    const size_t entitiesBefore = scene->getMovableObjects("Entity").size();

    // The whole sequence at 30 frames per second on a simulated clock, two updates per frame as in the game
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::time_point() + seconds(1000.0);
    beginSequence(mode, start);
    const float shots[12] = {0.0f, 0.4f, 0.75f, 0.9f, 1.3f, 2.0f, 4.0f, 8.0f, 12.0f, 15.2f, 16.3f, 17.3f};
    std::vector<Counts> counts;
    int frame = 0;
    Ogre::Vector3 shakenPosition = Ogre::Vector3::ZERO;
    Ogre::Real largestScale = 0.0f;
    bool heartBeforeBurst = true, noHeartAfterBurst = true, rubbleAfterBurst = true, noRubbleBeforeBurst = true;
    size_t effectsAtOneSecond = 0;
    for(int shot = 0; shot < 12; ++shot)
    {
        while(mode.mDefeatSequence.getElapsed() + 0.0001f < shots[shot])
        {
            ++frame;
            const std::chrono::steady_clock::time_point now = start + seconds(frame / 30.0);
            mode.updateDefeatSequence(now);
            mode.updateDefeatSequence(now);
            gRoot->renderOneFrame(1.0f / 30.0f);
            const float t = mode.mDefeatSequence.getElapsed();
            const bool heart = scene->hasEntity("DefeatHeart_entity");
            const bool rubble = scene->hasEntity("DefeatRubble_0");
            if(t < DefeatHeartBurstSettings::BURST_TIME)
            {
                heartBeforeBurst = heartBeforeBurst && heart;
                noRubbleBeforeBurst = noRubbleBeforeBurst && !rubble;
                Ogre::SceneNode* node = scene->getSceneNode("DefeatHeart_node");
                if((node->getPosition() - mode.mDefeatHeartPosition).length() > shakenPosition.length())
                    shakenPosition = node->getPosition() - mode.mDefeatHeartPosition;
                if(node->getScale().x > largestScale)
                    largestScale = node->getScale().x;
            }
            else if(t < DefeatSequenceSettings::FINISH)
            {
                noHeartAfterBurst = noHeartAfterBurst && !heart;
                rubbleAfterBurst = rubbleAfterBurst && rubble && scene->hasEntity("DefeatRubble_17") && !scene->hasEntity("DefeatRubble_18");
            }
            if(effectsAtOneSecond == 0 && t >= 1.0f)
                effectsAtOneSecond = scene->getMovableObjects("ParticleSystem").size();
        }
        char name[64];
        std::snprintf(name, sizeof(name), "%02d-t%05.2f.png", shot + 1, static_cast<double>(shots[shot]));
        counts.push_back(compare(capture(name), floor));
        std::cout << describe(name, counts.back()) << '\n';
        if(scene->hasParticleSystem("DefeatSwirl_particle"))
        {
            Ogre::ParticleSystem* swirl = scene->getParticleSystem("DefeatSwirl_particle");
            std::cout << "  swirl particles=" << swirl->getNumParticles() << " box=" << swirl->getWorldBoundingBox(true)
                << " node=" << swirl->getParentSceneNode()->_getDerivedPosition() << '\n';
        }
    }
    check(heartBeforeBurst && noRubbleBeforeBurst, "until 0.8 s the copy of the heart stands on the platform and there is no rubble");
    check(noHeartAfterBurst && rubbleAfterBurst, "from the burst to the end the copy is gone and the 18 rubble pieces lie there");
    check(shakenPosition.length() > 0.02f && shakenPosition.length() < 0.11f && std::fabs(shakenPosition.z) < 0.0001f, "the copy shakes sideways around the heart tile");
    check(largestScale > 1.03f && largestScale <= 1.0801f, "the copy pulses in size");
    check(effectsAtOneSecond == 4, "four burst particle systems run after the burst");
    // 0: 0 s, 1: 0.4 s, 2: 0.75 s, 3: 0.9 s, 4: 1.3 s, 5: 2 s, 6: 4 s, 7: 8 s, 8: 12 s, 9: 15.2 s, 10: 16.3 s, 11: 17.3 s
    const int frameSize = WIDTH * HEIGHT;
    check(counts[0].changed > frameSize / 20, "at the start the copy of the heart is clearly visible (more than 5 % of the picture)");
    check(counts[2].redExcess > counts[0].redExcess + 20000, "just before the burst the heart glows redder than at the start");
    check(counts[3].warm > frameSize / 25, "0.1 s after the burst the flash and the fireballs fill a large part of the picture");
    check(counts[4].warm > frameSize / 100 && counts[5].warm > frameSize / 200, "0.5 and 1.2 s after the burst there are many fireballs");
    check(counts[5].green > 150 || counts[4].green > 150, "green sparks are visible in the first seconds");
    check(counts[6].warm > 150, "3 s after the burst burning fireballs are still visible");
    check(counts[7].warm + counts[7].green > 30, "7 s after the burst embers or sparks are still visible");
    check(counts[5].warm > counts[7].warm && counts[3].warm > counts[6].warm, "the burst dies out over time");
    check(counts[9].changed > 1500 && counts[9].warm < 50, "after the explosion phase the rubble is visible on the platform and the fire is gone");
    check(counts[10].green > 1500 && counts[11].green > 800, "the swirl in the conqueror's green is clearly visible while it travels");
    check(scene->getMovableObjects("ParticleSystem").size() == 1 && scene->hasParticleSystem("DefeatSwirl_particle"), "at 17.3 s only the swirl particle system is left");

    // Run to the end: the finished hook removes everything
    while(!mode.mDefeatSequence.isFinished())
    {
        ++frame;
        mode.updateDefeatSequence(start + seconds(frame / 30.0));
    }
    gRoot->renderOneFrame(1.0f / 30.0f);
    check(mode.mDebriefingShown == 1, "the debriefing opens once at the end");
    check(!hasDefeatObjects(scene) && !hasDefeatResources() && scene->getMovableObjects("Entity").size() == entitiesBefore,
        "at the end no entity, node, particle system, cloned material or shard mesh of the sequence is left");
    check(!mode.mDefeatHeartShown && !mode.mDefeatRubbleShown && !mode.mDefeatExplosionEffectActive && !mode.mDefeatSwirlEffectActive, "at the end all flags say nothing is shown");

    // The destructor path: a new game mode (as in a later game) stopped while the copy stands and while everything runs
    const float stops[2] = {0.4f, 5.0f};
    for(int s = 0; s < 2; ++s)
    {
        GameMode early;
        beginSequence(early, start);
        int f = 0;
        while(early.mDefeatSequence.getElapsed() < stops[s])
        {
            ++f;
            early.updateDefeatSequence(start + seconds(f / 30.0));
            gRoot->renderOneFrame(1.0f / 30.0f);
        }
        check(hasDefeatObjects(scene) && hasDefeatResources(), "a later sequence creates its objects again (the names are free)");
        early.stopDefeatEffects();
        early.stopDefeatEffects();
        gRoot->renderOneFrame(1.0f / 30.0f);
        check(!hasDefeatObjects(scene) && !hasDefeatResources() && scene->getMovableObjects("Entity").size() == entitiesBefore,
            "stopping (destructor path) at " + std::to_string(stops[s]) + " s leaves nothing behind, also when called twice");
    }
    check(gErrors.empty(), "no error was logged");
    for(size_t i = 0; i < gErrors.size(); ++i)
        std::cout << "ERROR " << gErrors[i] << '\n';

    Ogre::MaterialManager::getSingleton().removeListener(&listener);
    generator->removeSceneManager(scene);
    gRoot->destroySceneManager(scene);
    Ogre::RTShader::ShaderGenerator::destroy();
    delete gRoot;
    }
    catch(const std::exception& e)
    {
        std::cout << "EXCEPTION " << e.what() << '\n';
        return 1;
    }
    std::cout << "RENDER CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''

for marker, text in (
        ('@@REMOVE@@', function(render_manager, 'template <typename Manager> bool RenderManager::removeIfExists(')),
        ('@@RENDER@@', between(render_manager, 'void RenderManager::rrCreateFreeParticleEffect(', 'void RenderManager::clearRoomConstructionEffects()')),
        ('@@RESET@@', function(camera_manager, 'void CameraManager::resetCamera(const Ogre::Vector3& position, const Ogre::Vector3& rotation)')),
        ('@@TARGET@@', function(camera_manager, 'Ogre::Vector3 CameraManager::getCameraViewTarget(')),
        ('@@OFFSET@@', function(camera_manager, 'Ogre::Vector3 CameraManager::getGroundOffset(')),
        ('@@CONSTANTS@@', constants),
        ('@@CUT@@', function(game_mode, 'void GameMode::cutCameraToHeart(')),
        ('@@UPDATE@@', function(game_mode, 'void GameMode::updateDefeatSequence(')),
        ('@@SWIRL@@', function(game_mode, 'void GameMode::startDefeatSwirl(')),
        ('@@BURST@@', function(game_mode, 'void GameMode::startDefeatBurst(')),
        ('@@STOP@@', function(game_mode, 'void GameMode::stopDefeatEffects(')),
        ('@@HIDE@@', function(game_mode, 'void GameMode::hideInterfaceForDefeat(')),
        ('@@FINISHED@@', function(game_mode, 'void GameMode::onDefeatSequenceFinished('))):
    assert marker in render, marker
    render = render.replace(marker, text)

# Static wiring checks on the production sources
start = function(game_mode, 'void GameMode::startDefeatSequence(')
assert 'rrCreateDefeatHeart(mDefeatHeartPosition);' in start and 'mDefeatSequence.isHeartKnown()' in start
assert 'rrCreateFreeParticleEffect' not in start, 'the burst effects start at the burst, not at the start'
assert 'stopDefeatEffects();' in function(game_mode, 'GameMode::~GameMode(')
assert 'stopDefeatEffects();' in function(game_mode, 'void GameMode::onDefeatSequenceFinished(')
print('WIRING OK: the start creates the heart copy, the end and the destructor remove every object')
particles = (repo / 'particles/HeartExplosion.particle').read_text()
for system in ('HeartExplosion', 'HeartExplosionFlash', 'HeartExplosionSparks', 'HeartExplosionSmoke'):
    assert 'particle_system ' + system + '\n' in particles, system
assert particles.count('{') == particles.count('}')
for emitter in re.findall(r'emitter \w+\s*\{[^}]*\}', particles):
    assert 'duration' in emitter, 'every burst emitter stops by itself'
# Area emitters default to a size of 100 in every direction; the swirl ring once had no depth and spread its
# particles over 100 units up and down, so almost none were in the picture
for script in ('HeartExplosion.particle', 'DefeatSwirl.particle'):
    text = (repo / 'particles' / script).read_text()
    for emitter in re.findall(r'emitter (?:Box|Ellipsoid|Ring|Cylinder|HollowEllipsoid)\s*\{[^}]*\}', text):
        for size in ('width', 'height', 'depth'):
            assert re.search(r'^\s*' + size + r'\s', emitter, re.MULTILINE), script + ': area emitter without ' + size
materials = (repo / 'materials/scripts/DefeatSequence.material').read_text()
for texture in re.findall(r'texture (\S+)', materials):
    assert (repo / 'materials/textures' / texture).is_file(), texture
print('SCRIPTS OK: four burst systems, every emitter has a duration, the new materials use existing textures')
code = burst_header_path.read_text() + pure + render
assert re.search(r'\bauto\b', code) is None and '[&]' not in code and '[=]' not in code
print('STYLE OK: no auto and no lambda in the new code and in the fixture')
for path in (Path(__file__), burst_header_path, repo / 'particles/HeartExplosion.particle',
             repo / 'materials/scripts/DefeatSequence.material', repo / 'source/modes/GameMode.cpp',
             repo / 'source/modes/GameMode.h', repo / 'source/render/RenderManager.cpp', repo / 'source/render/RenderManager.h'):
    bad = [b for b in path.read_bytes() if b < 32 and b not in (9, 10, 13)]
    assert not bad, path
print('BYTES OK: no stray control characters')

out = repo / 'build' / 'defeat-burst-preview'
out.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ)
environment['PATH'] = str(deps / 'install/bin') + os.pathsep + environment.get('PATH', '')


def run(executable, arguments, work):
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / executable)] + arguments, cwd=work, check=True, env=environment)
            return
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)


with tempfile.TemporaryDirectory(prefix='odp-heart-burst-') as directory:
    work = Path(directory)
    (work / 'pure.cpp').write_text(pure)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/W1', '/I', str(repo / 'source'), 'pure.cpp', '/Fepure.exe'],
                   cwd=work, check=True)
    run('pure.exe', [], work)
    (work / 'render.cpp').write_text(render)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/W1', '/I', str(deps / 'install/include/OGRE'),
                    '/I', str(deps / 'install/include/OGRE/RTShaderSystem'), '/I', str(repo / 'source'),
                    'render.cpp', '/Ferender.exe', '/link', '/LIBPATH:' + str(deps / 'install/lib'),
                    'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib'],
                   cwd=work, check=True)
    run('render.exe', [str(repo).replace('\\', '/'), str(deps / 'install').replace('\\', '/'), str(out).replace('\\', '/')], work)
