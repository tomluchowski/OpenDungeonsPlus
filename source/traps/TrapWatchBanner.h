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

#ifndef TRAPWATCHBANNER_H
#define TRAPWATCHBANNER_H

#include "Trap.h"
#include "traps/TrapType.h"

#include <cstdint>

//! \brief A post that shoots nothing. Guards of the owner's guard rooms patrol to it, and it calls them
//! when it notices an enemy within its aura. Calling the guards is its "shot": the clients are told (flag flares), and
//! the post then reloads for GuardRoomDistressSeconds (the flag hangs) until it may call again.
class TrapWatchBanner : public Trap
{
public:
    //! \brief Creates a watch banner that has not called the guards yet and starts loaded.
    TrapWatchBanner(GameMap* gameMap);

    //! \brief The type of this trap.
    virtual const TrapType getType() const override
    { return TrapType::watchBanner; }

    //! \brief The watch banner never fires.
    virtual bool shoot(Tile* tile) override
    { return false; }

    //! \brief The watch banner cannot be attacked.
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

    //! \brief Looks for enemy creatures within the aura of an installed banner and, if there is one,
    //! sends the guards of the owner's guard rooms to it (at most once per distress period). The call is reported to
    //! the clients as the shot of the post, then as a reload, and once more when the post is loaded again.
    virtual void doUpkeep() override;

    //! \brief Creates the entity shown on the given tile.
    virtual TrapEntity* getTrapEntity(Tile* tile) override;

    //! \brief The trap type of the watch banner.
    static const TrapType mTrapType;

private:
    //! \brief Turn from which the post may call the guards again
    int64_t mNextDistressTurn;
    //! \brief The post has called the guards and is reloading (not saved: a loaded post starts loaded)
    bool mReloading;
};

#endif // TRAPWATCHBANNER_H
