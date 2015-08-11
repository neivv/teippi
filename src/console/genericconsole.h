#ifndef GENERIC_CONSOLE_H
#define GENERIC_CONSOLE_H

#include "console.h"

namespace Common {

class GenericConsole : public Console
{
    public:
        GenericConsole();

    private:
        bool Color(const CmdArgs &args);
        bool Pid(const CmdArgs &args);
        bool Cls(const CmdArgs &args);
        bool Quit(const CmdArgs &args);
        bool Crash(const CmdArgs &args);
        bool Int3(const CmdArgs &args);
        bool Echo(const CmdArgs &args);
        bool Terminate(const CmdArgs &args);
        bool Cmds(const CmdArgs &args);
        bool Sleep(const CmdArgs &args);
};

} // namespace console

#endif // GENERIC_CONSOLE_H

