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

#include "ConsoleInterface.h"

#include "network/ClientNotification.h"
#include "network/ODClient.h"

#include <regex>
#include <pybind11/embed.h>


const std::map<std::string, const char*>& getDocString()
{ 
    static const std::map<std::string, const char*> &map = std::map<std::string, const char*>
        {
            {
                "addcreature",
                "Adds a new creature according to following parameters"
            },
            {
                "addgold",
                "'addgold' adds the given amount of gold to one player. It takes as arguments the ID of the player to"
                "whom the gold should be given and the amount. If the player's treasuries are full, no more gold is given."
                "Note that this command is available in server mode only. \n\nExample\n"
                "to give 5000 gold to player color 1 : addgold 1 5000"
            },
            {
                "addmana",
                "'addmana' adds the given amount of mana to one player. It takes as arguments the ID of the player to"
                "whom the man should be given and the amount. If the player's mana pool is full, no more mana is given."
                "Note that this command is available in server mode only. \n\nExample\n"
                "to give 25000 mana to player 1 : addmana 1 25000"
            },
            {
                "ambientlight",
                "The 'ambientlight' command sets the minimum light that every object in the scene is illuminated with. "
                "It takes as it's argument and RGB triplet whose values for red, green, and blue range from 0.0 to 1.0.\n\nExample:\n"
                "ambientlight 0.4 0.6 0.5\n\nThe above command sets the ambient light color to red=0.4, green=0.6, and blue = 0.5."
            },
            {
                "catmullspline",
                "Triggers the catmullspline camera movement behaviour.\n\nExample:\n"
                "catmullspline 4 5 4 6 5 7\n"
                "Make the camera follow a lazy curved path along the given coordinates pairs. "

            },
            {
                "circlearound",
                "Triggers the circle camera movement behaviour.\n\nExample:\n"
                "circlearound 6 4 8\n"
                "Make the camera follow a lazy a circle path around coors 6,4 with a radius of 8."
            },
            {
                "creaturevisdebug",
                "Visual debugging is a way to see a given creature\'s AI state.\n\nExample:\n"
                "creaturevisdebug skeletor\n\n"
                "The above command wil turn on visual debugging for the creature named \'skeletor\'. "
                "The same command will turn it back off again."
            },
            {
                "enableZPrePass",
                "enanbles visualization of Z - buffer, press capslock button to see it"

            },
            {
                "farclip",
                "Sets the maximal viewpoint clipping distance. Objects farther than that won't be rendered.\n\nE.g.: farclip 30.0"
            },
            {
                "fps",
                "'fps' (framespersecond) for short is a utility which displays or sets the maximum framerate at which the"
                "rendering will attempt to update the screen.\n\nExample:\n"
                "fps 35\n\nThe above command will set the current maximum framerate to 35 turns per second."
            },
            {
                "getposition",
                "gets position of a mouse in terms of x and y coordinates of gamemap"
            },
            {
                "help",
                ">help Lists available commands\n>help <command> displays description for <command>"

            },
            {
                "helpmessage",
                "Display help message"
            },
            {
                "list",
                "'list' (or 'ls' for short) is a utility which lists various types of information about the current game. "
                "Running list without an argument will produce a list of the lists available. "
                "Running list with an argument displays the contents of that list.\n\nExamples:\n"
                "list creatures\tLists all the creatures currently in the game.\n"
                "list classes\tLists all creature classes.\n"
                "list players\tLists every player in game.\n"
                "list network\tTells whether the game is running as a server, a client or as the map editor.\n"
                "list rooms\tLists all the current rooms in game.\n"
                "list colors\tLists all seat's color values.\n"
                "list goals\tLists The local player goals.\n"

            },
            {
                "listmeshanims",
                "'listmeshanims' lists all the animations for the given mesh."
            },
            {
                "logfloodfill",
                "'logfloodfill' logs the FloodFillValues of all the Tiles in the GameMap."
            },
            {
                "maxtime",
                "Sets the max time (in seconds) a message will be displayed in the info text area.\n\nExample:\n"
                "maxtime 5"
            },
            {
                "nearclip",
                "Sets the minimal viewpoint clipping distance. Objects nearer than that won't be rendered.\n\nE.g.: nearclip 3.0"
            },
            {
                "printentities",
                "prints all entities in the scenemanager "

            },
            {
                "printnodes",
                "prints all the scene nodes "               
            },
            {
                "seatvisdebug",
                "Visual debugging is a way to see all the tiles a given seat can see.\n\nExample:\n"
                "seatvisualdebug 1\n\nThe above command will show every tiles seat 1 can see.  The same command will turn it off."
            },
            {
                "setcamerafovy",
                "Sets the camera vertical field of view aspect ratio on the Y axis.\n\nExample:\n"
                "setcamerafovy 45"

            },
            {
                "setcreaturedest",
                "Sets the creature desitnation it should walk to \n\nExample\n"
                "setcreaturedest 0 0 "

            },
            {
                "setcreaturelevel",
                "Sets the level of a given creature.\n\nExample:\n"
                "setlevel NatureMonster1 10\n\nThe above command will set the creature \'NatureMonster1\' to 10."
            },
            {
                "setShadowFarClipDistance",
                "sets the camera shadow far clip distance "
            },
            {
                "setShadowNearClipDistance",
                "sets the camera shadow near clip distance "
            },
            {
                "setcamerafovy",
                "Sets the camera vertical field of view aspect ratio on the Y axis.\n\nExample:\n"
                "setcamerafovy 45"
            },
            {
                "setcameralightdirectionthreshold",
                "Sets the threshold between the camera and the light direction below which the LiSPSM projection is 'flattened', since coincident light and camera projections cause problems with the perspective skew. "
            },
            {
                "setcreaturedest",
                "Sets the creature desitnation it should walk to \n\nExample\n"
                "setcreaturedest 0 0 "
            },
            {
                "setcreaturelevel",
                "Sets the level of a given creature.\n\nExample:\n"
                "setlevel NatureMonster1 10\n\nThe above command will set the creature \'NatureMonster1\' to 10."

            },
            {
                "setloglevel",
                "'setloglevel' sets the logging level. If no module (source file) is given, sets global log level."
                " Otherwise, sets log level of given module\nExample:\n"
                "setloglevel ODApplication 0 => Sets specific log level for ODApplication source file\n"
                "setloglevel 1 => Sets global log level",
            },
            {
                "setoptimaladjustfactor",
                "Adjusts the parameter n to produce optimal shadows "
            },

            {
                "setshadowcamerafovy",
                "sets shadow camera field of view height in degreee"
            },
            {
                "setuseoptimaladjust",
                "Sets whether or not to use a slightly simpler version of the camera near point derivation (default is true) "
            },
            {
                "termwidth",
                "Sets the terminal width."
            },
            {
                "tilevisualdebug",
                "Enables middle clicking a tile and obtaining info for it",
            },
            {
                "togglefow",
                "Toggles on/off fog of war for every connected player",                
            },
            {
                "triggercompositor",
                "Starts the given compositor. The compositor must exist.\n\nExample:\n"
                "triggercompositor blacknwhite"

            },
            {
                "unlockskills",
                "Unlock all skills for every seats\n"
            }
        
        }; 
    return map; 
}


