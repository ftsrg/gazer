#ifndef _GAZER_SUPPORT_STOPWATCH_H
#define _GAZER_SUPPORT_STOPWATCH_H

#include <llvm/Support/Chrono.h>

namespace gazer
{

/**
 * Simple Stopwatch class for measuring execution times.
 */
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
