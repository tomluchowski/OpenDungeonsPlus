/*
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

#ifndef DEFEATSEQUENCE_H
#define DEFEATSEQUENCE_H

#include <chrono>
#include <cstdint>
#include <string>

//! \brief Timings (seconds after the heart was destroyed) and look of the defeat sequence.
namespace DefeatSequenceSettings
{
    //! The heart explosion effect and its subtitle last until this time
    const float EXPLOSION_END = 15.0f;
    //! The red tint fades out between EXPLOSION_END and this time
    const float TINT_END = 15.5f;
    const float TINT_ALPHA = 0.35f;
    const float SWIRL_START = 15.5f;
    const float SWIRL_END = 18.5f;
    //! Distance in tiles the swirl travels away from the heart
    const float SWIRL_DISTANCE = 14.0f;
    const float FADE_START = 19.5f;
    const float FADE_END = 28.5f;
    const float SECOND_SUBTITLE_START = 20.5f;
    //! End of the timeline; the debriefing window opens on the black screen afterwards
    const float FINISH = 29.5f;

    //! Low oblique camera used for the cut: height above the floor and pitch in degrees
    //! (0 looks straight down, like the game camera's default of 25).
    //! The pitch plus half the vertical field of view (22.5 degrees for the game camera's 45)
    //! must stay below 90, otherwise the upper corner rays of the view never reach the floor
    //! and CullingManager cannot compute the visible tiles.
    const float CAMERA_HEIGHT = 2.0f;
    const float CAMERA_PITCH = 60.0f;
}

//! Seconds elapsed in the game: the turn number divided by the turns per second.
//! Unusable input (no turns yet, no turn rate) gives 0.
inline int64_t debriefingElapsedSeconds(int64_t turnNumber, double turnsPerSecond)
{
    if(turnNumber <= 0 || turnsPerSecond <= 0.0)
        return 0;
    return static_cast<int64_t>(static_cast<double>(turnNumber) / turnsPerSecond);
}

//! Formats seconds as mm:ss, or h:mm:ss from one hour on. Negative values give 00:00.
inline std::string formatDebriefingTime(int64_t seconds)
{
    if(seconds < 0)
        seconds = 0;
    const int64_t hours = seconds / 3600;
    const int64_t minutes = (seconds / 60) % 60;
    const int64_t rest = seconds % 60;
    std::string result;
    if(hours > 0)
        result += std::to_string(hours) + ":";
    if(minutes < 10)
        result += "0";
    result += std::to_string(minutes) + ":";
    if(rest < 10)
        result += "0";
    result += std::to_string(rest);
    return result;
}

//! \brief State and timeline of the defeat sequence.
//! Everything that depends on the elapsed time is a static function so that it can be checked
//! without a running game.
class DefeatSequence
{
public:
    enum class Phase
    {
        Inactive,
        Explosion,
        Swirl,
        Fade,
        Finished
    };

    DefeatSequence() :
        mStarted(false),
        mFinished(false),
        mDebriefingOpen(false),
        mDebriefingConfirmed(false),
        mElapsed(0.0f),
        mStartTime(),
        mConquerorSeatId(-1),
        mHeartTileX(-1),
        mHeartTileY(-1)
    {
    }

    //! \brief Starts the sequence at startTime (wall clock). Returns false (and changes nothing) if it
    //! was already started.
    bool start(int32_t conquerorSeatId, int32_t heartTileX, int32_t heartTileY,
        std::chrono::steady_clock::time_point startTime)
    {
        if(mStarted)
            return false;
        mStarted = true;
        mStartTime = startTime;
        mConquerorSeatId = conquerorSeatId;
        mHeartTileX = heartTileX;
        mHeartTileY = heartTileY;
        return true;
    }

    //! \brief Moves the timeline to the wall-clock time now: the elapsed time is now minus the start
    //! time, not a sum of frame times. Calling it again with the same or an earlier time changes
    //! nothing, so it does not matter how often per frame it runs, and a long frame moves the
    //! timeline by exactly its length. Returns true exactly once, on the call that reaches the end.
    bool advanceTo(std::chrono::steady_clock::time_point now)
    {
        if(!mStarted || mFinished)
            return false;
        const float time = secondsBetween(mStartTime, now);
        if(time > mElapsed)
            mElapsed = time;
        if(mElapsed < DefeatSequenceSettings::FINISH)
            return false;
        mFinished = true;
        return true;
    }

    //! Opens the debriefing after the end of the timeline. Returns true exactly once.
    bool openDebriefing()
    {
        if(!mFinished || mDebriefingOpen)
            return false;
        mDebriefingOpen = true;
        return true;
    }

    //! The confirm button of the debriefing was clicked. Returns true exactly once, and only
    //! while the debriefing is open, so a double click leaves the game only once.
    bool confirmDebriefing()
    {
        if(!mDebriefingOpen || mDebriefingConfirmed)
            return false;
        mDebriefingConfirmed = true;
        return true;
    }

    //! True from the start on, also after the end: the screen stays black and the game gets no input.
    bool isStarted() const
    { return mStarted; }
    bool blocksInput() const
    { return mStarted; }
    //! While the debriefing is open the mouse still reaches the interface (not the game),
    //! so that its button can be clicked. Before that everything stays blocked.
    bool allowsGuiInput() const
    { return mDebriefingOpen; }
    bool isDebriefingOpen() const
    { return mDebriefingOpen; }
    bool isFinished() const
    { return mFinished; }
    float getElapsed() const
    { return mElapsed; }
    int32_t getConquerorSeatId() const
    { return mConquerorSeatId; }
    int32_t getHeartTileX() const
    { return mHeartTileX; }
    int32_t getHeartTileY() const
    { return mHeartTileY; }
    //! \brief The heart position is unknown when the server sent -1
    bool isHeartKnown() const
    { return mHeartTileX >= 0 && mHeartTileY >= 0; }
    //! \brief The swirl needs the seat colour of the conqueror; without a conqueror it is skipped
    bool isSwirlWanted() const
    { return mConquerorSeatId >= 0; }

    //! Seconds from start to now; 0 when now is not after start
    static float secondsBetween(std::chrono::steady_clock::time_point start,
        std::chrono::steady_clock::time_point now)
    {
        if(now <= start)
            return 0.0f;
        const std::chrono::duration<float> seconds = now - start;
        return seconds.count();
    }

    static Phase phaseAt(float t)
    {
        if(t < 0.0f)
            return Phase::Inactive;
        if(t < DefeatSequenceSettings::SWIRL_START)
            return Phase::Explosion;
        if(t < DefeatSequenceSettings::SWIRL_END)
            return Phase::Swirl;
        if(t < DefeatSequenceSettings::FINISH)
            return Phase::Fade;
        return Phase::Finished;
    }

    static bool isExplosionEffectActiveAt(float t)
    { return t >= 0.0f && t < DefeatSequenceSettings::EXPLOSION_END; }

    static bool isSwirlActiveAt(float t)
    { return t >= DefeatSequenceSettings::SWIRL_START && t < DefeatSequenceSettings::SWIRL_END; }

    //! \brief 0 when the swirl starts at the heart, 1 when it has arrived
    static float swirlProgressAt(float t)
    {
        if(t <= DefeatSequenceSettings::SWIRL_START)
            return 0.0f;
        if(t >= DefeatSequenceSettings::SWIRL_END)
            return 1.0f;
        return (t - DefeatSequenceSettings::SWIRL_START)
            / (DefeatSequenceSettings::SWIRL_END - DefeatSequenceSettings::SWIRL_START);
    }

    //! \brief Opacity of the red tint over the scene
    static float redTintAlphaAt(float t)
    {
        if(t < 0.0f || t >= DefeatSequenceSettings::TINT_END)
            return 0.0f;
        if(t < DefeatSequenceSettings::EXPLOSION_END)
            return DefeatSequenceSettings::TINT_ALPHA;
        return DefeatSequenceSettings::TINT_ALPHA
            * (1.0f - (t - DefeatSequenceSettings::EXPLOSION_END)
                / (DefeatSequenceSettings::TINT_END - DefeatSequenceSettings::EXPLOSION_END));
    }

    //! \brief Opacity of the black cover: 0 until FADE_START, rising linearly to 1 at FADE_END
    static float blackAlphaAt(float t)
    {
        if(t <= DefeatSequenceSettings::FADE_START)
            return 0.0f;
        if(t >= DefeatSequenceSettings::FADE_END)
            return 1.0f;
        return (t - DefeatSequenceSettings::FADE_START)
            / (DefeatSequenceSettings::FADE_END - DefeatSequenceSettings::FADE_START);
    }

    static bool isFirstSubtitleVisibleAt(float t)
    { return t >= 0.0f && t < DefeatSequenceSettings::EXPLOSION_END; }

    static bool isSecondSubtitleVisibleAt(float t)
    { return t >= DefeatSequenceSettings::SECOND_SUBTITLE_START; }

private:
    bool mStarted;
    bool mFinished;
    bool mDebriefingOpen;
    bool mDebriefingConfirmed;
    float mElapsed;
    std::chrono::steady_clock::time_point mStartTime;
    int32_t mConquerorSeatId;
    int32_t mHeartTileX;
    int32_t mHeartTileY;
};

#endif // DEFEATSEQUENCE_H
