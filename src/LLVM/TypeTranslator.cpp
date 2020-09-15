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
#include "gazer/LLVM/TypeTranslator.h"

#include "gazer/LLVM/Memory/MemoryObject.h"
#include "gazer/LLVM/Memory/MemoryModel.h"

using namespace gazer;

gazer::Type& LLVMTypeTranslator::get(const llvm::Type* type)
{
    assert(type != nullptr && "Cannot translate NULL types!");
    assert(type->isFirstClassType() && "Can only translate first class types!");

    auto& ctx = mMemoryTypes.getContext();

    switch (type->getTypeID()) {
        case llvm::Type::IntegerTyID: {
            auto width = type->getIntegerBitWidth();
            if (width == 1) {
                return BoolType::Get(ctx);
            }

            if (mSettings.ints == IntRepresentation::BitVectors) {
                return BvType::Get(ctx, width);
            }

            return IntType::Get(ctx);            
        }
        case llvm::Type::HalfTyID:
            return FloatType::Get(ctx, FloatType::Half);
        case llvm::Type::FloatTyID:
            return FloatType::Get(ctx, FloatType::Single);
        case llvm::Type::DoubleTyID:
            return FloatType::Get(ctx, FloatType::Double);
        case llvm::Type::FP128TyID:
            return FloatType::Get(ctx, FloatType::Quad);
        case llvm::Type::PointerTyID:
            return mMemoryTypes.handlePointerType(llvm::cast<llvm::PointerType>(type));
        case llvm::Type::ArrayTyID:
            return mMemoryTypes.handleArrayType(llvm::cast<llvm::ArrayType>(type));
        case llvm::Type::StructTyID: {
            auto structTy = llvm::cast<llvm::StructType>(type);
            std::vector<Type*> subtypeList;
            for (llvm::Type* llvmSubtype : structTy->subtypes()) {
                subtypeList.push_back(&this->get(llvmSubtype));
            }

            return TupleType::Get(subtypeList);
        }
        default:
            llvm::errs() << "Unsupported LLVM Type: " << *type << "\n";
            llvm_unreachable("Unsupported LLVM type.");
    }
}
