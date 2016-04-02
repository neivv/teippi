#ifndef PATCH_HOOK_H
#define PATCH_HOOK_H

/// Hooking functionality.
///
/// Mostly you just want to define the hooks with `Stdcall<FuncSignature>`,
/// possibly using the register marker types in arguments, eg.
/// `Stdcall<int(Eax<int>, void *, uint8_t *)>`
/// for a funtion which takes a `int` in eax, `void *` in stack arg 1,
/// and `uint8_t *` in stack arg 2.
/// The hook can be applied with `PatchContext::Hook()`.

namespace hook {
    /// Marker types for register arguments.
    template <class C> struct Eax {};
    template <class C> struct Ecx {};
    template <class C> struct Edx {};
    template <class C> struct Ebx {};
    template <class C> struct Esp {};
    template <class C> struct Ebp {};
    template <class C> struct Esi {};
    template <class C> struct Edi {};

    struct StackState {
        constexpr StackState(int stack_args)
            : arg_pos(0), hook_stack_amt(0), stack_diff(0), stack_arg_count(stack_args) { }
        /// Incremented by 1 when a value is pushed from stack,
        /// so e.g. Stdcall<int(int, int, int)> behaves as
        /// Stdcall<int(Stack<0, int>, Stack<1, int>, Stack<2, int>)>
        int arg_pos;
        /// Incremented by 1 whenever a value is pushed to stack.
        /// (So it's for every argument)
        int hook_stack_amt;
        /// hook_stack_amt and a possible 8 for pushad
        int stack_diff;
        /// Total amount of stack args.
        const int stack_arg_count;
    };

    template <typename C>
    struct WriteArgument {
        static int Write(StackState *state, uint8_t *out, int out_size) {
            int needed_length = Length(state);
            int offset = (state->stack_arg_count - state->arg_pos + state->stack_diff + 1) * sizeof(void *);
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

        static int Length(StackState *state) {
            state->arg_pos += 1;
            int offset = (state->stack_arg_count - state->arg_pos + state->stack_diff + 1) * sizeof(void *);
            if (offset >= 0x80) {
                return 7;
            } else {
                return 4;
            }
        }
    };

    inline int WriteByte(uint8_t *out, int out_size, uint8_t val) {
        if (out_size < 1)
            return -1;
        *out = val;
        return 1;
    }

#define WRITE_REG_ARGUMENT(Reg, byte) \
    template <typename C> \
    struct WriteArgument<Reg<C>> { \
        static int Write(StackState *stack_state, uint8_t *out, int out_size) { \
            return WriteByte(out, out_size, byte); \
        } \
        static int Length(StackState *state) { return 1; } \
    }; \


    template <typename Type>
    struct RemoveRegisterTagsImpl {
        using type = Type;
    };

    /// Used for converting type from `Eax<int>` to `int` etc.
    template <typename Type>
    using RemoveRegisterTags = typename RemoveRegisterTagsImpl<Type>::type;

#define REMOVE_REG_TAG(Reg) \
    template <typename Type> \
    struct RemoveRegisterTagsImpl<Reg<Type>> { \
        using type = Type; \
    };

    template <typename C>
    struct IsStackArg {
        static const bool value = true;
    };

    template <typename... Arguments>
    constexpr typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        StackArgCount() {
        return 0;
    }

    template <typename C, typename... Arguments>
    constexpr int StackArgCount() {
        return (IsStackArg<C>::value ? 1 : 0) + StackArgCount<Arguments...>();
    }

#define NOT_STACK_ARG(Reg) \
    template <typename Type> \
    struct IsStackArg<Reg<Type>> { \
        static const bool value = false; \
    };

#define REG_IMPLS(Reg, byte) \
    WRITE_REG_ARGUMENT(Reg, byte) \
    REMOVE_REG_TAG(Reg) \
    NOT_STACK_ARG(Reg)

REG_IMPLS(Eax, 0x50)
REG_IMPLS(Ecx, 0x51)
REG_IMPLS(Edx, 0x52)
REG_IMPLS(Ebx, 0x53)
REG_IMPLS(Esp, 0x54)
REG_IMPLS(Ebp, 0x55)
REG_IMPLS(Esi, 0x56)
REG_IMPLS(Edi, 0x57)

#undef REG_IMPLS
#undef REMOVE_TAG_IMPL
#undef WRITE_REG_ARGUMENT

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
        stack_state->stack_diff += 1;
        stack_state->hook_stack_amt += 1;
        if (written2 < 0)
            return false;
        return written + written2;
    }

    template <typename... Arguments>
    typename std::enable_if_t<(sizeof...(Arguments) == 0), int>
        WriteArgumentsLength(StackState *state) {
        return 0;
    }

