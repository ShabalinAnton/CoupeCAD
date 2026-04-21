#include "coupecad/logging/log_paths.h"

#include <cstdlib>

namespace coupecad::logging {

namespace {

std::filesystem::path home_directory() {
#if defined(_WIN32)
    if (const char* userprofile = std::getenv("USERPROFILE")) {
        return userprofile;
    }
    return std::filesystem::path{"C:\\"};
#else
    if (const char* home = std::getenv("HOME")) {
        return home;
    }
    return std::filesystem::path{"/"};
#endif
}

}  // namespace

std::filesystem::path default_log_directory() {
#if defined(__APPLE__)
    return home_directory() / "Library" / "Logs" / "CoupeCAD";
#elif defined(_WIN32)
    if (const char* appdata = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path{appdata} / "CoupeCAD" / "log";
    }
    return home_directory() / "AppData" / "Local" / "CoupeCAD" / "log";
#else
    if (const char* xdg = std::getenv("XDG_STATE_HOME")) {
        return std::filesystem::path{xdg} / "CoupeCAD" / "log";
    }
    return home_directory() / ".local" / "state" / "CoupeCAD" / "log";
#endif
}

std::filesystem::path default_log_file_path() {
    return default_log_directory() / "coupecad.log";
}

}  // namespace coupecad::logging
