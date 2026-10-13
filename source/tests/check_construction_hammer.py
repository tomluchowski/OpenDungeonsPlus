"""Exercise the real hand poses, tool attachments and transitions with Ogre.

Run from the Windows compiler environment; this starts an isolated hidden render
fixture, not a game session. Optional --compile-only prepares a blocked fixture.
"""
import argparse
import os
import re
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-only', action='store_true')
parser.add_argument('--pickaxe-audit', action='store_true')
parser.add_argument('--idle-audit', action='store_true')
args = parser.parse_args()
source = (root / 'source/render/RenderManager.cpp').read_text(encoding='utf-8')
controller = (root / 'source/modes/GameMode.cpp').read_text(encoding='utf-8')
building = re.search(r'const bool building = (.*?);', controller, re.S)[1]
building = building.replace('mGameMap->getGamePaused()', 'paused').replace(
    'mPlayerSelection.getCurrentAction()', 'action')


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


helpers = '\n'.join(function(name) for name in [
    'void createKeeperHandPoses(', 'void createKeeperHandDigAnimation(',
    'void createKeeperHandBuildAnimation(', 'Ogre::Vector3 getHammerStrikePoint(',
    'void alignKeeperHandPointer(',
    'void addPickaxePrism('])
helpers += '\n' + source[source.index('const char* const IDLE_HAND_ANIMATIONS[]'):source.index('void addPickaxePrism(')]
methods = '\n'.join(function(name) for name in [
    'void RenderManager::rrSetHandPose(', 'void RenderManager::rrPlayDigAnimation(',
    'void RenderManager::rrPlayBuildAnimation(',
    'bool RenderManager::rrIsIdleHandAnimationPlaying(', 'bool RenderManager::rrPlayIdleHandAnimation(',
    'void RenderManager::rrCancelIdleHandAnimation(',
    'Ogre::AnimationState* RenderManager::setEntityAnimation('])
