#include "coupecad/logging/logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <fmt/format.h>

#include <mutex>
#include <unordered_map>
#include <string>

namespace coupecad::logging {

namespace {

spdlog::level::level_enum to_spd(Level level) noexcept {
    switch (level) {
        case Level::Trace:    return spdlog::level::trace;
        case Level::Debug:    return spdlog::level::debug;
        case Level::Info:     return spdlog::level::info;
        case Level::Warning:  return spdlog::level::warn;
        case Level::Error:    return spdlog::level::err;
        case Level::Critical: return spdlog::level::critical;
        case Level::Off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

}  // namespace

struct Logger::Impl {
    mutable std::mutex mu;
    Level min_level = Level::Info;
    bool console_on = true;
    bool file_on = true;
    std::filesystem::path file_path;  // заполнится в Task 3
    std::unordered_map<std::string, Level> category_levels;
    std::shared_ptr<spdlog::logger> spd;  // создаётся в rebuild()

    void rebuild() {
        std::vector<spdlog::sink_ptr> sinks;
        if (console_on) {
            sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
        }
        // File sink будет добавлен в Task 3.
        spd = std::make_shared<spdlog::logger>(
            "coupecad", sinks.begin(), sinks.end());
        spd->set_level(to_spd(min_level));
        spd->set_pattern("%Y-%m-%dT%H:%M:%S.%e%z [%^%l%$] %v");
        spd->flush_on(spdlog::level::warn);
    }
};

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::reset_for_test() {
    auto& inst = instance();
    std::lock_guard lk(inst.impl_->mu);
    inst.impl_->min_level = Level::Info;
    inst.impl_->console_on = true;
    inst.impl_->file_on = true;
    inst.impl_->category_levels.clear();
    inst.impl_->rebuild();
}

Logger::Logger() : impl_(new Impl()) {
    impl_->rebuild();
}

Logger::~Logger() {
    delete impl_;
}

bool Logger::is_enabled(Level level, std::string_view category) const {
    std::lock_guard lk(impl_->mu);
    if (level == Level::Off) {
        return false;
    }
    auto it = impl_->category_levels.find(std::string(category));
    Level effective = (it != impl_->category_levels.end()) ? it->second : impl_->min_level;
    return static_cast<int>(level) >= static_cast<int>(effective);
}

void Logger::log(Level level,
                 std::string_view category,
                 std::string_view message,
                 std::source_location loc) {
    if (!is_enabled(level, category)) {
        return;
    }
    // Pre-format the message so we can use the plain string_view overload of
    // spdlog::logger::log(), avoiding the consteval FMT_STRING checks that
    // trigger compile errors with the spdlog 1.13 + fmt 10 combination.
    std::string formatted = fmt::format(
        "{:<14} {}:{}  {}",
        category,
        loc.file_name(),
        loc.line(),
        message);
    std::lock_guard lk(impl_->mu);
    impl_->spd->log(
        spdlog::source_loc{loc.file_name(), static_cast<int>(loc.line()), loc.function_name()},
        to_spd(level),
        spdlog::string_view_t(formatted));
}

void Logger::set_min_level(Level level) {
    std::lock_guard lk(impl_->mu);
    impl_->min_level = level;
    impl_->spd->set_level(to_spd(level));
}

Level Logger::min_level() const {
    std::lock_guard lk(impl_->mu);
    return impl_->min_level;
}

void Logger::enable_console(bool on) {
    std::lock_guard lk(impl_->mu);
    impl_->console_on = on;
    impl_->rebuild();
}

bool Logger::console_enabled() const {
    std::lock_guard lk(impl_->mu);
    return impl_->console_on;
}

void Logger::enable_file(bool on) {
    std::lock_guard lk(impl_->mu);
    impl_->file_on = on;
    impl_->rebuild();
}

bool Logger::file_enabled() const {
    std::lock_guard lk(impl_->mu);
    return impl_->file_on;
}

void Logger::set_log_file_path(std::filesystem::path path) {
    std::lock_guard lk(impl_->mu);
    impl_->file_path = std::move(path);
    impl_->rebuild();
}

std::filesystem::path Logger::log_file_path() const {
    std::lock_guard lk(impl_->mu);
    return impl_->file_path;
}

void Logger::set_category_level(std::string_view category, Level level) {
    std::lock_guard lk(impl_->mu);
    impl_->category_levels[std::string(category)] = level;
}

void Logger::clear_category_level(std::string_view category) {
    std::lock_guard lk(impl_->mu);
    impl_->category_levels.erase(std::string(category));
}

}  // namespace coupecad::logging
