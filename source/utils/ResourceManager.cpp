/*!
 * \file   ResourceManager.cpp
 * \date   12 April 2011
 * \author StefanP.MUC
 * \brief  This class handles all the resources (paths, files) needed by the
 *         sound and graphics facilities.
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

#include "utils/ResourceManager.h"

#include <OgreConfigFile.h>
#include <OgrePlatform.h>
#include <OgrePrerequisites.h>

#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
#include <pwd.h> // getpwuid()
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#include <pwd.h> // getpwuid()
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#include <shlobj.h> //to get paths related functions
#ifndef PATH_MAX
    #define PATH_MAX _MAX_PATH   // redefine _MAX_PATH to be compatible with Darwin's PATH_MAX
#endif
#endif

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <OgreString.h>
#include <OgreRenderTarget.h>
#include <OgreGpuProgramManager.h>
#include <OgreFileSystemLayer.h>

#include "utils/BuiltinData.h"
#include "utils/LogManager.h"
#include "utils/Helper.h"

#include <boost/program_options.hpp>

#include <fstream>

//! \brief Stamped into the extracted data folder so an upgrade can be noticed.
#ifdef OD_VERSION
#define OD_BUILTIN_DATA_VERSION OD_VERSION
#else
#define OD_BUILTIN_DATA_VERSION "undefined"
#endif

namespace
{
    //! \brief Whether the character ends a folder name. Windows takes both, and the paths
    //! here are a mix: some are built with '/', some come from the system or the player.
    bool isDirectorySeparator(char c)
    {
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
        return (c == '/') || (c == '\\');
#else
        return c == '/';
#endif
    }
}

template<> ResourceManager* Ogre::Singleton<ResourceManager>::msSingleton = nullptr;
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32 && defined(OD_DEBUG)
//On windows, if the application is compiled in debug mode, use the plugins with debug prefix.
const std::string ResourceManager::PLUGINSCFG = "plugins_d.cfg";
#else
const std::string ResourceManager::PLUGINSCFG = "plugins.cfg";
#endif
const std::string ResourceManager::RESOURCECFG = "resources.cfg";
const std::string ResourceManager::MUSICSUBPATH = "music/";
const std::string ResourceManager::SOUNDSUBPATH = "sounds/";
const std::string ResourceManager::CONFIGSUBPATH = "config/";
const std::string ResourceManager::GAMEDATASUBPATH = "gamedata/";
const std::string ResourceManager::SCRIPTSUBPATH = "scripts/";
const std::string ResourceManager::LANGUAGESUBPATH = "lang/";
const std::string ResourceManager::SHADERCACHESUBPATH = "shaderCache/";
const std::string ResourceManager::LOGFILENAME = "opendungeons.log";
const std::string ResourceManager::CEGUILOGFILENAME = "CEGUI.log";
const std::string ResourceManager::USERCFGFILENAME = "config.cfg";
const std::string ResourceManager::BUILTINVERSIONFILENAME = "VERSION";

const std::string ResourceManager::RESOURCEGROUPMUSIC = "Music";
const std::string ResourceManager::RESOURCEGROUPSOUND = "Sound";

/*! \brief Initializes all paths and reads the ogre config file.
 *
 *  Provide a nice cross platform solution for locating the configuration
 *  files. On windows files are searched for in the current working
 *  directory, on OS X however you must provide the full path, the helper
 *  function macBundlePath does this for us.
 */
ResourceManager::ResourceManager(boost::program_options::variables_map& options) :
        mServerMode(false),
        mForcedNetworkPort(-1),
        mLogLevel(LogMessageLevel::NORMAL),
        mGameDataPath("./"),
        mUserDataPath("./"),
        mUserConfigPath("./")
{
    setupDataPath(options);
    setupUserDataFolders(options);
    setupDefaultDataPath(options);
    // Needs the level paths, which setupDefaultDataPath() has just settled.
    setupServerMode(options);
}

