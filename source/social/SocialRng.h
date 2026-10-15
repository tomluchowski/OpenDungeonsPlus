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

#ifndef SOCIALRNG_H
#define SOCIALRNG_H

#include <cstddef>
#include <stdint.h>
#include <string>

namespace social
{

//! \brief 64-bit FNV-1a hash of a string. Identical on every platform.
inline uint64_t fnv1a64(const std::string& text)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    for(std::size_t i = 0; i < text.size(); ++i)
    {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(text[i]));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

//! \brief Tiny splitmix64 generator used to derive creature profiles from a name.
//! It is deliberately not utils/Random and does not use the std distributions: the
//! results have to be bit-identical on every machine and every compiler.
class Rng
{
public:
    explicit Rng(uint64_t seed) :
        mState(seed)
    {
    }

    uint64_t next()
    {
        mState += 0x9E3779B97F4A7C15ULL;
        uint64_t z = mState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    //! \brief Returns a value in [0, count[ (integer modulo only), or 0 if count is 0.
    uint32_t below(uint32_t count)
    {
        uint64_t value = next();
        if(count == 0)
            return 0;
        return static_cast<uint32_t>(value % count);
    }

private:
    uint64_t mState;
};

//! \brief One independent random stream per field of a creature: adding a field or
//! changing the pool of another field does not reshuffle the others.
inline Rng makeFieldRng(const std::string& creatureName, const std::string& field)
{
    return Rng(fnv1a64(creatureName) ^ fnv1a64("field:" + field));
}

}

#endif // SOCIALRNG_H
