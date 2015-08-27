#include <windows.h>
#include <wchar.h>
#include <string.h>

#include "types.h"

// From mpqdraft.cpp
extern HINSTANCE self_instance;

#pragma pack(push, 1)
struct RemoteThreadArgs
{
    FARPROC LoadLibraryW; // 0
    FARPROC GetProcAddress; // 4
    FARPROC MessageBoxW; // 8
    wchar_t *library_name; // c
    char *proc_name; // 10
    wchar_t *error; // 14
    wchar_t *load_library_name; // 18
    wchar_t *get_proc_address_name; // 1c
    FARPROC FreeLibrary; // 20
};
#pragma pack(pop)

/// Allocates small piece of code and data in process' address space, which can used called with
/// CreateRemoteThread. The code loads this .dll and calls Initialize()
/// Returns tuple (code, param), should VirtualFreeEx code after using (param is on same block)
tuple<void *, void *> CreateInjectCode(HANDLE process)
{
    // The assembly is written as uint8_t array to make it work easily across different compilers
    const uint8_t assembly[] = {
        //0xeb, 0xfe,             // infloop for debugging
        0x8b, 0x5c, 0xe4, 0x04, // mov ebx, [esp + 4] (arg struct)
        0xff, 0x73, 0x0c,       // push dword [ebx + c] (lib name)
        0xff, 0x13,             // call dword [ebx + 0] (LoadLibraryW)
        0x85, 0xc0,             // test eax, eax
        0x75, 0x12,             // jne load_ok
        0x6a, 0x00,             // push 0
        0xff, 0x73, 0x18,       // push dword [ebx + 18] (L"LoadLibraryW failed")
        0xff, 0x73, 0x14,       // push dword [ebx + 14] (L"Error")
        0x6a, 0x00,             // push 0
        0xff, 0x53, 0x08,       // call dword [ebx + 8] (MessageBoxW)
        0x31, 0xc0,             // xor eax, eax
        0xc2, 0x04, 0x00,       // ret 4
        // load_ok:
        0x89, 0xc6,             // mov esi, eax
        0xff, 0x73, 0x10,       // push dword [ebx + 10] (proc name)
        0x50,                   // push eax
        0xff, 0x53, 0x04,       // call dword [ebx + 4] (GetProcAddress)
        0x85, 0xc0,             // test eax, eax
        0x75, 0x16,             // jne proc_ok
        0x6a, 0x00,             // push 0
        0xff, 0x73, 0x1c,       // push dword [ebx + 1c] (L"GetProcAddress failed")
        0xff, 0x73, 0x14,       // push dword [ebx + 14] (L"Error")
        0x6a, 0x00,             // push 0
        0xff, 0x53, 0x08,       // call dword [ebx + 8] (MessageBoxW)
        0x56,                   // push esi
        0xff, 0x53, 0x20,       // call dword [ebx + 20] (FreeLibrary)
        0x31, 0xc0,             // xor eax, eax
        0xc2, 0x04, 0x00,       // ret 4
        // proc_ok:
        0xff, 0xd0,             // call eax
        0x31, 0xc0,             // xor eax, eax
        0xc2, 0x04, 0x00,       // ret 4
    };
    int asm_len = sizeof assembly;
    wchar_t library_name[MAX_PATH];
    int lib_name_len = GetModuleFileNameW(self_instance, library_name, MAX_PATH);
    if (lib_name_len == 0 || lib_name_len == MAX_PATH)
    {
        MessageBoxW(0, L"GetModuleFileNameW failed", L"Limit plugin", 0);
        return make_tuple(nullptr, nullptr);
    }
    lib_name_len = (lib_name_len + 1) * sizeof(wchar_t);
    const char *proc_name = "Initialize";
    int proc_name_len = strlen(proc_name) + 1;
    const wchar_t *error = L"Limit plugin error";
    int error_len = (wcslen(error) + 1) * sizeof(wchar_t);
    const wchar_t *load_library_error = L"LoadLibraryW failed";
    int load_library_error_len = (wcslen(load_library_error) + 1) * sizeof(wchar_t);
    const wchar_t *get_proc_address_error = L"GetProcAddress failed";
    int get_proc_address_error_len = (wcslen(get_proc_address_error) + 1) * sizeof(wchar_t);

    auto kernel32 = LoadLibraryW(L"kernel32");
    auto user32 = LoadLibraryW(L"user32");
    RemoteThreadArgs args = {
        GetProcAddress(kernel32, "LoadLibraryW"),
        GetProcAddress(kernel32, "GetProcAddress"),
        GetProcAddress(user32, "MessageBoxW"),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        GetProcAddress(kernel32, "FreeLibrary"),
    };
    FreeLibrary(kernel32);
    FreeLibrary(user32);

    void *memory = VirtualAllocEx(process, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (memory == NULL)
    {
        MessageBoxW(0, L"VirtualAllocEx failed", L"Limit plugin", 0);
        return make_tuple(nullptr, nullptr);
    }

    uintptr_t data_pos = (uintptr_t)memory + asm_len + sizeof(RemoteThreadArgs);
    args.library_name = (wchar_t *)data_pos;
    data_pos += lib_name_len;
    args.proc_name = (char *)data_pos;
    data_pos += proc_name_len;
    args.error = (wchar_t *)data_pos;
    data_pos += error_len;
    args.load_library_name = (wchar_t *)data_pos;
    data_pos += load_library_error_len;
    args.get_proc_address_name = (wchar_t *)data_pos;
    data_pos += get_proc_address_error_len;

#define CheckWriteProcessMemoryError(s) if (s == 0)\
        { MessageBoxW(0, L"WriteProcessMemory failed", L"Limit plugin", 0); return make_tuple(nullptr, nullptr); }
    data_pos = (uintptr_t)memory;
    auto result = WriteProcessMemory(process, (void *)data_pos, assembly, asm_len, NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += asm_len;
    result = WriteProcessMemory(process, (void *)data_pos, &args, sizeof(RemoteThreadArgs), NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += sizeof(RemoteThreadArgs);
    result = WriteProcessMemory(process, (void *)data_pos, library_name, lib_name_len, NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += lib_name_len;
    result = WriteProcessMemory(process, (void *)data_pos, proc_name, proc_name_len, NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += proc_name_len;
    result = WriteProcessMemory(process, (void *)data_pos, error, error_len, NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += error_len;
    result = WriteProcessMemory(process, (void *)data_pos, load_library_error, load_library_error_len, NULL);
    CheckWriteProcessMemoryError(result);
    data_pos += load_library_error_len;
    result = WriteProcessMemory(process, (void *)data_pos, get_proc_address_error, get_proc_address_error_len, NULL);
    CheckWriteProcessMemoryError(result);
#undef CheckWriteProcessMemoryError

    DWORD old_protect;
    result = VirtualProtectEx(process, memory, 0x1000, PAGE_EXECUTE_READ, &old_protect);
    if (result == 0)
    {
        // Ideally should not matter..
    }
    return make_tuple(memory, (void *)((uintptr_t)memory + asm_len));
}

/// Really sketchy function to inject this plugin to another process
BOOL InjectToProcess(HANDLE process)
{
    auto tuple = CreateInjectCode(process);
    void *code = std::get<0>(tuple);
    void *param = std::get<1>(tuple);
    if (code == nullptr || param == nullptr)
        return FALSE;

    HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)code, param, 0, NULL);
    if (thread == NULL)
    {
        MessageBoxW(0, L"CreateRemoteThread failed", L"Limits plugin", 0);
        return FALSE;
    }
    auto result = WaitForSingleObject(thread, 25000);
    CloseHandle(thread);
    if (result == WAIT_TIMEOUT)
    {
        MessageBoxW(0, L"Timeout while waiting for the inject thread", L"Limits plugin", 0);
        // Return TRUE anyways, just leaks the VirtualAllocEx memory (and may crash if didn't patch everything yet)
        return TRUE;
    }

    VirtualFreeEx(process, code, 0, MEM_RELEASE);
    if (result != WAIT_OBJECT_0)
    {
        MessageBoxW(0, L"WaitForSingleObject failed", L"Limits plugin", 0);
        return FALSE;
    }
    return TRUE;
}

/// Bwlauncher plugin entry point
extern "C" BOOL ApplyPatchSuspended(HANDLE process, DWORD process_id)
{
    return InjectToProcess(process);
}

/// Bwlauncher plugin post window creation hook
extern "C" BOOL ApplyPatch(HANDLE process, DWORD process_id)
{
    return TRUE;
}

struct ExchangeData
{
    int iPluginAPI;
    int iStarCraftBuild;
    BOOL bNotSCBWmodule;                //Inform user that closing BWL will shut down your plugin
    BOOL bConfigDialog;                 //Is Configurable
};

/// Bwlauncher plugin query
extern "C" void GetPluginAPI(ExchangeData *data)
{
    data->iPluginAPI = 4;
    data->iStarCraftBuild = 13; // 1.16.1
    data->bNotSCBWmodule = FALSE;
    data->bConfigDialog = FALSE;
}

/// Bwlauncher info query
extern "C" void GetData(char *name, char *description, char *url)
{
    // How large are these buffers??
    strcpy(name, "Limits plugin");
    strcpy(description, "Removes unit/bullet/etc limits from the game.\r\n\
Every player needs to be using this plugin for it to work in multiplayer \
and there is no compatibility with vanilla Starcraft.");
    strcpy(url, "");
}
