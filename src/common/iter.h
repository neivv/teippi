#ifndef ITER_H
#define ITER_H
#include <algorithm>
#include "optional.h"

namespace Common
{

template <class Self, class Item>
class Iterator
{
    public:
        class internal_iterator
        {
            public:
                internal_iterator(Self *p) : parent(p), value(parent->next()) {}
                internal_iterator() {}

                bool operator==(const internal_iterator &other) const { return !value.used && !other.value.used; }
                bool operator!=(const internal_iterator &other) const { return !(*this == other); }
                internal_iterator &operator++() { value = parent->next(); return *this; }
                const Item &operator*() { return value.value; }

            private:
                Self *parent;
                Optional<Item> value;
        };
        internal_iterator begin() { return internal_iterator(static_cast<Self *>(this)); }
        internal_iterator end() { return internal_iterator(); }
};

template <class Iter>
bool Empty(Iter &&it)
{
    return it.begin() == it.end();
}
}

#endif /* ITER_H */
