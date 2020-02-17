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
#include "GazerContextImpl.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallString.h>

using namespace gazer;

ExprRef<UndefExpr> UndefExpr::Get(Type& type)
{
    return type.getContext().pImpl->Exprs.create<UndefExpr>(type);
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::True(BoolType& type) {
    return type.getContext().pImpl->TrueLit;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::False(BoolType& type) {
    return type.getContext().pImpl->FalseLit;
}

ExprRef<IntLiteralExpr> IntLiteralExpr::Get(IntType& type, long long int value)
{
    return type.getContext().pImpl->Exprs.create<IntLiteralExpr>(type, value);
}

ExprRef<RealLiteralExpr> RealLiteralExpr::Get(RealType& type, boost::rational<long long int> value)
{
    return type.getContext().pImpl->Exprs.create<RealLiteralExpr>(type, value);
}

ExprRef<BvLiteralExpr> BvLiteralExpr::Get(BvType& type, const llvm::APInt& value)
{
    assert(type.getWidth() == value.getBitWidth() && "Bit width of type and value must match!");

    auto& pImpl = type.getContext().pImpl;

    return pImpl->Exprs.create<BvLiteralExpr>(type, value);
}

ExprRef<FloatLiteralExpr> FloatLiteralExpr::Get(FloatType& type, const llvm::APFloat& value)
{
    assert(llvm::APFloat::semanticsSizeInBits(value.getSemantics()) == type.getPrecision());

    return type.getContext().pImpl->Exprs.create<FloatLiteralExpr>(type, value);
}

ExprRef<ArrayLiteralExpr> ArrayLiteralExpr::Get(
    ArrayType& type,
    const MappingT& values,
    const ExprRef<LiteralExpr>& elze)
{
    assert(std::all_of(values.begin(), values.end(), [&type](auto& pair) {
        return pair.first->getType() == type.getIndexType()
            && pair.second->getType() == type.getElementType();
    }));
    assert(elze == nullptr || elze->getType() == type.getElementType());

    auto& pImpl = type.getContext().pImpl;

    return pImpl->Exprs.create<ArrayLiteralExpr>(type, values, elze);
}

void UndefExpr::print(llvm::raw_ostream& os) const {
    os << "undef";
}

void BoolLiteralExpr::print(llvm::raw_ostream& os) const {
    os << (mValue ? "True" : "False");
}

void IntLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue;
}

void RealLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue.numerator() << "/" << mValue.denominator();
}

void BvLiteralExpr::print(llvm::raw_ostream& os) const
{
    llvm::SmallString<100> buffer;
    mValue.toStringUnsigned(buffer, /*radix=*/16);

    os << "#" << buffer << "bv" << getType().getWidth();
}

void FloatLiteralExpr::print(llvm::raw_ostream& os) const
{
    llvm::SmallVector<char, 16> buffer;
    mValue.toString(buffer);
    os << buffer;
}

void ArrayLiteralExpr::print(llvm::raw_ostream& os) const
{
    os << "[";
    for (auto& [index, elem] : mMap) {
        os << *index << " -> " << *elem << ", ";
    }
    
    if (mElse != nullptr) {
        os << " else " << *mElse;
    }

    os << "]";
}

ExprRef<AtomicExpr> ArrayLiteralExpr::getValue(const ExprRef<LiteralExpr>& key) const
{
    assert(key->getType() == this->getType().getIndexType());
    auto it = mMap.find(key);
    if (it == mMap.end()) {
        return getDefault();
    }

    return it->second;
}

void ArrayLiteralExpr::Builder::addValue(const ExprRef<LiteralExpr>& index, ExprRef<LiteralExpr> element)
{
    mValues[index] = std::move(element);
}

void ArrayLiteralExpr::Builder::setDefault(const ExprRef<LiteralExpr>& expr)
{
    mElse = expr;
}

auto ArrayLiteralExpr::Builder::build() -> ExprRef<ArrayLiteralExpr>
{
    return ArrayLiteralExpr::Get(mType, mValues, mElse);
}