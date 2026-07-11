#pragma once
//
// GTUltra logging (C++20). Writes to std::clog with file:line context.
//
//   LOG_INFO / LOG_WARN / LOG_ERROR  — always emitted
//   LOG_DEBUG                        — only when GTULTRA_DEBUG=1 (or legacy GTULTRA_PFD_DEBUG=1)
//
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string_view>

enum class LogLevel : int { Debug, Info, Warn, Error };

inline bool log_debug_enabled()
{
    static const bool on = [] {
        const char* env = std::getenv("GTULTRA_DEBUG");
        if (!env || !env[0] || env[0] == '0') env = std::getenv("GTULTRA_PFD_DEBUG");
        return env && env[0] && env[0] != '0';
    }();
    return on;
}

struct LogContext {
    std::source_location loc;
    LogContext(std::source_location l = std::source_location::current()) : loc(l) {}
};

template <typename... Args>
void log_msg(LogLevel level, LogContext ctx, std::format_string<Args...> fmt, Args&&... args)
{
    if (level == LogLevel::Debug && !log_debug_enabled()) return;

    static std::mutex           log_mtx;
    std::lock_guard<std::mutex> lock(log_mtx);

    auto now = std::chrono::system_clock::now();
    static const std::string_view LABELS[] = { "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]" };
    std::string_view              label    = LABELS[int(level)];

    std::string_view file = ctx.loc.file_name();
    if (auto pos = file.find_last_of("/\\"); pos != std::string_view::npos) file.remove_prefix(pos + 1);

    std::clog << std::format("{:%H:%M:%S} {} [{}:{}] {}\n",
                             now,
                             label,
                             file,
                             ctx.loc.line(),
                             std::vformat(fmt.get(), std::make_format_args(args...)));
}

#define LOG_DEBUG(fmt, ...) log_msg(LogLevel::Debug, {}, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(fmt, ...) log_msg(LogLevel::Info, {}, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARN(fmt, ...) log_msg(LogLevel::Warn, {}, fmt __VA_OPT__(, ) __VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_msg(LogLevel::Error, {}, fmt __VA_OPT__(, ) __VA_ARGS__)
