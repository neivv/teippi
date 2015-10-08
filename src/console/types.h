#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <memory>
#include <algorithm>
#include <functional>

#include "../common/vector.h"

#ifdef __GNUC__
#define INT3() asm("int3")
#else
#define INT3() __asm { int 3 }
#endif

namespace Common {
template <typename C>
using ptr = std::unique_ptr<C>;

template <typename C>
using reference = std::reference_wrapper<C>;

#pragma pack(push)
#pragma pack(1)

class PatchContext;
class Point32;

template <typename C> class yint;

#define IncompatibleClass(Self, Class) \
    template <typename D> constexpr Self(const Class<D> &other) = delete; \
    template <typename D> operator Class<D>() const = delete; \
    template <typename D> bool operator==(const Class<D> &other) const = delete; \
    template <typename D> bool operator!=(const Class<D> &other) const = delete; \
    template <typename D> bool operator<(const Class<D> &other) const = delete; \
    template <typename D> bool operator>(const Class<D> &other) const = delete; \
    template <typename D> bool operator<=(const Class<D> &other) const = delete; \
    template <typename D> bool operator>=(const Class<D> &other) const = delete;

template <typename C>
class xint
{
    template <typename D> friend class xint;
    public:
        constexpr xint<C>() = default;
        constexpr xint<C>(C val_) : val(val_) {}
        template <typename D> constexpr xint<C>(const xint<D> &val_) : val(val_.val) {}
        template <typename D> constexpr xint<C>(xint<D> &&val_) : val(val_.val) {}
        operator C&() { return val; }
        constexpr operator C() const { return val; }

        template <typename D> constexpr bool operator==(const xint<D> other) const { return val == other.val; }
        template <typename D> constexpr bool operator!=(const xint<D> other) const { return !(*this == other); }
        template <typename D> constexpr bool operator<(const xint<D> other) const { return val < other.val; }
        template <typename D> constexpr bool operator>(const xint<D> other) const { return val > other.val; }
        template <typename D> constexpr bool operator<=(const xint<D> other) const { return !(val > other.val); }
        template <typename D> constexpr bool operator>=(const xint<D> other) const { return !(val < other.val); }
        IncompatibleClass(xint<C>, yint);

        template <typename D> xint<C> operator=(const xint<D> other) { val = other.val; return *this; }
        template <typename D> xint<C> operator=(xint<D> &&other) { val = other.val; return *this; }

    private:
        C val;
};

template <typename C>
class yint
{
    template <typename D> friend class yint;
    public:
        constexpr yint<C>() = default;
        constexpr yint<C>(C val_) : val(val_) {}
        template <typename D> constexpr yint<C>(const yint<D> &val_) : val(val_.val) {}
        template <typename D> constexpr yint<C>(yint<D> &&val_) : val(val_.val) {}
        operator C&() { return val; }
        constexpr operator C() const { return val; }

        template <typename D> constexpr bool operator==(const yint<D> other) const { return val == other.val; }
        template <typename D> constexpr bool operator!=(const yint<D> other) const { return !(*this == other); }
        template <typename D> constexpr bool operator<(const yint<D> other) const { return val < other.val; }
        template <typename D> constexpr bool operator>(const yint<D> other) const { return val > other.val; }
        template <typename D> constexpr bool operator<=(const yint<D> other) const { return !(val > other.val); }
        template <typename D> constexpr bool operator>=(const yint<D> other) const { return !(val < other.val); }
        IncompatibleClass(yint<C>, xint);

        template <typename D> yint<C> operator=(const yint<D> other) { val = other.val; return *this; }
        template <typename D> yint<C> operator=(yint<D> &&other) { val = other.val; return *this; }

    private:
        C val;
};

#undef IncompatibleClass

using x16u = xint<uint16_t>;
using x16s = xint<int16_t>;
using x32 = xint<int32_t>;
using y16u = yint<uint16_t>;
using y16s = yint<int16_t>;
using y32 = yint<int32_t>;

class Buf
{
    public:
        Buf(ptr<uint8_t[]> d, int l)
        {
            data = std::move(d);
            len = l;
        }
        Buf(void *d, int l) : data((uint8_t *)d)
        {
            len = l;
        }

