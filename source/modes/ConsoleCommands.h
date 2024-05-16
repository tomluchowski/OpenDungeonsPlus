#ifndef CONSOLECOMMANDS_H
#define CONSOLECOMMANDS_H

#include <map>
#include <string>

class ConsoleInterface;

namespace ConsoleCommands
{
    //! \brief Adds a number of console commands to the specified console
    void addConsoleCommands(ConsoleInterface& cl);
};
#endif // CONSOLECOMMANDS_H
