#ifndef ISCRIPT_H
#define ISCRIPT_H

#include "types.h"
#include "offsets.h"
#include "constants/iscript.h"
#include "common/iter.h"

namespace Iscript {

using OffsetSize = uint16_t;

struct Command
{
    Command() : opcode(0xff), pos(0), val(0) {}
    Command(uint8_t op) : opcode(op), pos(0), val(0) {}
    int ParamsLength() const;
    int Size() const { return 1 + ParamsLength(); }

    std::string DebugStr() const;

    int val1() const { return vals[0]; }
    int val2() const { return vals[1]; }

    uint8_t opcode;
    OffsetSize pos;
    union
    {
        int val;
        int16_t vals[2];
        const uint8_t *data;
    };
    Point point;
};

/// Used by the iscript command handlers to determine if the command was handled.
enum class CmdResult
{
    Handled,
    NotHandled,
    Stop,
};

/// Contains state needed by iscript functions:
///  - Input parameters like rng and iscript that are same for the entire execution
///  - bools can_delete/deleted to handle script finishing (Practically return values)
///  - The virtual function HandleCommand actually defines how iscript commands are handled.
struct Context
{
    constexpr Context(Rng *rng, bool can_delete) :
        iscript(*bw::iscript), rng(rng), can_delete(can_delete), deleted(false),
        cloak_state(Nothing), order_signal(0) { }

    // See CheckDeleted
    virtual ~Context() { Assert(!can_delete || !deleted); }

    virtual CmdResult HandleCommand(Image *img, Script *script, const Command &cmd) = 0;
    /// Called when iscript uses imgol/ul/etc to create another Image.
    virtual void NewOverlay(Image *img) { }

    /// Returns true if bullet needs to be deleted, and acknowledges
    /// that the state has been checked.
    /// Destroying this object while can_delete == true without calling
    /// CheckDeleted() causes an assertion failure, to make sure that
    /// functions handle bullet deletion in some way at least.
    bool CheckDeleted() {
        bool ret = deleted;
        deleted = false;
        return ret;
    }

    const uint8_t *iscript;
    Rng * const rng;
    const bool can_delete;
    bool deleted;
    /// These two are hacks for DrawFunc_ProgressFrame
    enum CloakState : uint8_t {
        Nothing,
        Cloaked,
        Decloaked,
    } cloak_state;
    uint8_t order_signal;
};

class Script
{
    public:
        OffsetSize header;
        OffsetSize pos;
        OffsetSize return_pos;
        uint8_t animation;
        uint8_t wait;

        void ProgressFrame(Context *ctx, Image *img);

        /// Returns name of the iscript animation
        static const char *AnimationName(int anim);

        /// Returns false if header does not exist
        bool Initialize(const uint8_t *iscript, int iscript_header_id);

    private:
        Command Decode(const uint8_t *data) const;
};

} // namespace iscript

#endif // ISCRIPT_H
