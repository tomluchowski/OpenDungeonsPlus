"""Compute the tile culling rays of the defeat camera with the real Ogre library, without a game.

After the camera cut of the defeat sequence the game log showed, every frame,
"I didn't find the intersection point for 0th ray" and "... 1th ray" from CullingManager.cpp. The cut used a
pitch of 68 degrees; with the game camera's vertical field of view of 45 degrees the upper edge of the view
then points 90.5 degrees away from straight down, above the horizon, so the two upper corner rays never reach
the floor and the culling kept stale corners.

This check links the installed OgreMain library, builds the camera the way CameraManager does (camera node,
child node, camera near clip 0.02, far clip 300, default field of view), and runs the real code cut out of the
production sources by signature: CameraManager::resetCamera, getCameraViewTarget and getGroundOffset,
GameMode::cutCameraToHeart with the real DefeatSequenceSettings, and CullingManager::computeIntersectionPoints
with its ground plane.
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
camera_manager = (repo / 'source/camera/CameraManager.cpp').read_text()
camera_header = (repo / 'source/camera/CameraManager.h').read_text()
culling = (repo / 'source/camera/CullingManager.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


ground_plane = re.search(r'^static const Ogre::Plane GROUND_PLANE\(.*\);$', culling, re.MULTILINE)
assert ground_plane
assert 'createCamera("RTS", 0.02, 300.0);' in camera_manager

probe = r'''
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <Ogre.h>
#include "modes/DefeatSequence.h"

int gRayErrors = 0;
std::vector<std::string> gErrors;
#define OD_LOG_ERR(x) do {++gRayErrors;gErrors.push_back(x);} while(0)
namespace Helper {template<typename T> std::string toString(T v){return std::to_string(v);}}

@@GROUND@@

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

class GameMode
{
public:
    void cutCameraToHeart(const Ogre::Vector3& heartPosition);
};
@@CUT@@

class CullingManager
{
public:
    bool computeIntersectionPoints(Ogre::Camera* camera, std::vector<Ogre::Vector3>& ogreVectors);
};
@@INTERSECT@@

int gChecks = 0, gFailures = 0;
void check(bool ok, const std::string& msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}

// Sign of the z component of the cross product (b - a) x (p - a)
float side(const Ogre::Vector3& a, const Ogre::Vector3& b, const Ogre::Vector3& p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// True if p lies inside the quadrilateral of the four floor corners (corner order of Ogre: top right,
// top left, bottom left, bottom right)
bool insideCorners(const std::vector<Ogre::Vector3>& corners, const Ogre::Vector3& p)
{
    bool positive = false;
    bool negative = false;
    for(int i = 0; i < 4; ++i)
    {
        const float s = side(corners[i], corners[(i + 1) % 4], p);
        positive = positive || s > 0.0f;
        negative = negative || s < 0.0f;
    }
    return !(positive && negative);
}

// Floor corners seen by the camera, through the real CullingManager code. Returns the number of rays
// that found no intersection.
int floorCorners(CameraManager& cameraManager, std::vector<Ogre::Vector3>& corners)
{
    // The scene graph update every rendered frame does (SceneManager::_updateSceneGraph)
    cameraManager.getActiveCamera()->getSceneManager()->getRootSceneNode()->_update(true, false);
    CullingManager culling;
    corners.assign(4, Ogre::Vector3(-1000.0f, -1000.0f, -1000.0f));
    const int before = gRayErrors;
    culling.computeIntersectionPoints(cameraManager.getActiveCamera(), corners);
    return gRayErrors - before;
}

int main()
{
    Ogre::Root* root = new Ogre::Root("", "", "ogre-check.log");
    Ogre::SceneManager* sceneManager = root->createSceneManager();
    // As CameraManager::createCamera("RTS", 0.02, 300.0) and createCameraNode("RTS")
    Ogre::Camera* camera = sceneManager->createCamera("RTS");
    camera->setNearClipDistance(0.02f);
    camera->setFarClipDistance(300.0f);
    Ogre::SceneNode* node = sceneManager->getRootSceneNode()->createChildSceneNode("RTS_node");
    Ogre::SceneNode* node2 = node->createChildSceneNode("RTS_node2");
    node2->attachObject(camera);
    CameraManager cameraManager(camera, node);
    ODFrameListener::getSingleton().mCameraManager = &cameraManager;
    GameMode mode;

    check(std::fabs(camera->getFOVy().valueDegrees() - 45.0f) < 0.001f, "the game camera has Ogre's default vertical field of view of 45 degrees");

    const float aspects[5] = {4.0f / 3.0f, 16.0f / 10.0f, 16.0f / 9.0f, 21.0f / 9.0f, 32.0f / 9.0f};
    const Ogre::Vector3 hearts[3] = {Ogre::Vector3(58.0f, 102.0f, 0.0f), Ogre::Vector3(3.0f, 3.0f, 0.0f), Ogre::Vector3(120.0f, 7.0f, 0.0f)};
    for(int a = 0; a < 5; ++a)
    {
        camera->setAspectRatio(aspects[a]);
        for(int h = 0; h < 3; ++h)
        {
            const std::string where = " (aspect " + std::to_string(aspects[a]) + ", heart " + std::to_string(h) + ")";
            // Some game view before the cut, far away from the heart
            cameraManager.resetCamera(Ogre::Vector3(20.0f, 20.0f, 10.0f), Ogre::Vector3(25.0f, 0.0f, 0.0f));
            std::vector<Ogre::Vector3> corners;
            check(floorCorners(cameraManager, corners) == 0, "the game view before the cut reaches the floor" + where);

            mode.cutCameraToHeart(hearts[h]);
            const int misses = floorCorners(cameraManager, corners);
            check(misses == 0, "after the cut every corner ray of the culling reaches the floor" + where);
            bool finite = true;
            float farthest = 0.0f;
            for(int i = 0; i < 4; ++i)
            {
                finite = finite && std::fabs(corners[i].z) < 0.001f && corners[i].x > -999.0f;
                const float distance = std::sqrt((corners[i].x - hearts[h].x) * (corners[i].x - hearts[h].x)
                    + (corners[i].y - hearts[h].y) * (corners[i].y - hearts[h].y));
                if(distance > farthest)
                    farthest = distance;
            }
            check(finite, "the four culling corners are fresh points on the floor" + where);
            check(farthest < 40.0f, "the culled area stays within 40 tiles of the heart" + where);
            check(insideCorners(corners, hearts[h]), "the heart lies inside the culled area" + where);
            const Ogre::Vector3 target = cameraManager.getCameraViewTarget();
            check(std::fabs(target.x - hearts[h].x) < 0.01f && std::fabs(target.y - hearts[h].y) < 0.01f, "the camera looks at the heart" + where);
            const Ogre::Vector3 position = node->getPosition();
            check(std::fabs(position.z - DefeatSequenceSettings::CAMERA_HEIGHT) < 0.001f, "the camera is at the cut height" + where);
            const float pitch = std::acos(-camera->getDerivedDirection().z) * 180.0f / 3.14159265f;
            check(std::fabs(pitch - DefeatSequenceSettings::CAMERA_PITCH) < 0.01f, "the camera has the cut pitch" + where);
        }
    }
    check(DefeatSequenceSettings::CAMERA_PITCH + 22.5f < 90.0f - 5.0f, "the pitch leaves at least 5 degrees between the upper view edge and the horizon");
    check(DefeatSequenceSettings::CAMERA_PITCH > 25.0f && DefeatSequenceSettings::CAMERA_HEIGHT < 3.0f, "the cut is lower and more oblique than the game camera (pitch 25, height 3 to 16)");

    // The pose before the fix, as in the game log: the two upper rays are lost
    {
        camera->setAspectRatio(16.0f / 10.0f);
        cameraManager.resetCamera(Ogre::Vector3(58.0f, 102.0f, 2.0f), Ogre::Vector3(68.0f, 0.0f, 0.0f));
        std::vector<Ogre::Vector3> corners;
        gErrors.clear();
        const int misses = floorCorners(cameraManager, corners);
        check(misses == 2 && gErrors.size() == 2 && gErrors[0].find("0th ray") != std::string::npos
            && gErrors[1].find("1th ray") != std::string::npos, "reproduced: pitch 68 loses the 0th and 1th ray, as in the log");
    }

    delete root;
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''

for marker, text in (
        ('@@GROUND@@', ground_plane.group(0)),
        ('@@RESET@@', function(camera_manager, 'void CameraManager::resetCamera(const Ogre::Vector3& position, const Ogre::Vector3& rotation)')),
        ('@@TARGET@@', function(camera_manager, 'Ogre::Vector3 CameraManager::getCameraViewTarget(')),
        ('@@OFFSET@@', function(camera_manager, 'Ogre::Vector3 CameraManager::getGroundOffset(')),
        ('@@CUT@@', function(game_mode, 'void GameMode::cutCameraToHeart(')),
        ('@@INTERSECT@@', function(culling, 'bool CullingManager::computeIntersectionPoints('))):
    assert marker in probe, marker
    probe = probe.replace(marker, text)

# Static wiring checks on the production sources.
start = function(game_mode, 'void GameMode::startDefeatSequence(')
assert 'cutCameraToHeart(mDefeatHeartPosition);' in start
assert 'inline Ogre::SceneNode* getActiveCameraNode() const' in camera_header
assert 'mMainCullingManager->computeIntersectionPoints(cam, mCameraTilesIntersections);' in \
    function((repo / 'source/modes/GameEditorModeBase.cpp').read_text(), 'void GameEditorModeBase::onFrameStarted(')
print('WIRING OK: the defeat start cuts the camera, the frame update computes the culling rays of the active camera')
assert re.search(r'\bauto\b', probe) is None
print('STYLE OK: no auto in the fixture')
for path in (Path(__file__), repo / 'source/modes/DefeatSequence.h'):
    bad = [b for b in path.read_bytes() if b < 32 and b not in (9, 10, 13)]
    assert not bad, path
print('BYTES OK: no stray control characters')

with tempfile.TemporaryDirectory(prefix='odp-defeat-camera-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/W1', '/I', str(deps / 'install/include/OGRE'),
                    '/I', str(repo / 'source'), 'check.cpp', '/Fecheck.exe', '/link',
                    '/LIBPATH:' + str(deps / 'install/lib'), 'OgreMain.lib'],
                   cwd=work, check=True)
    environment = dict(os.environ)
    environment['PATH'] = str(deps / 'install/bin') + os.pathsep + environment.get('PATH', '')
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / 'check.exe')], cwd=work, check=True, env=environment)
            break
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)
