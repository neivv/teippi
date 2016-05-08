#ifndef UNITLIST_H
#define UNITLIST_H

#include "types.h"
#include "patch/memory.h"
#include <string.h>

class TempMemoryPool;

#define UNITS_PER_PART 15

// Myös kommentoidut = &dummy

template <class Type, unsigned size>
class UnitListPart
{
    public:
        Type units[size];
        UnitListPart<Type, size> *next;
};

template <class Type, unsigned size>
class UnitListPartIt
{
    public:
        friend class UnitList<Type, size>;

        UnitListPartIt()
        {
        }
        UnitListPartIt(UnitListPart<Type, size> *beg)
        {
            Reset(beg);
        }
        void Reset(UnitListPart<Type, size> *beg)
        {
            part = beg;
            pos = 0;
        }
        Type &operator*() const
        {
            return part->units[pos];
        }
        bool operator!=(const UnitListPartIt<Type, size> &other) const
        {
            return part != other.part || pos != other.pos;
        }
        UnitListPartIt &operator++()
        {
            pos++;
            if (pos == size)
            {
                part = part->next;
                pos = 0;
            }
            return *this;
        }

    private:
        UnitListPart<Type, size> *part;
        uint32_t pos; // Emt oisko uint8_t parempi vai tuo koska kuitenkin padding
};

static_assert(sizeof(UnitListPart<int, 15>) == 0x40, "sizeof(UnitListPart)");

template <class Type, unsigned size>
class UnitList
{
    public:
        UnitList()
        {
            Reset();
        }

        void Reset()
        {
            memset(&beg, 0, sizeof(UnitListPart<Type, size>));
            end.Reset(&beg);
        }

        ~UnitList()
        {
            // Luotetaan ClearAll()
        }

        void Add(Type unit, TempMemoryPool *allocation_pool)
        {
            end.part->units[end.pos++] = unit;
            if (end.pos == size)
            {
                end.part->next = allocation_pool->Allocate<UnitListPart<Type, size>>();
                end.part = end.part->next;
                end.pos = 0;

                memset(end.part, 0, sizeof(UnitListPart<Type, size>));
            }
        }

        UnitListPartIt<Type, size> Begin() { return UnitListPartIt<Type, size>(&beg); }
        const UnitListPartIt<Type, size> &End() { return end; }

    private:
        UnitListPart<Type, size> beg;
        UnitListPartIt<Type, size> end;
};


#endif // UNITLIST_H

