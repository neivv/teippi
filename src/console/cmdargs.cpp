#include "cmdargs.h"

#include <string.h>

CmdArgs::CmdArgs(const char *str)
{
    int len = strlen(str) + 1;
    string = new char[len];
    memcpy(string, str, len);

    char *entry = strtok(string, " \t");
    while (entry)
    {
        args.push_back(entry);
        entry = strtok(0, " \t");
    }
}

CmdArgs::~CmdArgs()
{
    delete[] string;
}

const char *CmdArgs::operator[](unsigned int pos) const
{
    if (pos >= args.size())
        return "";
    return args[pos];
}