hand_update = function('void RenderManager::updateRenderAnimations(').split('    for(auto it =')[0]
hand_update = hand_update.replace('void RenderManager::updateRenderAnimations(', 'void RenderManager::updateHand(') + '}\n'
methods += '\n' + hand_update
start = source.index('    mHandPickaxe = mSceneManager->createManualObject(')
end = source.index('    mHandHammer->setVisible(false);', start) + len('    mHandHammer->setVisible(false);')
factory = source[start:end]
probe = r'''
#include <Ogre.h>
#include <OgreBone.h>
#include <OgreTagPoint.h>
#include <OgreKeyFrame.h>
#include <OgreSubMesh.h>
#include <limits>
#include <OgreRTShaderSystem.h>
#include <Bites/OgreSGTechniqueResolverListener.h>
#include <iostream>
#define OD_LOG_ERR(message) ((void)0)
const Ogre::uint8 OD_RENDER_QUEUE_ID_GUI = 101;
namespace Random { unsigned choice=0; unsigned Uint(unsigned low,unsigned high) {
    if(low!=0 || high!=1) throw std::runtime_error("idle random range"); return choice;
} }
HELPERS
struct RenderManager {
    Ogre::SceneManager* mSceneManager = nullptr;
    Ogre::SceneNode* mHeldCreatureGrip = nullptr;
    bool mHeldCreatureDisplayEnabled = false;
    unsigned mHandKeeperHandVisibility = 0;
    Ogre::AnimationState* mHandAnimationState = nullptr;
    Ogre::ManualObject* mHandPickaxe = nullptr;
    Ogre::Entity* mHandHammer = nullptr;
    Ogre::ManualObject* mHandIdleProp = nullptr;
    Ogre::Vector3 mHammerStrikePoint = Ogre::Vector3::ZERO;
    std::string mHandPose = "Idle";
    void rrSetHandPose(bool, bool, bool = false);
    void rrPlayDigAnimation();
    void rrPlayBuildAnimation();
    bool rrIsIdleHandAnimationPlaying() const;
    bool rrPlayIdleHandAnimation();
    void rrCancelIdleHandAnimation();
    void updateHand(float);
    Ogre::AnimationState* setEntityAnimation(Ogre::Entity*, const std::string&, bool);
    void createTools(Ogre::Entity* keeperHandEnt) { FACTORY }
};
METHODS
int checks = 0;
void check(bool value, const char* label) {
    ++checks;
    if(!value) throw std::runtime_error(label);
}
int main() {
    try {
        enum class SelectedAction { buildRoom, buildTrap, selectTile, none, castSpell, sellRoom };
        const auto selectingBuild = [](bool overGui, bool holding, bool paused, bool validTile, SelectedAction action) {
            int value = 0; int* tile = validTile ? &value : nullptr;
            return BUILDING;
        };
        for(bool gui:{false,true})for(bool holding:{false,true})for(bool paused:{false,true})for(bool tile:{false,true})
            for(auto action:{SelectedAction::buildRoom,SelectedAction::buildTrap,SelectedAction::selectTile,
                SelectedAction::none,SelectedAction::castSpell,SelectedAction::sellRoom})
                check(selectingBuild(gui,holding,paused,tile,action) ==
                    (!gui&&!holding&&!paused&&tile&&(action==SelectedAction::buildRoom||action==SelectedAction::buildTrap)),
                    "construction input respects GUI, holding, pause, map and action precedence");
        Ogre::Root engine("", "", "hammer-Ogre.log");
        engine.loadPlugin("RenderSystem_GL3Plus.dll");
        engine.loadPlugin("Codec_STBI.dll");
        auto* renderer = engine.getAvailableRenderers().front();
        engine.setRenderSystem(renderer);
        renderer->setConfigOption("Full Screen", "No");
        engine.initialise(false);
        Ogre::NameValuePairList options; options["hidden"] = "true";
        auto* window = engine.createRenderWindow("ConstructionHammerCheck", 640, 640, false, &options);
        auto& resources = Ogre::ResourceGroupManager::getSingleton();
        resources.addResourceLocation("../../models", "FileSystem", "Graphics");
        resources.addResourceLocation("../../materials/textures", "FileSystem", "Graphics");
        resources.addResourceLocation("OGRE_PREFIX/Media/Main", "FileSystem", "OgreInternal");
        resources.addResourceLocation("OGRE_PREFIX/Media/Main", "FileSystem", "Graphics");
        resources.addResourceLocation("OGRE_PREFIX/Media/RTShaderLib/GLSL", "FileSystem", "Graphics");
        Ogre::RTShader::ShaderGenerator::initialize();
        resources.initialiseAllResourceGroups();
        resources.addResourceLocation("../../materials/scripts", "FileSystem", "Graphics");
        auto& materials = Ogre::MaterialManager::getSingleton();
        for(const char* name : {"Keeperhand.material", "HandTool.material", "BasicHammer.material"})
            materials.parseScript(resources.openResource(name, "Graphics"), "Graphics");
        auto* hammerPass = materials.getByName("HandTool/Hammer", "Graphics")->getTechnique(0)->getPass(0);
        check(hammerPass->getVertexColourTracking() == Ogre::TVC_NONE, "authored mesh needs no vertex colour stream");
        check(hammerPass->getDepthCheckEnabled() && hammerPass->getDepthWriteEnabled(), "hammer obeys grip occlusion");
        check(hammerPass->getTextureUnitState(0)->getTextureName() == "BasicHammer.png", "authored diffuse atlas retained");
        auto* shaders = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        OgreBites::SGTechniqueResolverListener listener(shaders); materials.addListener(&listener);
        RenderManager r;
        r.mSceneManager = engine.createSceneManager("DefaultSceneManager");
        shaders->addSceneManager(r.mSceneManager);
        auto* hand = r.mSceneManager->createEntity("keeperHandEnt", "Keeperhand.mesh", "Graphics");
        auto* node = r.mSceneManager->getRootSceneNode()->createChildSceneNode();
        node->setOrientation(Ogre::Quaternion(Ogre::Degree(65), Ogre::Vector3::UNIT_Z) *
            Ogre::Quaternion(Ogre::Degree(35), Ogre::Vector3::UNIT_Y));
        node->attachObject(hand);
        r.mHeldCreatureGrip = r.mSceneManager->createSceneNode();
        createKeeperHandPoses(hand); createKeeperHandDigAnimation(hand);
        createKeeperHandBuildAnimation(hand); r.createTools(hand);
        createKeeperHandIdleAnimations(hand);
        r.mHandIdleProp=r.mSceneManager->createManualObject("IdleProp");
        r.mHandIdleProp->setDynamic(true);r.mHandIdleProp->setLightMask(0);
        node->attachObject(r.mHandIdleProp);r.mHandIdleProp->setVisible(false);
        auto* camera = r.mSceneManager->createCamera("Camera");
        auto* cameraNode = r.mSceneManager->getRootSceneNode()->createChildSceneNode();
        cameraNode->attachObject(camera); cameraNode->setPosition(0, -.02f, .45f);
        cameraNode->lookAt(Ogre::Vector3(0, -.02f, 0), Ogre::Node::TS_WORLD);
        camera->setNearClipDistance(.001f);
        auto* viewport = window->addViewport(camera);
        viewport->setMaterialScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        r.mHandAnimationState = r.setEntityAnimation(hand, "Idle", true);
        r.rrSetHandPose(false, false, true);
        check(r.mHandPose == "Build" && r.mHandHammer->isVisible() && !r.mHandPickaxe->isVisible(), "building shows only hammer");
        check(hand->getSubEntity(0)->getMaterialName() == "Keeperhand/ToolGrip", "closed hand obscures shaft");
        check(r.mHandHammer->getMesh()->getName() == "BasicHammer.mesh", "existing hammer mesh reused");
        check(r.mHandHammer->getSubEntity(0)->getMaterialName() == "HandTool/Hammer", "isolated hand material");
        auto* attachment = static_cast<Ogre::TagPoint*>(r.mHandHammer->getParentNode());
        check(attachment->getPosition().positionEquals(Ogre::Vector3(0,.030f,-.009f)), "shaft uses accepted grasp position");
        const auto pickaxeOrientation=r.mHandPickaxe->getParentNode()->getOrientation();
        check((attachment->getOrientation()*Ogre::Vector3::UNIT_Z).positionEquals(
            pickaxeOrientation*Ogre::Vector3::UNIT_Y), "hammer shaft matches the pickaxe shaft angle");
        const auto mesh=r.mHandHammer->getMesh();
        std::vector<Ogre::Vector3> vertices;
        for(unsigned sub=0;sub<mesh->getNumSubMeshes();++sub) {
            const auto* part=mesh->getSubMesh(sub);
            const auto* data=part->useSharedVertices?mesh->sharedVertexData:part->vertexData;
            const auto* element=data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
            auto buffer=data->vertexBufferBinding->getBuffer(element->getSource());
            Ogre::HardwareBufferLockGuard lock(buffer,Ogre::HardwareBuffer::HBL_READ_ONLY);
            auto* bytes=static_cast<unsigned char*>(lock.pData);
            for(size_t i=0;i<data->vertexCount;++i) {
                float* value;
                element->baseVertexPointerToElement(bytes+(data->vertexStart+i)*buffer->getVertexSize(),&value);
                vertices.emplace_back(value);
            }
        }
        Ogre::Vector3 leftFace=vertices.front(),oppositeFace=vertices.front();
        for(const auto& vertex:vertices) {
            if(vertex.y>leftFace.y)leftFace=vertex;
            if(vertex.y<oppositeFace.y)oppositeFace=vertex;
        }
        const auto headAxis=(leftFace-oppositeFace).normalisedCopy();
        check((attachment->getOrientation()*headAxis).positionEquals(
            pickaxeOrientation*Ogre::Vector3::UNIT_X,.002f),
            "actual hammer striking faces match the pickaxe head angle");
        check(r.mHammerStrikePoint.positionEquals(leftFace,.00001f),
            "hammer cursor uses the actual screen-left striking face");
        engine._fireFrameStarted();engine._fireFrameRenderingQueued();hand->_updateAnimation();node->_update(true,true);
        alignKeeperHandPointer(hand,r.mHandAnimationState,r.mHandHammer,r.mHammerStrikePoint);
        node->_update(true,true);window->update();engine._fireFrameEnded();
        const auto readyFace=node->_getFullTransform()*(attachment->_getFullLocalTransform()*r.mHammerStrikePoint);
        check(readyFace.length()<.00001f,"left hammer face is the pointer while selecting a tile");
        const auto rightFace=node->_getFullTransform()*(attachment->_getFullLocalTransform()*oppositeFace);
        check(rightFace.x>readyFace.x,"the cursor face is visibly left of the opposite hammer end");
        r.mHandAnimationState=r.setEntityAnimation(hand,"Build",true);
        for(float scale : {.8f, 1.f, 1.2f}) {
            node->setScale(scale, scale, scale);
            engine._fireFrameStarted(); engine._fireFrameRenderingQueued(); hand->_updateAnimation(); node->_update(true, true);
            alignKeeperHandPointer(hand, r.mHandAnimationState);
            const auto oldFace = node->_getFullTransform() * (attachment->_getFullLocalTransform() * r.mHammerStrikePoint);
            check(oldFace.length() > .02f, "old model-origin alignment reproduces the off-target head");
            alignKeeperHandPointer(hand, r.mHandAnimationState, r.mHandHammer, r.mHammerStrikePoint);
            node->_update(true,true);
            const auto face = node->_getFullTransform() * (attachment->_getFullLocalTransform() * r.mHammerStrikePoint);
            check(face.length() < .00001f, "hammer striking face matches pointer origin at each scale");
            window->update();
            window->writeContentsToFile("hammer-" + std::to_string(int(scale * 100)) + ".png");
            engine._fireFrameEnded();
        }
        for(float scale:{.8f,1.f,1.2f}) {
            node->setScale(scale,scale,scale);
            alignKeeperHandPointer(hand,r.mHandAnimationState,r.mHandHammer,r.mHammerStrikePoint);
            const auto restingOffset=node->getPosition();
            r.rrPlayBuildAnimation();
            check(r.mHandAnimationState->getLength()==hand->getAnimationState("DigSwing")->getLength(),
                "construction uses exactly the digging strike duration");
            check(!r.mHandAnimationState->getLoop() && r.mHandHammer->isVisible() && !r.mHandPickaxe->isVisible(),
                "build strike is one shot with only its hammer visible");
            for(int sample=0;sample<=16;++sample) {
                r.mHandAnimationState->setTimePosition(r.mHandAnimationState->getLength()*sample/16.f);
                for(int settle=0;settle<2;++settle) {
                    engine._fireFrameStarted(); engine._fireFrameRenderingQueued();
                    alignKeeperHandPointer(hand,r.mHandAnimationState,r.mHandHammer,r.mHammerStrikePoint);
                    node->_update(true,true);window->update();engine._fireFrameEnded();
                }
                const auto face=node->_getFullTransform()*(attachment->_getFullLocalTransform()*r.mHammerStrikePoint);
                check(node->getPosition().positionEquals(restingOffset,.00001f),
                    "pointer alignment preserves the digging wrist arc");
                if(sample==8)check(face.length()>.01f,"strike moves freely away from the ready pointer");
                if(sample==0||sample==16)check(face.length()<.00001f,
                    "ready hammer face and completed stroke align with the pointer");
                check(hand->getSubEntity(0)->getMaterialName()=="Keeperhand/ToolGrip","strike retains solid closed grip");
                if(scale==1.f && sample%4==0)window->writeContentsToFile("hammer-strike-"+std::to_string(sample)+".png");
                std::vector<Ogre::Quaternion> rotations;
                std::vector<Ogre::Vector3> positions;
                for(unsigned short b=0;b<hand->getSkeleton()->getNumBones();++b) {
                    const auto* bone=hand->getSkeleton()->getBone(b);
                    rotations.push_back(bone->_getDerivedOrientation());
                    positions.push_back(bone->_getDerivedPosition());
                }
                r.mHandAnimationState=r.setEntityAnimation(hand,"DigSwing",false);
                r.mHandAnimationState->setTimePosition(r.mHandAnimationState->getLength()*sample/16.f);
                engine._fireFrameStarted();engine._fireFrameRenderingQueued();
                hand->_updateAnimation();node->_update(true,true);window->update();engine._fireFrameEnded();
                for(unsigned short b=0;b<hand->getSkeleton()->getNumBones();++b) {
                    const auto* bone=hand->getSkeleton()->getBone(b);
                    check(std::abs(rotations[b].Dot(bone->_getDerivedOrientation()))>.99999f,
                        "every hammer hand bone matches the digging angle at the same stroke time");
                    check(positions[b].positionEquals(bone->_getDerivedPosition(),.00001f),
                        "every hammer hand bone matches the digging movement at the same stroke time");
                }
                r.mHandAnimationState=r.setEntityAnimation(hand,"BuildSwing",false);
            }
            r.rrPlayBuildAnimation();check(r.mHandAnimationState->getTimePosition()==0,"next build restarts strike");
            r.mHandAnimationState=r.setEntityAnimation(hand,r.mHandPose,true);
            check(r.mHandAnimationState->getAnimationName()=="Build","strike returns to construction pose");
        }
        node->setScale(1,1,1);node->setPosition(Ogre::Vector3::ZERO);
        for(const std::string pose : {"Dig", "Point", "Idle", "Build"}) {
            r.rrSetHandPose(pose == "Point", pose == "Dig", pose == "Build");
            check(r.mHandHammer->isVisible() == (pose == "Build"), "hammer follows selected contextual pose");
            check(r.mHandPickaxe->isVisible() == (pose == "Dig"), "pickaxe remains exclusive to digging");
        }
        for(const std::string action : {"Pickup", "Drop", "Slap"}) {
            r.mHandAnimationState = r.setEntityAnimation(hand, action, false);
            r.rrSetHandPose(false, false, true);
            check(!r.mHandHammer->isVisible() && !r.mHandPickaxe->isVisible(), "one-shot hides tools");
            r.mHandAnimationState = r.setEntityAnimation(hand, r.mHandPose, true);
            check(r.mHandHammer->isVisible(), "latest construction pose restored");
        }
        r.rrPlayDigAnimation();
        check(r.mHandPickaxe->isVisible() && !r.mHandHammer->isVisible(), "dig strike retains pickaxe");
        r.mHandAnimationState = r.setEntityAnimation(hand, "Build", true);
        r.mHandKeeperHandVisibility = 1; r.rrSetHandPose(false, false, true);
        check(!r.mHandHammer->isVisible(), "hidden hand hides hammer");
        r.mHandKeeperHandVisibility = 0; r.rrSetHandPose(false, false, true);
        check(r.mHandHammer->isVisible(), "reshown hand restores hammer");
        node->setScale(1,1,1);
        for(unsigned choice:{0u,1u}) {
            Random::choice=choice;r.rrSetHandPose(false,false);
            // Complete the ordinary pointing transition before starting an idle effect.
            r.mHandAnimationState=r.setEntityAnimation(hand,"Idle",true);
            engine._fireFrameStarted();engine._fireFrameRenderingQueued();
            alignKeeperHandPointer(hand,r.mHandAnimationState);hand->_updateAnimation();
            node->_update(true,true);window->update();engine._fireFrameEnded();
            const auto emptyHandRotation=hand->getSkeleton()->getBone("Hand1")->_getDerivedOrientation();
            check(r.rrPlayIdleHandAnimation(),"idle effect starts on free hand");
            check(r.rrIsIdleHandAnimationPlaying()&&!r.mHandAnimationState->getLoop(),"idle effect is a one-shot");
            check(r.mHandAnimationState->getAnimationName()==IDLE_HAND_ANIMATIONS[choice],"random choice selects either authored effect");
            check(!r.rrPlayIdleHandAnimation(),"active effect is not restarted");
            auto* idle=r.mHandAnimationState;
            for(unsigned i=0;i<=12;++i) {
                idle->setTimePosition(idle->getLength()*i/12.f);
                for(int settle=0;settle<2;++settle) {
                    engine._fireFrameStarted();engine._fireFrameRenderingQueued();
                    alignKeeperHandPointer(hand,idle);updateKeeperHandIdleProp(hand,idle,r.mHandIdleProp);
                    node->_update(true,true);window->update();engine._fireFrameEnded();
                }
                check(!r.mHandHammer->isVisible()&&!r.mHandPickaxe->isVisible(),"idle effect hides normal tools");
                check(r.mHandIdleProp->isVisible()==(i>0&&i<12),"props enter and leave with the effect");
                if(i>0&&i<12) check(r.mHandIdleProp->getBoundingBox().getSize().length()<.3f,"prop remains hand-sized");
                const auto wristRotation=hand->getSkeleton()->getBone("Hand1")->_getDerivedOrientation();
                check(std::abs(emptyHandRotation.Dot(wristRotation))>.99999f,
                    "watch and yo-yo keep the empty hand angle for their entire duration");
                if(choice==1&&i>0&&i<12) {
                    const auto* finger=hand->getSkeleton()->getBone("Index3");
                    const auto anchor=finger->_getDerivedPosition()+finger->_getDerivedOrientation()*Ogre::Vector3(-.000284253f,.0155774f,.000218656f);
                    const auto* data=r.mHandIdleProp->getSection(0)->getRenderOperation()->vertexData;
                    const auto* element=data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
                    auto buffer=data->vertexBufferBinding->getBuffer(element->getSource());
                    Ogre::HardwareBufferLockGuard lock(buffer,Ogre::HardwareBuffer::HBL_READ_ONLY);
                    auto* bytes=static_cast<unsigned char*>(lock.pData);float* a;float* b;
                    element->baseVertexPointerToElement(bytes,&a);element->baseVertexPointerToElement(bytes+buffer->getVertexSize(),&b);
                    check(((Ogre::Vector3(a)+Ogre::Vector3(b))*.5f-anchor).length()<.00001f,"yo-yo string stays attached to animated fingertip");
                }
                if(IDLE_AUDIT) window->writeContentsToFile("idle-"+std::to_string(choice)+"-"+std::to_string(i)+".png");
            }
            if(choice==1) {
                const auto sampleFinger=[&](float time) {
                    idle->setTimePosition(time);
                    for(int settle=0;settle<2;++settle) {
                        engine._fireFrameStarted();engine._fireFrameRenderingQueued();
                        alignKeeperHandPointer(hand,idle);updateKeeperHandIdleProp(hand,idle,r.mHandIdleProp);
                        node->_update(true,true);window->update();engine._fireFrameEnded();
                    }
                    const auto* finger=hand->getSkeleton()->getBone("Index3");
                    return finger->_getDerivedPosition()+finger->_getDerivedOrientation()*
                        Ogre::Vector3(-.000284253f,.0155774f,.000218656f);
                };
                for(int cycle=0;cycle<3;++cycle) {
                    const auto release=sampleFinger(.6f+cycle);
                    const auto pull=sampleFinger(1.1f+cycle);
                    const auto returned=sampleFinger(1.6f+cycle);
                    check((pull-release).length()>.003f,"each yo-yo turnaround has a visible finger pull");
                    check((returned-release).length()<.0001f,"finger releases again for the next yo-yo cycle");
                }
            }
            r.rrCancelIdleHandAnimation();
            check(!r.rrIsIdleHandAnimationPlaying()&&!r.mHandIdleProp->isVisible(),"cancellation immediately hides prop");
            check(r.mHandAnimationState->getAnimationName()=="Idle","cancellation restores current context");
            r.rrPlayIdleHandAnimation();r.rrSetHandPose(false,false,true);
            check(r.mHandAnimationState->getAnimationName()=="Build"&&r.mHandHammer->isVisible(),"new construction pose preempts idle effect");
            check(r.rrPlayIdleHandAnimation(),"an untouched construction selection also permits idle feedback");
            r.rrSetHandPose(false,false,true);
            check(r.rrIsIdleHandAnimationPlaying(),"unchanged contextual pose does not cancel idle");
            r.updateHand(5);
            check(r.mHandAnimationState->getAnimationName()=="Build"&&r.mHandHammer->isVisible(),"natural completion restores selected tool");
            r.rrSetHandPose(false,false);r.rrPlayIdleHandAnimation();r.updateHand(5);
            check(!r.rrIsIdleHandAnimationPlaying()&&!r.mHandIdleProp->isVisible(),"production update returns naturally to the base pose without a prop");
            r.mHandKeeperHandVisibility=1;
            check(!r.rrPlayIdleHandAnimation(),"hidden hand cannot start an idle effect");
            r.mHandKeeperHandVisibility=0;
            for(const char* action:{"Pickup","Drop","Slap","DigSwing","BuildSwing"}) {
                r.mHandAnimationState=r.setEntityAnimation(hand,action,false);
                check(!r.rrPlayIdleHandAnimation(),"idle cannot interrupt an action animation");
                r.rrCancelIdleHandAnimation();
                check(r.mHandAnimationState->getAnimationName()==action,"idle cancellation leaves action animations unchanged");
            }
            r.updateHand(5);r.mHeldCreatureDisplayEnabled=true;
            auto* held=r.mHeldCreatureGrip->createChildSceneNode();r.rrSetHandPose(false,false);
            check(r.mHandPose=="Hold"&&!r.rrPlayIdleHandAnimation(),"held creature prevents idle effects");
            r.mHeldCreatureGrip->removeChild(held);r.mSceneManager->destroySceneNode(held);
            r.mHeldCreatureDisplayEnabled=false;
        }
        r.mSceneManager->destroyManualObject(r.mHandIdleProp);
        hand->detachAllObjectsFromBone();
        r.mSceneManager->destroyEntity(r.mHandHammer);
        r.mSceneManager->destroyManualObject(r.mHandPickaxe);
        r.mSceneManager->destroyEntity(hand);
        shaders->removeSceneManager(r.mSceneManager); engine.destroySceneManager(r.mSceneManager);
        materials.removeListener(&listener); Ogre::RTShader::ShaderGenerator::destroy();
        std::cout << "CHECKS=" << checks << " FAILURES=0\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
'''
probe = probe.replace('HELPERS', helpers).replace('FACTORY', factory).replace('METHODS', methods).replace('BUILDING', building)
probe = probe.replace('PICKAXE_AUDIT', 'true' if args.pickaxe_audit else 'false')
probe = probe.replace('IDLE_AUDIT', 'true' if args.idle_audit else 'false')
out = root / 'build/construction-hammer-check'
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'check.cpp'
prefix = Path(os.environ['CMAKE_PREFIX_PATH'])
cpp.write_text(probe.replace('OGRE_PREFIX', prefix.as_posix()), encoding='utf-8')
command = ['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', f'/I{prefix / "include/OGRE"}',
           f'/I{prefix / "include/OGRE/RTShaderSystem"}', str(cpp), '/Fe:check.exe', '/link',
           f'/LIBPATH:{prefix / "lib"}', 'OgreMain.lib', 'OgreRTShaderSystem.lib', 'OgreBites.lib']
with (out / 'compile.log').open('w') as log:
    subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
if not args.compile_only:
    with (out / 'result.log').open('w') as log:
        subprocess.run([str(out / 'check.exe')], cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
    result = (out / 'result.log').read_text(errors='replace')
    assert 'Error: ScriptCompiler' not in result and "Can't assign material" not in result, result
    print(next(line for line in result.splitlines() if line.startswith('CHECKS=')))
