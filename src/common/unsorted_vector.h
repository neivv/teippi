#ifndef UNSORTED_VECTOR_H
#define UNSORTED_VECTOR_H

#include <vector>
#include <memory>

// Atm only for unique_ptr
template <class C>
class UnsortedPtrVector : public std::vector<std::unique_ptr<C>>
{
    public:
        class safe_iterator
        {
            typedef typename std::vector<std::unique_ptr<C>>::iterator base_iterator;
            public:
                safe_iterator(UnsortedPtrVector *p, intptr_t i) : parent(p), index(i), previous(nullptr)
                {
                    if (index >= parent->size())
                        index = -1;
                    else
                        previous = (*parent)[index].get();
                }

                bool operator==(const safe_iterator &o) const
                {
                    Assert(o.parent == parent);
                    if (index == -1 || index >= parent->size())
                        return o.index == -1;
                    return index == o.index;
                }
                bool operator!=(const safe_iterator &other) const { return !(*this == other); }
                bool operator<(const base_iterator &o) const
                {
                    if (index == -1)
                        return false;
                    if (parent->end() == o)
                        return true;
                    return static_cast<vector<std::unique_ptr<C>> *>(parent)->begin() + index < o;
                }
                bool operator>=(const base_iterator &other) const { return !(*this < other); }
                bool operator>(const base_iterator &o) const
                {
                    if (parent->end() == o)
                        return false;
                    if (index == -1)
                        return true;
                    return static_cast<vector<std::unique_ptr<C>> *>(parent)->begin() + index > o;
                }
                bool operator<=(const base_iterator &other) const { return !(*this < other); }
                safe_iterator operator++()
                {
                    Assert(index != -1);
                    if (index >= parent->size())
                    {
                        index = -1;
                        previous = nullptr;
                    }
                    else
                    {
                        if (previous == nullptr || (*parent)[index].get() == previous)
                            index++;
                        if (index == parent->size())
                            index = -1;
                        else
                            previous = (*parent)[index].get();
                    }
                    return *this;
                }
                std::unique_ptr<C> &operator*() { return (*parent)[index]; }

            private:
                UnsortedPtrVector *parent;
                intptr_t index;
                C *previous;
        };
        class SafeIteratorMaster
        {
            public:
                SafeIteratorMaster(UnsortedPtrVector *p) : parent(p) {}
                safe_iterator begin() { return safe_iterator(parent, 0); }
                safe_iterator end() { return safe_iterator(parent, -1); }

            private:
                UnsortedPtrVector *parent;
        };

        std::unique_ptr<C> erase_at(uintptr_t entry)
        {
            std::swap(this->back(), (*this)[entry]);
            auto erased = move(this->back());
            this->pop_back();
            return erased;
        }

        // Allows deleting during iteration without skipping over anything or crashing
        safe_iterator safe_begin() { return safe_iterator(this, 0); }
        safe_iterator safe_end() { return safe_iterator(this, -1); }
        SafeIteratorMaster SafeIter() { return SafeIteratorMaster(this); }
};

#endif /* UNSORTED_VECTOR_H */
