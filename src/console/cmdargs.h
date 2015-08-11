#ifndef CMDARGS_H
#define CMDARGS_H

#include <vector>

class CmdArgs
{
    public:
        CmdArgs(const char *str);
        ~CmdArgs();
        const char *operator[](unsigned int pos) const;

    private:
        std::vector<const char *> args;
        char *string;
};

#endif // CMDARGS_H

