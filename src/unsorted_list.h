#ifndef UNSORTED_LIST_H
#define UNSORTED_LIST_H

#include "types.h"

/// A simple linked list of fixed-size arrays. Only way to
/// remove entries in middle is to do it while iterating through
/// the list, which swaps the last entry in place. As such,
/// it should not be used for storing data needing sorting,
/// and having multiple separate iterations at once or just
/// storing iterators is not allowed, as removing will
/// invalidate all iterators. Does not provide random access
/// like a deque does, as it would add (very minor, but currently
/// unneeded) performance cost to operations.
///
/// N specifies the size of a single array of objects.
///
/// The interface is lazily done, implementing just the bare
/// minimum needed by code. Currently used to contain game
/// objects in LoneSpriteSystem and BulletSystem.

template <class T, uintptr_t N>
struct UnsortedList_Chunk {
    T values[N];
    UnsortedList_Chunk *next;
    UnsortedList_Chunk *prev;
};

template <class T, uintptr_t N = 32, class Allocator = std::allocator<UnsortedList_Chunk<T, N>>>
class UnsortedList : protected Allocator
{
    typedef UnsortedList_Chunk<T, N> Chunk;
    friend class entry;
    public:

        class entry_iterator;
        class entry_it_recompare;
        class _Entries;
        /// Basic iterator, does not contain way to refer the parent,
        /// and lacks the functions to manipulate it.
        template <class ChunkPtr_Mutability>
        class base_iterator {
            friend class UnsortedList;
            friend class entry;
            public:
                base_iterator(ChunkPtr_Mutability c) : chunk(c), pos(0) { }
                base_iterator &operator++() {
                    pos++;
                    if (pos == N && chunk->next != nullptr) {
                        pos = 0;
                        chunk = chunk->next;
                    }
                    return *this;
                }

                base_iterator &operator--() {
                    if (pos == 0) {
                        pos = N;
                        chunk = chunk->prev;
                    }
                    pos--;
                    return *this;
                }

                bool operator==(const base_iterator &o) const {
                    return chunk == o.chunk && pos == o.pos;
                }

                bool operator!=(const base_iterator &o) const { return !(*this == o); }

            protected:
                base_iterator(ChunkPtr_Mutability c, uintptr_t p) : chunk(c), pos(p) { }
                ChunkPtr_Mutability chunk;
                uintptr_t pos;
        };

        class iterator : public base_iterator<Chunk *> {
            public:
                iterator(Chunk *c) : base_iterator<Chunk *>(c) { }
                T& operator*() { return this->chunk->values[this->pos]; }
                T* operator->() { return &this->chunk->values[this->pos]; }
                operator T*() { return &this->chunk->values[this->pos]; }
        };

        class const_iterator : public base_iterator<const Chunk *> {
            public:
                const_iterator(const Chunk *c) : base_iterator<const Chunk *>(c) { }
                const_iterator(const iterator &i) : base_iterator<const Chunk *>(i.chunk, i.pos) { }
                const T& operator*() { return this->chunk->values[this->pos]; }
                const T* operator->() { return &this->chunk->values[this->pos]; }
                operator const T*() { return &this->chunk->values[this->pos]; }
        };

        class entry {
            friend class entry_iterator;
            public:
                T& operator*() { return iter->chunk->values[iter->pos]; }
                T* operator->() { return &iter->chunk->values[iter->pos]; }
                operator T*() { return &iter->chunk->values[iter->pos]; }

                /// Destroys the object pointed by iterator and swaps the last object
                /// to take its place. The iterator is left in a state, where operator++
                /// needs to be called before using it in any way. After that, it
                /// will dereference to the object which was swapped from the end.
                /// (So range-for loops work nicely)
                void swap_erase() {
                    iter->chunk->values[iter->pos] = std::move(iter->parent->back());
                    iter->parent->pop();
                    iter->erased = true;
                }

                /// Moves entry to another UnsortedList
                /// Yes, this makes move constructors required
                void move_to(UnsortedList *dest) {
                    dest->emplace(std::move(iter->chunk->values[iter->pos]));
                    swap_erase();
                }

            private:
                entry(entry_iterator *i) : iter(i) {}
                entry_iterator *iter;
        };

        /// Created from Entries().begin()/end() (See comment in entry_it_recompare)
        class entry_iterator : public iterator
        {
            friend class entry;
            friend class entry_it_recompare;
            public:
                entry_iterator(UnsortedList *p, Chunk *c) : iterator(c), parent(p), erased(false) { }
                entry_iterator(UnsortedList *p, const iterator &base) : iterator(base),
                    parent(p), erased(false) { }

                entry_iterator &operator++() {
                    if (erased)
                        erased = false;
                    else
                        iterator::operator++();
                    return *this;
                }

