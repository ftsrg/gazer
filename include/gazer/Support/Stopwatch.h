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
#ifndef GAZER_SUPPORT_STOPWATCH_H
#define GAZER_SUPPORT_STOPWATCH_H

#include <llvm/Support/Chrono.h>

namespace gazer
{

/// Simple Stopwatch class for measuring execution times.
template<
    class TimeUnit = std::chrono::milliseconds,
    class Clock = std::chrono::high_resolution_clock
>
class Stopwatch
{
public:
    Stopwatch()
        : mStarted(Clock::now()), mStopped(Clock::now()), mRunning(false)
    {}

public:
    void start()
    {
        if (!mRunning) {
            mStarted = Clock::now();
            mRunning = true;
        }
    }

    void stop()
    {
        if (mRunning) {
            mStopped = Clock::now();
            mRunning = false;
        }
    }

    void reset()
    {
        mRunning = false;
        mStopped = mStarted;
    }

    TimeUnit elapsed()
    {
        if (mRunning) {
            return std::chrono::duration_cast<TimeUnit>(Clock::now() - mStarted);
        }

        return std::chrono::duration_cast<TimeUnit>(mStopped - mStarted);
    }

    void format(llvm::raw_ostream& os, llvm::StringRef style = "")
    {
        auto tu = elapsed();
        llvm::format_provider<TimeUnit>::format(tu, os, style);
    }

private:
    std::chrono::time_point<Clock> mStarted;
    std::chrono::time_point<Clock> mStopped;
    bool mRunning;
};

}

#endif