    template <typename C, typename... Arguments>
    int WriteArgumentsLength(StackState *state) {
        int length = WriteArgumentsLength<Arguments...>(state);
        length += WriteArgument<C>::Length(state);
        state->stack_diff += 1;
        state->hook_stack_amt += 1;
        return length;
    }

    inline int WriteJump(uint8_t *out, int out_size, void *target_) {
        if (out_size < 5) { return -5; }
        uintptr_t target = (uintptr_t)target_;
        *out++ = 0xe9;
        uintptr_t value = target - (uintptr_t)out - 4;
        memcpy(out, &value, 4);
        return 5;
    }

    template <typename Ret, typename... Args>
    int WriteCall(uint8_t *out, int out_size, Ret (*target_)(Args...)) {
        if (out_size < 5) { return -5; }
        uintptr_t target = (uintptr_t)target_;
        *out++ = 0xe8;
        uintptr_t value = target - (uintptr_t)out - 4;
        memcpy(out, &value, 4);
        return 5;
    }

    template <typename Type>
    inline int WritePushConstant(uint8_t *out, int out_size, Type value) {
        static_assert(sizeof(Type) == 4, "Bad constant size");
        if (out_size < 5) {
            return -5;
        }
        *out++ = 0x68;
        auto ptr = &value;
        memcpy(out, ptr, 4);
        return 5;
    }

    inline int WritePushad(StackState *state, uint8_t *out, int out_size) {
        if (out_size < 1) {
            return -1;
        }
        *out++ = 0x60;
        state->stack_diff += 8;
        return 1;
    }

    inline int WritePopad(StackState *state, uint8_t *out, int out_size) {
        if (out_size < 1) {
            return -1;
        }
        *out++ = 0x61;
        state->stack_diff -= 8;
        return 1;
    }

    inline int WriteStackArgPop(uint8_t *out, int out_size, int arg_count) {
        if (arg_count == 0) {
            return 0;
        } else if (arg_count < 0x20) {
            if (out_size < 3) { return -3; }
            *out++ = 0x83;
            *out++ = 0xc4;
            *out++ = arg_count * sizeof(void *);
            return 3;
        } else {
            if (out_size < 6) { return -6; }
            *out++ = 0x81;
            *out++ = 0xc4;
            uint32_t val = arg_count * 4;
            memcpy(out, &val, 4);
            return 6;
        }
    }

    inline int WriteReturn(uint8_t *out, int out_size, int pop_arg_count) {
        if (pop_arg_count == 0) {
            if (out_size < 1) { return -1; }
            *out = 0xc3;
            return 1;
        } else {
            if (out_size < 3) { return -3; }
            *out++ = 0xc2;
            uint16_t value = pop_arg_count * sizeof(void *);
            memcpy(out, &value, 2);
            return 3;
        }
    }

    /// As `Hook()` requires cdecl function, this can be used to convert to
    /// class member call. It gets passed the member function pointer
    /// as a argument, which gets written to the generated assembly wrapper.
    template <typename MemFn, typename Signature>
    struct MemberFnHelper;

    template <typename MemFn, typename Ret, typename Self, typename... Args>
    struct MemberFnHelper<MemFn, Ret(Self *, Args...)> {
        static Ret Func(MemFn *fn, Self *self, Args... rest) {
            return (self->*(*fn))(rest...);
        }
    };

    template <typename... Args>
    struct IsFirstArgClassPtr {
        const static bool value = false;
    };

    template <typename First, typename... Rest>
    struct IsFirstArgClassPtr<First, Rest...> {
        const static bool value = std::is_class<std::remove_pointer_t<First>>::value &&
            std::is_pointer<First>::value;
    };

    template <bool Has, typename Ret, typename... Args>
    class MemberFn {
        public:
            using MemberFnTarget = void *;
    };

    template <typename Ret, typename Class, typename... Args>
    class MemberFn<true, Ret, Class, Args...> {
        public:
            /// The hook `Stdcall<int(Unit *, int)>` can also be hooked
            /// to int Unit::Func(int).
            using MemberFnTarget = std::conditional_t<std::is_const<std::remove_pointer_t<Class>>::value,
                Ret (std::remove_pointer_t<Class>::*)(Args...) const,
                Ret (std::remove_pointer_t<Class>::*)(Args...)>;
    };

    template <typename Ret, typename... Args>
    class HookBase : public MemberFn<IsFirstArgClassPtr<Args...>::value, Ret, Args...> {
        public:
            using MemFn = MemberFn<IsFirstArgClassPtr<Args...>::value, Ret, Args...>;
            using Target = Ret (Args...);
    };

    template <typename Signature>
    class Stdcall;

