#pragma once

#include <mutex>
#include <iostream>
#include <sstream>

/**
 * Thread-safe logging utilities
 * Prevents interleaved output from multiple threads
 */
class Logger
{
public:
    static Logger &Instance()
    {
        static Logger instance;
        return instance;
    }

    template <typename... Args>
    void Log(Args &&...args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        LogHelper(oss, std::forward<Args>(args)...);
        std::cout << oss.str() << std::endl;
    }

private:
    Logger() = default;
    std::mutex mutex_;

    template <typename T>
    void LogHelper(std::ostringstream &oss, T &&arg)
    {
        oss << arg;
    }

    template <typename T, typename... Args>
    void LogHelper(std::ostringstream &oss, T &&first, Args &&...rest)
    {
        oss << first;
        LogHelper(oss, std::forward<Args>(rest)...);
    }
};

// Convenience macro for thread-safe logging
#define LOG(...) Logger::Instance().Log(__VA_ARGS__)
