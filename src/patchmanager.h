#ifndef PATCHMANAGER_H
#define PATCHMANAGER_H

#include <vector>
#include <stdint.h>
#include <string>
#include "memory.h"
#include "x86.h"
#include "patch_hook.h"

#include "common/assert.h"

const int PATCH_HOOKBEFORE = 2;
const int PATCH_HOOK = 2;
const int PATCH_REPLACE = 0x20;
const int PATCH_REPLACE_DWORD = 0x80;
const int PATCH_SAFECALLHOOK = 0xe;
const int PATCH_CALLHOOK = 0xa;
const int PATCH_JMPHOOK = 0x12;
const int PATCH_NOP = 0x40;
// Return false to run original code afterwards
// length contains size of popped stack args
const int PATCH_OPTIONALHOOK = 0x102;

namespace Common
{

class PatchManager;

struct Patch
{
    Patch() {}
    constexpr Patch(void *address, uint16_t flags) : address(address), previous_data(nullptr),
        length(0), flags(flags), applied(true) { }
    void *address;
    uint8_t *previous_data;
    uint16_t length;
    uint16_t flags;
    bool applied;
};

class PatchContext
{
    friend class PatchManager;
    public:
        PatchContext(PatchContext &&other);

        bool Patch(void *address, void *buf, int length, int flags);
        template <typename Func> bool JumpHook(void *address, Func &hook)
        {
            return Patch(address, (void *)&hook, 0, PATCH_JMPHOOK);
        }

        bool ImportPatch(uintptr_t base, const char *dll, const char *function, void *hook);

        int GetDiff() const { return diff; }

        template <typename Hook>
        void Hook(const Hook &hook, typename Hook::Target target);

        template <typename Hook>
        void Hook(const Hook &hook, typename Hook::MemberFnTarget target);

        template <typename Hook>
        void CallHook(const Hook &hook, typename Hook::Target target);

        template <typename Hook>
        void CallHook(const Hook &hook, typename Hook::MemberFnTarget target);

    private:
        PatchContext(PatchManager *parent, const char *module_name, uint32_t expected_base);

        UnprotectModule protect;
        uintptr_t diff;
        PatchManager *parent;
};

class PatchManager
{
    friend class PatchContext;
    public:
        PatchManager();
        ~PatchManager();

        PatchContext BeginPatch(const char *module_name, uint32_t expected_base);

        void Unpatch(bool repatch = false);
        void Repatch();

        void *AllocExecMem(int amt) const;
        void FreeExecMem(void *mem) const;

    private:
        std::vector<struct Patch> patches;
        std::vector<std::string> patched_modules;

        uint32_t ChangeProtection(void *address, int length, uint32_t protection);
        void *hookcode_heap;
};

template <typename HookType>
void PatchContext::Hook(const HookType &hook, typename HookType::Target target) {
    uint8_t *address = (uint8_t *)(hook.address + diff);
    struct Patch patch(address, PATCH_HOOK);
    patch.length = 5;
    auto wrapper_length = hook.WrapperLength(false);
    uint8_t *wrapper = (uint8_t *)parent->AllocExecMem(wrapper_length + patch.length);
    auto written_length = hook.WriteConversionWrapper(target, wrapper, wrapper_length, false);
    Assert(written_length == wrapper_length);
    patch.previous_data = wrapper + wrapper_length;
    memcpy(patch.previous_data, address, patch.length);
    hook::WriteJump(address, 5, wrapper);
    parent->patches.push_back(patch);
}

/// Pointers to member functions are fun...
/// They are a lot more complicated than regular pointers to functions,
/// as they have to work with virtual functions.
/// As such, they get stored to memory, and the hook assembly passes that
/// pointer with any other arguments to a intermediate function,
/// which does `return (class_var->*func)(args...);`
/// Sigh.
template <typename HookType>
void PatchContext::Hook(const HookType &hook, typename HookType::MemberFnTarget target) {
    uint8_t *address = (uint8_t *)(hook.address + diff);
    struct Patch patch(address, PATCH_HOOK);
    patch.length = 5;
    auto wrapper_length = hook.MemFnWrapperLength(false);
    auto member_fn_size = sizeof(typename HookType::MemberFnTarget);
    uint8_t *wrapper = (uint8_t *)parent->AllocExecMem(wrapper_length + patch.length + member_fn_size);
    auto member_fn_addr = (typename HookType::MemberFnTarget *)(wrapper + wrapper_length + patch.length);
    memcpy(member_fn_addr, &target, member_fn_size);
    auto written_length = hook.WriteMemFnWrapper(member_fn_addr, wrapper, wrapper_length, false);
    Assert(written_length == wrapper_length);
    patch.previous_data = wrapper + wrapper_length;
    memcpy(patch.previous_data, address, patch.length);
    hook::WriteJump(address, 5, wrapper);
    parent->patches.push_back(patch);
}

/// A hook that doesn't replace anything.
template <typename HookType>
void PatchContext::CallHook(const HookType &hook, typename HookType::Target target) {
    uint8_t *address = (uint8_t *)(hook.address + diff);
    struct Patch patch(address, PATCH_HOOK);
    patch.length = CountInstructionLength(address, 5);
    auto wrapper_length = hook.WrapperLength(true);
    uint8_t *wrapper = (uint8_t *)parent->AllocExecMem(wrapper_length + patch.length + 5);
    auto written_length = hook.WriteConversionWrapper(target, wrapper, wrapper_length, true);
    Assert(written_length == wrapper_length);
    patch.previous_data = wrapper + wrapper_length;
    CopyInstructions(patch.previous_data, address, patch.length);
    hook::WriteJump(address, 5, wrapper);
    hook::WriteJump(patch.previous_data + patch.length, 5, address + patch.length);
    parent->patches.push_back(patch);
}

template <typename HookType>
void PatchContext::CallHook(const HookType &hook, typename HookType::MemberFnTarget target) {
    uint8_t *address = (uint8_t *)(hook.address + diff);
    struct Patch patch(address, PATCH_HOOK);
    patch.length = CountInstructionLength(address, 5);
    auto wrapper_length = hook.MemFnWrapperLength(true);
    auto member_fn_size = sizeof(typename HookType::MemberFnTarget);
    uint8_t *wrapper = (uint8_t *)parent->AllocExecMem(wrapper_length + patch.length + 5 + member_fn_size);
    auto member_fn_addr = (typename HookType::MemberFnTarget *)(wrapper + wrapper_length + patch.length + 5);
    memcpy(member_fn_addr, &target, member_fn_size);
    auto written_length = hook.WriteMemFnWrapper(member_fn_addr, wrapper, wrapper_length, true);
    Assert(written_length == wrapper_length);
    patch.previous_data = wrapper + wrapper_length;
    memcpy(patch.previous_data, address, patch.length);
    hook::WriteJump(address, 5, wrapper);
    hook::WriteJump(patch.previous_data + patch.length, 5, address + patch.length);
    parent->patches.push_back(patch);
}

} // namespace Common

extern Common::PatchManager *patch_mgr;

#endif // PATCHMANAGER_H