void ResourceManager::setupDataPath(boost::program_options::variables_map& options)
{
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    //TODO - Test osx support
    char applePath[1024];
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    assert(mainBundle);

    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
    assert(mainBundleURL);

    CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
    assert(cfStringRef);

    CFStringGetCString(cfStringRef, applePath, 1024, kCFStringEncodingASCII);

    CFRelease(mainBundleURL);
    CFRelease(cfStringRef);

    // Not applePath + "/": that is a pointer plus a pointer, which does not compile.
    mMacBundlePath = std::string(applePath) + "/";

    mGameDataPath = mMacBundlePath + "Contents/Resources/";
#else // Windows and linux

    std::string path;
#ifdef OD_DATA_PATH
    path = std::string(OD_DATA_PATH);
#else
    path = ".";
#endif

    if(!path.empty())
    {
        mGameDataPath = path;
        if (!isDirectorySeparator(*mGameDataPath.rbegin()))
        {
            mGameDataPath.append("/");
        }

#if defined(OGRE_VERSION) && OGRE_VERSION >= 0x10A00
        mGameDataPath = Ogre::FileSystemLayer::resolveBundlePath(mGameDataPath);
#endif
    }

    // Test whether there is data in "./" and remove the system path in that case.
    // Useful for developers.
    std::string resourceCfg = "./" + RESOURCECFG;
    if (boost::filesystem::exists(resourceCfg.c_str()))
    {
        // Don't warn on Windows as it is the default behaviour...
#if OGRE_PLATFORM != OGRE_PLATFORM_WIN32
        OD_LOG_INF("Note: Found data in the current folder. This data will be used instead of the installed one." + '\n');
#endif
        mGameDataPath = "./";
    }

    OD_LOG_INF( "Game data path is: " + mGameDataPath + '\n' );

#ifndef OGRE_STATIC_LIB
#ifdef OD_PLUGINS_CFG_PATH
    path = std::string(OD_PLUGINS_CFG_PATH);
#else
    path = "./";
#endif
    mPluginsPath = path + "/" + PLUGINSCFG;
#if defined(OGRE_VERSION) && OGRE_VERSION >= 0x10A00
   mPluginsPath = Ogre::FileSystemLayer::resolveBundlePath(mPluginsPath);
#endif
#endif

    // Test whether there is a plugins.cfg file in "./" and remove the system plugins.cfg path in that case.
    // Useful for developers.
    std::string pluginsCfg = "./" + PLUGINSCFG;
    if (boost::filesystem::exists(pluginsCfg.c_str()))
    {
        // Don't warn on Windows as it is the default behaviour...
#if OGRE_PLATFORM != OGRE_PLATFORM_WIN32
        OD_LOG_INF ( "Note: Found a " + PLUGINSCFG + " file in the current folder. "
                     + "This file will be used instead of the installed one." + '\n');
#endif
        mPluginsPath = pluginsCfg;
    }

#endif // Windows and Linux

    OD_LOG_INF( PLUGINSCFG + " path is: " + mPluginsPath + '\n');

    mScriptPath = mGameDataPath + SCRIPTSUBPATH;
    mSoundPath = mGameDataPath + SOUNDSUBPATH;
    mMusicPath = mGameDataPath + MUSICSUBPATH;
    mLanguagePath = mGameDataPath + LANGUAGESUBPATH;

    // mConfigPath is not set here: where the configuration comes from also depends on the
    // user data folder, so setupDefaultDataPath() settles it once both are known.
}

void ResourceManager::setupDefaultDataPath(boost::program_options::variables_map& options)
{
    mUserGameDataPath = mUserDataPath + GAMEDATASUBPATH;

    // Reading the configuration and the shipped levels out of a folder the player owns is
    // what makes the game independent from a system wide data folder: it no longer
    // matters whether the one it was configured with was ever installed, or is readable.
    // The files come from the copies compiled into the executable, see BuiltinData.h.
    auto itOption = options.find("gamedata");
    if(itOption != options.end())
    {
        // Explicit override, mostly useful to run against an edited source tree.
        mDefaultDataPath = itOption->second.as<std::string>();
        if(!mDefaultDataPath.empty() && !isDirectorySeparator(*mDefaultDataPath.rbegin()))
            mDefaultDataPath += '/';
    }
    else
    {
        uint32_t nbExtracted = extractBuiltinData();
        if(nbExtracted > 0)
        {
            OD_LOG_INF("Wrote " + Helper::toString(static_cast<int32_t>(nbExtracted))
                       + " default data files to " + mUserGameDataPath);
        }

        checkBuiltinDataVersion();

        mDefaultDataPath = mUserGameDataPath;
    }

    mConfigPath = mDefaultDataPath + CONFIGSUBPATH;

    OD_LOG_INF("Default data path is: " + mDefaultDataPath + '\n');
}

