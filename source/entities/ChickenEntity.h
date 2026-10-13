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

#ifndef CHICKENENTITY_H
#define CHICKENENTITY_H

#include "entities/RenderedMovableEntity.h"

#include <string>
#include <iosfwd>

class Creature;
class Room;
class GameMap;
class Tile;
class ODPacket;

class ChickenEntity: public RenderedMovableEntity
{
public:
    ChickenEntity(GameMap* gameMap, const std::string& hatcheryName);
    ChickenEntity(GameMap* gameMap);

    virtual void doUpkeep() override;

    virtual double getMoveSpeed() const override
    { return 0.4; }

    virtual GameEntityType getObjectType() const override;

    virtual bool tryPickup(Seat* seat) override;
    virtual void pickup() override;
    virtual bool tryDrop(Seat* seat, Tile* tile) override;

    virtual void correctEntityMovePosition(Ogre::Vector2& position) override;

    bool eatChicken(Creature* creature);

    bool canSlap(Seat* seat) override;

    void slap() override
    { mIsSlapped = true; }

    inline bool getLockEat(const Creature& worker) const
    { return mLockedEat; }

    //! \brief Locks the chicken for the creature or releases it. Only the creature that holds the lock
    //! can release it. Taking the lock from another creature is remembered for the relationships
    //! (see getSnatchedFrom).
    void setLockEat(const Creature& worker, bool lock);

    //! \brief Relationships option only: true if the creature may take away this chicken from the
    //! creature of the same keeper that locked it, because it is right next to the chicken and
    //! closer to it than the one that locked it.
    bool canSnatch(const Creature& creature) const;

    //! \brief Name of the creature that lost the chicken to the one that ate it, empty if nobody did.
    inline const std::string& getSnatchedFrom() const
    { return mSnatchedFrom; }

    static ChickenEntity* getChickenEntityFromStream(GameMap* gameMap, std::istream& is);
    static ChickenEntity* getChickenEntityFromPacket(GameMap* gameMap, ODPacket& is);
    static std::string getChickenEntityStreamFormat();
protected:
    void exportToStream(std::ostream& os) const override;
    bool importFromStream(std::istream& is) override;

private:
    enum ChickenState
    {
        free,
        eaten,
        dying
    };
    ChickenState mChickenState;
    int32_t mNbTurnOutsideHatchery;
    int32_t mNbTurnDie;
    bool mIsSlapped;
    bool mLockedEat;
    //! Name of the creature that holds the lock on this chicken, empty while it is not locked
    std::string mLockOwner;
    //! Name of the creature that lost the lock on this chicken to another one, empty if nobody did
    std::string mSnatchedFrom;

    void addTileToListIfPossible(int x, int y, Room* currentHatchery, std::vector<Tile*>& possibleTileMove);
};

#endif // CHICKENENTITY_H
