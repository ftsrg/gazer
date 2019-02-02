#ifndef GAZER_SUPPORT_DEBUG_H
#define GAZER_SUPPORT_DEBUG_H

namespace gazer
{
    extern bool IsDebugEnabled;
}

#ifndef NDEBUG

#define GAZER_DEBUG(X)                          \
    do {                                        \
        if (::gazer::IsDebugEnabled) {          \
            X;                                  \
        }                                       \
    } while (false);                            \

#else

#define GAZER_DEBUG(X) do { } while (false);    \

#endif // NDEBUG
#endif // GAZER_SUPPORT_DEBUG_H