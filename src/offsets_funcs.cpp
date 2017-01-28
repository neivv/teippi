#include "offsets_funcs.h"

#include "patch/func.hpp"

const uintptr_t starcraft_exe_base = 0x00400000;
const uintptr_t storm_dll_base = 0x15000000;

using patch_func::Eax;
using patch_func::Ecx;
using patch_func::Edx;
using patch_func::Ebx;
using patch_func::Esi;
using patch_func::Edi;
using patch_func::Stack;

#define BW_FUNC(signature, name, addr, ...) \
    name.Init<__VA_ARGS__>(exec_heap, addr + diff)
#define FUNC_LIST_START(namesp, name) \
    void InitFuncs_ ## name( \
            Common::PatchManager *exec_heap, uintptr_t current_base_address, uintptr_t base \
        ) \
{ \
    using namespace namesp; \
    uintptr_t diff = current_base_address - base;

#define FUNC_LIST_END(name) }

#include "offsets_funcs_1161.h"

void InitBwFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address)
{
    InitFuncs_bw_1161(exec_heap, current_base_address, starcraft_exe_base);
}

void InitStormFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address)
{
    InitFuncs_storm_1161(exec_heap, current_base_address, storm_dll_base);
}

#define BW_FUNC(signature, name, addr, ...) \
    patch_func::Stdcall<signature> name
#include "offsets_funcs_1161.h"
