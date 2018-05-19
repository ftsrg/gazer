#ifndef _GAZER_SUPPORT_ERROR_H
#define _GAZER_SUPPORT_ERROR_H

#include <llvm/Support/ErrorHandling.h>

#include <cassert>
#include <iostream>

/*
#define GAZER_UNREACHABLE(MSG) llvm_unreachable(MSG)

inline void gazer_unreachable_impl(const char* msg, const char* file, unsigned line)
{
    std::cerr << "Unreachable code path executed";
    if (file) {
        std::cerr << " at " << file << ":" << line << ".";
    }
    std::cerr << "\n";

    abort();
}
*/
#endif
