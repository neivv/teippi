#ifndef FUNC_HPP
#define FUNC_HPP

#include "../common/assert.h"
#include "write_asm.h"
#include "patchmanager.h"

namespace patch_func {
    struct StackState {
        constexpr StackState(int arg_count)
            : arg_pos(0), stack_diff(0), arg_count(arg_count) { }
        /// Incremented by 1 when a value is taken from stack.
        /// (For every argument)
        int arg_pos;
        /// Incremented when a value is pushed to stack.
        /// (For arguments that aren't in registers)
        /// When 0, [esp] is the return adddress of wrapper.
        int stack_diff;
        /// Total amount of args.
        const int arg_count;
    };

    template <typename C>
    struct WriteArgument {
    };

    template <>
    struct WriteArgument<Stack> {
        static int Write(StackState *state, uint8_t *out, int out_size) {
            int needed_length = Size(state);
            int offset = (state->arg_count - state->arg_pos + state->stack_diff) * sizeof(void *);
            if (out_size < needed_length) {
                return -needed_length;
            }
            *out++ = 0xff;
            if (offset >= 0x80) {
                *out++ = 0xb4;
                *out++ = 0xe4;
                memcpy(out, &offset, sizeof(int));
            } else {
                *out++ = 0x74;
                *out++ = 0xe4;
                *out++ = offset;
            }
            return needed_length;
        }

        static int Size(StackState *state) {
            state->arg_pos += 1;
            state->stack_diff += 1;
            int offset = (state->arg_pos + state->stack_diff) * sizeof(void *);
            if (offset >= 0x80) {
                return 7;
            } else {
                return 4;
            }
        }

        static int WritePush(StackState *state, uint8_t *out, int out_size) { return 0; }
        static int PushSize(StackState *state) { return 0; }
        static int WritePop(StackState *state, uint8_t *out, int out_size) { return 0; }
        static int PopSize(StackState *state) { return 0; }
    };

    inline int WriteByte(uint8_t *out, int out_size, uint8_t val) {
        if (out_size < 1)
            return -1;
        *out = val;
        return 1;
    }

    /// `Preserve` is true if the register's value needs to be preserved by the wrapper.
    template <uint8_t Byte, bool Preserve>
    struct RegArg {
        static int Write(StackState *state, uint8_t *out, int out_size) {
            static_assert(Byte < 8, "Register id must be less than 8");
            int needed_length = Size(state);
            int offset = (state->arg_count - state->arg_pos + state->stack_diff + 1) * sizeof(void *);
            if (out_size < needed_length) {
                return -needed_length;
            }
            *out++ = 0x8b;
            if (offset >= 0x80) {
                *out++ = 0x84 + Byte * 8;
                *out++ = 0xe4;
                memcpy(out, &offset, sizeof(int));
            } else {
                *out++ = 0x44 + Byte * 8;
                *out++ = 0xe4;
                *out++ = offset;
            }
            return needed_length;
        }

        static int Size(StackState *state) {
            state->arg_pos += 1;
            int offset = (state->arg_count - state->arg_pos + state->stack_diff + 1) * sizeof(void *);
            if (offset >= 0x80) {
                return 7;
            } else {
                return 4;
            }
        }

        static int WritePush(StackState *state, uint8_t *out, int out_size) {
            auto size = PushSize(state);
            if (out_size < size) {
                return -size;
            }
            if (Preserve) {
                *out = 0x50 + Byte;
            }
            return size;
        }

        static int PushSize(StackState *state) {
            if (Preserve) {
                state->stack_diff += 1;
                return 1;
            } else {
                return 0;
            }
        }

        static int WritePop(StackState *state, uint8_t *out, int out_size) {
            auto size = PushSize(state);
            if (out_size < size) {
                return -size;
            }
            if (Preserve) {
                *out = 0x58 + Byte;
            }
            return size;
        }

        static int PopSize(StackState *state) {
            if (Preserve) {
                state->stack_diff -= 1;
                return 1;
            } else {
                return 0;
            }
        }
    };

