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

//! \brief Rules of the dungeon heart health ring of the top-left HUD badge. Kept free of
//! any engine type so that the server rules, the client state and the drawing share them.
namespace HeartHealthRing
{
    //! Where a full ring starts, in degrees clockwise from the top.
    const float RING_START_DEGREES = 15.0f;

    //! Length of a full ring in degrees (it ends at 345 degrees).
    const float RING_FULL_SPAN_DEGREES = 330.0f;

    //! The server only tells the owner about changes of at least one percentage point.
    const float NOTIFY_STEP = 0.01f;

    //! A change of exactly NOTIFY_STEP must count although float rounding can leave it a little short.
    const float NOTIFY_TOLERANCE = 0.0001f;

    const float PI = 3.14159265f;

    //! Seconds without a further message after which the "under attack" glow is switched off.
    const float ATTACK_GLOW_SECONDS = 3.0f;

    inline float clampFraction(float fraction)
    {
        // Written so that a NaN ends up as 0
        if(!(fraction > 0.0f))
            return 0.0f;
        return std::min(1.0f, fraction);
    }

    //! \brief Number of degrees of the ring that are shown for a health fraction.
    inline float visibleSpanDegrees(float fraction)
    {
        return RING_FULL_SPAN_DEGREES * clampFraction(fraction);
    }

    //! \brief True if the ring pixel at (dx, dy) from the badge centre (y pointing down) is part
    //! of the visible arc: clockwise from RING_START_DEGREES over the visible span.
    inline bool isRingLit(float dx, float dy, float fraction)
    {
        const float span = visibleSpanDegrees(fraction);
        if(span <= 0.0f)
            return false;

        float degrees = std::atan2(dx, -dy) * 180.0f / PI;
        if(degrees < 0.0f)
            degrees += 360.0f;
        return degrees >= RING_START_DEGREES && degrees <= RING_START_DEGREES + span;
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
        return std::abs(fraction - lastSent) >= NOTIFY_STEP - NOTIFY_TOLERANCE;
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
