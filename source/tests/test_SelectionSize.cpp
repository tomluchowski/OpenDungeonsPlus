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

#include "utils/SelectionSize.h"

#define BOOST_TEST_MODULE SelectionSize
#include "BoostTestTargetConfig.h"

namespace
{
struct FakeTile
{
    FakeTile(int x, int y) : mX(x), mY(y) {}
    int getX() const { return mX; }
    int getY() const { return mY; }
    int mX;
    int mY;
};

std::vector<FakeTile*> makeArea(std::vector<FakeTile>& storage, int x0, int y0, int w, int h)
{
    storage.clear();
    storage.reserve(static_cast<size_t>(w * h));
    for(int y = y0; y < y0 + h; ++y)
        for(int x = x0; x < x0 + w; ++x)
            storage.push_back(FakeTile(x, y));
    std::vector<FakeTile*> tiles;
    for(size_t i = 0; i < storage.size(); ++i)
        tiles.push_back(&storage[i]);
    return tiles;
}
}

BOOST_AUTO_TEST_CASE(test_SelectionSizeMatchesMarkedArea)
{
    std::vector<FakeTile> storage;
    int width = 0;
    int height = 0;

    BOOST_CHECK(getSelectionSize(makeArea(storage, 10, 20, 4, 3), width, height));
    BOOST_CHECK_EQUAL(width, 4);
    BOOST_CHECK_EQUAL(height, 3);

    BOOST_CHECK(getSelectionSize(makeArea(storage, 0, 0, 5, 4), width, height));
    BOOST_CHECK_EQUAL(width, 5);
    BOOST_CHECK_EQUAL(height, 4);
}

BOOST_AUTO_TEST_CASE(test_SelectionSizeIgnoresOrderAndGaps)
{
    // Only some tiles of the dragged rectangle are marked: the size is that of the marked tiles
    FakeTile a(7, 3);
    FakeTile b(4, 5);
    std::vector<FakeTile*> tiles;
    tiles.push_back(&a);
    tiles.push_back(&b);
    int width = 0;
    int height = 0;
    BOOST_CHECK(getSelectionSize(tiles, width, height));
    BOOST_CHECK_EQUAL(width, 4);
    BOOST_CHECK_EQUAL(height, 3);
}

BOOST_AUTO_TEST_CASE(test_SelectionSizeEmpty)
{
    std::vector<FakeTile*> tiles;
    int width = 5;
    int height = 5;
    BOOST_CHECK(!getSelectionSize(tiles, width, height));
    BOOST_CHECK_EQUAL(width, 0);
    BOOST_CHECK_EQUAL(height, 0);
}
