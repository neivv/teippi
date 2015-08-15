#ifndef POSSEARCH_HPP
#define POSSEARCH_HPP 

#include "unitsearch.h"
#include "yms.h"

#include <algorithm>

template <class C>
void PosSearch<C>::Clear()
{
    left_to_value.clear();
    left_to_right.clear();
    left_to_top.clear();
    left_to_bottom.clear();
    left_positions.clear();

    left_positions.push_back(INT_MAX - 1); // Dirty optimization: Having extra entry that is always last possible allows faster searching
}

template <class C>
int PosSearch<C>::NewFind(x32 left_pos)
{
    // There's the last entry optimization - it would not be bad to include it but might save a single comparision
    return std::lower_bound(left_positions.begin(), left_positions.end() - 1, left_pos) - left_positions.begin();
}

template <class C>
void PosSearch<C>::RemoveAt(uintptr_t pos)
{
    left_positions.erase(left_positions.begin() + pos);
    left_to_right.erase(left_to_right.begin() + pos);
    left_to_top.erase(left_to_top.begin() + pos);
    left_to_bottom.erase(left_to_bottom.begin() + pos);
    left_to_value.erase(left_to_value.begin() + pos);
}


template <class C>
void PosSearch<C>::Add(uintptr_t pos, C &&val, const Rect16 &box)
{
    left_positions.emplace(left_positions.begin() + pos, box.left);
    left_to_right.emplace(left_to_right.begin() + pos, box.right);
    left_to_value.emplace(left_to_value.begin() + pos, move(val));
    left_to_top.emplace(left_to_top.begin() + pos, box.top);
    left_to_bottom.emplace(left_to_bottom.begin() + pos, box.bottom);
}

// return array is sorted by left
template <class C>
void PosSearch<C>::Find(const Rect16 &rect, C *out, C **out_end)
{
    Assert(rect.left <= rect.right && rect.top <= rect.bottom);
    unsigned int beg, end;
    int find_right = rect.right, find_left = rect.left;
    find_left = rect.left - max_width;
    beg = NewFind(find_left);
    end = NewFind(find_right);

    for (unsigned int it = beg; it < end; it++)
    {
        if (left_to_right[it] > rect.left)
        {
            if (rect.top < left_to_bottom[it] && rect.bottom > left_to_top[it])
                *out++ = left_to_value[it];
        }
    }

    *out_end = out;
}

// Not too much optimized
template <class C>
template <class F, class F2>
C *PosSearch<C>::FindNearest(const Point &pos, const Rect16 &area, F IsValid, F2 Position)
{
    int right_pos = NewFind(pos.x);
    int left_pos = right_pos - 1;
    int max_dist = INT_MAX;
    C *closest = nullptr;
    bool cont = true;
    while (cont)
    {
        cont = false;
        if (left_pos >= 0)
        {
            int left = left_positions[left_pos];
            int widest_right = left + max_width;
            if (widest_right < area.left || pos.x - widest_right > max_dist)
                left_pos = -1;
            else
            {
                cont = true;
                C *value = &left_to_value[left_pos];
                Point val_pos = Position(*value);
                if (val_pos.y < area.bottom && val_pos.y >= area.top && val_pos.x >= area.left)
                {
                    if (IsValid(*value))
                    {
                        int dist = Distance(pos, val_pos);
                        if (dist < max_dist)
                        {
                            closest = value;
                            max_dist = dist;
                        }
                    }
                }
                left_pos--;
            }
        }
        if (right_pos < (int)Size())
        {
            int right = left_to_right[right_pos];
            int widest_left = right - max_width;
            if (widest_left > area.right || widest_left - pos.x > max_dist)
                right_pos = Size();
            else
            {
                cont = true;
                C *value = &left_to_value[right_pos];
                Point val_pos = Position(*value);
                if (val_pos.y < area.bottom && val_pos.y >= area.top && val_pos.x < area.right)
                {
                    if (IsValid(*value))
                    {
                        int dist = Distance(pos, val_pos);
                        if (dist < max_dist)
                        {
                            closest = value;
                            max_dist = dist;
                        }
                    }
                }
                right_pos++;
            }
        }
    }
    return closest;
}

#endif /* POSSEARCH_HPP */
