/*!
 * \file   BuiltinData.h
 * \brief  The default configuration files and levels, compiled into the executable
 *
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

#ifndef BUILTINDATA_H_
#define BUILTINDATA_H_

#include <cstddef>

/*! \brief The contents of config/ and levels/ as of the build, embedded in the binary.
 *
 * ResourceManager writes these into the user data folder when it finds no populated game
 * data folder to read from. Carrying them in the executable is what allows the game to
 * run when the system wide data folder it was configured with does not exist, instead of
 * depending on whatever folder it happens to be launched from.
 *
 * The definitions live in a file generated at build time by
 * cmake/GenerateBuiltinData.cmake.
 */
namespace BuiltinData
{
    struct File
    {
        //! \brief Path relative to a data folder, e.g. "config/global.cfg"
        const char* mPath;

        //! \brief File contents. Not null terminated, and not text in general.
        const unsigned char* mData;

        //! \brief Number of bytes in mData
        std::size_t mSize;
    };

    extern const File FILES[];
    extern const std::size_t FILE_COUNT;

    //! \brief Digest of the names and contents of every file above. Stamped into the
    //! folder they are extracted to, so that a build carrying different defaults can be
    //! noticed: already extracted files are never overwritten.
    extern const char* const CONTENT_DIGEST;
}

#endif // BUILTINDATA_H_
