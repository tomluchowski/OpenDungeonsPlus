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

#ifndef DEFEATHEARTBURST_H
#define DEFEATHEARTBURST_H

#include <cmath>
#include <cstdint>
#include <vector>

//! Look and timing of the heart that bursts at the start of the defeat sequence and of the rubble it
//! leaves. Times are seconds of the defeat timeline (see DefeatSequence), lengths are in tiles.
namespace DefeatHeartBurstSettings
{
    //! The copy of the heart shakes and glows from the start until it bursts at this time
    const float BURST_TIME = 0.8f;
    //! Largest sideways offset of the shaking heart, reached just before the burst
    const float SHAKE_AMPLITUDE = 0.07f;
    //! Largest extra size of the pulsing heart (0.08 is 8 percent), reached just before the burst
    const float PULSE_AMPLITUDE = 0.08f;
    //! Height above the floor where the burst effects start (the heart mesh is about 2.5 high)
    const float BURST_HEIGHT = 1.0f;
    //! Number of rubble pieces that stay on the platform
    const uint32_t RUBBLE_PIECES = 18;
    //! The rubble centres end within this distance of the heart centre (the platform is 3 by 3 tiles)
    const float RUBBLE_RADIUS = 0.85f;
    //! A piece waits up to this long after the burst before it starts to fall
    const float RUBBLE_MAX_DELAY = 0.25f;
    //! Every piece lies still at this time after the burst
    const float RUBBLE_SETTLE_DURATION = 1.0f;
    //! Seed of the pseudo-random rubble layout, so that every defeat shows the same pile
    const uint32_t RUBBLE_SEED = 20260925u;
}

//! One rubble piece: where it starts at the burst and where it rests, relative to the heart centre on
//! the floor. Angles are in degrees: tiltX and tiltY lean the piece, turn is its heading.
struct DefeatRubblePiece
{
    float startX;
    float startY;
    float startZ;
    float restX;
    float restY;
    float restZ;
    float tiltX;
    float tiltY;
    float turn;
    //! Extra heading while the piece flies, it runs down to 0 when the piece lands
    float spin;
    //! Height of the arc the piece flies on above the straight line from start to rest
    float arc;
    float delay;
    float scaleX;
    float scaleY;
    float scaleZ;
};

//! Where a rubble piece is at a given time (same units as DefeatRubblePiece)
struct DefeatRubblePose
{
    float x;
    float y;
    float z;
    float tiltX;
    float tiltY;
    float turn;
};

namespace DefeatHeartBurst
{
    const float PI = 3.14159265f;

    //! 0 at the start, rising linearly to 1 at the burst, 0 outside of that time
    inline float heartRampAt(float t)
    {
        if(t < 0.0f || t >= DefeatHeartBurstSettings::BURST_TIME)
            return 0.0f;
        return t / DefeatHeartBurstSettings::BURST_TIME;
    }

    //! The copy of the heart is shown from the start until the burst
    inline bool isHeartShownAt(float t)
    { return t >= 0.0f && t < DefeatHeartBurstSettings::BURST_TIME; }

    inline bool isBurstDueAt(float t)
    { return t >= DefeatHeartBurstSettings::BURST_TIME; }

    //! Sideways offset of the shaking heart; the shaking grows until the burst
    inline float heartShakeXAt(float t)
    { return DefeatHeartBurstSettings::SHAKE_AMPLITUDE * heartRampAt(t) * std::sin(2.0f * PI * 13.0f * t); }

    inline float heartShakeYAt(float t)
    { return DefeatHeartBurstSettings::SHAKE_AMPLITUDE * heartRampAt(t) * std::sin(2.0f * PI * 17.0f * t + 1.3f); }

    //! Size factor of the pulsing heart, between 1 and 1 + PULSE_AMPLITUDE
    inline float heartPulseScaleAt(float t)
    {
        const float wave = 0.5f + 0.5f * std::sin(2.0f * PI * 6.0f * t);
        return 1.0f + DefeatHeartBurstSettings::PULSE_AMPLITUDE * heartRampAt(t) * wave;
    }

    //! How hot the heart glows, between 0 (its normal look) and 1; it flickers and grows until the burst
    inline float heartGlowAt(float t)
    {
        const float flicker = 0.6f + 0.4f * std::sin(2.0f * PI * 9.0f * t);
        return heartRampAt(t) * flicker;
    }

