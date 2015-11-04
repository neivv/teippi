#ifndef ISCRIPT_H
#define ISCRIPT_H

#include "types.h"
#include "offsets.h"
#include "constants/iscript.h"
#include "common/iter.h"

struct IscriptContext
{
    IscriptContext()
    {
        iscript = *bw::iscript;
        unit = nullptr;
        bullet = nullptr;
    }
    IscriptContext(Unit *unit_)
    {
        iscript = *bw::iscript;
        unit = unit_;
        bullet = nullptr;
    }
    IscriptContext(Bullet *bullet_)
    {
        iscript = *bw::iscript;
        unit = nullptr;
        bullet = bullet_;
    }

    const uint8_t *iscript;
    Unit *unit;
    Bullet *bullet;
    Image *img;
};

class Iscript
{
    public:
        uint16_t header;
        uint16_t pos;
        uint16_t return_pos;
        uint8_t animation;
        uint8_t wait;

        typedef uint16_t Offset;

        struct Command
        {
            Command() : opcode(0xff), pos(0), val(0) {}
            Command(uint8_t op) : opcode(op), pos(0), val(0) {}
            int ParamsLength() const;
            int Size() const { return 1 + ParamsLength(); }
            bool IsStoppingCommand() const
            {
                switch (opcode)
                {
                    case IscriptOpcode::Wait:
                    case IscriptOpcode::WaitRand:
                    case IscriptOpcode::End:
                        return true;
                    default:
                        return false;
                }
            }

            std::string DebugStr() const;

            int val1() const { return vals[0]; }
            int val2() const { return vals[1]; }

            uint8_t opcode;
            Offset pos;
            union
            {
                int val;
                int16_t vals[2];
                const uint8_t *data;
            };
            Point point;
        };

        class GetCommands_C : Iterator<GetCommands_C, Command>
        {
            public:
                Optional<Command> next()
                {
                    if (stop)
                        return Optional<Command>();
                    auto cmd = isc->ProgressUntilCommand(ctx, rng);
                    if (cmd.IsStoppingCommand() || IgnoreRestCheck(cmd))
                        stop = true;
                    return move(cmd);
                }

                // In source file as it accesses ctx->unit
                bool IgnoreRestCheck(const Iscript::Command &cmd) const;
                GetCommands_C(Iscript *i, const IscriptContext *c, Rng *r) : stop(false), isc(i), ctx(c), rng(r) {}
                void SetContext(const IscriptContext *c) { ctx = c; }

            private:
                bool stop;
                Iscript *isc;
                const IscriptContext *ctx;
                Rng *rng;
        };
        GetCommands_C GetCommands(const IscriptContext *ctx, Rng *rng)
        {
            return GetCommands_C(this, ctx, rng);
        }

        /// Returns false if header does not exist
        bool Initialize(int iscript_header_id);

    private:
        Command ProgressUntilCommand(const IscriptContext *ctx, Rng *rng);
        Command Decode(const uint8_t *data) const;
};

#endif // ISCRIPT_H
