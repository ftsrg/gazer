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
#ifndef GAZER_TRACE_VerificationResult_H
#define GAZER_TRACE_VerificationResult_H

#include "gazer/Trace/Trace.h"
#include <llvm/ADT/Twine.h>

namespace gazer
{

class VerificationResult
{
public:
    static constexpr unsigned SuccessErrorCode = 0;
    static constexpr unsigned GeneralFailureCode = 1;

    enum Status { Success, Fail, Timeout, Unknown, BoundReached, InternalError };
protected:
    explicit VerificationResult(Status status, std::string message = "")
        : mStatus(status), mMessage(std::move(message))
    {}

public:
    [[nodiscard]] bool isSuccess() const { return mStatus == Success; }
    [[nodiscard]] bool isFail() const { return mStatus == Fail; }
    [[nodiscard]] bool isUnknown() const { return mStatus == Unknown; }

    [[nodiscard]] Status getStatus() const { return mStatus; }

    llvm::StringRef getMessage() const { return mMessage; }

    static std::unique_ptr<VerificationResult> CreateSuccess();

    static std::unique_ptr<VerificationResult> CreateFail(unsigned ec, std::unique_ptr<Trace> trace = nullptr);

    static std::unique_ptr<VerificationResult> CreateUnknown();
    static std::unique_ptr<VerificationResult> CreateTimeout();
    static std::unique_ptr<VerificationResult> CreateInternalError(llvm::Twine message);
    static std::unique_ptr<VerificationResult> CreateBoundReached();

    virtual ~VerificationResult() = default;

private:
    Status mStatus;
    std::string mMessage;
};

class FailResult final : public VerificationResult
{
public:
    explicit FailResult(
        unsigned errorCode,
        std::unique_ptr<Trace> trace = nullptr
    ) : VerificationResult(VerificationResult::Fail), mErrorID(errorCode),
        mTrace(std::move(trace))
    {}

    [[nodiscard]] bool hasTrace() const { return mTrace != nullptr; }
    [[nodiscard]] Trace& getTrace() const { return *mTrace; }
    [[nodiscard]] unsigned getErrorID() const { return mErrorID; }

    static bool classof(const VerificationResult* result) {
        return result->getStatus() == VerificationResult::Fail;
    }

private:
    unsigned mErrorID;
    std::unique_ptr<Trace> mTrace;
};

} // end namespace gazer

#endif