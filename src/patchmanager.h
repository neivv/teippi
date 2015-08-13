#ifndef PATCHMANAGER_H
#define PATCHMANAGER_H

#include <vector>
#include <stdint.h>
#include <string>
#include "memory.h"

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

#ifdef __GNUC__
#define REG_EAX(type, var) type var; asm volatile("" : "=ea"(var))
#define REG_ECX(type, var) type var; asm volatile("" : "=ec"(var))
#define REG_EDX(type, var) type var; asm volatile("" : "=ed"(var))
#define REG_EBX(type, var) type var; asm volatile("" : "=eb"(var))
#define REG_ESI(type, var) type var; asm volatile("" : "=eS"(var))
#define REG_EDI(type, var) type var; asm volatile("" : "=eD"(var))
#else // Assume msvc
#define REG_EAX(type, var) type var; __asm { mov var, eax }
#define REG_ECX(type, var) type var; __asm { mov var, ecx }
#define REG_EDX(type, var) type var; __asm { mov var, edx }
#define REG_EBX(type, var) type var; __asm { mov var, ebx }
#define REG_ESI(type, var) type var; __asm { mov var, esi }
#define REG_EDI(type, var) type var; __asm { mov var, edi }
#endif

struct Patch
{
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

    private:
        PatchContext(PatchManager *parent, const char *module_name, uint32_t expected_base);

        UnprotectModule protect;
        uint32_t diff;
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

} // namespace Common

extern Common::PatchManager *patch_mgr;

#endif // PATCHMANAGER_H

