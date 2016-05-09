#ifndef FUNC_H
#define FUNC_H

namespace Common { class PatchManager; }

namespace patch_func {
    struct Eax {};
    struct Ecx {};
    struct Edx {};
    struct Ebx {};
    struct Esp {};
    struct Ebp {};
    struct Esi {};
    struct Edi {};
    struct Stack {};

    template <typename Signature>
    class Stdcall;

    template <typename Ret, typename... Args>
    class Stdcall<Ret (Args...)> {
        public:
            constexpr Stdcall() : wrapper_code(nullptr) { }

            template <typename... ArgLoc>
            void Init(Common::PatchManager *exec_heap, uintptr_t address);

            template <typename... ArgLoc>
            static uintptr_t WrapperSize();

            template <typename... ArgLoc>
            static int WriteWrapper(uint8_t *out, uintptr_t out_len, uintptr_t target);

            Ret operator()(Args... args) const {
                return wrapper_code(args...);
            }

        private:
            Ret (*wrapper_code)(Args...);
    };

} // namespace patch_func

#endif /* FUNC_H */
