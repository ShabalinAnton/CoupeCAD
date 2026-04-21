#pragma once

#include "coupecad/logging/log_level.h"

#include <fmt/core.h>

#include <filesystem>
#include <source_location>
#include <string_view>

namespace coupecad::logging {

class Logger {
public:
    // Process-wide singleton. Lazy-init с дефолтной конфигурацией
    // (см. spec §6.3): min_level=Info, console=on, file=on,
    // путь — платформенный (см. log_paths.h).
    static Logger& instance();

    // Только для тестов: пересоздаёт singleton с дефолтами. Не вызывать
    // из production-кода. Объявление здесь, чтобы тесты могли вызвать
    // через прямой include logger.h.
    static void reset_for_test();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(Level level,
             std::string_view category,
             std::string_view message,
             std::source_location loc = std::source_location::current());

    // Шорткаты с fmt-форматированием.
    template <class... Args>
    void critical(std::string_view category,
                  fmt::format_string<Args...> fmt,
                  Args&&... args) {
        log_formatted(Level::Critical, category, fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void error(std::string_view category,
               fmt::format_string<Args...> fmt,
               Args&&... args) {
        log_formatted(Level::Error, category, fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void warn(std::string_view category,
              fmt::format_string<Args...> fmt,
              Args&&... args) {
        log_formatted(Level::Warning, category, fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void info(std::string_view category,
              fmt::format_string<Args...> fmt,
              Args&&... args) {
        log_formatted(Level::Info, category, fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void debug(std::string_view category,
               fmt::format_string<Args...> fmt,
               Args&&... args) {
        log_formatted(Level::Debug, category, fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void trace(std::string_view category,
               fmt::format_string<Args...> fmt,
               Args&&... args) {
        log_formatted(Level::Trace, category, fmt, std::forward<Args>(args)...);
    }

    // Конфигурация min_level и sinks.
    void set_min_level(Level level);
    Level min_level() const;

    void enable_console(bool on);
    bool console_enabled() const;

    void enable_file(bool on);
    bool file_enabled() const;

    void set_log_file_path(std::filesystem::path path);
    std::filesystem::path log_file_path() const;

    // Категорийные фильтры. Если для категории установлен уровень —
    // он перебивает min_level() для этой категории.
    void set_category_level(std::string_view category, Level level);
    void clear_category_level(std::string_view category);

private:
    Logger();
    ~Logger();

    template <class... Args>
    void log_formatted(Level level,
                       std::string_view category,
                       fmt::format_string<Args...> fmt,
                       Args&&... args) {
        // Проверка уровня — здесь, чтобы избежать форматирования при
        // глушённом уровне (это горячий путь).
        if (!is_enabled(level, category)) {
            return;
        }
        log(level, category, fmt::format(fmt, std::forward<Args>(args)...));
    }

    bool is_enabled(Level level, std::string_view category) const;

    struct Impl;
    Impl* impl_;
};

}  // namespace coupecad::logging
