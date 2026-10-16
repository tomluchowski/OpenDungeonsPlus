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

#ifndef NAMEALIASES_H
#define NAMEALIASES_H

#include <cstdint>
#include <string>

//! \brief Maps names that older savegames, levels and config files may still contain to the current names.
//! The old names are stored only as hashes, see NameAliases.cpp and docs/development/NAME-ALIASES.md.
namespace NameAliases
{
//! \brief 64-bit FNV-1a hash of the lowercase name
uint64_t hashName(const std::string& name);

//! \brief Returns the current name if name is an old name, name itself otherwise. Case-insensitive on the old name.
std::string resolve(const std::string& name);
}

#endif // NAMEALIASES_H
