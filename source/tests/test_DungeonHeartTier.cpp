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

// This local Boost install has no separately built unit_test_framework
// library (see docs/development/WINDOWS-DEV-SETUP.md's Boost component
// list), so this test uses Boost.Test's header-only "included" form instead
// of the usual BoostTestTargetConfig.h, which needs that missing library.
// OVERRIDE_BOOST_TEST_INCLUDED_WARNING
#define BOOST_TEST_MODULE DungeonHeartTier
#include <boost/test/included/unit_test.hpp>

#include "rooms/HeartHealthTier.h"

BOOST_AUTO_TEST_CASE(test_DungeonHeartTier)
{
    // Full and lightly damaged health stays healthy.
    BOOST_CHECK(computeHeartHealthTierFromFraction(1.0) == HeartHealthTier::healthy);
    BOOST_CHECK(computeHeartHealthTierFromFraction(2.0 / 3.0) == HeartHealthTier::healthy);

    // Just below the healthy cutoff and down to the critical cutoff is damaged.
    BOOST_CHECK(computeHeartHealthTierFromFraction(2.0 / 3.0 - 0.01) == HeartHealthTier::damaged);
    BOOST_CHECK(computeHeartHealthTierFromFraction(0.5) == HeartHealthTier::damaged);
    BOOST_CHECK(computeHeartHealthTierFromFraction(1.0 / 3.0) == HeartHealthTier::damaged);

    // Below the critical cutoff, and a fully destroyed heart, is critical.
    BOOST_CHECK(computeHeartHealthTierFromFraction(1.0 / 3.0 - 0.01) == HeartHealthTier::critical);
    BOOST_CHECK(computeHeartHealthTierFromFraction(0.0) == HeartHealthTier::critical);
}
