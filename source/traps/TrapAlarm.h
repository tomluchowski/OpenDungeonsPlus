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

#ifndef TRAPALARM_H
#define TRAPALARM_H

#include "Trap.h"
#include "traps/TrapType.h"

//! \brief A trap that warns its owner when an enemy creature comes near. It does no damage.
class TrapAlarm : public Trap
{
public:
    //! \brief Creates an alarm trap and reads its reload time and number of warnings from the config.
    TrapAlarm(GameMap* gameMap);

    //! \brief The type of this trap.
    virtual const TrapType getType() const override
    { return TrapType::alarm; }

    //! \brief Warns the owner when an enemy creature is within the alarm radius of the given tile.
    //! \return True if the alarm went off, false if no enemy was near.
    virtual bool shoot(Tile* tile) override;

    //! \brief The alarm cannot be attacked.
    virtual bool isAttackable(Tile* tile, Seat* seat) const override
    {
        return false;
    }

    //! \brief The tile mesh is displayed under the trap.
    virtual bool displayTileMesh() const override
    { return true; }

    //! \brief The trap object covers the whole tile under
    //! but while it built, the ground tile still must be shown.
    virtual bool shouldDisplayGroundTile() const override
    { return true; }

    //! \brief Creates the entity shown on the given tile.
    virtual TrapEntity* getTrapEntity(Tile* tile) override;

    //! \brief The trap type of the alarm trap.
    static const TrapType mTrapType;
};

#endif // TRAPALARM_H

