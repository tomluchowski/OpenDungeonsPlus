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

#ifndef TRAPTRIGGER_H
#define TRAPTRIGGER_H

#include "Trap.h"
#include "traps/TrapType.h"

//! \brief A pressure trap without damage. When an enemy steps on it, it sets off every trap and
//! door weapon of the same owner on the eight surrounding tiles, and so every trigger trap next to it.
class TrapTrigger : public Trap
{
public:
    //! \brief Creates a trigger trap and reads its reload time and number of shots from the config.
    TrapTrigger(GameMap* gameMap);

    //! \brief The type of this trap.
    virtual const TrapType getType() const override
    { return TrapType::trigger; }

    //! \brief When an enemy stands on the tile (or another trigger trap sets this one off), sets off every
    //! trap and door weapon of the owner on the eight surrounding tiles.
    //! \return True if at least one neighbouring trap fired.
    virtual bool shoot(Tile* tile) override;

    //! \brief The trigger trap cannot be attacked.
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

    //! \brief The trap type of the trigger trap.
    static const TrapType mTrapType;
};

#endif // TRAPTRIGGER_H
