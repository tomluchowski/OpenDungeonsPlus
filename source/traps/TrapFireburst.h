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

#ifndef TRAPFIREBURST_H
#define TRAPFIREBURST_H

#include "Trap.h"
#include "traps/TrapType.h"

//! \brief A hidden trap that bursts into flames when an enemy comes close. Everybody in the area takes damage.
class TrapFireburst : public Trap
{
public:
    //! \brief Creates a fireburst trap and reads its reload time, damage, number of shots and radius from the config.
    TrapFireburst(GameMap* gameMap);

    //! \brief The type of this trap.
    virtual const TrapType getType() const override
    { return TrapType::fireburst; }

    //! \brief Bursts into flames when an enemy creature comes within the radius of the given tile.
    //! \return True if the burst went off, false if no enemy was close.
    virtual bool shoot(Tile* tile) override;

    //! \brief The fireburst trap cannot be attacked.
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

    //! \brief The trap type of the fireburst trap.
    static const TrapType mTrapType;

private:
    //! \brief Radius in tiles of the fire burst
    uint32_t mRadius;
};

#endif // TRAPFIREBURST_H