                entry_iterator &operator--() {
                    if (erased)
                        erased = false;
                    iterator::operator--();
                    return *this;
                }

                entry operator*() { return entry(this); }

            private:
                UnsortedList *parent;
                bool erased;
        };

        /// C++ range-for does not generally work with end changing its
        /// value during iteration, so a hack is needed to have operator==
        /// always recheck parent->end() if other.end == true. Will most
        /// likely cause really weird behavior if used outside range-for,
        /// in which case the plain entry_iterator should be used.
        class entry_it_recompare {
            friend class _Entries;
            public:
                bool operator==(const entry_it_recompare &o) const {
                    // Should only be compared against the entry with nullptr pointers
                    Assert(o.base.parent == nullptr);
                    return base == base.parent->end();
                }
                bool operator!=(const entry_it_recompare &o) const { return !(*this == o); }

                entry_it_recompare& operator++() { ++base; return *this; }
                entry operator*() { return *base; }

            private:
                entry_it_recompare(UnsortedList *p, Chunk *c) : base(p, c) { }
                entry_it_recompare() : base(nullptr, nullptr) { }
                entry_iterator base;
        };

        UnsortedList() : head(nullptr), tail(head), size_(0) { }
        ~UnsortedList() {
            clear();
        }

        // Too lazy to implement something that is not used
        UnsortedList(const UnsortedList &other) = delete;
        UnsortedList& operator=(const UnsortedList &other) = delete;

        iterator begin() { return iterator(head); }
        iterator end() { return iterator(tail); }
        const_iterator begin() const { return const_iterator(head); }
        const_iterator end() const { return const_iterator(tail); }
        entry_iterator entries_begin() { return entry_iterator(this, head); }
        entry_iterator entries_end() { return entry_iterator(this, tail); }
        T &back() {
            auto it = end();
            --it;
            return *it;
        }

        class _Entries {
            public:
                _Entries(UnsortedList *s) : self(s) { }

                entry_it_recompare begin() { return entry_it_recompare(self, self->head); }
                entry_it_recompare end() { return entry_it_recompare(); }

            private:
                UnsortedList *self;
        };

        /// Returns container whose iterators work otherwise same, but they allow
        /// erasing/moving the entry. This is the main way of removing objects from
        /// the container, deleting them as the ones matching deletion criteria
        /// come up during the iteration. Only use with range-for.
        _Entries Entries() { return _Entries(this); }
        friend class _Entries;

        void clear() {
            if (head == nullptr)
                return;

            Chunk *chunk = head;
            while (chunk != tail.chunk) {
                for (auto i = 0; i < N; i++)
                    chunk->values[i].~T();

                Chunk *next = chunk->next;
                this->deallocate(chunk, 1);
                chunk = next;
            }
            for (auto i = 0; i < tail.pos; i++)
                tail.chunk->values[i].~T();
            chunk = tail.chunk->next;
            this->deallocate(tail.chunk, 1);
            while (chunk != nullptr) {
                Chunk *next = chunk->next;
                this->deallocate(chunk, 1);
                chunk = next;
            }

            size_ = 0;
            head = nullptr;
            tail = iterator(head);
        }

        void clear_keep_capacity() {
            if (head == nullptr)
                return;

            Chunk *chunk = head;
            while (chunk != tail.chunk) {
                for (auto i = 0; i < N; i++)
                    chunk->values[i].~T();

                Chunk *next = chunk->next;
                chunk = next;
            }
            for (auto i = 0; i < tail.pos; i++)
                tail.chunk->values[i].~T();

            size_ = 0;
            tail = iterator(head);
        }

        template<class... Args>
        void emplace(Args &&... args) {
            if (head == nullptr) {
                head = this->allocate(1);
                head->next = nullptr;
                head->prev = nullptr;
                tail.chunk = head;
                tail.pos = 0;
            }
            else if (tail.pos == N) {
                // Atm tail.pos == N only if there is no next chunk,
                // otherwise it would always roll over to 0 in next chunk.
                Assert(tail.chunk->next == nullptr);
                Chunk *next = this->allocate(1);
                next->next = nullptr;
                next->prev = tail.chunk;
                tail.chunk->next = next;
                tail.chunk = tail.chunk->next;
                tail.pos = 0;
            }
            new (&tail.chunk->values[tail.pos]) T (std::forward<Args>(args)...);
            size_ += 1;
            ++tail;
        }

        void pop() {
            // Keep one empty chunk
            if (tail.pos == 0 && tail.chunk->next != nullptr) {
                this->deallocate(tail.chunk->next, 1);
                tail.chunk->next = nullptr;
            }
            back().~T();
            --tail;
            size_ -= 1;
        }

        uintptr_t size() const { return size_; }


    private:
        Chunk *head;
        iterator tail;
        uintptr_t size_;
};

#endif /* UNSORTED_LIST_H */
