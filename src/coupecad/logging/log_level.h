#pragma once

namespace coupecad::logging {

// Семантика уровней — см. spec §6.1.
// Off строго больше Trace в значении: Off используется как "максимальный
// порог", который глушит любое сообщение.
enum class Level {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    Off = 6,
};

constexpr const char* level_name(Level level) noexcept {
    switch (level) {
        case Level::Trace:    return "TRACE";
        case Level::Debug:    return "DEBUG";
        case Level::Info:     return "INFO";
        case Level::Warning:  return "WARN";
        case Level::Error:    return "ERROR";
        case Level::Critical: return "CRITICAL";
        case Level::Off:      return "OFF";
    }
    return "?";
}

}  // namespace coupecad::logging