void ResourceManager::setupServerMode(boost::program_options::variables_map& options)
{
    auto itOption = options.find("server");
    if(itOption != options.end())
    {
        mServerMode = true;
        std::string filePath = getGameLevelPathMultiplayer() + itOption->second.as<std::string>();
        boost::filesystem::path level(filePath);
        if(!boost::filesystem::exists(level))
        {
            OD_LOG_ERR("Wanted level not found: " + filePath +  '\n');
            exit(1);
        }
        mServerModeLevel = level.string();

        auto it2 = options.find("mscreator");
        if(it2 != options.end())
        {
            mServerModeCreator = it2->second.as<std::string>();
        }
    }

    // If the game is launched with both official server mode and custom server
    // mode, we do not consider custom server mode
    if(!mServerMode)
    {
        itOption = options.find("servercustom");
        if(itOption != options.end())
        {
            mServerMode = true;
            std::string filePath = getUserLevelPathMultiplayer() + itOption->second.as<std::string>();
            boost::filesystem::path level(filePath);
            if(!boost::filesystem::exists(level))
            {
                OD_LOG_ERR("Wanted level not found: " + filePath +  '\n');
                exit(1);
            }
            mServerModeLevel = level.string();

            auto it2 = options.find("mscreator");
            if(it2 != options.end())
            {
                mServerModeCreator = it2->second.as<std::string>();
            }
        }
    }

    if(!mServerMode)
    {
        itOption = options.find("serversave");
        if(itOption != options.end())
        {
            mServerMode = true;
            std::string filePath = mSaveGamePath + itOption->second.as<std::string>();
            boost::filesystem::path level(filePath);
            if(!boost::filesystem::exists(level))
            {
                OD_LOG_ERR("Wanted level not found: " + filePath +  '\n');
                exit(1);
            }
            mServerModeLevel = level.string();

            auto it2 = options.find("mscreator");
            if(it2 != options.end())
            {
                mServerModeCreator = it2->second.as<std::string>();
            }
        }
    }
}

