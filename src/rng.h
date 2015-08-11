#ifndef RNG_H
#define RNG_H

#include "offsets.h"

class Rng
{
    public:
        unsigned int Rand(unsigned int vals)
        {
            (*bw::all_rng_calls)++;
            seed = seed * 0x15A4E35 + 1;
            uint32_t ret = (seed >> 0x10) & 0x7fff;
            return ret % vals;
        }

        uint32_t seed;
};

inline int Rand(int rng_id)
{
    if (!*bw::use_rng)
        return 0;
    bw::rng_calls[rng_id]++;
    (*bw::all_rng_calls)++;
    uint32_t seed = *bw::rng_seed * 0x15A4E35 + 1;
    *bw::rng_seed = seed;
    return (seed >> 0x10) & 0x7fff;
}

inline bool EnableRng(bool enable)
{
    bool prev = *bw::use_rng;
    *bw::use_rng = enable;
    return prev;
}

static Rng *main_rng = (Rng *)bw::rng_seed.v();
#endif /* RNG_H */
