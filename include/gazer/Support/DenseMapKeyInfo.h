//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_SUPPORT_DENSEMAPKEYINFO_H
#define GAZER_SUPPORT_DENSEMAPKEYINFO_H

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/Hashing.h>

namespace gazer
{

/// \brief A DenseMap key information struct for APInt keys.
///
/// NOTE: this class does not allow 1-width APInts as keys,
/// as they are used for empty and tombstone identifications.
///
/// In the LLVM libraries, DenseMap key information for APInts
/// is hidden in LLVMContextImpl.
/// Furthermore, constructors which would allow the construction
/// of 0-width APInts are also unaccessible from the outside.
struct DenseMapAPIntKeyInfo
{
    static inline llvm::APInt getEmptyKey()
    {
        llvm::APInt value(1, 0);
        return value;
    }

    static inline llvm::APInt getTombstoneKey()
    {
        llvm::APInt value(1, 1);
        return value;
    }

    static unsigned getHashValue(const llvm::APInt &Key) {
        return static_cast<unsigned>(llvm::hash_value(Key));
    }

    static bool isEqual(const llvm::APInt &lhs, const llvm::APInt &rhs) {
        return lhs.getBitWidth() == rhs.getBitWidth() && lhs == rhs;
    }
};

/// \brief A DenseMap key information struct for APFloat keys.
///
/// Based on the implementation found in LLVMContextImpl.h
struct DenseMapAPFloatKeyInfo
{
    static inline llvm::APFloat getEmptyKey() {
        return llvm::APFloat(llvm::APFloat::Bogus(), 1);
    }
    static inline llvm::APFloat getTombstoneKey() {
        return llvm::APFloat(llvm::APFloat::Bogus(), 2);
    }

    static unsigned getHashValue(const llvm::APFloat &key) {
        return static_cast<unsigned>(hash_value(key));
    }

    static bool isEqual(const llvm::APFloat &lhs, const llvm::APFloat &rhs) {
        return lhs.bitwiseIsEqual(rhs);
    }
};

} // end namespace gazer

#endif
