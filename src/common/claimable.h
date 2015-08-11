#ifndef CLAIMABLE_H
#define CLAIMABLE_H

#include "assert.h"
template <class C> class Claimable;

template <class C>
class Claimed
{
    friend class Claimable<C>;
    public:
        Claimed(Claimable<C> *val) : value(val) {}
        ~Claimed() { if (value) Drop(); }
        Claimed(const Claimed &other) = delete;
        Claimed(Claimed &&other)
        {
            value = other.value;
            other.value = nullptr;
        }

        C *operator ->() { return &value->value; }
        C& Inner() { return value->value; }

    private:
        void Drop()
        {
            value->Return(this);
            value = nullptr;
        }

        Claimable<C> *value;
};

template <class C>
class Claimable
{
    friend class Claimed<C>;
    public:
        Claimable() { SetClaimed(false); }
        Claimed<C> Claim()
        {
            Assert(!IsClaimed());
            SetClaimed(true);
            return Claimed<C>(this);
        }

    private:
        void Return(Claimed<C> *value)
        {
            Assert(value->value == this);
            SetClaimed(false);
        }

        C value;
#ifdef DEBUG
        bool claimed;
        bool IsClaimed() const { return claimed; }
        void SetClaimed(bool val) { claimed = val; }
#else
        bool IsClaimed() const { return false; }
        void SetClaimed(bool val) {}
#endif
};
#endif /* CLAIMABLE_H */