    template <typename Ret, typename... Args>
    class Stdcall<Ret (Args...)> : public HookBase<Ret, RemoveRegisterTags<Args>...> {
        public:
            using Base = HookBase<Ret, RemoveRegisterTags<Args>...>;
            using MemFn = typename Base::MemFn;
            static const int StackSize = StackArgCount<Args...>();

            constexpr Stdcall(uintptr_t address) : address(address) {
            }

            /// `call_hook` is true for call hooks.
            static int WriteConversionWrapper(typename Base::Target target, uint8_t *out,
                    int buf_size, bool call_hook) {
                StackState stack_state(StackSize);
                uint8_t *pos = out;
                auto Write = [&](int amt) {
                    pos += amt;
                    buf_size -= amt;
                    return amt;
                };
                int written;
                if (call_hook) {
                    written = Write(WritePushad(&stack_state, pos, buf_size));
                    if (written < 0) { return written; }
                }
                written = Write(WriteArguments<Args...>(&stack_state, pos, buf_size));
                if (written < 0) { return written; }
                written = Write(WriteCall(pos, buf_size, target));
                if (written < 0) { return written; }
                written = Write(WriteStackArgPop(pos, buf_size, stack_state.hook_stack_amt));
                if (written < 0) { return written; }
                if (!call_hook) {
                    written = Write(WriteReturn(pos, buf_size, stack_state.arg_pos));
                    if (written < 0) { return written; }
                } else {
                    written = Write(WritePopad(&stack_state, pos, buf_size));
                    if (written < 0) { return written; }
                }
                return pos - out;
            }

            /// Returns the buf_size required in WriteArgumentConversion
            static int WrapperLength(bool call_hook) {
                StackState stack_state(StackSize);
                int arg_len = WriteArgumentsLength<Args...>(&stack_state);
                int stack_revert_ins_size = [=]() {
                    if (stack_state.hook_stack_amt == 0) { return 0; }
                    else if (stack_state.hook_stack_amt < 0x20) { return 3; }
                    else { return 6; }
                }();
                int ret_ins_size = [=]() {
                    if (call_hook) { return 0; }
                    if (stack_state.arg_pos == 0) { return 1; }
                    else { return 3; }
                }();
                int call_pushpop_size = call_hook ? 2 : 0;
                return arg_len + 5 + stack_revert_ins_size + ret_ins_size + call_pushpop_size;
            }

            static int WriteMemFnWrapper(typename MemFn::MemberFnTarget *target, uint8_t *out,
                    int buf_size, bool call_hook) {
                StackState stack_state(StackSize);
                uint8_t *pos = out;
                auto Write = [&](int amt) {
                    pos += amt;
                    buf_size -= amt;
                    return amt;
                };
                auto intermediate_target =
                    MemberFnHelper<typename MemFn::MemberFnTarget, typename Base::Target>::Func;

                int written;
                if (call_hook) {
                    written = Write(WritePushad(&stack_state, pos, buf_size));
                    if (written < 0) { return written; }
                }
                written = Write(WriteArguments<Args...>(&stack_state, pos, buf_size));
                if (written < 0) { return written; }
                written = Write(WritePushConstant(pos, buf_size, target));
                if (written < 0) { return written; }
                written = Write(WriteCall(pos, buf_size, intermediate_target));
                if (written < 0) { return written; }
                written = Write(WriteStackArgPop(pos, buf_size, stack_state.hook_stack_amt + 1));
                if (written < 0) { return written; }
                if (!call_hook) {
                    written = Write(WriteReturn(pos, buf_size, stack_state.arg_pos));
                    if (written < 0) { return written; }
                } else {
                    written = Write(WritePopad(&stack_state, pos, buf_size));
                    if (written < 0) { return written; }
                }
                return pos - out;
            }

            /// Returns the buf_size required in WriteArgumentConversion
            static int MemFnWrapperLength(bool call_hook) {
                StackState stack_state(StackSize);
                int arg_len = WriteArgumentsLength<Args...>(&stack_state);
                stack_state.stack_diff += 1;
                stack_state.hook_stack_amt += 1;
                int stack_revert_ins_size = [=]() {
                    if (stack_state.hook_stack_amt == 0) { return 0; }
                    else if (stack_state.hook_stack_amt < 0x20) { return 3; }
                    else { return 6; }
                }();
                int ret_ins_size = [=]() {
                    if (call_hook) { return 0; }
                    if (stack_state.arg_pos == 0) { return 1; }
                    else { return 3; }
                }();
                int call_pushpop_size = call_hook ? 2 : 0;
                return arg_len + 5 + 5 + stack_revert_ins_size + ret_ins_size + call_pushpop_size;
            }

            uintptr_t address;
    };

} // namespace hook

#endif /* PATCH_HOOK_H */
