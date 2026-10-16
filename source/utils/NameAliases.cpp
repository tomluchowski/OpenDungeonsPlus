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

#include "utils/NameAliases.h"

#include <cctype>
#include <cstddef>

namespace NameAliases
{
namespace
{
struct AliasEntry
{
    uint64_t mOldNameHash;
    const char* mNewName;
};

// Hash (see hashName) of an old name and the name that replaced it.
const AliasEntry ALIASES[] =
{
    { 0xc2e36dae4fa92ba9ULL, "Defector" },
    { 0xebb6ba9f1bb36afcULL, "HexenHen" },
    { 0x3ebfce12ba6591ceULL, "DefectorPrice" },
    { 0x07189ed04edc38a9ULL, "HexenHenPrice" },
    { 0x1eb5d3c128e01e4bULL, "DefectorNbTurns" },
    { 0x83fd4821972c1610ULL, "HexenHenNbTurns" },
    { 0x0d966eb99b481be4ULL, "DefectorCooldown" },
    { 0x751fda58ff8e9335ULL, "HexenHenCooldown" },
    { 0xea6e2641e5aa5320ULL, "WatchBannerCostPerTile" },
    { 0xf11f805888770df5ULL, "WatchBannerWorkshopPointsPerTile" },
    { 0x1b8ed332f5999e5cULL, "WatchBannerAuraTiles" },
    { 0xea5ce66d289c14b4ULL, "IronboundDoorCostPerTile" },
    { 0x0659273bfd7b79b6ULL, "IronboundDoorPointsPerTile" },
    { 0x4d1e3e15545063c4ULL, "IronboundDoorHP" },
    { 0x6f9980a82006cc8cULL, "RunedDoorCostPerTile" },
    { 0xcae444515713542eULL, "RunedDoorPointsPerTile" },
    { 0x486cf42cdb24c44cULL, "RunedDoorHP" },
    { 0x5dbabe71752a61c2ULL, "RunedDoorManaToFire" },
    { 0x9a76263f0432455bULL, "RunedDoorRegenPerTurn" },
    { 0xac46967da4c30effULL, "RunedDoorReloadTurns" },
    { 0xc64a120ca900f225ULL, "RunedDoorDamage" },
    { 0xfea599dbea82e978ULL, "spellHexenHen" },
    { 0xaf1772cc154463d5ULL, "spellDefector" },
    { 0xd93144790c52abbfULL, "trapDoorIronbound" },
    { 0xa6a9ead8659e219dULL, "trapDoorRuned" },
    { 0x979dc0f5d17eadfdULL, "trapWatchBanner" },
    { 0x6bfc6edafe8335dcULL, "ManaWellBonusPerTile" },
    { 0x72e77dd3320fce48ULL, "RoomConvertClaimRate" },
    { 0xcabd9ef942198d29ULL, "manaWellGround" },
};
}

uint64_t hashName(const std::string& name)
{
    uint64_t hash = 14695981039346656037ULL;
    for(std::size_t i = 0; i < name.size(); ++i)
    {
        hash ^= static_cast<uint64_t>(std::tolower(static_cast<unsigned char>(name[i])));
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string resolve(const std::string& name)
{
    if(name.empty())
        return name;

    uint64_t hash = hashName(name);
    for(std::size_t i = 0; i < sizeof(ALIASES) / sizeof(ALIASES[0]); ++i)
    {
        if(ALIASES[i].mOldNameHash == hash)
            return ALIASES[i].mNewName;
    }

    return name;
}
}
