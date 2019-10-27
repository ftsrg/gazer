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
    explicit VerificationResult(Status status)
        : mStatus(status)
    {}

public:
    [[nodiscard]] bool isSuccess() const { return mStatus == Success; }
    [[nodiscard]] bool isFail() const { return mStatus == Fail; }
    [[nodiscard]] bool isUnknown() const { return mStatus == Unknown; }

    [[nodiscard]] Status getStatus() const { return mStatus; }

    static std::unique_ptr<VerificationResult> CreateSuccess();

    static std::unique_ptr<VerificationResult> CreateFail(unsigned ec, std::unique_ptr<Trace> trace = nullptr);

    static std::unique_ptr<VerificationResult> CreateUnknown();
    static std::unique_ptr<VerificationResult> CreateTimeout();
    static std::unique_ptr<VerificationResult> CreateInternalError(llvm::Twine message);

    virtual ~VerificationResult() = default;

private:
    Status mStatus;
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

class SuccessResult final : public VerificationResult
{
public:
    SuccessResult()
        : VerificationResult(VerificationResult::Success)
    {}

    static bool classof(const VerificationResult* result) {
        return result->getStatus() == VerificationResult::Success;
    }
};

class UnknownResult final : public VerificationResult
{
public:
    explicit UnknownResult(std::string message = "")
        : VerificationResult(VerificationResult::Unknown), mMessage(std::move(message))
    {}

    [[nodiscard]] llvm::StringRef getMessage() const { return mMessage; }

    static bool classof(const VerificationResult* result) {
        return result->getStatus() == VerificationResult::Unknown;
    }

private:
    std::string mMessage;
};

class InternalErrorResult final : public VerificationResult
{
public:
    InternalErrorResult(llvm::Twine message)
        : VerificationResult(VerificationResult::InternalError),
        mMessage(message.str())
    {}

    llvm::StringRef getMessage() const { return mMessage; }

    static bool classof(const VerificationResult* result) {
        return result->getStatus() == VerificationResult::InternalError;
    }

private:
    std::string mMessage;
};

} // end namespace gazer

#endif