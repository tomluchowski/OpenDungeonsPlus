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

#ifndef CREATUREACTIONEATCHICKEN_H
#define CREATUREACTIONEATCHICKEN_H

#include "creatureaction/CreatureAction.h"
#include "entities/GameEntity.h"

class ChickenEntity;
class GameEntity;

class CreatureActionEatChicken : public CreatureAction, public GameEntityListener
{
public:
    //! \brief gift: the keeper dropped the chicken for a creature that is not hungry. The creature sniffs at it
    //! for a while, then eats it slowly anyway (smaller meal, longer pause afterwards).
    CreatureActionEatChicken(Creature& creature, ChickenEntity& chicken, bool gift = false);
    virtual ~CreatureActionEatChicken();

    CreatureActionType getType() const override
    { return CreatureActionType::eatChicken; }

    std::function<bool()> action() override;

    std::string getListenerName() const override;
    bool notifyDead(GameEntity* entity) override;
    bool notifyRemovedFromGameMap(GameEntity* entity) override;
    bool notifyPickedUp(GameEntity* entity) override;
    bool notifyDropped(GameEntity* entity) override;

    static bool handleEatChicken(Creature& creature, ChickenEntity* chicken);

    //! \brief Eats the chicken like handleEatChicken and then replaces the hatchery meal by the gift meal.
    static bool handleGiftChicken(Creature& creature, ChickenEntity* chicken);

    //! \brief True if the creature is idle, not hungry and able to take a chicken the keeper dropped.
    static bool canAcceptGift(const Creature& creature);

private:
    ChickenEntity* mChicken;
    bool mGift;
};

#endif // CREATUREACTIONEATCHICKEN_H
