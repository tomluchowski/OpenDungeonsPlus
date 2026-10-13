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

#ifndef HEARTHEALTHTIER_H
#define HEARTHEALTHTIER_H

//! \brief The dungeon heart's visual health tier (see RoomDungeonTemple). Drives
//! which mesh/rig variant of the heart is displayed and how fast its pulse
//! animation runs.
enum class HeartHealthTier
{
    healthy,
    damaged,
    critical
};

//! \brief Pure health-fraction-to-tier mapping, kept free of GameMap/Room
//! dependencies so it can be unit tested directly.
//! \param healthFraction The heart's current HP divided by its maximum HP,
//! expected in [0.0, 1.0] (values outside that range are clamped to the
//! nearest tier).
HeartHealthTier computeHeartHealthTierFromFraction(double healthFraction);

#endif // HEARTHEALTHTIER_H
