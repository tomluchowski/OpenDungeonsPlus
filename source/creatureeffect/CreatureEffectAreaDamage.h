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


#ifndef CREATUREEFFECTAREADAMAGE_H
#define CREATUREEFFECTAREADAMAGE_H

#include "creatureeffect/CreatureEffect.h"

//! \brief A zone of damage around a tile (gas cloud, hail storm). The effect sits on the caster
//! and, every turn while it lasts, hurts every enemy creature of the caster that stands within
//! the radius of the tile. The zone ends when the effect runs out or when the caster dies.
class CreatureEffectAreaDamage : public CreatureEffect
{
public:
    CreatureEffectAreaDamage(int32_t nbTurnsEffect, int32_t tileX, int32_t tileY, double radius,
            double damagePerTurn, const std::string& particleEffectName) :
        CreatureEffect(nbTurnsEffect, particleEffectName),
        mTileX(tileX),
        mTileY(tileY),
        mRadius(radius),
        mDamagePerTurn(damagePerTurn)
    {}

    CreatureEffectAreaDamage() :
        CreatureEffect(),
        mTileX(0),
        mTileY(0),
        mRadius(0.0),
        mDamagePerTurn(0.0)
    {}

    virtual ~CreatureEffectAreaDamage()
    {}

    virtual const std::string& getEffectName() const override;

    virtual void exportToStream(std::ostream& os) const override;
    virtual bool importFromStream(std::istream& is) override;

    static CreatureEffectAreaDamage* load(std::istream& is);

protected:
    virtual void applyEffect(Creature& creature) override;

private:
    int32_t mTileX;
    int32_t mTileY;
    double mRadius;
    double mDamagePerTurn;
};

#endif // CREATUREEFFECTAREADAMAGE_H
