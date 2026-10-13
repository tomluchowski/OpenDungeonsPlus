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

#ifndef TRAPFEAR_H
#define TRAPFEAR_H

#include "Trap.h"
#include "traps/TrapType.h"

//! \brief A trap that scares the enemy creatures in sight and makes them run away. It deals no damage.
class TrapFear : public Trap
{
public:
    //! \brief Creates a fear trap on the given map
    TrapFear(GameMap* gameMap);

    //! \brief Returns the trap type (fear)
    virtual const TrapType getType() const override
    { return TrapType::fear; }

    //! \brief Scares every enemy creature in sight of the tile so that it runs away; fearless creatures are skipped
    virtual bool shoot(Tile* tile) override;
    //! \brief The fear trap cannot be attacked
    virtual bool isAttackable(Tile* tile, Seat* seat) const override
    {
        return false;
    }

    //! \brief The trap mesh is shown on the tile
    virtual bool displayTileMesh() const override
    { return true; }

    //! \brief The trap object covers the whole tile under
    //! but while it built, the ground tile still must be shown.
    virtual bool shouldDisplayGroundTile() const override
    { return true; }

    //! \brief Creates the entity that shows the trap on the tile
    virtual TrapEntity* getTrapEntity(Tile* tile) override;

    static const TrapType mTrapType;

private:
    //! \brief Radius in tiles in which enemy creatures get scared
    uint32_t mRadius;
    //! \brief Number of turns the scared creatures run away
    uint32_t mFearTurns;
};

#endif // TRAPFEAR_H
