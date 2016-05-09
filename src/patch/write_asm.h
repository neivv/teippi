#ifndef ASSEMBLER_H
#define ASSEMBLER_H

/// Contains functions to write some assembly instructions to memory,
/// which are needed for function hooks/calls

namespace hook {
    inline int WriteJump(uint8_t *out, int out_size, void *target_) {
        if (out_size < 5) { return -5; }
        uintptr_t target = (uintptr_t)target_;
        *out++ = 0xe9;
        uintptr_t value = target - (uintptr_t)out - 4;
        memcpy(out, &value, 4);
        return 5;
    }

    inline int WriteCall(uint8_t *out, int out_size, void *target_) {
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

    inline int WritePushad(uint8_t *out, int out_size) {
        if (out_size < 1) {
            return -1;
        }
        *out++ = 0x60;
        return 1;
    }

    inline int WritePopad(uint8_t *out, int out_size) {
        if (out_size < 1) {
            return -1;
        }
        *out++ = 0x61;
        return 1;
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
}
#endif /* ASSEMBLER_H */
