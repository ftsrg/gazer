#include "gazer/Trace/VerificationResult.h"

#include <llvm/ADT/Twine.h>

using namespace gazer;

std::unique_ptr<VerificationResult> VerificationResult::CreateSuccess()
{
    return std::make_unique<SuccessResult>();
}

std::unique_ptr<VerificationResult> VerificationResult::CreateFail(unsigned ec, std::unique_ptr<Trace> trace)
{
    return std::make_unique<FailResult>(ec, std::move(trace));
}

std::unique_ptr<VerificationResult> VerificationResult::CreateUnknown()
{
    return std::make_unique<UnknownResult>();
}

std::unique_ptr<VerificationResult> VerificationResult::CreateInternalError(llvm::Twine message)
{
    return std::make_unique<InternalErrorResult>(message);
}

std::unique_ptr<VerificationResult> VerificationResult::CreateTimeout()
{
    return std::make_unique<UnknownResult>();
}
