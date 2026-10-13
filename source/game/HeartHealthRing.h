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

#ifndef HEARTHEALTHRING_H
#define HEARTHEALTHRING_H

#include <algorithm>
#include <cmath>
#include <cstdint>

//! \brief Rules of the dungeon heart health ring of the top-left HUD badge. Kept free of
//! any engine type so that the server rules, the client state and the drawing share them.
namespace HeartHealthRing
{
    //! The ring is split into this many segments by spokes; one segment stands for one sixth of
    //! the health. The segments are filled clockwise from the top.
    const int SEGMENT_COUNT = 6;

    //! Angular size of one segment in degrees.
    const float SEGMENT_DEGREES = 360.0f / SEGMENT_COUNT;

    //! Half width in degrees of the spoke that separates two segments. The first spoke is at the top.
    const float SPOKE_HALF_DEGREES = 5.0f;

    //! The server only tells the owner about changes of at least one percentage point.
    const float NOTIFY_STEP = 0.01f;

    //! Seconds without a further message after which the "under attack" glow is switched off.
    const float ATTACK_GLOW_SECONDS = 3.0f;

    inline float clampFraction(float fraction)
    {
        // Written so that a NaN ends up as 0
        if(!(fraction > 0.0f))
            return 0.0f;
        return std::min(1.0f, fraction);
    }

    //! \brief Angle in degrees, clockwise from the top, of the point (dx, dy) from the badge
    //! centre (y pointing down), in [0, 360).
    inline float ringDegrees(float dx, float dy)
    {
        float degrees = std::atan2(dx, -dy) * 180.0f / 3.14159265f;
        if(degrees < 0.0f)
            degrees += 360.0f;
        if(degrees >= 360.0f)
            degrees = 0.0f;
        return degrees;
    }

    //! \brief True if the ring pixel at (dx, dy) is on one of the spokes between the segments.
    inline bool isSpoke(float dx, float dy)
    {
        const float inSegment = std::fmod(ringDegrees(dx, dy), SEGMENT_DEGREES);
        return inSegment < SPOKE_HALF_DEGREES || inSegment > SEGMENT_DEGREES - SPOKE_HALF_DEGREES;
    }

    //! \brief True if the ring pixel at (dx, dy) is lit. Segment k covers the health from k/6 to
    //! (k+1)/6: it is lit completely when the health is above that, and a segment that is only
    //! partly covered is lit clockwise over the covered part. The spokes are never lit.
    inline bool isRingLit(float dx, float dy, float fraction)
    {
        if(isSpoke(dx, dy))
            return false;

        const float degrees = ringDegrees(dx, dy);
        const int segment = std::min(SEGMENT_COUNT - 1, static_cast<int>(degrees / SEGMENT_DEGREES));
        const float covered = std::max(0.0f, std::min(1.0f,
            clampFraction(fraction) * SEGMENT_COUNT - segment));
        return degrees - segment * SEGMENT_DEGREES <= covered * SEGMENT_DEGREES;
    }

    //! \brief Health in whole percent for the tooltip, rounded to the nearest, 0 to 100.
    inline int healthPercent(double hp, double maxHP)
    {
        if(!(maxHP > 0.0))
            return 0;
        return static_cast<int>(std::floor(100.0 * clampFraction(static_cast<float>(hp / maxHP)) + 0.5));
    }

    //! \brief Server side: should the owner be told about the new fraction? lastSent is the
    //! fraction of the previous message, negative if none was sent yet.
    inline bool shouldNotify(float lastSent, float fraction)
    {
        if(lastSent < 0.0f)
            return true;
        // A destroyed heart is always announced
        if(fraction <= 0.0f)
            return lastSent > 0.0f;
        return std::abs(fraction - lastSent) >= NOTIFY_STEP - 0.0001f;
    }

    //! \brief Server side: may a message that only carries a new whole HP be sent? At most one per
    //! second. A turn counter that went backwards (a new game on the same socket) counts as due.
    inline bool isHpMessageDue(int64_t turnsSinceLastMessage, double turnsPerSecond)
    {
        return turnsSinceLastMessage < 0
            || static_cast<double>(turnsSinceLastMessage) >= turnsPerSecond;
    }

    //! \brief Client side: what the badge shows and whether it has to be redrawn.
    struct BadgeState
    {
        BadgeState() :
            mFraction(1.0f),
            mHP(-1.0),
            mMaxHP(-1.0),
            mGlow(false),
            mGlowRemaining(0.0f),
            mDirty(true)
        {}

        //! \brief Stores a message of the server. A destroyed heart never glows.
        void receive(float fraction, bool underAttack)
        {
            mFraction = clampFraction(fraction);
            mGlow = underAttack && mFraction > 0.0f;
            mGlowRemaining = mGlow ? ATTACK_GLOW_SECONDS : 0.0f;
            mDirty = true;
        }

        //! \brief Stores the exact heart HP of the last message of the server, for the tooltip.
        void setPoints(double hp, double maxHP)
        {
            mHP = hp;
            mMaxHP = maxHP;
            mDirty = true;
        }

        //! \brief Frame update: switches the glow off once its time is over.
        void update(float timeSinceLastFrame)
        {
            if(!mGlow)
                return;
            mGlowRemaining -= timeSinceLastFrame;
            if(mGlowRemaining <= 0.0f)
            {
                mGlow = false;
                mGlowRemaining = 0.0f;
                mDirty = true;
            }
        }

        //! \brief True (once) when the badge has to be drawn again.
        bool takeDirty()
        {
            const bool dirty = mDirty;
            mDirty = false;
            return dirty;
        }

        float mFraction;
        double mHP;
        double mMaxHP;
        bool mGlow;
        float mGlowRemaining;
        bool mDirty;
    };
}

#endif // HEARTHEALTHRING_H
