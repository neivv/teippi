#include "memory.h"
#include <windows.h>
#include <stdio.h>

SYSTEM_INFO sysinfo;

template class std::vector<PageGroup>;
void DumpMemory(const char *filename, uintptr_t start, uintptr_t len)
{
    FILE *filu = fopen(filename, "wb");
    if (!filu)
        return;
    fwrite((void *)start, len, 1, filu);
    fclose(filu);
}

void InitSystemInfo()
{
    GetSystemInfo(&sysinfo);
}

UnprotectModule &UnprotectModule::operator=(UnprotectModule &&other)
{
    protections = std::move(other.protections);
    return *this;
}

void UnprotectModule::Init(void *module)
{
    if (!module)
        module = GetModuleHandle(0);

    // Toivotaan että .text on ennen mitää ei-img pageja
    MEMORY_BASIC_INFORMATION page_info;
    VirtualQuery(module, &page_info, sizeof(page_info));
    while (page_info.Type == MEM_IMAGE)
    {
        // Sama vaa unprotect aivan kaikki...
        protections.emplace_back(page_info.BaseAddress, page_info.RegionSize, page_info.Protect);
        unsigned long tmp;
        VirtualProtect(page_info.BaseAddress, page_info.RegionSize, PAGE_EXECUTE_READWRITE, &tmp);

        VirtualQuery((void *)((uint8_t *)page_info.BaseAddress + page_info.RegionSize), &page_info, sizeof(page_info));
    }

}

UnprotectModule::~UnprotectModule()
{
    for (auto page = protections.begin(); page != protections.end(); ++page)
    {
        unsigned long tmp;
        VirtualProtect(page->first_page, page->length, page->protection, &tmp);
    }
}

TempMemoryPool::TempMemoryPool(uint32_t default_chunk_sz)
{
    mem_chunk_default_size = default_chunk_sz;
    AddMemArea(default_chunk_sz);
    current_mem = memory.begin();
    pos = current_mem->first;
}

TempMemoryPool::~TempMemoryPool()
{
    for (auto &pair : memory)
    {
        VirtualFree(pair.first, 0, MEM_RELEASE);
    }
}

void TempMemoryPool::AddMemArea(uint32_t mem_chunk_size)
{
    uint8_t *chunk = (uint8_t *)VirtualAlloc(0, mem_chunk_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); // 4mt
    memory.emplace_back(chunk, chunk + mem_chunk_size);
}

void TempMemoryPool::ClearAll()
{
    current_mem = memory.begin();
    pos = current_mem->first;
}

uint8_t *TempMemoryPool::AllocateBase(uint32_t size)
{
    if (pos + size <= current_mem->second)
    {
        uint8_t *ret = pos;
        pos += size;
        return ret;
    }
    else
    {
        ++current_mem;

        if (current_mem == memory.end())
        {
            AddMemArea(mem_chunk_default_size);
            current_mem = memory.end() - 1;
        }
        pos = current_mem->first + size;
        return current_mem->first;
    }
}
