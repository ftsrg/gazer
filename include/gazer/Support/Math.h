#ifndef GAZER_SUPPORT_MATH_H
#define GAZER_SUPPORT_MATH_H

namespace gazer::math
{

/// Calculates the power of two integers and returns an int.
/// Does not account for overflow and does not work for negative exponents.
unsigned ipow(unsigned base, unsigned exp)
{
    if (exp == 0) {
        return 1;
    }

    if (exp == 1) {
        return base;
    }

    unsigned tmp = ipow(base, exp / 2);
    if (exp % 2 == 0) {
        return tmp * tmp;
    }

    return base * tmp * tmp;
}

}

#endif
