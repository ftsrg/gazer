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
#include "gazer/LLVM/Memory/MemoryObject.h"

using namespace gazer;

// LLVM pass implementation
//-----------------------------------------------------------------------------

char MemoryObjectPass::ID;

bool MemoryObjectPass::runOnFunction(llvm::Function& module)
{
    return false;
}

MemoryObject* MemorySSABuilder::createMemoryObject(
    MemoryAllocType allocType,
    MemoryObjectType objectType,
    MemoryObject::MemoryObjectSize size,
    llvm::Type* valueType,
    llvm::StringRef name
) {
    auto& ptr= mObjects.emplace_back(std::make_unique<MemoryObject>(
        mId++, allocType, objectType, size, valueType, name
    ));

    return &*ptr;
}

MemoryObject::~MemoryObject()
{}

void MemoryObject::print(llvm::raw_ostream& os) const
{
    os << "(" << mId << (mName.empty() ? "" : ", \"" + mName + "\"") << ", allocType=";
    switch (mAllocType) {
        case MemoryAllocType::Unknown: os << "Unknown"; break;
        case MemoryAllocType::Global: os << "Global"; break;
        case MemoryAllocType::Alloca: os << "Alloca"; break;
        case MemoryAllocType::Heap: os << "Heap"; break;
    }

    os << ", objectType=";
    switch (mObjectType) {
        case MemoryObjectType::Unknown: os << "Unknown"; break;
        case MemoryObjectType::Scalar: os << "Scalar"; break;
        case MemoryObjectType::Array: os << "Array"; break;
        case MemoryObjectType::Struct: os << "Struct"; break;
    }
    os << ", size=";
    if (mSize == UnknownSize) {
        os << "Unknown";
    } else {
        os << mSize;
    }

    os << ")";
}

void MemoryObject::addDefinition(MemoryObjectDef* def)
{

}

memory::EntryDef* MemorySSABuilder::getEntryDefinition(MemoryObject* object)
{
    return nullptr;
}

memory::StoreDef* MemorySSABuilder::addDefinition(MemoryObject* object, llvm::StoreInst& inst)
{
    auto def = new memory::StoreDef(object, object->mDefs.size(), inst);
    object->addDefinition(def);

    return def;
}
