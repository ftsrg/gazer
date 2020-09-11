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

#include "ThetaType.h"

using namespace gazer;
using namespace gazer::theta;

std::string gazer::theta::thetaType(Type &type)
{
    switch (type.getTypeID()) {
        case Type::IntTypeID:
            return "int";
        case Type::RealTypeID:
            return "rat";
        case Type::BoolTypeID:
            return "bool";
        case Type::ArrayTypeID: {
            auto& arrTy = llvm::cast<ArrayType>(type);
            return "[" + thetaType(arrTy.getIndexType()) + "] -> " + thetaType(arrTy.getElementType());
        }
        case Type::BvTypeID: {
            auto& bvTy = llvm::cast<BvType>(type);
            return "bv[" + std::to_string(bvTy.getWidth()) + "]";
        }
        default:
            llvm_unreachable("Types which are unsupported by theta should have been eliminated earlier!");
    }
}

std::string gazer::theta::defaultValueForType(Type &type)
{
    switch (type.getTypeID()) {
        case Type::IntTypeID:
            return "0";
        case Type::RealTypeID:
            return "0%1";
        case Type::BoolTypeID:
            return "false";
        case Type::ArrayTypeID: {
            auto& arrTy = llvm::cast<ArrayType>(type);
            return "[<" + thetaType(arrTy.getIndexType()) + ">default <- " + defaultValueForType(arrTy.getElementType()) + "]";
        }
        case Type::BvTypeID: {
            auto& bvTy = llvm::cast<BvType>(type);
            return std::to_string(bvTy.getWidth()) + "'d0";
        }
        default:
            llvm_unreachable("Types which are unsupported by theta should have been eliminated earlier!");
    }
}
