#ifndef _GAZER_TRACE_VerificationResult_H
#define _GAZER_TRACE_VerificationResult_H

#include "gazer/Trace/Trace.h"

namespace gazer
{

class VerificationResult
{
public:
    enum Status { Success, Fail, Timeout, Unknown, BoundReached, InternalError };
protected:
    VerificationResult(Status status)
        : mStatus(status)
    {}

public:
    bool isSuccess() const { return mStatus == Success; }
    bool isFail() const { return mStatus == Fail; }
    bool isUnknown() const { return mStatus == Unknown; }

    Status getStatus() const { return mStatus; }

    static std::unique_ptr<VerificationResult> CreateSuccess();

    static std::unique_ptr<VerificationResult> CreateFail(unsigned ec, std::unique_ptr<Trace> trace = nullptr);

    static std::unique_ptr<VerificationResult> CreateUnknown();
    static std::unique_ptr<VerificationResult> CreateTimeout();
    static std::unique_ptr<VerificationResult> CreateInternalError(std::string message);

    virtual ~VerificationResult() = default;

private:
    Status mStatus;
};

class FailResult final : public VerificationResult
{
public:
    FailResult(
        unsigned errorCode,
        std::unique_ptr<Trace> trace = nullptr
    ) : VerificationResult(VerificationResult::Fail), mErrorID(errorCode),
        mTrace(std::move(trace))
    {}

    Trace& getTrace() const { return *mTrace; }
    unsigned getErrorID() const { return mErrorID; }

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
};

class UnknownResult final : public VerificationResult
{
public:
    UnknownResult(std::string message = "")
        : VerificationResult(VerificationResult::Unknown), mMessage(message)
    {}

    llvm::StringRef getMessage() const { return mMessage; }

private:
    std::string mMessage;
};

class InternalErrorResult final : public VerificationResult
{
public:
    InternalErrorResult(std::string message)
        : VerificationResult(VerificationResult::InternalError),
        mMessage(std::move(message))
    {}

    llvm::StringRef getMessage() const { return mMessage; }

private:
    std::string mMessage;
};

}

#endif