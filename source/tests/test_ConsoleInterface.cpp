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

#include "modes/AbstractModeManager.h"
#include "modes/ConsoleInterface.h"

#include <functional>
#include <string>

#define BOOST_TEST_MODULE ConsoleInterface
#include "BoostTestTargetConfig.h"

#include <boost/lexical_cast.hpp>

class TestModeManager: public AbstractModeManager
{
public:
    TestModeManager(ModeType mode)
        : mode(mode)
    {
    }

    ModeType getCurrentModeType() const override
    {
        return mode;
    }

private:
    ModeType mode;
};


template<typename StringT>
void appendText(StringT text)
{
    BOOST_TEST_MESSAGE("printed: " << text);
}

using MockConsole = ConsoleInterface;

Command::Result testCommand(const Command::ArgumentList_t& args, MockConsole& console, AbstractModeManager& mm)
{
    console.print("multiple aliases function");
    for(auto&& arg : args)
    {
        console.print("Argument: " + arg + "\n");
    }
    int x = 0;
    ++x;
    try {
        x = boost::lexical_cast<int>(args[1]);
    }
    catch (const boost::bad_lexical_cast& e)
    {
        return Command::Result::INVALID_ARGUMENT;
    }

    return Command::Result::SUCCESS;
}

BOOST_AUTO_TEST_CASE(test_Command)
{
    auto t = TestModeManager::ModeType::GAME;
    Command c(testCommand,Command::cStubServer,"descr",{t});
    BOOST_CHECK(c.isAllowedInMode(t));
}

BOOST_AUTO_TEST_CASE(test_ConsoleInterface)
{

    TestModeManager modeManager(TestModeManager::ModeType::GAME);
    TestModeManager::ModeType mt = TestModeManager::ModeType::GAME;
    using ModeManager = TestModeManager;
    MockConsole interface(appendText<std::string>);
    {
        BOOST_CHECK(
            interface.addCommandAux("test1", "test1 description",
                                 [](const Command::ArgumentList_t& args, MockConsole& console, AbstractModeManager& mm)
                                    {
                                        for(auto&& arg : args)
                                        {
                                            console.print("Arg: " + arg);
                                        }
                                        return Command::Result::SUCCESS;
                                    },
                                 Command::cStubServer,
                                 {AbstractModeManager::ModeType::GAME, AbstractModeManager::ModeType::EDITOR}
            )
        );
    }
    BOOST_CHECK(interface.addCommandAux("aliasedcommand",
                         "aliased command desciption",
                         testCommand,
                         Command::cStubServer,
                         {ModeManager::ModeType::GAME},
                         {"aliasedcmd",
                         "alsdcmd"}));

    unsigned int count = 0;
    //Check that scrolling up or down without a history does not do anything
    BOOST_CHECK(!interface.scrollCommandHistoryPositionDown());
    BOOST_CHECK(!interface.scrollCommandHistoryPositionUp("text"));

    // Executing a command does not record it: the history belongs to whoever owns the
    // prompt, and the game console records what was typed itself. Do the same here.
    std::function<Command::Result(const std::string&, TestModeManager::ModeType)> execute = [&interface, &modeManager, &count](const std::string& cmd, TestModeManager::ModeType mode)
    {
        Command::Result result = interface.tryExecuteClientCommand(cmd, mode, modeManager);
        interface.getCommandHistoryBuffer().emplace_back(cmd);
        ++count;
        return result;
    };

    //try executing commands
    BOOST_CHECK(execute("help", mt) == Command::Result::SUCCESS);
    execute("test0", mt);
    execute("test1", mt);
    execute("test1 123 abc", mt);
    BOOST_CHECK(execute("aliasedcommand", TestModeManager::ModeType::EDITOR) == Command::Result::WRONG_MODE);
    BOOST_CHECK(execute("aliasedcommand 1", mt) == Command::Result::SUCCESS);
    BOOST_CHECK(execute("aliasedcmd argument1", mt) == Command::Result::INVALID_ARGUMENT);
    BOOST_CHECK(execute("aliasedcmd 184467440737095516100", mt) == Command::Result::INVALID_ARGUMENT);
    execute("alsdcmd argument1", mt);

    ConsoleInterface::String_t str;
    //Test command complete completion
    BOOST_CHECK(interface.tryCompleteCommand("aliasedco", str));
    BOOST_CHECK(str.compare("aliasedcommand") == 0);

    //Test command partial completion
    BOOST_CHECK(interface.tryCompleteCommand("a", str));
    BOOST_CHECK(str.compare("al") == 0);

    //Test non existing command completion
    BOOST_CHECK(!interface.tryCompleteCommand("li", str));

    //More scrolling tests
    BOOST_CHECK(!interface.scrollCommandHistoryPositionDown());
    BOOST_CHECK((*interface.scrollCommandHistoryPositionUp("commandPrompt")).compare("alsdcmd argument1") == 0);
    BOOST_CHECK((*interface.scrollCommandHistoryPositionDown()).compare("not commandPrompt") != 0);
    BOOST_CHECK(!interface.scrollCommandHistoryPositionDown());

    //Check command history, we should be at the bottom now:
    BOOST_CHECK(!interface.scrollCommandHistoryPositionDown());
    unsigned int test_count = 0;
    while(true)
    {
        auto res = interface.scrollCommandHistoryPositionUp("commandPrompt");
        if(!res)
        {
            break;
        }
        ++test_count;
        BOOST_TEST_MESSAGE("command" << *res << " count: " << test_count);
    }
    BOOST_CHECK(test_count == count);

}
