#include "patchmanager.h"

#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <functional>

template class std::vector<Common::Patch>;

Common::PatchManager *patch_mgr;

namespace Common
{

PatchManager::PatchManager()
{
    // There's large limit in case something goes wrong
    hookcode_heap = HeapCreate(0x00040000, 0, 0x100000); // 0x00040000 == HEAP_CREATE_ENABLE_EXECUTE, mingw didn't have
}

PatchManager::~PatchManager()
{
    for (auto it = patches.begin(); it != patches.end(); ++it)
    {
        delete it->previous_data;
    }
}

void PatchManager::Unpatch(bool repatch)
{
    std::vector<UnprotectModule> protections;
    protections.emplace_back(GetModuleHandle(NULL));
    for (const auto &mod : patched_modules)
        protections.emplace_back(GetModuleHandle(mod.c_str()));

    std::vector<uint8_t> buf;
    buf.reserve(256);
    auto lambda = [&](Patch &patch)
    {
        if (patch.applied == repatch)
            return;
        if (patch.length > buf.capacity())
            buf.reserve(patch.length);

        using namespace std::placeholders;
        std::function<void(void *, void *)> backup_to_code = std::bind(memcpy, _1, _2, patch.length);
        auto code_to_backup = backup_to_code;
        if ((patch.flags & PATCH_CALLHOOK) == PATCH_CALLHOOK)
        {
            if (repatch)
                code_to_backup = std::bind(CopyInstructions, _1, _2, patch.length);
            else
                backup_to_code = std::bind(CopyInstructions, _1, _2, patch.length);
        }
        code_to_backup(buf.data(), patch.address);
        backup_to_code(patch.address, patch.previous_data);
        code_to_backup(patch.previous_data, buf.data());
        patch.applied = repatch;
    };
    if (repatch)
        std::for_each(patches.begin(), patches.end(), lambda);
    else
        std::for_each(patches.rbegin(), patches.rend(), lambda);
}

void PatchManager::Repatch()
{
    Unpatch(true);
}

PatchContext::PatchContext(PatchManager *parent_, const char *module_name, uint32_t expected_base)
{
    void *handle = GetModuleHandle(module_name);
    diff = (uint32_t)handle - expected_base;
    protect.Init(handle);
    parent = parent_;
}

PatchContext::PatchContext(PatchContext &&other)
{
    protect = std::move(other.protect);
    diff = other.diff;
    parent = other.parent;
}

PatchContext PatchManager::BeginPatch(const char *module_name, uint32_t expected_base)
{
    if (module_name)
    {
        bool found = false;
        for (const auto &mod : patched_modules)
        {
            if (strcmpi(module_name, mod.c_str()) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            patched_modules.push_back(module_name);
        }
    }
    return PatchContext(this, module_name, expected_base);
}

bool PatchContext::Patch(void *address, void *buf, int length, int flags)
{
    address = (uint8_t *)address + diff;
    try
    {
        struct Patch patch;

        patch.flags = flags;
        patch.address = address;
        patch.applied = true;

        if (flags & PATCH_REPLACE)
        {
            patch.length = length;
            patch.previous_data = new uint8_t[length];
            memcpy(patch.previous_data, address, length);
            memcpy(address, buf, length);
        }
        else if (flags & PATCH_REPLACE_DWORD)
        {
            patch.length = length;
            patch.previous_data = new uint8_t[length];
            memcpy(patch.previous_data, address, length);
            memcpy(address, &buf, length);
        }
        else if (flags & PATCH_NOP)
        {
            patch.length = length;
            patch.previous_data = new uint8_t[length];
            memcpy(patch.previous_data, address, length);
            memset(address, 0x90, length);
        }
        else if ((flags & (PATCH_JMPHOOK)) == (PATCH_JMPHOOK))
        {
            patch.length = 5;
            patch.previous_data = new uint8_t[5];
            memcpy(patch.previous_data, address, 5);

            *(uint8_t *)address = 0xe9;
            uint32_t *reljmp = (uint32_t *)((uint8_t *)address + 1);
            reljmp[0] = (uint32_t)buf - ((uint32_t)reljmp + 4);
        }
        else if (flags & PATCH_HOOK)
        {
            bool optional_hook = (flags & PATCH_OPTIONALHOOK) == PATCH_OPTIONALHOOK;
            bool safe_hook = (flags & PATCH_SAFECALLHOOK) == PATCH_SAFECALLHOOK;
            int stack_arg_bytes = length;
            patch.length = CountInstructionLength(address, 5);
            int hookcode_size = 10;
            if (safe_hook)
                hookcode_size += 2 + stack_arg_bytes / 4 * 4; // 4 bytes for each repushed arg
            if (optional_hook)
                hookcode_size += 7;
            patch.previous_data = (uint8_t *)parent->AllocExecMem(hookcode_size + patch.length);
            uint8_t *customcode;
            CopyInstructions(patch.previous_data + hookcode_size - 5, address, patch.length);
            customcode = patch.previous_data;

            if (safe_hook)
            {
                *customcode++ = 0x60; // Pushad
                for (int i = 0; i < stack_arg_bytes / 4; i++)
                {
                    *customcode++ = 0xff; // Push dword [esp + x]
                    *customcode++ = 0x74;
                    *customcode++ = 0xe4;
                    *customcode++ = 0x20 + stack_arg_bytes;
                }
            }
            customcode[0] = 0xe8;

            *(uint8_t *)address = 0xe9;
            uint32_t *reljmp = (uint32_t *)((uint8_t *)address + 1); // address -> hookbuf
            reljmp[0] = (uint32_t)patch.previous_data - ((uint32_t)reljmp + 4);
            reljmp = (uint32_t *)(customcode + 1); // hookbuf -> dest
            reljmp[0] = (uint32_t)buf - ((uint32_t)reljmp + 4);

            customcode += 5;
            if (optional_hook)
            {
                *customcode++ = 0x84; // Test
                *customcode++ = 0xc0; // Al, al
            }
            if (safe_hook)
            {
                *customcode++ = 0x61; // Popad
            }
            if (optional_hook)
            {
                *customcode++ = 0x74; // Je
                *customcode++ = 0x01; // Eip + 1
                *customcode++ = 0xc2; // Ret
                *customcode++ = stack_arg_bytes & 0xff; // Stack size
                *customcode++ = (stack_arg_bytes >> 8) & 0xff;
            }

            customcode += patch.length;
            patch.previous_data += hookcode_size - 5;

            customcode[0] = 0xe9;

            reljmp = (uint32_t *)(customcode + 1); // hookbuf -> ret
            reljmp[0] = (uint32_t)address + patch.length - ((uint32_t)reljmp + 4);
        }

        parent->patches.push_back(patch);
        return true;
    }
    catch (const char *exp)
    {
        char buf[400];
        snprintf(buf, 200, "Expection \"%s\" while patching %p", exp, address);
        MessageBox(0, buf, "", 0);
        return false;
    }
}

void *PatchManager::AllocExecMem(int amt) const
{
    return HeapAlloc(hookcode_heap, 0, amt);
}

void PatchManager::FreeExecMem(void *mem) const
{
    HeapFree(hookcode_heap, 0, mem);
}

bool PatchContext::ImportPatch(uintptr_t base, const char *dll, const char *function, void *hook)
{
    base += diff;
    uint32_t pe_header_address = *(uint32_t *)(base + 0x3c);
    uint32_t *import_table_address = (uint32_t *)(base + pe_header_address + 0x80);
    uint32_t import_entries = import_table_address[1] / 0x14;

    uint8_t *import_header = (uint8_t *)(base + import_table_address[0]);
    for (uint32_t i = 0; i < import_entries; i++)
    {
        uint32_t str_off = *(uint32_t *)(import_header + 0xc);
        const char *str = (const char *)(base + str_off);
        if (strcmpi(str, dll) == 0)
        {
            const char **import_lookup = (const char **)(base + *(uint32_t *)import_header);
            int j = 0;
            for (; *import_lookup; import_lookup++, j++)
            {
                const char *value = *import_lookup + base;
                if ((uint32_t)value & 0x80000000)
                    continue;
                if (strcmp(value + 2, function) == 0)
                {
                    void **import_address = (void **)(base + *(uint32_t *)(import_header + 0x10));
                    import_address[j] = hook;
                    return true;
                }
            }
            return false;
        }
        import_header += 0x14;
    }
    return false;
}

} // namespace Common
