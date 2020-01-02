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
#include "gazer/Trace/VerificationResult.h"

#include <llvm/ADT/Twine.h>

using namespace gazer;

std::unique_ptr<VerificationResult> VerificationResult::CreateSuccess()
{
    return std::unique_ptr<VerificationResult>(new VerificationResult(Success));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateFail(unsigned ec, std::unique_ptr<Trace> trace)
{
    return std::make_unique<FailResult>(ec, std::move(trace));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateUnknown()
{
    return std::unique_ptr<VerificationResult>(new VerificationResult(Unknown));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateInternalError(llvm::Twine message)
{
    return std::unique_ptr<VerificationResult>(new VerificationResult(InternalError, message.str()));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateTimeout()
{
    return std::unique_ptr<VerificationResult>(new VerificationResult(Timeout));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateBoundReached()
{
    return std::unique_ptr<VerificationResult>(new VerificationResult(BoundReached));
}
