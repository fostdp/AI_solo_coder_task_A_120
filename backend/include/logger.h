#pragma once

#include <string>
#include <memory>
#include <sstream>
#include <vector>
#include <cstdio>
#include <iostream>

#ifdef HAS_SPDLOG
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#endif

namespace crossbow {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5
};

namespace detail {

inline void format_helper(std::ostringstream& oss, const char* fmt) {
    while (*fmt) {
        if (*fmt == '{' && *(fmt + 1) == '}') {
            break;
        }
        oss << *fmt++;
    }
}

template <typename T, typename... Args>
void format_helper(std::ostringstream& oss, const char* fmt, const T& value, const Args&... args) {
    while (*fmt) {
        if (*fmt == '{' && *(fmt + 1) == '}') {
            oss << value;
            format_helper(oss, fmt + 2, args...);
            return;
        }
        oss << *fmt++;
    }
}

template <typename... Args>
std::string format_str(const char* fmt, const Args&... args) {
    std::ostringstream oss;
    format_helper(oss, fmt, args...);
    return oss.str();
}

inline void log_fallback(LogLevel level, const std::string& msg) {
    const char* prefix = "[INFO] ";
    std::ostream* out = &std::cout;
    switch (level) {
        case LogLevel::Trace:    prefix = "[TRACE] "; break;
        case LogLevel::Debug:    prefix = "[DEBUG] "; break;
        case LogLevel::Info:     prefix = "[INFO] ";  break;
        case LogLevel::Warn:     prefix = "[WARN] ";  out = &std::cerr; break;
        case LogLevel::Error:    prefix = "[ERROR] "; out = &std::cerr; break;
        case LogLevel::Critical: prefix = "[CRIT] ";  out = &std::cerr; break;
    }
    *out << prefix << msg << std::endl;
}

}

class Logger {
public:
    static void init(const std::string& log_file = "", bool console = true, LogLevel level = LogLevel::Info);
    static void set_level(LogLevel level);
    static LogLevel get_level();

#ifdef HAS_SPDLOG
    static std::shared_ptr<spdlog::logger>& get() { return logger_; }
#endif

private:
#ifdef HAS_SPDLOG
    static std::shared_ptr<spdlog::logger> logger_;
#endif
    static LogLevel level_;
};

}

#ifdef HAS_SPDLOG

#define LOG_TRACE(...)    spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...)    spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...)     spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)     spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...)    spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)

#else

#define LOG_TRACE(...)    do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Trace)    crossbow::detail::log_fallback(crossbow::LogLevel::Trace,    crossbow::detail::format_str(__VA_ARGS__)); } while(0)
#define LOG_DEBUG(...)    do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Debug)    crossbow::detail::log_fallback(crossbow::LogLevel::Debug,    crossbow::detail::format_str(__VA_ARGS__)); } while(0)
#define LOG_INFO(...)     do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Info)     crossbow::detail::log_fallback(crossbow::LogLevel::Info,     crossbow::detail::format_str(__VA_ARGS__)); } while(0)
#define LOG_WARN(...)     do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Warn)     crossbow::detail::log_fallback(crossbow::LogLevel::Warn,     crossbow::detail::format_str(__VA_ARGS__)); } while(0)
#define LOG_ERROR(...)    do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Error)    crossbow::detail::log_fallback(crossbow::LogLevel::Error,    crossbow::detail::format_str(__VA_ARGS__)); } while(0)
#define LOG_CRITICAL(...) do { if (crossbow::Logger::get_level() <= crossbow::LogLevel::Critical) crossbow::detail::log_fallback(crossbow::LogLevel::Critical, crossbow::detail::format_str(__VA_ARGS__)); } while(0)

#endif
