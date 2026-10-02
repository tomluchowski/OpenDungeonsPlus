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

#ifndef GIFTBOXBONUS_H
#define GIFTBOXBONUS_H

#include "entities/GiftBoxEntity.h"

#include <string>
#include <iosfwd>

class Seat;

//! \brief A gift box giving one of the one-shot bonuses: extra mana, extra gold, a
//! view of the whole map for the rest of the game or a level for the creatures of the seat that brings
//! the box to its dungeon temple. The type is one of GiftBoxType::mana, gold, revealMap
//! and levelUp. The meaning of the amount depends on the type: mana points, gold coins,
//! number of levels given. The types revealMap and healAll do not use it; healAll heals
//! every creature of the seat completely. The types makeSafe, weakenWalls,
//! stunWorkers, makeHappy, makeUnhappy and killCreatures do not use the amount either, receiveWorkers uses it
//! as the number of workers.
class GiftBoxBonus: public GiftBoxEntity
{
public:
    GiftBoxBonus(GameMap* gameMap, const std::string& baseName, GiftBoxType type, uint32_t amount);
    GiftBoxBonus(GameMap* gameMap, GiftBoxType type);

    virtual void applyEffect() override;

    inline uint32_t getAmount() const
    { return mAmount; }

    //! \brief Applies the bonus to the seat. The tile is where workers or left over gold appear.
    //! Used when the box is delivered and when a stored special is used.
    static void applyBonus(GameMap* gameMap, Seat* seat, GiftBoxType type, uint32_t amount, Tile* positionTile);

    //! \brief The name of the special, used for the button and the message
    static std::string getDisplayName(GiftBoxType type);

    //! \brief What the special does, used as the tooltip of its button
    static std::string getDescription(GiftBoxType type);

    //! \brief The amount used when a box is placed in the editor
    static uint32_t getDefaultAmount(GiftBoxType type);

protected:
    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

private:
    uint32_t mAmount;
};

#endif // GIFTBOXBONUS_H
