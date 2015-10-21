#ifndef VECTOR_H
#define VECTOR_H

#include <algorithm>
#include <vector>
#include <functional>

#include "assert.h"

namespace Common {

template<class Iter, class Eq>
class unique_class {
    friend class iterator;
    public:
        class iterator {
            public:
                iterator(Iter i, unique_class *p) : pos(i), parent(p) {}
                bool operator==(const iterator &o) const { return pos == o.pos; }
                bool operator!=(const iterator &o) const { return !(*this == o); }
                typename std::iterator_traits<Iter>::reference operator*() { return *pos; }
                iterator &operator++() {
                    const auto &prev = *pos;
                    do {
                        ++pos;
                    } while (*this != parent->end() && parent->eq(*pos, prev));
                    return *this;
                }

            private:
                Iter pos;
                unique_class *parent;
        };
        unique_class(Iter &&b, Iter &&e, Eq &&eq_) :
            begin_iter(std::move(b)), end_iter(std::move(e)), eq(std::move(eq_)) {}
        iterator begin() { return iterator(begin_iter, this); }
        iterator end() { return iterator(end_iter, this); }

    private:
        Iter begin_iter;
        Iter end_iter;
        Eq eq;
};
template<class Iter, class Eq>
inline unique_class<Iter, Eq> iter_unique(Iter &&b, Iter &&e, Eq &&e_) {
    return unique_class<Iter, Eq>(std::move(b), std::move(e), std::move(e_));
}

// std::vector without implicit copies
template <class C>
class vector : public std::vector<C> {
    public:
        vector() {}
        vector(std::initializer_list<C> init) : std::vector<C>(init) { }
        vector(vector &&other) : std::vector<C>(move(other)) {}
        vector(const vector &other) = delete;
        vector &operator=(const vector &other) = delete;
        vector &operator=(vector &&other) = default;

        unique_class<typename std::vector<C>::iterator, typename std::equal_to<C>> Unique() {
            return iter_unique(std::move(this->begin()), std::move(this->end()), std::move(std::equal_to<C>()));
        }
        template<class Eq>
        unique_class<typename std::vector<C>::iterator, Eq> Unique(Eq eq) {
            return iter_unique(std::move(this->begin()), std::move(this->end()), std::move(eq));
        }

        C& operator[](typename std::vector<C>::size_type pos) {
            Assert(this->begin() + pos < this->end());
            return std::vector<C>::operator[](pos);
        }

        const C& operator[](typename std::vector<C>::size_type pos) const {
            Assert(this->begin() + pos < this->end());
            return std::vector<C>::operator[](pos);
        }
};

}
#endif /* VECTOR_H */
