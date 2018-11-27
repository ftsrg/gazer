#ifndef _GAZER_TRACE_SAFETYRESULT_H
#define _GAZER_TRACE_SAFETYRESULT_H

#include "gazer/Trace/Trace.h"

#include <optional>

namespace gazer
{

class SafetyResult
{
public:
    enum Status { Success, Fail, Unknown };
protected:
    SafetyResult(Status status)
        : mStatus(status)
    {}

public:
    bool isSuccess() const { return mStatus == Success; }
    bool isFail() const { return mStatus == Fail; }
    bool isUnknown() const { return mStatus == Unknown; }

    Status getStatus() const { return mStatus; }

    static std::unique_ptr<SafetyResult> CreateSuccess();

    static std::unique_ptr<SafetyResult> CreateFail(unsigned ec, std::unique_ptr<Trace> trace = nullptr);
    static std::unique_ptr<SafetyResult> CreateFail(unsigned ec, LocationInfo location, std::unique_ptr<Trace> trace = nullptr);

    static std::unique_ptr<SafetyResult> CreateUnknown();

private:
    Status mStatus;
};

class FailResult final : public SafetyResult
{
public:
    FailResult(
        unsigned errorCode,
        std::optional<LocationInfo> location,
        std::unique_ptr<Trace> trace = nullptr
    ) : SafetyResult(SafetyResult::Fail), mErrorID(errorCode),
    mTrace(std::move(trace)), mLocation(location)
    {}

    Trace& getTrace() const { return *mTrace; }
    unsigned getErrorID() const { return mErrorID; }
    std::optional<LocationInfo> getLocation() const { return mLocation; }

    static bool classof(const SafetyResult* result) {
        return result->getStatus() == SafetyResult::Fail;
    }

private:
    unsigned mErrorID;
    std::unique_ptr<Trace> mTrace;
    std::optional<LocationInfo> mLocation;
};

class SuccessResult final : public SafetyResult
{
public:
    SuccessResult()
        : SafetyResult(SafetyResult::Success)
    {}
};

class UnknownResult final : public SafetyResult
{
public:
    UnknownResult()
        : SafetyResult(SafetyResult::Unknown)
    {}
};

}

#endif