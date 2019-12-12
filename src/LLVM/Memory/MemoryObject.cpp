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

MemoryObject::~MemoryObject() = default;

llvm::BasicBlock* MemoryObjectDef::getParentBlock() const
{
    if (auto instAnnot = llvm::dyn_cast<memory::InstructionAnnotationDef>(this)) {
        return instAnnot->getInstruction()->getParent();
    }

    if (auto bbAnnot = llvm::dyn_cast<memory::BlockAnnotationDef>(this)) {
        return bbAnnot->getBlock();
    }

    llvm_unreachable("A memory object definition must annotate either an instruction or a block!");
}

void MemoryObject::print(llvm::raw_ostream& os) const
{
    os
        << "(" << mId << (mName.empty() ? "" : ", \"" + mName + "\"")
        << ", objectType=" << mObjectType
        << ", size=";
    if (mSize == UnknownSize) {
        os << "Unknown";
    } else {
        os << mSize;
    }

    os << ")";
}

void MemoryObject::addDefinition(MemoryObjectDef* def)
{
    mDefs.emplace_back(std::unique_ptr<MemoryObjectDef>(def));
}

void MemoryObject::addUse(MemoryObjectUse* use)
{
    mUses.emplace_back(std::unique_ptr<MemoryObjectUse>(use));
}

void MemoryObject::setEntryDef(MemoryObjectDef* def)
{
    assert(
        llvm::isa<memory::LiveOnEntryDef>(def)
        || llvm::isa<memory::GlobalInitializerDef>(def));
    mEntryDef = def;
}

void MemoryObject::setExitUse(MemoryObjectUse* use)
{
    assert(llvm::isa<memory::RetUse>(use));
    mExitUse = use;
}

void MemoryObjectDef::print(llvm::raw_ostream& os) const
{
    os << mVersion << " := ";
    this->doPrint(os);
}

std::string MemoryObjectDef::getName() const
{
    std::string objName = getObject()->getName();
    if (objName.empty()) {
        objName = std::to_string(getObject()->getId());
    }

    return objName + "_" + std::to_string(mVersion);
}

MemoryObjectDef* memory::PhiDef::getIncomingDefForBlock(const llvm::BasicBlock* bb) const
{
    return mEntryList.lookup(bb);
}

void memory::PhiDef::addIncoming(MemoryObjectDef* def, const llvm::BasicBlock* bb)
{
    mEntryList[bb] = def;
}

void memory::LiveOnEntryDef::doPrint(llvm::raw_ostream& os) const
{
    os << "liveOnEntry(" << getObject()->getName() << ")";
}

void memory::GlobalInitializerDef::doPrint(llvm::raw_ostream& os) const
{
    os << "globalInit(" << getObject()->getName();
    if (mGlobalVariable->hasInitializer()) {
        os << ", ";
        mGlobalVariable->getInitializer()->printAsOperand(os);
    }
    os << ")";
}

void memory::StoreDef::doPrint(llvm::raw_ostream& os) const
{
    os << "store(" << getObject()->getName() << ")";
}

void memory::CallDef::doPrint(llvm::raw_ostream& os) const
{
    os << "call(" << getObject()->getName() << ")";
}

void memory::AllocaDef::doPrint(llvm::raw_ostream& os) const
{
    os << "alloca(" << getObject()->getName() << ")";
}

void memory::PhiDef::doPrint(llvm::raw_ostream& os) const
{
    os << "phi(" << getObject()->getName() << ", {";
    for (auto& entry : mEntryList) {
        os << "[" << entry.second->getVersion() << ", " << entry.first->getName() << "], ";
    }
    os << "})";
}

void memory::LoadUse::print(llvm::raw_ostream& os) const
{
    os << "load(" << getObject()->getName() << ", ";
    if (getReachingDef() == nullptr) {
        os << "???";
    } else {
        os << getReachingDef()->getVersion();
    }
    os << ", ";
    mLoadInst->printAsOperand(os);
    os << ")";
}

void memory::CallUse::print(llvm::raw_ostream& os) const
{
    os << "call(" << getObject()->getName() << ", ";
    if (getReachingDef() == nullptr) {
        os << "???";
    } else {
        os << getReachingDef()->getVersion();
    }
    os << ")";
}

void memory::RetUse::print(llvm::raw_ostream& os) const
{
    os << "ret(" << getObject()->getName() << ", ";
    if (getReachingDef() == nullptr) {
        os << "???";
    } else {
        os << getReachingDef()->getVersion();
    }
    os << ")";
}