ConsoleInterface::ConsoleInterface(PrintFunction_t printFunction)
    :mPrintFunction(printFunction), mCommandHistoryBuffer(), mHistoryPos(mCommandHistoryBuffer.begin())
{
   addCommand("help",
              ConsoleInterface::helpCommand,
              Command::cStubServer,
              {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR},
              {});
}

bool ConsoleInterface::addCommandAux(String_t name, String_t description,
                CommandClientFunction_t commandClient,
                CommandServerFunction_t commandServer,
                std::initializer_list<ModeType> allowedModes,
                std::initializer_list<String_t> aliases)
{
    CommandPtr_t commandPtr = std::make_shared<Command>(commandClient, commandServer, description, allowedModes);
    mCommandMap.emplace(name, commandPtr);
    for(auto& alias : aliases)
    {
        mCommandMap.emplace(alias, commandPtr);
    }
    return true;
}

Command::Result ConsoleInterface::tryExecuteClientCommand(String_t commandString,
                                                    ModeType modeType,
                                                    AbstractModeManager& modeManager)
{
    print(">> " + commandString);
    Command::ArgumentList_t tokenList;
    boost::algorithm::split(tokenList,
                            commandString, boost::algorithm::is_space(),
                            boost::algorithm::token_compress_on);

    const String_t& commandName = tokenList.front();
    auto it = mCommandMap.find(commandName);
    if(it != mCommandMap.end())
    {
        // Check if there is something to do on client side
        try
        {
            Command::Result result = Command::Result::SUCCESS;
            result = it->second->executeClient(tokenList, modeType, *this, modeManager);

            return result;
        }
        catch(const std::invalid_argument& e)
        {
            print(String_t("Invalid argument: ") + e.what());
            return Command::Result::INVALID_ARGUMENT;
        }
        catch(const std::out_of_range& e)
        {
            print(String_t("Argument out of range") + e.what());
            return Command::Result::INVALID_ARGUMENT;
        }
    }
    else
    {
        print("Unknown command: \"" + commandString + "\"");
        return Command::Result::NOT_FOUND;
    }
}

