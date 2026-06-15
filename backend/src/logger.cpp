#include "logger.h"
#include <iostream>

namespace crossbow {

LogLevel Logger::level_ = LogLevel::Info;

#ifdef HAS_SPDLOG
std::shared_ptr<spdlog::logger> Logger::logger_;
#endif

void Logger::init(const std::string& log_file, bool console, LogLevel level) {
    set_level(level);

#ifdef HAS_SPDLOG
    std::vector<spdlog::sink_ptr> sinks;

    if (console) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        sinks.push_back(console_sink);
    }

    if (!log_file.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        sinks.push_back(file_sink);
    }

    logger_ = std::make_shared<spdlog::logger>("crossbow", sinks.begin(), sinks.end());
    logger_->set_level(static_cast<spdlog::level::level_enum>(level));
    logger_->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(logger_);
    spdlog::register_logger(logger_);
#else
    (void)log_file;
    (void)console;
#endif
}

void Logger::set_level(LogLevel level) {
    level_ = level;
#ifdef HAS_SPDLOG
    if (logger_) {
        logger_->set_level(static_cast<spdlog::level::level_enum>(level));
    }
#endif
}

LogLevel Logger::get_level() {
    return level_;
}

}
