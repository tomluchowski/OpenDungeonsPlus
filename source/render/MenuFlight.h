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

#ifndef MENUFLIGHT_H
#define MENUFLIGHT_H

//! \brief Timing and look of the camera flight shown when the main menu opens after the defeat debriefing.
//! The main menu is a flat picture, so the "flight" is a zoom on that picture.
namespace MenuFlightSettings
{
    //! Seconds from the start of the flight until the menu is usable
    const float DURATION = 3.0f;
    //! Size of the picture at the start, as a multiple of its normal size (1 is the normal menu view)
    const float START_SCALE = 1.6f;
    //! Longest time step taken into account, so that a slow frame cannot skip the flight
    const float MAX_STEP = 0.25f;
}

//! \brief State of the menu flight: starts once per request, runs for DURATION, then stays finished.
class MenuFlight
{
public:
    //! Progress from 0 (start) to 1 (end); times outside the flight are clamped
    static float progressAt(float time)
    {
        if(time <= 0.0f)
            return 0.0f;
        if(time >= MenuFlightSettings::DURATION)
            return 1.0f;
        return time / MenuFlightSettings::DURATION;
    }

    //! Size of the picture at a time: START_SCALE at the start, slowing down to exactly 1 at the end
    static float scaleAt(float time)
    {
        const float remaining = 1.0f - progressAt(time);
        return 1.0f + (MenuFlightSettings::START_SCALE - 1.0f) * remaining * remaining * remaining;
    }

    //! Starts the flight. Returns false (and changes nothing) while a flight is already running.
    bool start()
    {
        if(mActive)
            return false;
        mActive = true;
        mElapsed = 0.0f;
        return true;
    }

    //! Advances by a frame time (at most MAX_STEP). Returns true on the step that ends the flight.
    bool advance(float elapsed)
    {
        if(!mActive)
            return false;
        if(elapsed > MenuFlightSettings::MAX_STEP)
            elapsed = MenuFlightSettings::MAX_STEP;
        if(elapsed > 0.0f)
            mElapsed += elapsed;
        if(mElapsed < MenuFlightSettings::DURATION)
            return false;
        mElapsed = MenuFlightSettings::DURATION;
        mActive = false;
        return true;
    }

    //! Stops the flight without finishing it (the menu scene is being freed)
    void reset()
    {
        mActive = false;
        mElapsed = 0.0f;
    }

    bool isActive() const
    { return mActive; }

    //! Current size of the picture: the normal size (1) unless a flight is running
    float getScale() const
    { return mActive ? scaleAt(mElapsed) : 1.0f; }

private:
    bool mActive = false;
    float mElapsed = 0.0f;
};

#endif // MENUFLIGHT_H
