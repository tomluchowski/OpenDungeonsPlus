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

#ifndef KEEPERWEALTH_H
#define KEEPERWEALTH_H

#include "rooms/TreasurySettings.h"

//! \brief The wealth tier of a keeper: how rich it is, judged by the gold it holds against the capacity of its
//! treasuries.
namespace KeeperWealth
{
//! Tier 0: not rich. Tier 1: gold held reaches the share of the treasury capacity and at least the minimum
//! amount (config/treasury.cfg PortalRichShare and PortalRichMinGold). More tiers can be added at the end.
inline int tier(int gold, int goldMax)
{
    if(goldMax <= 0 || gold < TreasurySettings::current().portalRichMinGold)
        return 0;
    return (static_cast<float>(gold) >= TreasurySettings::current().portalRichShare * static_cast<float>(goldMax)) ? 1 : 0;
}
}

#endif // KEEPERWEALTH_H
