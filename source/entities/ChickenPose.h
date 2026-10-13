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

#ifndef CHICKENPOSE_H
#define CHICKENPOSE_H

#include <string>

//! \brief Names of the poses of hatchery animals. The server sends them as animation names, the
//! client plays a skeleton animation and adds the procedural motion for the pose.
namespace ChickenPose
{
    static const std::string strut = "Strut";
    static const std::string chase = "Chase";
    static const std::string flee = "Flee";
    static const std::string mount = "Mount";
    static const std::string cackle = "Cackle";
    static const std::string perch = "Perch";
    static const std::string crow = "Crow";
    static const std::string guard = "Guard";
    static const std::string lead = "Lead";
    static const std::string roost = "Roost";
    static const std::string lay = "Lay";
    static const std::string wobble = "Wobble";
    static const std::string emerge = "Emerge";

    //! \brief True if the name is one of the poses above.
    inline bool isPose(const std::string& name)
    {
        return (name == strut) || (name == chase) || (name == flee) || (name == mount) ||
            (name == cackle) || (name == perch) || (name == crow) || (name == guard) ||
            (name == lead) || (name == roost) || (name == lay) || (name == wobble) ||
            (name == emerge);
    }

    //! \brief True if the pose is a way of walking.
    inline bool isWalkPose(const std::string& name)
    {
        return (name == strut) || (name == chase) || (name == flee);
    }

    //! \brief The skeleton animation that is played for a pose ("Walk" or "Idle").
    inline std::string skeletonAnimation(const std::string& name)
    {
        return isWalkPose(name) ? "Walk" : "Idle";
    }
}

#endif // CHICKENPOSE_H
