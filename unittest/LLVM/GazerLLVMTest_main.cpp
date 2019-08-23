#include <gtest/gtest.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>

#include <filesystem>

namespace gazer::unittest
{
    std::string ResourcesPath;
}

int main(int argc, char *argv[])
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        llvm::errs() << "No test resource directory was supplied.\n";
        return 1;
    }

    namespace fs = std::filesystem;

    std::error_code error;

    fs::path path(argv[1]);
    auto absPath = fs::absolute(path, error);
    if (error) {
        llvm::errs() << "Test resource directory was not found: " << error.message() << "\n";
        return 1;
    }
    
    gazer::unittest::ResourcesPath = absPath.string();

    return RUN_ALL_TESTS();
}