Command::Result ConsoleInterface::tryExecuteServerCommand(const std::vector<std::string>& args, GameMap& gameMap)
{
    if(args.empty())
    {
        print("Invalid empty command");
        return Command::Result::INVALID_ARGUMENT;
    }
    const std::string& commandName = args[0];
    auto it = mCommandMap.find(commandName);
    if(it != mCommandMap.end())
    {
        // Check if there is something to do on client side
        try
        {
            Command::Result result = Command::Result::SUCCESS;
            result = it->second->executeServer(args, *this, gameMap);

            return result;
        }
        catch(const std::invalid_argument& e)
        {
            print(std::string("Invalid argument: ") + e.what());
            return Command::Result::INVALID_ARGUMENT;
        }
        catch(const std::out_of_range& e)
        {
            print(std::string("Argument out of range") + e.what());
            return Command::Result::INVALID_ARGUMENT;
        }
    }
    else
    {
        print("Unknown command: \"" + commandName + "\"");
        return Command::Result::NOT_FOUND;
    }
}

bool ConsoleInterface::tryCompleteCommand(const String_t& prefix, String_t& completedCmd)
{
    std::vector<const String_t*> matches;
    for(auto& element : mCommandMap)
    {
        if(boost::algorithm::starts_with(element.first, prefix))
        {
            matches.push_back(&element.first);
        }
    }

    if(matches.empty())
        return false;

    if(matches.size() == 1)
    {
        completedCmd = *(*matches.begin());
        return true;
    }

    // There are several matches. We display them
    for(auto match : matches)
    {
        const String_t& str = *match;
        print(str);
    }
    print("\n");

    // We try to complete until there is a difference between the matches in
    // a way like Linux does
    completedCmd = prefix;
    std::size_t index = completedCmd.length();
    // We take the first entry as reference
    const String_t& refStr = *(*matches.begin());
    while(index < refStr.length())
    {
        bool isDif = false;
        for(auto match : matches)
        {
            const String_t& str = *match;

            if(str.length() <= index)
            {
                isDif = true;
                break;
            }

            if(str[index] != refStr[index])
            {
                isDif = true;
                break;
            }
        }

        if(isDif)
            break;

        completedCmd += refStr[index];
        ++index;
    }
    return true;
}

boost::optional<const ConsoleInterface::String_t&> ConsoleInterface::scrollCommandHistoryPositionUp(const ConsoleInterface::String_t& currentPrompt)
{
    //If the list is empty, or we are at the top, return none
    if(mCommandHistoryBuffer.empty() || mHistoryPos == mCommandHistoryBuffer.begin())
    {
        return boost::none;
    }
    else
    {
        if(!isInHistory())
        {
            mTemporaryCommandString = currentPrompt;
        }
        --mHistoryPos;
        return boost::optional<const String_t&>(*mHistoryPos);
    }
}

boost::optional<const ConsoleInterface::String_t&> ConsoleInterface::scrollCommandHistoryPositionDown()
{
    //If we are at the start, don't scroll
    if(!isInHistory())
    {
        return boost::none;
    }
    else
    {
        ++mHistoryPos;
        if(mHistoryPos == mCommandHistoryBuffer.end())
        {
            return boost::optional<const String_t&>(mTemporaryCommandString);
        }
        return boost::optional<const String_t&>(*mHistoryPos);
    }
}

boost::optional<const ConsoleInterface::String_t&> ConsoleInterface::getCommandDescription(const String_t& command)
{
    auto it = mCommandMap.find(command);
    if(it != mCommandMap.end())
    {
        return boost::optional<const String_t&>(it->second->getDescription());
    }
    else
    {
        return boost::none;
    }
}

Command::Result ConsoleInterface::helpCommand(const Command::ArgumentList_t& args, ConsoleInterface& console, AbstractModeManager&)
{
    if(args.size() > 1)
    {
        auto result = console.getCommandDescription(args[1]);
        if(result)
        {
            console.print("Help for command \"" + args[1] + "\":\n");
            console.print(*result + "\n");
        }
        else
        {
            console.print("Help for command \"" + args[1] + "\" not found!\n");
        }
    }
    else
    {
        console.print("Commands:\n");
        for(auto& it : console.mCommandMap)
        {
            console.print(it.first);
        }
        console.print("\n");
    }
    return Command::Result::SUCCESS;
}
