#pragma once

#include <filesystem>

namespace coupecad::logging {

// Возвращает каталог для лог-файлов, специфичный для платформы:
//   Linux:   $XDG_STATE_HOME/CoupeCAD/log  (фолбэк ~/.local/state/CoupeCAD/log)
//   macOS:   $HOME/Library/Logs/CoupeCAD
//   Windows: %LOCALAPPDATA%/CoupeCAD/log
// Каталог НЕ создаётся; вызывающий должен создать его перед записью.
std::filesystem::path default_log_directory();

// Полный путь к лог-файлу по умолчанию (директория + имя файла).
// Имя файла фиксированное — `coupecad.log`; ротация управляется sink'ом.
std::filesystem::path default_log_file_path();

}  // namespace coupecad::logging
