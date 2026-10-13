"""Exercise the production defeat sequence timeline and its GameMode driver without a game.

DefeatSequence.h is compiled as it is. The GameMode member functions that drive the sequence are cut out of
GameMode.cpp by signature and compiled against small mocks of CEGUI, the render manager and the game map.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import time

repo = Path(__file__).resolve().parents[2]
game_mode = (repo / 'source/modes/GameMode.cpp').read_text()
game_mode_header = (repo / 'source/modes/GameMode.h').read_text()
sequence_header = (repo / 'source/modes/DefeatSequence.h').read_text()
client = (repo / 'source/network/ODClient.cpp').read_text()
frame_listener = (repo / 'source/render/ODFrameListener.cpp').read_text()
mode_manager = (repo / 'source/modes/ModeManager.cpp').read_text()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def constant(name):
    match = re.search(r'^const std::string ' + name + r' = .*;$', game_mode, re.MULTILINE)
    assert match, name
    return match.group(0)


probe = r'''
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "modes/DefeatSequence.h"
#include "modes/DefeatHeartBurst.h"

std::vector<std::string> gLog;
#define OD_LOG_INF(x) gLog.push_back(x)
namespace Helper {template<typename T> std::string toString(T v){return std::to_string(v);}}
namespace Ogre
{
struct Vector3
{
    float x, y, z;
    Vector3():x(0),y(0),z(0){}
    Vector3(float ax, float ay, float az):x(ax),y(ay),z(az){}
    Vector3 operator+(const Vector3& o) const {return Vector3(x+o.x,y+o.y,z+o.z);}
    Vector3 operator*(float f) const {return Vector3(x*f,y*f,z*f);}
    float squaredLength() const {return x*x+y*y+z*z;}
    void normalise(){float l=std::sqrt(squaredLength());x/=l;y/=l;z/=l;}
    static const Vector3 UNIT_Y;
};
const Vector3 Vector3::UNIT_Y(0,1,0);
struct ColourValue {float r,g,b,a;};
}
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
struct Effect {std::string event,name,script;bool hasColour;Ogre::ColourValue colour;Ogre::Vector3 position;};
struct RenderManager
{
    std::vector<Effect> effects;
    static RenderManager& getSingleton(){static RenderManager manager;return manager;}
    void rrCreateFreeParticleEffect(const std::string& name,const std::string& script,const Ogre::Vector3& p,const Ogre::ColourValue* c)
    {Effect e;e.event="create";e.name=name;e.script=script;e.hasColour=c!=nullptr;if(c)e.colour=*c;e.position=p;effects.push_back(e);}
    void rrMoveFreeParticleEffect(const std::string& name,const Ogre::Vector3& p)
    {Effect e;e.event="move";e.name=name;e.hasColour=false;e.position=p;effects.push_back(e);}
    void rrDestroyFreeParticleEffect(const std::string& name)
    {Effect e;e.event="destroy";e.name=name;e.hasColour=false;effects.push_back(e);}
    // The copy of the heart and the rubble are recorded as effects named "Heart" and "Rubble"
    size_t rubbleCount = 0;
    void rrCreateDefeatHeart(const Ogre::Vector3& p)
    {Effect e;e.event="create";e.name="Heart";e.hasColour=false;e.position=p;effects.push_back(e);}
    void rrUpdateDefeatHeart(const Ogre::Vector3& p, float, float)
    {Effect e;e.event="move";e.name="Heart";e.hasColour=false;e.position=p;effects.push_back(e);}
    void rrDestroyDefeatHeart()
    {Effect e;e.event="destroy";e.name="Heart";e.hasColour=false;effects.push_back(e);}
    void rrCreateDefeatRubble(size_t count)
    {rubbleCount=count;Effect e;e.event="create";e.name="Rubble";e.hasColour=false;effects.push_back(e);}
    void rrMoveDefeatRubblePiece(size_t, const Ogre::Vector3& p, const Ogre::Vector3&, const Ogre::Vector3&)
    {Effect e;e.event="move";e.name="Rubble";e.hasColour=false;e.position=p;effects.push_back(e);}
    void rrDestroyDefeatRubble()
    {Effect e;e.event="destroy";e.name="Rubble";e.hasColour=false;effects.push_back(e);}
};
struct Camera {Ogre::Vector3 direction;Ogre::Vector3 getDerivedDirection() const {return direction;}};
struct CameraManager {Camera camera;Camera* getActiveCamera(){return &camera;}};
struct ODFrameListener
{
    CameraManager cameraManager;
    static ODFrameListener& getSingleton(){static ODFrameListener listener;return listener;}
    CameraManager* getCameraManager(){return &cameraManager;}
};
CONSTANTS

class GameMode
{
public:
    GameMode():mRootWindow(&mRoot),mGameMap(&mMap){}
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
    CEGUI::Window mRoot;
    CEGUI::Window* mRootWindow;
    GameMap mMap;
    GameMap* mGameMap;
};
UPDATE
SWIRL
BURST
STOP
HIDE
FINISHED

int gChecks = 0, gFailures = 0;
void check(bool ok, const char* msg) {++gChecks;if(!ok){++gFailures;std::cout << "FAIL " << msg << '\n';}}
bool near(float a, float b) {return std::fabs(a - b) < 0.001f;}
typedef DefeatSequence DS;
std::chrono::steady_clock::duration seconds(double value)
{
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(value));
}

struct Run
{
    GameMode mode;
    std::chrono::steady_clock::time_point start, now;
    int finishes = 0;
    float finishWallTime = -1;
    CEGUI::Window ui1, ui2, tint, fade, subtitle, marker;
    int explosionDestroys = 0, swirlCreates = 0, swirlDestroys = 0, swirlMoves = 0;
    int burstCreates = 0, heartMoves = 0, heartDestroys = 0, rubbleCreates = 0, rubbleMoves = 0, rubbleDestroys = 0;
    float burstTime = -1, heartDestroyTime = -1, rubbleCreateTime = -1, rubbleDestroyTime = -1, burstHeight = -1;
    float largestShake = 0;
    float firstExplosionDestroy = -1, swirlCreateTime = -1, swirlDestroyTime = -1, finishTime = -1;
    bool sequenceWindowHidden = false;
    int uiShownAfterHide = 0;
    Ogre::ColourValue swirlColour;
    bool swirlColourGiven = false;
    float lastSwirlDistance = -1;
    bool swirlMonotonic = true;

    void begin(int32_t conqueror, int32_t hx, int32_t hy)
    {
        gLog.clear();RenderManager::getSingleton().effects.clear();
        mode.mMap.seat.colour = Ogre::ColourValue{0.2f, 0.9f, 0.1f, 0.5f};
        ODFrameListener::getSingleton().cameraManager.camera.direction = Ogre::Vector3(0, 1, -0.5f);
        mode.mRoot.children = {&ui1, &ui2, &tint, &fade, &subtitle, &marker};
        mode.mDefeatTint = &tint;mode.mDefeatFade = &fade;mode.mDefeatSubtitle = &subtitle;mode.mDefeatCameraMarker = &marker;
        start = std::chrono::steady_clock::time_point() + seconds(1000.0);
        now = start;
        mode.mDefeatSequence.start(conqueror, hx, hy, start);
        mode.mDefeatHeartPosition = Ogre::Vector3(float(hx), float(hy), 0.0f);
        // As startDefeatSequence: the copy of the heart stands there; the explosion starts at the burst
        if(mode.mDefeatSequence.isHeartKnown())
        {
            mode.mDefeatHeartShown = true;
            RenderManager::getSingleton().rrCreateDefeatHeart(mode.mDefeatHeartPosition);
        }
    }

    // one frame of dt seconds of wall clock; the game calls the update from both frame hooks, so
    // updatesPerFrame calls see the same time; the interface is shown again first, as game code does
    // when it refreshes a window
    void frame(float dt, int updatesPerFrame = 1)
    {
        ui1.visible = true;
        size_t before = RenderManager::getSingleton().effects.size();
        bool wasFinished = mode.mDefeatSequence.isFinished();
        now += seconds(dt);
        for(int call = 0; call < updatesPerFrame; ++call)
            mode.updateDefeatSequence(now);
        if(ui1.visible || ui2.visible) ++uiShownAfterHide;
        if(!mode.mDefeatSequence.isFinished() && (!tint.visible || !fade.visible || !subtitle.visible || !marker.visible)) sequenceWindowHidden = true;
        float t = mode.mDefeatSequence.getElapsed();
        std::vector<Effect>& effects = RenderManager::getSingleton().effects;
        for(size_t i = before; i < effects.size(); ++i)
        {
            Effect& e = effects[i];
            if(e.name == "DefeatHeartExplosion" && e.event == "destroy") {++explosionDestroys;if(firstExplosionDestroy < 0) firstExplosionDestroy = t;}
            if(e.name == "DefeatSwirl" && e.event == "create")
            {
                ++swirlCreates;swirlCreateTime = t;swirlColourGiven = e.hasColour;swirlColour = e.colour;
            }
            if(e.name == "DefeatSwirl" && e.event == "move")
            {
                ++swirlMoves;
                float distance = e.position.y - mode.mDefeatHeartPosition.y;
                if(distance < lastSwirlDistance) swirlMonotonic = false;
                lastSwirlDistance = distance;
            }
            if(e.name == "DefeatSwirl" && e.event == "destroy") {++swirlDestroys;swirlDestroyTime = t;}
            if(e.event == "create" && e.name.compare(0, 20, "DefeatHeartExplosion") == 0) {++burstCreates;burstTime = t;burstHeight = e.position.z;}
            if(e.name == "Heart" && e.event == "move")
            {
                ++heartMoves;
                float shake = std::sqrt((e.position.x - mode.mDefeatHeartPosition.x) * (e.position.x - mode.mDefeatHeartPosition.x)
                    + (e.position.y - mode.mDefeatHeartPosition.y) * (e.position.y - mode.mDefeatHeartPosition.y));
                if(shake > largestShake) largestShake = shake;
            }
            if(e.name == "Heart" && e.event == "destroy") {++heartDestroys;heartDestroyTime = t;}
            if(e.name == "Rubble" && e.event == "create") {++rubbleCreates;rubbleCreateTime = t;}
            if(e.name == "Rubble" && e.event == "move") ++rubbleMoves;
            if(e.name == "Rubble" && e.event == "destroy") {++rubbleDestroys;rubbleDestroyTime = t;}
        }
        if(!wasFinished && mode.mDefeatSequence.isFinished())
        {
            finishTime = t;++finishes;
            finishWallTime = static_cast<float>(std::chrono::duration<double>(now - start).count());
        }
    }
};

int main()
{
    // Timeline boundaries
    check(DS::phaseAt(-1.0f) == DS::Phase::Inactive, "before the start nothing runs");
    check(DS::phaseAt(0.0f) == DS::Phase::Explosion && DS::phaseAt(15.49f) == DS::Phase::Explosion, "explosion phase 0 to 15.5");
    check(DS::phaseAt(15.5f) == DS::Phase::Swirl && DS::phaseAt(18.49f) == DS::Phase::Swirl, "swirl phase 15.5 to 18.5");
    check(DS::phaseAt(18.5f) == DS::Phase::Fade && DS::phaseAt(29.49f) == DS::Phase::Fade, "fade phase until 29.5");
    check(DS::phaseAt(29.5f) == DS::Phase::Finished && DS::phaseAt(100.0f) == DS::Phase::Finished, "finished from 29.5");
    check(DS::isExplosionEffectActiveAt(0.0f) && DS::isExplosionEffectActiveAt(14.99f) && !DS::isExplosionEffectActiveAt(15.0f), "explosion effect lasts 15 s");
    check(!DS::isSwirlActiveAt(15.49f) && DS::isSwirlActiveAt(15.5f) && DS::isSwirlActiveAt(18.49f) && !DS::isSwirlActiveAt(18.5f), "swirl active 15.5 to 18.5");
    check(near(DS::swirlProgressAt(15.5f), 0.0f) && near(DS::swirlProgressAt(17.0f), 0.5f) && near(DS::swirlProgressAt(18.5f), 1.0f), "swirl progress is linear over 3 s");
    check(near(DS::redTintAlphaAt(0.0f), 0.35f) && near(DS::redTintAlphaAt(14.99f), 0.35f), "red tint during the explosion");
    check(near(DS::redTintAlphaAt(15.25f), 0.175f) && near(DS::redTintAlphaAt(15.5f), 0.0f) && near(DS::redTintAlphaAt(20.0f), 0.0f), "red tint fades out after the explosion");
    check(near(DS::blackAlphaAt(19.5f), 0.0f) && near(DS::blackAlphaAt(19.0f), 0.0f), "no black before 19.5 s");
    check(near(DS::blackAlphaAt(24.0f), 0.5f) && near(DS::blackAlphaAt(28.5f), 1.0f) && near(DS::blackAlphaAt(29.5f), 1.0f), "fade to black takes 9 s");
    check(DS::isFirstSubtitleVisibleAt(0.0f) && DS::isFirstSubtitleVisibleAt(14.99f) && !DS::isFirstSubtitleVisibleAt(15.0f), "first subtitle 0 to 15 s");
    check(!DS::isSecondSubtitleVisibleAt(20.49f) && DS::isSecondSubtitleVisibleAt(20.5f) && DS::isSecondSubtitleVisibleAt(29.5f), "second subtitle from 20.5 s");

    // One-shot guard, input gate, step limit
    DefeatSequence sequence;
    check(!sequence.isStarted() && !sequence.blocksInput(), "input is not blocked before the start");
    const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::time_point() + seconds(100.0);
    check(near(DS::secondsBetween(t0, t0 + seconds(29.5)), 29.5f) && near(DS::secondsBetween(t0, t0), 0.0f)
        && near(DS::secondsBetween(t0, t0 - seconds(3.0)), 0.0f), "wall-clock seconds since the start, never negative");
    check(!sequence.advanceTo(t0 + seconds(1.0)) && near(sequence.getElapsed(), 0.0f), "a sequence that was not started does not advance");
    check(sequence.start(2, 17, 23, t0), "first start request is accepted");
    check(sequence.blocksInput() && sequence.isHeartKnown() && sequence.isSwirlWanted(), "started: input blocked, heart known, swirl wanted");
    check(!sequence.start(3, 5, 6, t0 + seconds(4.0)), "second start request is ignored");
    check(sequence.getConquerorSeatId() == 2 && sequence.getHeartTileX() == 17 && sequence.getHeartTileY() == 23, "second request did not change the data");
    sequence.advanceTo(t0 + seconds(4.0));
    check(near(sequence.getElapsed(), 4.0f), "the second request did not move the start time");
    sequence.advanceTo(t0 + seconds(10.0));
    check(near(sequence.getElapsed(), 10.0f), "one long frame moves the timeline by exactly its wall-clock length");
    sequence.advanceTo(t0 + seconds(10.0));
    sequence.advanceTo(t0 + seconds(10.0));
    check(near(sequence.getElapsed(), 10.0f), "more updates at the same time (several per frame) change nothing");
    sequence.advanceTo(t0 + seconds(7.0));
    sequence.advanceTo(t0 - seconds(3.0));
    check(near(sequence.getElapsed(), 10.0f), "an earlier time or a time before the start is ignored");
    int ends = 0;
    for(int i = 0; i < 1000; ++i) if(sequence.advanceTo(t0 + seconds(10.0 + 0.1 * i))) ++ends;
    check(ends == 1, "the end is reported exactly once");
    check(sequence.getElapsed() >= 29.5f && sequence.getElapsed() < 29.6f, "the end is reached at 29.5 s of wall clock");
    check(sequence.isFinished() && sequence.blocksInput() && !sequence.start(1, 1, 1, t0), "after the end input stays blocked and a restart is refused");
    DefeatSequence unknown;
    unknown.start(-1, -1, -1, t0);
    check(!unknown.isSwirlWanted() && !unknown.isHeartKnown(), "-1 conqueror: no swirl; -1 heart: position unknown");
    DefeatSequence halfKnown;
    halfKnown.start(0, 4, -1, t0);
    check(!halfKnown.isHeartKnown() && halfKnown.isSwirlWanted(), "a heart with only one coordinate counts as unknown; seat 0 is a valid conqueror");

    // The GameMode driver over a whole sequence at 60 frames per second
    {
        Run run;run.begin(1, 17, 23);
        for(int i = 0; i < 60 * 40; ++i) run.frame(1.0f / 60.0f);
        check(run.explosionDestroys == 1 && run.firstExplosionDestroy >= 14.99f && run.firstExplosionDestroy < 15.1f, "explosion effect is removed once, at about 15 s");
        check(run.burstCreates == 4 && run.burstTime >= 0.8f && run.burstTime < 0.82f && near(run.burstHeight, DefeatHeartBurstSettings::BURST_HEIGHT), "the four burst effects start once, at the burst (0.8 s), in the heart's middle");
        check(run.heartMoves > 40 && run.largestShake > 0.02f && run.largestShake <= DefeatHeartBurstSettings::SHAKE_AMPLITUDE * 1.4143f, "until the burst the copy of the heart shakes around its tile");
        check(run.heartDestroys == 1 && run.heartDestroyTime >= 0.8f && run.heartDestroyTime < 0.82f, "the copy of the heart disappears once, at the burst");
        check(run.rubbleCreates == 1 && run.rubbleCreateTime >= 0.8f && run.rubbleCreateTime < 0.82f && RenderManager::getSingleton().rubbleCount == DefeatHeartBurstSettings::RUBBLE_PIECES, "the rubble is created once at the burst, with every piece");
        check(run.rubbleMoves > 1000 && run.rubbleDestroys == 1 && run.rubbleDestroyTime >= 29.5f && run.rubbleDestroyTime < 29.6f, "the rubble is placed every frame and removed once, at the end (black screen)");
        check(run.swirlCreates == 1 && run.swirlCreateTime >= 15.5f && run.swirlCreateTime < 15.6f, "swirl is created once at about 15.5 s");
        check(run.swirlColourGiven && near(run.swirlColour.r, 0.2f) && near(run.swirlColour.g, 0.9f) && near(run.swirlColour.b, 0.1f) && near(run.swirlColour.a, 1.0f), "swirl uses the seat colour of the conqueror with full opacity");
        check(run.swirlMoves > 100 && run.swirlMonotonic && run.lastSwirlDistance > 12.0f && run.lastSwirlDistance <= DefeatSequenceSettings::SWIRL_DISTANCE + 0.01f, "swirl moves away from the heart along the camera direction");
        check(run.swirlDestroys == 1 && run.swirlDestroyTime >= 18.5f && run.swirlDestroyTime < 18.6f, "swirl is removed once, at about 18.5 s");
        check(run.finishTime >= 29.5f && run.finishTime < 29.6f, "the end is reached at 29.5 s");
        int finishedLogs = 0;for(size_t i = 0; i < gLog.size(); ++i) if(gLog[i] == "Defeat sequence finished") ++finishedLogs;
        check(finishedLogs == 1, "the finished hook runs exactly once");
        check(run.uiShownAfterHide == 0, "windows that the game shows again are hidden in the same frame");
        check(!run.sequenceWindowHidden, "the windows of the sequence itself are never hidden while the sequence runs");
        check(run.mode.mDebriefingShown == 1 && !run.subtitle.visible && !run.marker.visible, "the debriefing is opened once and takes the place of subtitle and marker");
        check(near(run.fade.alpha, 1.0f) && near(run.tint.alpha, 0.0f), "the screen ends black without tint");
        check(run.subtitle.text == "That's it for today. Until next time.", "the second subtitle stays until the end");
    }
    {
        Run run;run.begin(1, 17, 23);
        for(int i = 0; i < 60 * 5; ++i) run.frame(1.0f / 60.0f);
        check(run.subtitle.text == "Your dungeon heart has been destroyed." && near(run.tint.alpha, 0.35f) && near(run.fade.alpha, 0.0f), "after 5 s: first subtitle, red tint, no fade");
        check(run.explosionDestroys == 0 && run.swirlCreates == 0, "after 5 s the explosion still runs and there is no swirl");
        for(int i = 0; i < 60 * 12; ++i) run.frame(1.0f / 60.0f);
        check(run.subtitle.text == "" && run.swirlCreates == 1, "at 17 s no subtitle and the swirl runs");
        for(int i = 0; i < 60 * 2; ++i) run.frame(1.0f / 60.0f);
        check(run.subtitle.text == "" && near(run.fade.alpha, 0.0f), "at 19 s there is no subtitle and no fade yet");
        for(int i = 0; i < 60 * 2; ++i) run.frame(1.0f / 60.0f);
        check(run.subtitle.text == "That's it for today. Until next time." && run.fade.alpha > 0.15f && run.fade.alpha < 0.18f, "at 21 s the fade has begun and the second subtitle shows");
    }
    // Two updates per frame, as the game does it (ODFrameListener::frameStarted and ModeManager::update both
    // call onFrameStarted): the timeline must still take 29.5 s of wall clock, not half of it
    {
        Run run;run.begin(1, 17, 23);
        int frames = 0;
        while(!run.mode.mDefeatSequence.isFinished() && frames < 40 * 60) {run.frame(1.0f / 40.0f, 2);++frames;}
        check(run.finishes == 1 && run.finishWallTime >= 29.5f && run.finishWallTime < 29.53f, "two updates per frame: the end comes after 29.5 s of wall clock");
        check(frames >= 1180 && frames <= 1181, "two updates per frame at 40 fps: about 1180 frames until the end");
        check(run.explosionDestroys == 1 && run.firstExplosionDestroy >= 15.0f && run.firstExplosionDestroy < 15.03f, "two updates per frame: the explosion ends at 15 s");
        check(run.swirlCreates == 1 && run.swirlDestroys == 1 && run.swirlDestroyTime >= 18.5f && run.swirlDestroyTime < 18.53f, "two updates per frame: the swirl runs until 18.5 s");
        check(run.mode.mDebriefingShown == 1, "two updates per frame: the debriefing opens once");
        for(int i = 0; i < 100; ++i) run.frame(1.0f / 40.0f, 3);
        check(run.finishes == 1 && run.mode.mDebriefingShown == 1 && near(run.fade.alpha, 1.0f), "after the end further updates keep the screen black and open nothing again");
    }
    // Long frames: the timeline keeps to the wall clock, no effect is left behind and every one-shot runs once
    {
        Run run;run.begin(1, 17, 23);
        run.frame(0.1f);
        run.frame(6.0f);
        run.frame(0.9f);
        check(near(run.tint.alpha, 0.35f) && run.subtitle.text == "Your dungeon heart has been destroyed.", "after a 6 s frame the timeline is at 7 s");
        for(int i = 0; i < 40; ++i) run.frame(0.9f);
        check(run.finishes == 1 && run.finishWallTime >= 29.5f && run.finishWallTime < 30.41f, "0.9 s frames: the end comes with the first frame after 29.5 s");
        check(run.explosionDestroys == 1 && !run.mode.mDefeatExplosionEffectActive, "0.9 s frames: the explosion is removed once");
        check(run.swirlCreates == 1 && run.swirlDestroys == 1 && !run.mode.mDefeatSwirlEffectActive, "0.9 s frames: the swirl is created and removed once");
        check(run.mode.mDebriefingShown == 1 && near(run.fade.alpha, 1.0f), "0.9 s frames: black screen and one debriefing");
    }
    {
        Run run;run.begin(1, 17, 23);
        run.frame(15.4f);
        run.frame(3.5f);
        run.frame(12.0f);
        check(run.burstCreates == 0 && run.explosionDestroys == 0 && run.swirlCreates == 0 && !run.mode.mDefeatSwirlEffectActive, "a first frame past the explosion phase skips the burst effects and the swirl without leaving an effect");
        check(run.heartDestroys == 1 && run.rubbleCreates == 1, "... but the copy of the heart still bursts into rubble");
        check(run.rubbleDestroys == 1, "a frame across the end removes the rubble");
        check(run.finishes == 1 && run.mode.mDebriefingShown == 1 && near(run.fade.alpha, 1.0f), "a frame across the end finishes once");
    }
    {
        Run run;run.begin(-1, 17, 23);
        for(int i = 0; i < 60 * 35; ++i) run.frame(1.0f / 60.0f);
        check(run.swirlCreates == 0 && run.swirlMoves == 0, "unknown conqueror (-1): no swirl");
        check(run.finishTime >= 29.5f, "the sequence still ends without a swirl");
    }
    {
        Run run;run.begin(7, 17, 23);
        for(int i = 0; i < 60 * 35; ++i) run.frame(1.0f / 60.0f);
        check(run.swirlCreates == 0 && run.swirlMoves == 0 && run.finishTime >= 29.5f, "conqueror seat that does not exist: no swirl, no crash");
    }
    {
        Run run;run.begin(1, 17, 23);
        for(int i = 0; i < 60 * 8; ++i) run.frame(1.0f / 60.0f);
        run.mode.stopDefeatEffects();
        int destroys = 0, rubble = 0, all = 0;
        for(size_t i = 0; i < RenderManager::getSingleton().effects.size(); ++i)
        {
            const Effect& e = RenderManager::getSingleton().effects[i];
            if(e.event == "destroy") ++all;
            if(e.event == "destroy" && e.name.compare(0, 20, "DefeatHeartExplosion") == 0) ++destroys;
            if(e.event == "destroy" && e.name == "Rubble") ++rubble;
        }
        check(destroys == 4 && rubble == 1 && !run.mode.mDefeatExplosionEffectActive && !run.mode.mDefeatRubbleShown, "leaving the mode removes the running burst effects and the rubble");
        run.mode.stopDefeatEffects();
        int after = 0;for(size_t i = 0; i < RenderManager::getSingleton().effects.size(); ++i) if(RenderManager::getSingleton().effects[i].event == "destroy") ++after;
        check(after == all, "stopping again removes nothing more");
    }
    std::cout << "CHECKS=" << gChecks << " FAILURES=" << gFailures << '\n';
    return gFailures ? 1 : 0;
}
'''

constants = '\n'.join(constant(name) for name in (
    'DEFEAT_EXPLOSION_EFFECT_NAME', 'DEFEAT_SWIRL_EFFECT_NAME', 'DEFEAT_FIRST_SUBTITLE', 'DEFEAT_SECOND_SUBTITLE'))
constants += '\n' + '\n'.join(re.findall(r'^const (?:size_t|std::string) DEFEAT_BURST_\w+.*;$', game_mode, re.MULTILINE))
probe = (probe.replace('CONSTANTS', constants)
    .replace('UPDATE', function(game_mode, 'void GameMode::updateDefeatSequence('))
    .replace('SWIRL\n', function(game_mode, 'void GameMode::startDefeatSwirl(') + '\n')
    .replace('BURST\n', function(game_mode, 'void GameMode::startDefeatBurst(') + '\n')
    .replace('STOP', function(game_mode, 'void GameMode::stopDefeatEffects('))
    .replace('HIDE', function(game_mode, 'void GameMode::hideInterfaceForDefeat('))
    .replace('FINISHED', function(game_mode, 'void GameMode::onDefeatSequenceFinished(')))

# Static wiring checks on the production sources.
guard = '{\n    resetIdleHand();\n    if(mDefeatSequence.blocksInput())\n'
for signature in ('bool GameMode::mouseMoved(', 'bool GameMode::mousePressed(', 'bool GameMode::mouseReleased(',
                  'bool GameMode::keyPressed(', 'bool GameMode::keyReleased('):
    body = function(game_mode, signature)
    assert body[body.index('{'):].startswith(guard), signature
    lines = body[body.index('{') + len(guard):].split('\n')
    assert lines[0] == '        return true;' or (lines[0] == '    {' and '        return true;' in lines[1:4]
                                                   and lines.index('    }') > lines.index('        return true;')), signature
print('WIRING OK: mouse move/press/release and key press/release are gated right after the idle-hand reset and return before any game code while the sequence runs')
assert 'if(mDefeatSequence.blocksInput())' in function(game_mode, 'void GameMode::updateCameraControls(')
assert 'updateDefeatSequence(std::chrono::steady_clock::now());' in function(game_mode, 'void GameMode::onFrameStarted(')
start = function(game_mode, 'void GameMode::startDefeatSequence(')
assert 'if(!mDefeatSequence.start(' in start and start.index('return;') < start.index('cutCameraToHeart')
assert 'mDefeatSequence.start(conquerorSeatId, heartTileX, heartTileY, startTime)' in start and 'updateDefeatSequence(startTime);' in start
assert 'mDefeatSequence.advanceTo(now)' in function(game_mode, 'void GameMode::updateDefeatSequence(')
# The reason for the clock: onFrameStarted of the current mode runs twice per frame
assert 'currentMode->onFrameStarted(evt);' in function(frame_listener, 'bool ODFrameListener::frameStarted(')
assert 'mModeManager->update(evt);' in function(frame_listener, 'bool ODFrameListener::frameRenderingQueued(')
assert 'currentMode->onFrameStarted(evt);' in function(mode_manager, 'void ModeManager::update(')
assert 'mDefeatSequence.isHeartKnown()' in start and 'getCameraViewTarget()' in start
assert 'onDefeatSequenceFinished' in game_mode_header
assert 'startDefeatSequence(conquerorSeatId, heartTileX, heartTileY)' in client
print('WIRING OK: camera controls blocked, frame update called, start is one-shot, unknown heart keeps the camera target')
for name in ('HeartExplosion', 'DefeatSwirl'):
    script = (repo / ('particles/' + name + '.particle')).read_text()
    assert 'particle_system ' + name in script
added = sequence_header + function(game_mode, 'void GameMode::startDefeatSequence(') + function(game_mode, 'void GameMode::updateDefeatSequence(')
assert re.search(r'\bauto\b', added) is None
print('STYLE OK: no auto in the new code')

with tempfile.TemporaryDirectory(prefix='odp-defeat-sequence-') as directory:
    work = Path(directory)
    (work / 'check.cpp').write_text(probe)
    subprocess.run(['cl', '/nologo', '/EHsc', '/MD', '/std:c++14', '/I', str(repo / 'source'), 'check.cpp', '/Fecheck.exe'],
                   cwd=work, check=True)
    # Windows may block a freshly compiled executable (WinError 4551) for a moment
    for attempt in range(4):
        try:
            subprocess.run([str(work / 'check.exe')], cwd=work, check=True)
            break
        except OSError as error:
            if attempt == 3:
                raise
            print('retrying after', error)
            time.sleep(2)