        const char * AsCStr() { return (const char *)data.get(); }
        ptr<uint8_t[]> data;
        int len;
};

// This is simply a nicer alternative to std::pair<data, len>, it will not do any raii etc.
// Thus it should only be used as readonly function parameter
template <class C>
class Array
{
    public:
        class Iterator : public std::iterator<std::random_access_iterator_tag, C>
        {
            public:
                Iterator(C *c) : pos(c) {}
                Iterator &operator++() { pos++; return *this; }
                bool operator==(const Iterator &other) const { return other.pos == pos; }
                bool operator!=(const Iterator &other) const { return !(*this == other); }
                std::intptr_t operator-(const Iterator &other) const { return pos - other.pos; }
                Iterator &operator+=(std::intptr_t amt) { pos += amt; return *this; }
                bool operator<(const Iterator &other) const { return pos < other.pos; }
                C &operator*() { return *pos; }
                C *ptr() { return pos; }

            private:
                C *pos;
        };

        Array(C *c, int len_) : data(c), len(len_) {}
        Array(C *c, C *end) : data(c), len(end - c) {}
        void SetBeg(C *beg) { len = data + len - beg; data = beg; }
        void SetBeg(Iterator beg) { len = data + len - beg.ptr(); data = beg.ptr(); }

        Iterator begin() { return Iterator(data); }
        Iterator end() { return Iterator(data + len); }

        C *data;
        C &operator[](unsigned int pos) { return data[pos]; }
        uintptr_t len;
};

/// Vector should be used unless there's a need to push items without constant bounds checking
template <class C>
class OwnedArray
{
    public:
        class Iterator : public std::iterator<std::random_access_iterator_tag, C>
        {
            public:
                Iterator(C *c) : pos(c) {}
                Iterator &operator++() { pos++; return *this; }
                bool operator==(const Iterator &other) const { return other.pos == pos; }
                bool operator!=(const Iterator &other) const { return !(*this == other); }
                std::intptr_t operator-(const Iterator &other) const { return pos - other.pos; }
                Iterator &operator+=(std::intptr_t amt) { pos += amt; return *this; }
                bool operator<(const Iterator &other) const { return pos < other.pos; }
                bool operator>=(const Iterator &other) const { return !(pos < other.pos); }
                C &operator*() { return *pos; }
                C *ptr() { return pos; }

            private:
                C *pos;
        };

        OwnedArray() : len(0) {}
        OwnedArray(C *c, int len_) : data(c), len(len_) {}
        OwnedArray(C *c, C *end) : data(c), len(end - c) {}

        Iterator begin() { return Iterator(data); }
        Iterator end() { return Iterator(data.get() + len); }

        void clear()
        {
            data.reset(nullptr);
            len = 0;
        }
        void resize(uintptr_t entries)
        {
            if (len < entries)
            {
                data.reset(new C[entries]);
                len = entries;
            }
        }

        ptr<C[]> data;
        C &operator[](unsigned int pos) { return data[pos]; }
        uintptr_t len;
};

class Point16
{
    public:
        constexpr Point16() : x(0xffff), y(0xffff) {}
        constexpr Point16(x16u x_, y16u y_) : x(x_), y(y_) {}

        Point16 &operator=(const Point16 &other) { x = other.x; y = other.y; return *this; }
        constexpr bool operator== (const Point16 &other) const { return other.x == x && other.y == y; }
        constexpr bool operator!= (const Point16 &other) const { return !(*this == other); }

        constexpr Point16 operator+(const Point16 &other) const { return Point16(x + other.x, y + other.y); }
        Point16 &operator+=(const Point16 &other) { x += other.x; y += other.y; return *this; }

        constexpr Point16 operator-(const Point16 &other) const { return Point16(x - other.x, y - other.y); }

        constexpr uint32_t AsDword() const { return y << 16 | x; }