    //! Next value of the pseudo-random generator of the layout, in [0, 1)
    inline float nextRandom(uint32_t& state)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / 16777216.0f;
    }

    //! The rubble pile: always the same pieces, placed like seeds of a sunflower (even spread over a
    //! disc) with random jitter. Pieces near the middle lie higher and lean more, as if piled on each other.
    inline std::vector<DefeatRubblePiece> buildRubbleLayout()
    {
        std::vector<DefeatRubblePiece> pieces;
        uint32_t state = DefeatHeartBurstSettings::RUBBLE_SEED;
        const uint32_t count = DefeatHeartBurstSettings::RUBBLE_PIECES;
        for(uint32_t i = 0; i < count; ++i)
        {
            DefeatRubblePiece piece;
            const float spread = std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(count));
            const float radius = DefeatHeartBurstSettings::RUBBLE_RADIUS * spread * (0.8f + 0.2f * nextRandom(state));
            const float angle = 2.39996f * static_cast<float>(i) + 0.5f * (nextRandom(state) - 0.5f);
            // 1 in the middle of the pile, 0 at its border
            const float middle = 1.0f - radius / DefeatHeartBurstSettings::RUBBLE_RADIUS;
            piece.restX = radius * std::cos(angle);
            piece.restY = radius * std::sin(angle);
            piece.restZ = 0.03f + 0.25f * middle * (0.5f + 0.5f * nextRandom(state));
            const float maxTilt = 8.0f + 32.0f * middle;
            piece.tiltX = maxTilt * (2.0f * nextRandom(state) - 1.0f);
            piece.tiltY = maxTilt * (2.0f * nextRandom(state) - 1.0f);
            piece.turn = 360.0f * nextRandom(state);
            // Two statements: the order of two calls in one expression is unspecified in C++
            const float spinSign = nextRandom(state) < 0.5f ? -1.0f : 1.0f;
            piece.spin = spinSign * (120.0f + 240.0f * nextRandom(state));
            // The pieces fly out of the heart's middle
            piece.startX = 0.25f * piece.restX;
            piece.startY = 0.25f * piece.restY;
            piece.startZ = 0.6f + 1.0f * nextRandom(state);
            piece.arc = 0.2f + 0.4f * nextRandom(state);
            piece.delay = DefeatHeartBurstSettings::RUBBLE_MAX_DELAY * nextRandom(state);
            piece.scaleX = 0.7f + 0.7f * nextRandom(state);
            piece.scaleY = 0.6f + 0.6f * nextRandom(state);
            piece.scaleZ = 0.8f + 0.8f * nextRandom(state);
            pieces.push_back(piece);
        }
        return pieces;
    }

    //! 0 while the piece waits after the burst, rising to 1 when it has landed
    inline float rubbleFallProgress(const DefeatRubblePiece& piece, float sinceBurst)
    {
        const float fall = DefeatHeartBurstSettings::RUBBLE_SETTLE_DURATION - DefeatHeartBurstSettings::RUBBLE_MAX_DELAY;
        const float progress = (sinceBurst - piece.delay) / fall;
        if(progress <= 0.0f)
            return 0.0f;
        if(progress >= 1.0f)
            return 1.0f;
        return progress;
    }

    //! Where the piece is sinceBurst seconds after the burst: it flies from its start on an arc to its
    //! rest position, turning, and lies still from RUBBLE_SETTLE_DURATION on
    inline DefeatRubblePose rubblePoseAt(const DefeatRubblePiece& piece, float sinceBurst)
    {
        const float p = rubbleFallProgress(piece, sinceBurst);
        DefeatRubblePose pose;
        pose.x = piece.startX + (piece.restX - piece.startX) * p;
        pose.y = piece.startY + (piece.restY - piece.startY) * p;
        pose.z = piece.startZ + (piece.restZ - piece.startZ) * p + piece.arc * 4.0f * p * (1.0f - p);
        pose.tiltX = piece.tiltX * p;
        pose.tiltY = piece.tiltY * p;
        pose.turn = piece.turn + piece.spin * (1.0f - p);
        return pose;
    }
}

#endif // DEFEATHEARTBURST_H
