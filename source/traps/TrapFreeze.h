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

#ifndef TRAPFREEZE_H
#define TRAPFREEZE_H

#include "Trap.h"
#include "traps/TrapType.h"

//! \brief A pressure trap that paralyses every enemy creature standing on its tile when it fires.
//! Frozen creatures can neither move nor fight. A creature that is low on health when caught shatters and dies.
class TrapFreeze : public Trap
{
public:
    //! \brief Creates a freeze trap and reads its reload time, number of shots, freeze time and shatter limit from the config.
    TrapFreeze(GameMap* gameMap);

    //! \brief The type of this trap.
    virtual const TrapType getType() const override
    { return TrapType::freeze; }

    //! \brief Freezes every enemy creature standing on the tile; one with little health left shatters.
    //! \return True if at least one creature was frozen or shattered.
    virtual bool shoot(Tile* tile) override;

    //! \brief The freeze trap cannot be attacked.
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

    //! \brief The trap type of the freeze trap.
    static const TrapType mTrapType;

private:
    //! \brief How many turns the creatures on the tile stay frozen
    int32_t mFreezeTurns;

    //! \brief A creature whose health is at or below this percentage of its maximum health shatters
    double mShatterHpPercent;
};

#endif // TRAPFREEZE_H