        constexpr bool IsValid() const { return x != 0xffff || y != 0xffff; }
        x16u x;
        y16u y;
};

class Point32
{
    public:
        Point32() {}
        constexpr Point32(const Point16 &other) : x(other.x), y(other.y) {}
        constexpr Point32(x32 x_, y32 y_) : x(x_), y(y_) {}
        Point32 &operator=(const Point32 &other) { x = other.x; y = other.y; return *this; }
        constexpr bool operator== (const Point32 &other) const { return other.x == x && other.y == y; }
        constexpr bool operator!= (const Point32 &other) const { return !(*this == other); }

        Point32 operator+(const Point32 &other) const { return Point32(x + other.x, y + other.y); }
        Point32 &operator+=(const Point32 &other) { x += other.x; y += other.y; return *this; }
        Point32 operator-(const Point32 &other) const { return Point32(x - other.x, y - other.y); }
        Point32 operator*(int mult) const { return Point32(x * mult, y * mult); }

        Point32 Negate() const { return Point32(0, 0) - *this; }
        Point16 ToPoint16() const { return Point16(x, y); }

        x32 x;
        y32 y;
};

inline Point32 operator+(const Point16 &a, const Point32 &b) { return Point32(a) + b; }
inline Point32 operator-(const Point16 &a, const Point32 &b) { return Point32(a) - b; }

typedef Point32 Point;

template <typename C>
class Rect
{
    public:
        Rect() {}
        Rect(xint<C> l, yint<C> t, xint<C> r, yint<C> b) : left(l), top(t), right(r), bottom(b) {}
        Rect(const Point32 &pos, int radius) : left(pos.x - radius), top(pos.y - radius), right(pos.x + radius),
            bottom(pos.y + radius) {}
        xint<C> left;
        yint<C> top;
        xint<C> right;
        yint<C> bottom;
        bool IsValid() { return left != right && top != bottom; }

        Rect OffsetBy(const Point32 &pos)
        {
            Rect copy = *this;
            copy.left += pos.x;
            copy.right += pos.x;
            copy.top += pos.y;
            copy.bottom += pos.y;
            return copy;
        }
};

using Rect32 = Rect<int32_t>;

class Rect16
{
    public:
        Rect16() {}
        Rect16(x16u l, y16u t, x16u r, y16u b) : left(l), top(t), right(r), bottom(b) {}
        Rect16(std::nullptr_t t) { left = right = 0; }
        template <class C>
        Rect16(const Rect<C> &other) : left(other.left), top(other.top), right(other.right), bottom(other.bottom) {}
        // First one is width == height == radius * 2
        // Second one is width == height == radius * 2 - 1
        // First is what bw uses, so might as well
        Rect16(const Point16 &pos, int rad)
            : left(std::max(pos.x - rad, 0)), top(std::max(pos.y - rad, 0)), right(pos.x + rad), bottom(pos.y + rad) {}
        /*
        Rect16(const Point16 &pos, int rad)
        {
            if (rad == 0)
            {
                left = right = pos.x;
                top = bottom = pos.y;
            }
            else
            {
                left = std::max(pos.x - rad + 1, 0);
                top = std::max(pos.y - rad + 1, 0);
                right = pos.x + rad;
                bottom = pos.y + rad;
            }
        }
        */
        // This is mix between the two radius constructors, uses the first one when w/h is even, and second one when w/h is odd
        Rect16(const Point16 &pos, int width, int height)
        {
            left = std::max(pos.x - width / 2 + (width & 1), 0);
            right = pos.x + width / 2;
            top = std::max(pos.y - height / 2 + (height & 1), 0);
            bottom = pos.y + height / 2;
        }

        Rect16 Clipped(const Rect16 &other) const {
            return Rect16(std::max(left, other.left), std::max(top, other.top),
                    std::min(right, other.right), std::min(bottom, other.bottom));
        }

        x32 Width() const { return right - left; }
        y32 Height() const { return bottom - top; }

        bool IsValid() { return left != right && top != bottom; }

        x16u left;
        y16u top;
        x16u right;
        y16u bottom;
};

#pragma pack(pop)
}


#endif // COMMON_TYPES_H
