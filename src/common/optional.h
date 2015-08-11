#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <algorithm>

namespace Common {

template <class Type>
class Optional
{
    public:
        Optional(Type &&val) : used(true), value(std::move(val)) {}
        Optional(Optional &&other) { *this = std::move(other); }
        Optional() : used(false) {}
        ~Optional() {
            if (used)
                value.~Type();
        }

        Optional &operator=(Optional &&other) {
            used = other.used;
            if (used)
                value = std::move(other.value);
            return *this;
        }

        Optional &operator=(Type &&val) {
            if (!used) {
                used = true;
                new (&value) Type(move(val));
            } else {
                value = std::move(val);
            }
            return *this;
        }

        bool operator==(const Optional &other) const {
            if (used == false)
                return other.used == false;
            else if (other.used == false)
                return false;
            else
                return take() == other.take();
        }

        explicit operator bool() const { return used; }
        bool operator !() const { return !used; }
        Type &take() { return value; }
        const Type &take() const { return value; }
        bool used;
        union {
            char unused;
            Type value;
        };
};
}

#endif /* OPTIONAL_H */
