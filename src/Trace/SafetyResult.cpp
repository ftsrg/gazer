    
#include "gazer/Trace/SafetyResult.h"

using namespace gazer;

std::unique_ptr<SafetyResult> SafetyResult::CreateSuccess()
{
    return std::make_unique<SuccessResult>();
}

std::unique_ptr<SafetyResult> SafetyResult::CreateFail(unsigned ec, std::unique_ptr<Trace> trace)
{
    return std::make_unique<FailResult>(ec, std::move(trace));
}

std::unique_ptr<SafetyResult> SafetyResult::CreateUnknown()
{
    return std::make_unique<UnknownResult>();
}