bool ResourceManager::isSameAsBuiltin(const std::string& path,
                                      const BuiltinData::File& file)
{
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if(!stream.is_open())
        return false;

    std::string content((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
    if(content.size() != file.mSize)
        return false;

    return std::equal(content.begin(), content.end(),
                      reinterpret_cast<const char*>(file.mData));
}

void ResourceManager::checkBuiltinDataVersion()
{
    const std::string stampPath = mUserGameDataPath + BUILTINVERSIONFILENAME;
    const std::string stamp = std::string(OD_BUILTIN_DATA_VERSION) + " " + BuiltinData::CONTENT_DIGEST;

    if(mNbStaleBuiltinFiles > 0)
    {
        // Files already there are never overwritten, so the game goes on reading whatever
        // was extracted first. That is what keeps edited files safe, but it also means an
        // upgrade, or a rebuilt config/ in a source tree, has no effect until the folder
        // is removed. Say so rather than let it puzzle anyone. Files this build simply
        // added, such as a new level, are already in place and are not reported here.
        OD_LOG_WRN(Helper::toString(static_cast<int32_t>(mNbStaleBuiltinFiles))
                   + " file(s) in " + mUserGameDataPath + " differ from the ones this build"
                   " carries and are kept as they are. Delete that folder to have the"
                   " current ones written again, or pass --gamedata to read them from"
                   " elsewhere.");
    }

    std::ofstream stream(stampPath, std::ios::out | std::ios::trunc);
    if(stream.is_open())
        stream << stamp << std::endl;
}

uint32_t ResourceManager::extractBuiltinData()
{
    uint32_t nbExtracted = 0;
    mNbStaleBuiltinFiles = 0;
    for(std::size_t index = 0; index < BuiltinData::FILE_COUNT; ++index)
    {
        const BuiltinData::File& file = BuiltinData::FILES[index];
        const boost::filesystem::path destination =
            boost::filesystem::path(mUserGameDataPath) / file.mPath;

        try
        {
            // Never overwrite: a file already there may have been edited on purpose, and
            // this runs on every launch, not only the first one. Files this build would
            // have written differently are counted, so that only a real difference is
            // reported and simply adding a level stays quiet.
            if(boost::filesystem::exists(destination))
            {
                if(!isSameAsBuiltin(destination.string(), file))
                    ++mNbStaleBuiltinFiles;
                continue;
            }

            boost::filesystem::create_directories(destination.parent_path());

            std::ofstream stream(destination.string(), std::ios::out | std::ios::binary);
            if(!stream.is_open())
            {
                OD_LOG_ERR("Couldn't write default file: " + destination.string());
                continue;
            }

            if(file.mSize > 0)
                stream.write(reinterpret_cast<const char*>(file.mData), file.mSize);

            ++nbExtracted;
        }
        catch(const boost::filesystem::filesystem_error& e)
        {
            OD_LOG_ERR("Couldn't write default file " + destination.string() + ": " + e.what());
        }
    }

    return nbExtracted;
}

void ResourceManager::setupUserDataFolders(boost::program_options::variables_map& options)
{
    // Empty the members so we can check their validity later.
    mUserDataPath.clear();
    mUserConfigPath.clear();

    auto itOption = options.find("appData");
    if(itOption != options.end())
    {
        mUserDataPath = itOption->second.as<std::string>();
        if(!mUserDataPath.empty())
        {
            if(!isDirectorySeparator(*mUserDataPath.rbegin()))
                mUserDataPath += '/';

            mUserConfigPath = mUserDataPath + "cfg/";
        }
    }
    else
    {
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
        passwd* pw = getpwuid(getuid());
        if(pw)
        {
            mUserDataPath = std::string(pw->pw_dir) + "/Library/Application Support/opendungeons/";
            mUserConfigPath = mUserDataPath + "cfg/";
        }
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
        // $XDG_DATA_HOME/opendungeons/
        // equals to: ~/.local/share/opendungeons/ most of the time
        if (std::getenv("XDG_DATA_HOME"))
        {
            mUserDataPath = std::string(std::getenv("XDG_DATA_HOME")) + "/opendungeons/";
        }
        else
        {
            // We create a sane default if possible: ~/.local/share/opendungeons
            passwd *pw = getpwuid(getuid());
            if(pw)
            {
                mUserDataPath = std::string(pw->pw_dir) + "/.local/share/opendungeons/";
            }
        }

        // $XDG_CONFIG_HOME/opendungeons
        // equals to: ~/.config/opendungeons/ most of the time
        if (std::getenv("XDG_CONFIG_HOME"))
        {
            mUserConfigPath = std::string(std::getenv("XDG_CONFIG_HOME")) + "/opendungeons/";
        }
        else
        {
            // We create a sane default if possible: ~/.config/opendungeons
            passwd *pw = getpwuid(getuid());
            if(pw)
            {
                mUserConfigPath = std::string(pw->pw_dir) + "/.config/opendungeons/";
            }
        }

#elif OGRE_PLATFORM == OGRE_PLATFORM_WIN32
        char path[MAX_PATH];
        // %APPDATA% (%USERPROFILE%\Application Data)
        if(SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path)))
        {
            mUserDataPath = std::string(path) + "/opendungeons/";
            mUserConfigPath = mUserDataPath + "cfg/";
        }
#else
#error("Unknown platform!")
#endif
    }

    // Use local defaults if everything else failed.
    if (mUserDataPath.empty())
        mUserDataPath = "./";
    if (mUserConfigPath.empty())
        mUserConfigPath = "./cfg/";

    try
    {
        boost::filesystem::create_directories(mUserDataPath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR( "Fatal error creating user data folder: " + e.what() +  '\n');
        exit(1);
    }

    OD_LOG_INF( + "User data path is: " + mUserDataPath + '\n');

    try
    {
        boost::filesystem::create_directories(mUserConfigPath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating user config folder: " + e.what() +  '\n');
        exit(1);
    }
    OD_LOG_INF("User config path is: " + mUserConfigPath + '\n');

    try
    {
      boost::filesystem::create_directories(mUserDataPath.c_str() + SHADERCACHESUBPATH);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating shader cache folder: " + e.what() +  '\n');
        exit(1);
    }

    mReplayPath = mUserDataPath + "replay/";
    try
    {
      boost::filesystem::create_directories(mReplayPath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating replay folder: " + e.what() +  '\n');
        exit(1);
    }

    mSaveGamePath = mUserDataPath + "saves/";
    try
    {
      boost::filesystem::create_directories(mSaveGamePath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating replay folder: " + e.what() +  '\n');
        exit(1);
    }

    mUserSkirmishLevelsPath = mUserDataPath + "levels/skirmish/";
    try
    {
      boost::filesystem::create_directories(mUserSkirmishLevelsPath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating user skirmish levels folder: " + e.what() +  '\n');
        exit(1);
    }

    mUserMultiplayerLevelsPath = mUserDataPath + "levels/multiplayer/";
    try
    {
      boost::filesystem::create_directories(mUserMultiplayerLevelsPath);
    }
    catch (const boost::filesystem::filesystem_error& e)
    {
        //TODO - Exit gracefully
        OD_LOG_ERR("Fatal error creating user multiplayer levels folder: " + e.what() +  '\n');
        exit(1);
    }

    itOption = options.find("log");
    if(itOption != options.end())
    {
        // We change log file
        mOgreLogFile = mUserDataPath + itOption->second.as<std::string>();
    }
    else
    {
        // Default log file
        mOgreLogFile = mUserDataPath + LOGFILENAME;
    }

    // The server mode options name a level, so they are handled once the default data
    // path is known, in setupServerMode().

    itOption = options.find("port");
    if(itOption != options.end())
        mForcedNetworkPort = itOption->second.as<int32_t>();

    itOption = options.find("loglevel");
    if(itOption != options.end())
        mLogLevel = static_cast<LogMessageLevel>(itOption->second.as<int32_t>());

    mUserConfigFile = mUserConfigPath + USERCFGFILENAME;
    mCeguiLogFile = mUserDataPath + CEGUILOGFILENAME;
    mShaderCachePath = mUserDataPath + SHADERCACHESUBPATH;

    // Backup the Ogre log files from the previous three instances
    try
    {
        if(boost::filesystem::exists(mOgreLogFile + ".2"))
            boost::filesystem::rename(mOgreLogFile + ".2", mOgreLogFile + ".3");
        if(boost::filesystem::exists(mOgreLogFile + ".1"))
            boost::filesystem::rename(mOgreLogFile + ".1", mOgreLogFile + ".2");
        if(boost::filesystem::exists(mOgreLogFile))
            boost::filesystem::rename(mOgreLogFile, mOgreLogFile + ".1");
    }
    catch(const boost::filesystem::filesystem_error& e)
    {
        OD_LOG_ERR("ERROR: couldn't rename logs " + e.what() +  '\n');
    }
}

void ResourceManager::setupOgreResources(uint16_t shaderLanguageVersion)
{
    Ogre::ConfigFile cf;
    cf.load(mGameDataPath + RESOURCECFG);

#if defined(OGRE_VERSION) && OGRE_VERSION < 0x10A00
    // Go through all sections & settings in the file
    Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

    Ogre::String secName = "";
    Ogre::String typeName = "";
    Ogre::String archName = "";
    while(seci.hasMoreElements())
    {
        secName = seci.peekNextKey();
        Ogre::ConfigFile::SettingsMultiMap* settings = seci.getNext();
        Ogre::ConfigFile::SettingsMultiMap::iterator i;
        for (i = settings->begin(); i != settings->end(); ++i)
        {
            typeName = i->first;
            archName = mGameDataPath + i->second;
#else
    const auto settings = cf.getSettingsBySection();

    for(const auto& section : settings)
    {
        const Ogre::String& secName = section.first;
        const auto& settingsMap = section.second;
        for(const auto& setting: settingsMap)
        {
            const Ogre::String& typeName = setting.first;
            Ogre::String archName = setting.second;

            // Do not modify absolute paths. A leading '/' is not what makes one on Windows,
            // where they start with a drive letter, so let boost decide.
            if(!archName.empty() && !boost::filesystem::path(archName).is_absolute())
                archName = mGameDataPath + archName;
            else
                archName = Ogre::FileSystemLayer::resolveBundlePath(archName);

#endif // OGRE_VERSION < 0x10A00
            OD_LOG_INF("Resource in section: " + secName + " Type: " + typeName
                       + " Path: " + archName);
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
            // OS X does not set the working directory relative to the app,
            // In order to make things portable on OS X we need to provide
            // the loading with it's own bundle path location.
            // Unlike windows you can not rely on the curent working directory
            // for locating your configuration files and resources.

            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    Ogre::String(std::string(mMacBundlePath) + archName), typeName, secName, false);
#else
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                    archName, typeName, secName, false);
#endif
        }
    }

    // Adds the correct GLSL shader path depending on the GPU capacity
    Ogre::GpuProgramManager& gpuProgramManager = Ogre::GpuProgramManager::getSingleton();
    Ogre::ResourceGroupManager& resourceGroupManager = Ogre::ResourceGroupManager::getSingleton();
    if((OGRE_VERSION < 0x10C00) && gpuProgramManager.isSyntaxSupported("glsl"))
    {
        OD_LOG_INF("Supported shader version is: " + Helper::toString(shaderLanguageVersion));

        // Use patched version of shader on shader version 130+ systems
        if(shaderLanguageVersion >= 130)
        {
            resourceGroupManager.addResourceLocation(mGameDataPath + "materials/RTShaderLib/GLSL/130", "FileSystem", "Graphics");
        }
        else
        {
            resourceGroupManager.addResourceLocation(mGameDataPath + "materials/RTShaderLib/GLSL/120", "FileSystem", "Graphics");
        }
    }
}

bool ResourceManager::hasFileEnding(const std::string& filename, const std::string& ending)
{
    return (filename.length() < ending.length())
            ? false
            : filename.compare(filename.length() - ending.length(),
                    ending.length(), ending) == 0;
}

std::vector<std::string> ResourceManager::listAllFiles(const std::string& directoryName)
{
    std::vector<std::string> files;
    using namespace boost::filesystem;

    if(is_directory(directoryName))
    {
        for(directory_iterator it(directoryName); it != directory_iterator(); ++it)
        {
            files.push_back(it->path().string());
        }
    }
    return files;
}

Ogre::StringVectorPtr ResourceManager::listAllMusicFiles()
{
    return Ogre::ResourceGroupManager::getSingleton().
            listResourceNames(RESOURCEGROUPMUSIC);
}

void ResourceManager::takeScreenshot(Ogre::RenderTarget* renderTarget)
{
    static int screenShotCounter = 0;

    static std::locale loc(std::wcout.getloc(), new boost::posix_time::time_facet("%Y-%m-%d_%H%M%S"));

    std::ostringstream ss;
    ss.imbue(loc);
    ss << "ODscreenshot_" << boost::posix_time::second_clock::local_time()
       << "_" << screenShotCounter++ << ".png";
    renderTarget->writeContentsToFile(getUserDataPath() + ss.str());
}

std::string ResourceManager::buildReplayFilename()
{
    static std::locale loc(std::wcout.getloc(), new boost::posix_time::time_facet("%Y%m%d_%H%M%S"));
    std::ostringstream ss;
    ss.imbue(loc);
    ss << "replay_" << boost::posix_time::second_clock::local_time() << ".odr";
    return ss.str();
}

void ResourceManager::buildCommandOptions(boost::program_options::options_description& desc)
{
    desc.add_options()
        ("log", boost::program_options::value<std::string>(), "log file to use")
        ("server", boost::program_options::value<std::string>(), "Launches the game on server mode and opens the given level from official levels path")
        ("servercustom", boost::program_options::value<std::string>(), "Launches the game on server mode and opens the given level from custom levels path")
        ("serversave", boost::program_options::value<std::string>(), "Launches the game on server mode and opens the given saved game")
        ("appData", boost::program_options::value<std::string>(), "Sets appData to the given path (where logs, replays, ... are saved)")
        ("gamedata", boost::program_options::value<std::string>(), "Reads the configuration and the shipped levels from the given path instead of the copies extracted below appData")
        ("mscreator", boost::program_options::value<std::string>(), "Sets the creator for this map to connect to the master server. server/servercustom/serversave option needs to be on")
        ("port", boost::program_options::value<int32_t>(), "Sets the port used. Note that the port is used for both single and multi player")
        ("loglevel", boost::program_options::value<int32_t>(), "Sets the log level (between 0=Trivial and 3=Critical)")
    ;
}

std::string ResourceManager::getGameLevelPathSkirmish() const
{
    return getDefaultDataPath() + "levels/skirmish/";
}

std::string ResourceManager::getGameLevelPathMultiplayer() const
{
    return getDefaultDataPath() + "levels/multiplayer/";
}


std::string ResourceManager::getGameScriptsPath() const
{
    return getGameDataPath() + "python/";

}
