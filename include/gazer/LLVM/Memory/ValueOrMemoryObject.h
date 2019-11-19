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
#ifndef GAZER_LLVM_MEMORY_VALUEORMEMORYOBJECT_H
#define GAZER_LLVM_MEMORY_VALUEORMEMORYOBJECT_H

#include <llvm/ADT/DenseMapInfo.h>
#include <llvm/Support/Casting.h>

#include <variant>

namespace llvm
{
    class Value;
    class raw_ostream;
}

namespace gazer
{

class MemoryObjectDef;

/// A wrapper class which can either hold an LLVM IR value or a gazer memory object definition.
class ValueOrMemoryObject
{
    // Note: all of this probably could be done more easily by using DerivedUser in MemoryObject,
    // however this method seems less fragile, even if it introduces considerably more boilerplate.
public:
    /* implicit */ ValueOrMemoryObject(const llvm::Value* value)
        : mVariant(value)
    {}

    /* implicit */ ValueOrMemoryObject(const MemoryObjectDef* def)
        : mVariant(def)
    {}

    ValueOrMemoryObject(const ValueOrMemoryObject&) = default;
    ValueOrMemoryObject& operator=(const ValueOrMemoryObject&) = default;

    // We do not provide conversion operators, as an unchecked conversion may lead to runtime errors.
    operator const llvm::Value*() = delete;
    operator const MemoryObjectDef*() = delete;

    bool isValue() const { return std::holds_alternative<const llvm::Value*>(mVariant); }
    bool isMemoryObjectDef() const { return std::holds_alternative<const MemoryObjectDef*>(mVariant); }

    bool operator==(const ValueOrMemoryObject& rhs) const { return mVariant == rhs.mVariant; }
    bool operator!=(const ValueOrMemoryObject& rhs) const { return mVariant != rhs.mVariant; }

    const llvm::Value* asValue() const { return std::get<const llvm::Value*>(mVariant); }
    const MemoryObjectDef* asMemoryObjectDef() const { return std::get<const MemoryObjectDef*>(mVariant); }

    bool hasName() const;

    /// Returns the name of the contained value or memory object
    std::string getName() const;

private:
    std::variant<const llvm::Value*, const MemoryObjectDef*> mVariant;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ValueOrMemoryObject& rhs);

} // end namespace gazer

namespace llvm
{

// Some LLVM-specific magic to make DenseMap and isa<> work seamlessly with ValueOrMemoryObject.
// Note that while custom isa<> works, currently there is no support for cast<> or dynamic_cast<>.
template<>
struct DenseMapInfo<gazer::ValueOrMemoryObject>
{
    // Empty and tombstone will be represented by a variant holding an invalid Value pointer.
    static inline gazer::ValueOrMemoryObject getEmptyKey() {
        return DenseMapInfo<Value*>::getEmptyKey();
    }

    static inline gazer::ValueOrMemoryObject getTombstoneKey() {
        return DenseMapInfo<Value*>::getTombstoneKey();
    }

    static unsigned getHashValue(const gazer::ValueOrMemoryObject& val) {
        // This is a bit weird, but needed to get the pointer value from the variant properly.
        if (val.isValue()) {
            return DenseMapInfo<llvm::Value*>::getHashValue(val.asValue());
        }

        return DenseMapInfo<gazer::MemoryObjectDef*>::getHashValue(val.asMemoryObjectDef());
    }

    static bool isEqual(const gazer::ValueOrMemoryObject& lhs, const gazer::ValueOrMemoryObject& rhs) {
        return lhs == rhs;
    }
};

template<class To>
struct isa_impl<To, gazer::ValueOrMemoryObject, std::enable_if_t<std::is_base_of_v<Value, To>>>
{
    static inline bool doit(const gazer::ValueOrMemoryObject& val) {
        if (!val.isValue()) {
            return false;
        }

        return To::classof(val.asValue());
    }
};

template<class To>
struct isa_impl<To, gazer::ValueOrMemoryObject, std::enable_if_t<std::is_base_of_v<gazer::MemoryObjectDef, To>>>
{
    static inline bool doit(const gazer::ValueOrMemoryObject& val) {
        if (!val.isMemoryObjectDef()) {
            return false;
        }

        return To::classof(val.asMemoryObjectDef());
    }
};

} // end namespace llvm

#endif //GAZER_LLVM_MEMORY_VALUEORMEMORYOBJECT_H
