#ifndef _GAZER_SUPPORT_FLOAT_H
#define _GAZER_SUPPORT_FLOAT_H

#include <cstdint>
#include <cstring>
#include <cassert>

namespace gazer
{

inline double double_from_uint64(uint64_t bits)
{
    double result;
    assert(sizeof(result) == sizeof(bits));
    memcpy(&result, &bits, sizeof(bits));

    return result;
}

inline float float_from_uint64(uint64_t bits)
{
    float result;
    uint32_t trunc = bits;
    assert(sizeof(trunc) == sizeof(bits));
    memcpy(&result, &bits, sizeof(trunc));

    return result;
}


}

#endif
