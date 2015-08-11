#include "genericconsole.h"

#include <stdlib.h>
#include <string>

#include "windows_wrap.h"

using std::bind;

namespace Common
{

GenericConsole::GenericConsole()
{
    AddCommand("color", &GenericConsole::Color);
    AddCommand("cls", &GenericConsole::Cls);
    AddCommand("quit", &GenericConsole::Quit);
    AddCommand("exit", &GenericConsole::Quit);
    AddCommand("crash", &GenericConsole::Crash);
    AddCommand("int3", &GenericConsole::Int3);
    AddCommand("echo", &GenericConsole::Echo);
    AddCommand("pid", &GenericConsole::Pid);
    AddCommand("term", &GenericConsole::Terminate);
    AddCommand("terminate", &GenericConsole::Terminate);
    AddCommand("help", &GenericConsole::Cmds);
    AddCommand("cmds", &GenericConsole::Cmds);
    AddCommand("sleep", &GenericConsole::Sleep);
}

bool GenericConsole::Sleep(const CmdArgs &args)
{
    auto amt = atoi(args[1]);
    ::Sleep(amt);
    return true;
}

bool GenericConsole::Cmds(const CmdArgs &args)
{
    std::string buf = "";
    for (const auto &pair : commands)
    {
        buf += pair.first;
        buf.push_back(' ');
    }
    Print(buf);
    return true;
}

bool GenericConsole::Terminate(const CmdArgs &args)
{
    TerminateProcess(GetCurrentProcess(), 0);
    return false; // Well, returning here is certainly a failure
}

bool GenericConsole::Pid(const CmdArgs &args)
{
    auto pid = GetCurrentProcessId();
    Printf("%d %x", pid, pid);
    return true;
}

bool GenericConsole::Color(const CmdArgs &args)
{
    if (strcmp(args[1], "bg") == 0)
        colors[Color::bg] = strtoul(args[2], 0, 16);
    else if (strcmp(args[1], "text") == 0)
        colors[Color::text] = strtoul(args[2], 0, 16);
    else if (strcmp(args[1], "own") == 0)
        colors[Color::own] = strtoul(args[2], 0, 16);
    else if (strcmp(args[1], "border") == 0)
        colors[Color::border] = strtoul(args[2], 0, 16);
    else if (strcmp(args[1], "typo") == 0)
        colors[Color::typo] = strtoul(args[2], 0, 16);
    else if (strcmp(args[1], "fail") == 0)
        colors[Color::fail] = strtoul(args[2], 0, 16);
    else
        return false;
    dirty = true;
    return true;
}

bool GenericConsole::Cls(const CmdArgs &args)
{
    ClearScreen();
    return true;
}

bool GenericConsole::Quit(const CmdArgs &args)
{
    exit(0);
    return true;
}

bool GenericConsole::Crash(const CmdArgs &args)
{
    *(int *)42 = 42;
    return true;
}

bool GenericConsole::Int3(const CmdArgs &args)
{
    asm("int3");
    return true;
}

bool GenericConsole::Echo(const CmdArgs &args)
{
    int i = 0;
    std::string str;
    while (strcmp(args[i], "") != 0)
    {
        if (!str.empty())
            str += ' ';
        str += args[++i];
    }

    Print(str);
    return true;
}

} // namespace console
