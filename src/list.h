#ifndef LIST_H
#define LIST_H

#include <stdint.h>
#include <cstddef>
#include "console/assert.h"

template <typename T, typename M> M get_member_type(M T::*);
template <typename T, typename M> T get_class_type(M T::*);

template <class C, unsigned offset, bool reverse>
class ListIterator
{
    public:
        ListIterator(C *val) { value = val; }

        void operator++() { value = *(C **)((uint8_t *)value + offset + (reverse * sizeof(void *))); }
        bool operator!=(const ListIterator &other) const { return other.value != value; }
        C *operator*() const { return value; }
        ListIterator prev() const
        {
            auto val = *(C **)((uint8_t *)value + offset + (!reverse * sizeof(void *)));
            return ListIterator(val);
        }

    private:
        C *value;
};

template <class C, unsigned offset>
class ListHead
{
    public:
        ListHead() { value = 0; }
        operator C*&() { return value; }
        C *& AsRawPointer() { return value; }
        ListHead &operator=(C * const val) { value = val;  return *this; }
        C *&operator*() { return value; }
        C *operator-> () const { return value; }

        ListIterator<C, offset, false> begin() const
        {
            return ListIterator<C, offset, false>(value);
        }
        ListIterator<C, offset, false> end() const
        {
            return ListIterator<C, offset, false>(0);
        }

    private:
        C *value;
};

template <class C, unsigned offset>
class ListEntry
{
    public:
        ListEntry()
        {
        }
        void Remove(ListHead<C, offset> &first)
        {
            if (next)
                ((ListEntry *)((uint8_t *)next + offset))->prev = prev;
            if (prev)
                ((ListEntry *)((uint8_t *)prev + offset))->next = next;
            else
            {
                Assert(first == self());
                first = next;
            }
        }

        void Add(ListHead<C, offset> &first)
        {
            next = first;
            prev = 0;
            first = self();
            if (next)
                ((ListEntry *)((uint8_t *)next + offset))->prev = self();
        }

        void Change(ListHead<C, offset> &remove_list, ListHead<C, offset> &add_list)
        {
            Remove(remove_list);
            Add(add_list);
        }

        C *next;
        C *prev;

    private:
        C *self() const
        {
            return (C *)((uint8_t *)this - offset);
        }
};

template <class C, unsigned offset>
class RevListHead
{
    public:
        RevListHead() { value = 0; }
        operator C*&() { return value; }
        C *& AsRawPointer() { return value; }
        C *operator-> () const { return value; }
        RevListHead &operator=(C * const val) { value = val;  return *this; }

        ListIterator<C, offset, true> begin() const
        {
            return ListIterator<C, offset, true>(value);
        }
        ListIterator<C, offset, true> end() const
        {
            return ListIterator<C, offset, true>(0);
        }

    private:
        C *value;
};

template <class C, unsigned offset>
class RevListEntry
{
    public:
        RevListEntry()
        {
        }
        void Remove(RevListHead<C, offset> &first)
        {
            if (next)
                ((RevListEntry *)((uint8_t *)next + offset))->prev = prev;
            if (prev)
                ((RevListEntry *)((uint8_t *)prev + offset))->next = next;
            else
            {
                Assert(first == self());
                first = next;
            }
        }

        void Add(RevListHead<C, offset> &first)
        {
            next = first;
            prev = 0;
            first = self();
            if (next)
                ((RevListEntry *)((uint8_t *)next + offset))->prev = self();
        }

        void Change(RevListHead<C, offset> &remove_list, RevListHead<C, offset> &add_list)
        {
            Remove(remove_list);
            Add(add_list);
        }

        C *prev;
        C *next;

    private:
        C *self() const
        {
            return (C *)((uint8_t *)this - offset);
        }
};

template <class C, unsigned offset> class DummyListEntry;

template <class C, unsigned offset>
class DummyListIt
{
    public:
        DummyListIt(C *val_)
        {
            val = val_;
        }
        C *operator*() const
        {
            return val;
        }
        void operator++()
        {
            val = ((DummyListEntry<C, offset> *)((uint8_t *)val + offset))->next;
        }
        bool operator!=(const DummyListIt &other) const
        {
            return val != other.val;
        }

    C *val;
};


template <class C, unsigned offset>
class DummyListHead
{
    friend class DummyListEntry<C, offset>;
    public:
        DummyListHead()
        {
            Reset();
        }
        void Reset()
        {
            C *base = (C *)((uint8_t *)&dummy - offset);
            dummy.next = base;
            dummy.prev = base;
        }

        DummyListIt<C, offset> begin() const
        {
            return dummy.next;
        }
        DummyListIt<C, offset> end() const
        {
            return (C *)((uint8_t *)&dummy - offset);
        }

        void MergeTo(DummyListHead<C, offset> &other, bool at_end = true)
        {
            C *base = (C *)((uint8_t *)&dummy - offset);
            if (dummy.next == base)
                return;
            if (at_end)
            {
                ToEntry(dummy.prev)->next = other.dummy.self();
                ToEntry(dummy.next)->prev = other.dummy.prev;
                ToEntry(other.dummy.prev)->next = dummy.next;
                other.dummy.prev = dummy.prev;
            }
            else
            {
                ToEntry(dummy.next)->prev = other.dummy.self();
                ToEntry(dummy.prev)->next = other.dummy.next;
                ToEntry(other.dummy.next)->prev = dummy.prev;
                other.dummy.next = dummy.next;
            }
            Reset();
        }

    private:
        static inline DummyListEntry<C, offset> *ToEntry(C *base)
        {
            return DummyListEntry<C, offset>::ToEntry(base);
        }
        DummyListEntry<C, offset> dummy;
};

template <class C, unsigned offset>
class DummyListEntry
{
    public:
        DummyListEntry()
        {
        }
        void Remove()
        {
            ToEntry(next)->prev = prev;
            ToEntry(prev)->next = next;
        }

        C *self() const
        {
            return (C *)((uint8_t *)this - offset);
        }

        void Add(DummyListHead<C, offset> &new_list)
        {
            next = new_list.dummy.self();
            prev = new_list.dummy.prev;
            ToEntry(next)->prev = self();
            ToEntry(prev)->next = self();
        }

        void Change(DummyListHead<C, offset> &new_list)
        {
            Remove();
            Add(new_list);
        }
        static inline DummyListEntry<C, offset> *ToEntry(C *base)
        {
            return ((DummyListEntry *)((uint8_t *)base + offset));
        }

        C *next;
        C *prev;
};

static_assert(sizeof(ListEntry<int, 0>) == 8, "sizeof(ListEntry)");
static_assert(sizeof(ListHead<int, 0>) == 4, "sizeof(ListHead)");
static_assert(sizeof(RevListEntry<int, 0>) == 8, "sizeof(RevListEntry)");
static_assert(sizeof(RevListHead<int, 0>) == 4, "sizeof(RevListHead)");
static_assert(sizeof(DummyListEntry<int, 0>) == 8, "sizeof(DummyListEntry)");
static_assert(sizeof(DummyListHead<int, 0>) == 8, "sizeof(DummyListHead)");

#endif // LIST_H

