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

#ifndef WALLTORCHCONFIG_H
#define WALLTORCHCONFIG_H

#include "rooms/WallTorches.h"

#include <cstdint>

//! \brief The values of the wall torches in config/rooms.cfg. A key that is missing gets its default (the
//! same as in that file) and is logged once.
//! The placement values are used by the server only. The light values are only read by the client.
class WallTorchConfig
{
public:
    //! \brief Reads the values from the rooms configuration (the ConfigManager has to be loaded)
    static WallTorchConfig load();

    //! Room side divisor (WallTorchRoomSideDivisor), corridor spacing (WallTorchCorridorSpacing) and
    //! minimum distance (WallTorchMinDistance)
    WallTorchPlacement mPlacement;
    //! WallTorchActiveLights: torches nearest to the camera that cast real light
    uint32_t mActiveLights;
    //! WallTorchActiveLightsReduced: the same with the reduced effects setting
    uint32_t mActiveLightsReduced;
    //! WallTorchLightRadius: range of the light in tiles
    double mLightRadius;
    //! WallTorchLightColorR/G/B (0 to 1)
    double mLightColorR;
    double mLightColorG;
    double mLightColorB;
    //! WallTorchLightIntensity: also used for campaign maps that have their own map lights
    double mLightIntensity;
    //! WallTorchFlickerStrength (0 to 1) and WallTorchFlickerSpeed
    double mFlickerStrength;
    double mFlickerSpeed;

private:
    WallTorchConfig();
};

#endif // WALLTORCHCONFIG_H
