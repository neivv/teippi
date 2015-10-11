#ifndef MEMORY_HOO
#define MEMORY_HOO
#include <stdint.h>
#include <vector>
#include <stddef.h>

struct _SYSTEM_INFO;
extern _SYSTEM_INFO sysinfo;

void InitSystemInfo();
void DumpMemory(const char *filename, uintptr_t start, uintptr_t len);

class PageGroup
{
    public:
        PageGroup() {}
        PageGroup(void *f, uintptr_t l, uint32_t p) : first_page(f), length(l), protection(p) {}
        void *first_page;
        uintptr_t length;
        uint32_t protection;
};

class UnprotectModule
{
    public:
        UnprotectModule() {}
        UnprotectModule(const UnprotectModule &other) = delete;
        UnprotectModule(UnprotectModule &&other) { *this = std::move(other); }
        UnprotectModule &operator=(UnprotectModule &&other);

        void *operator new(size_t size) = delete;
        void *operator new[](size_t size) = delete;
        void Init(void *module);

        UnprotectModule(void *module) { Init(module); }
        ~UnprotectModule();

    private:
        std::vector<PageGroup> protections;
};

// Todo: Isompi mem chunk size tj? ehkä tulee liikaa hukka-osia
class TempMemoryPool
{
    public:
        TempMemoryPool(uint32_t mem_chunk_sz = 0x200000);
        ~TempMemoryPool();

        void AddMemArea(uint32_t mem_chunk_size);
        void ClearAll();
        void SetPos(void *pos_) { pos = (uint8_t *)pos_; } // Käytä varoen

        template <typename T>
        T *Allocate(uint32_t count = 1)
        {
            return new(AllocateBase(count * sizeof(T))) T;
        }

    private:
        uint8_t *AllocateBase(uint32_t size);

        std::vector<std::pair<uint8_t *, uint8_t *>> memory;
        std::vector<std::pair<uint8_t *, uint8_t *>>::iterator current_mem;
        uint8_t *pos;
        uint32_t mem_chunk_default_size;

};

#endif // MEMORY_HOO