    template <>
    struct WriteArgument<Eax> : public RegArg<0x0, false> { };
    template <>
    struct WriteArgument<Ecx> : public RegArg<0x1, false> { };
    template <>
    struct WriteArgument<Edx> : public RegArg<0x2, false> { };
    template <>
    struct WriteArgument<Ebx> : public RegArg<0x3, true> { };
    template <>
    struct WriteArgument<Esp> : public RegArg<0x4, false> { };
    template <>
    struct WriteArgument<Ebp> : public RegArg<0x5, true> { };
    template <>
    struct WriteArgument<Esi> : public RegArg<0x6, true> { };
    template <>
    struct WriteArgument<Edi> : public RegArg<0x7, true> { };

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WriteArguments(StackState *stack_state, uint8_t *out, int out_size) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WriteArguments(StackState *stack_state, uint8_t *out, int out_size) {
        int written = WriteArguments<Arguments...>(stack_state, out, out_size);
        if (written < 0)
            return false;
        int written2 = WriteArgument<C>::Write(stack_state, out + written, out_size - written);
        if (written2 < 0)
            return false;
        return written + written2;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WritePushes(StackState *stack_state, uint8_t *out, int out_size) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WritePushes(StackState *stack_state, uint8_t *out, int out_size) {
        int written = WritePushes<Arguments...>(stack_state, out, out_size);
        if (written < 0)
            return false;
        int written2 = WriteArgument<C>::WritePush(stack_state, out + written, out_size - written);
        if (written2 < 0)
            return false;
        return written + written2;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WritePops(StackState *stack_state, uint8_t *out, int out_size) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WritePops(StackState *stack_state, uint8_t *out, int out_size) {
        int written = WriteArgument<C>::WritePop(stack_state, out, out_size);
        if (written < 0)
            return false;
        int written2 = WritePops<Arguments...>(stack_state, out + written, out_size - written);
        if (written2 < 0)
            return false;
        return written + written2;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WriteArgumentsSize(StackState *state) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WriteArgumentsSize(StackState *state) {
        auto total = WriteArgument<C>::Size(state);
        total += WriteArgumentsSize<Arguments...>(state);
        return total;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WritePushesSize(StackState *state) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WritePushesSize(StackState *state) {
        auto total = WritePushesSize<Arguments...>(state);
        total += WriteArgument<C>::PushSize(state);
        return total;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WritePopsSize(StackState *state) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WritePopsSize(StackState *state) {
        auto total = WritePopsSize<Arguments...>(state);
        total += WriteArgument<C>::PopSize(state);
        return total;
    }

    template <typename Ret, typename... Args>
    template <typename... ArgLoc>
    void Stdcall<Ret (Args...)>::Init(Common::PatchManager *exec_heap, uintptr_t address) {
        static_assert(sizeof...(Args) == sizeof...(ArgLoc),
                "Template parameters passed to Init() do not match func argument count");
        auto wrapper_size = WrapperSize<ArgLoc...>();
        uint8_t *wrapper = (uint8_t *)exec_heap->AllocExecMem(wrapper_size);
        auto result = WriteWrapper<ArgLoc...>(wrapper, wrapper_size, address);
        if (result != wrapper_size) { Assert(result == wrapper_size); }
        wrapper_code = (Ret (*)(Args...))wrapper;
    }

    template <typename Ret, typename... Args>
    template <typename... ArgLoc>
    uintptr_t Stdcall<Ret (Args...)>::WrapperSize() {
        StackState stack_state(sizeof...(Args));
        auto push_size = WritePushesSize<ArgLoc...>(&stack_state);
        auto arg_size = WriteArgumentsSize<ArgLoc...>(&stack_state);
        auto pop_size = WritePopsSize<ArgLoc...>(&stack_state);
        return arg_size + push_size + pop_size + 5 + 1;
    }

    template <typename Ret, typename... Args>
    template <typename... ArgLoc>
    int Stdcall<Ret (Args...)>::WriteWrapper(uint8_t *out, uintptr_t out_len, uintptr_t target) {
        StackState stack_state(sizeof...(Args));
        uint8_t *pos = out;
        auto Write = [&](int amt) {
            pos += amt;
            out_len -= amt;
            return amt;
        };
        int written;
        written = Write(WritePushes<ArgLoc...>(&stack_state, pos, out_len));
        if (written < 0) { return written; }
        written = Write(WriteArguments<ArgLoc...>(&stack_state, pos, out_len));
        if (written < 0) { return written; }
        written = Write(hook::WriteCall(pos, out_len, (void *)target));
        if (written < 0) { return written; }
        written = Write(WritePops<ArgLoc...>(&stack_state, pos, out_len));
        if (written < 0) { return written; }
        written = Write(hook::WriteReturn(pos, out_len, 0));
        if (written < 0) { return written; }
        return pos - out;
    }
}

#endif /* FUNC_HPP */
