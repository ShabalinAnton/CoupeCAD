# Stage 1a — Logging + Core Entities + Read-only Geometry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Поднять модуль логирования `coupecad_logging`, доменные сущности Core (`Project / Cabinet / Panel / Material / Hardware` плюс типы-фундаменты) и чистую read-only функцию вычисления геометрии. Без команд/undo (Stage 1b) и без `.ccad` I/O (Stage 1c).

**Architecture:** Два новых статических CMake-таргета: `coupecad_logging` (тонкая обёртка над `spdlog` с уровнями, console+rotating file sinks, per-category overrides) и `coupecad_core` (data structures + UUID/units helpers + read-only `compute_panel_geometry`). Оба Qt-free, оба линкуются в тестовые бинарники. Core PUBLIC-зависит от `coupecad_logging` (любой пользователь Core может писать в общий лог).

**Tech Stack:** C++20, CMake 3.25+, Conan 2; новые зависимости: `spdlog/1.13.0` (логирование), `fmt/10.2.1` (publik в API logger'а), `stduuid/1.2.3` (UUID для id.h). GoogleTest как раньше. Qt **не используется**.

**Definition of Done.**
- `cmake --build --preset default && ctest --preset default` локально и в CI зелёный, оба новых таргета собраны.
- Все unit-тесты `tests/logging/...` и `tests/core/...` проходят.
- Логгер пишет в console и в `~/Library/Logs/CoupeCAD/coupecad-YYYY-MM-DD.log` (на macOS) при дефолтных настройках; настройки переопределяются программно.
- Можно создать `Project`, заполнить его `Cabinet` с панелями и фурнитурой через прямые сеттеры (без команд — Stage 1b), вычислить `compute_panel_geometry` для каждой роли и получить корректные размеры/координаты.

---

## File Structure

После Stage 1a в репо появятся (новые файлы помечены `+`):

```
conanfile.py                                  M  (добавляются deps)
CMakeLists.txt                                M  (add_subdirectory(src))
src/                                          +
└── CMakeLists.txt                            +  (агрегатор)
src/coupecad/                                 +
├── logging/                                  +
│   ├── CMakeLists.txt                        +
│   ├── logger.h                              +  (публичный API logger'а)
│   ├── logger.cpp                            +  (реализация поверх spdlog)
│   ├── log_level.h                           +  (enum Level)
│   └── log_paths.h/.cpp                      +  (платформенные пути для лог-файла)
└── core/                                     +
    ├── CMakeLists.txt                        +
    ├── id.h                                  +  (strong-type wrapper'ы UUID + UuidGenerator)
    ├── units.h                               +  (Millimeters, Money, RGBA, Vec3, Quat, Dimensions)
    ├── errors.h                              +  (CoupecadException hierarchy)
    ├── material.h, material.cpp              +
    ├── hardware.h, hardware.cpp              +
    ├── panel.h, panel.cpp                    +
    ├── cabinet.h, cabinet.cpp                +
    ├── project.h, project.cpp                +  (включая IProjectObserver, ChangeSet)
    └── geometry.h, geometry.cpp              +  (compute_panel_geometry)

tests/                                        M  (CMakeLists добавляет new subdirs)
├── CMakeLists.txt                            M
├── logging/                                  +
│   ├── CMakeLists.txt                        +
│   ├── logger_levels_test.cpp                +
│   ├── logger_sinks_test.cpp                 +
│   ├── logger_categories_test.cpp            +
│   ├── logger_file_rotation_test.cpp         +
│   └── logger_concurrent_test.cpp            +
└── core/                                     +
    ├── CMakeLists.txt                        +
    ├── id_test.cpp                           +
    ├── units_test.cpp                        +
    ├── material_test.cpp                     +
    ├── hardware_test.cpp                     +
    ├── panel_test.cpp                        +
    ├── cabinet_test.cpp                      +
    ├── project_test.cpp                      +
    └── geometry_test.cpp                     +
```

**Ответственности.** `logging/` — кросс-резная инфраструктура, без знания доменных типов. `core/` — данные и read-only функции, без знания о Qt/UI/OpenCASCADE. Каждый `.h/.cpp` файл — одна ответственность.

---

## Task 1: Conan deps + src/-агрегатор + регистрация двух новых таргетов

**Files:**
- Modify: `conanfile.py`
- Create: `src/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root)

- [ ] **Step 1: Расширить `conanfile.py` — добавить spdlog, fmt, stduuid.**

Переписать целиком (на месте):

```python
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class CoupeCADConan(ConanFile):
    name = "coupecad"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    # Stage 0: только GoogleTest. Qt подключается извне (aqtinstall/install-qt-action).
    # Stage 1a: добавляются spdlog (логирование), fmt (форматирование),
    # stduuid (UUID для id.h).
    # OpenCASCADE добавится в Stage 2 (Geometry layer).
    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("fmt/10.2.1")
        self.requires("stduuid/1.2.3")
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        self.folders.build = "build/default"
        self.folders.generators = "build/default"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()
```

- [ ] **Step 2: Создать `src/CMakeLists.txt` (агрегатор поддиректорий).**

```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
```

- [ ] **Step 3: В корневом `CMakeLists.txt` подключить `src/`.**

В корневой `CMakeLists.txt` найти строку `add_subdirectory(apps/coupecad)` и **перед** ней вставить:

```cmake
add_subdirectory(src)
```

После правки начало списка subdirectories должно выглядеть так:

```cmake
add_subdirectory(src)
add_subdirectory(apps/coupecad)

if(COUPECAD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 4: Создать пустые `src/coupecad/logging/CMakeLists.txt` и `src/coupecad/core/CMakeLists.txt` чтобы CMake configure не падал.**

```sh
mkdir -p src/coupecad/logging src/coupecad/core
printf '# Implemented in Stage 1a Task 2.\n' > src/coupecad/logging/CMakeLists.txt
printf '# Implemented in Stage 1a Task 5.\n' > src/coupecad/core/CMakeLists.txt
```

- [ ] **Step 5: Прогнать `conan install`, убедиться что всё подтягивается.**

```sh
conan install . --build=missing -s build_type=Debug
```

Ожидается: команда успешно скачивает / собирает `spdlog`, `fmt`, `stduuid` (большая часть — прекомпилированы в Conan Center на нашем профиле macOS+apple-clang17). Должна закончиться `Install finished successfully`.

> **Если профиль macOS не имеет прекомпилированного бинаря spdlog/fmt** — Conan соберёт их из исходников за минуты (это маленькие библиотеки, не Qt). Ничего страшного.

- [ ] **Step 6: Проверить что cmake configure проходит.**

```sh
export Qt6_DIR=$HOME/Qt/6.7.3/macos/lib/cmake/Qt6
cmake --preset default
```

Ожидается: configure успешен (поскольку `tests/` ещё не знает про новые подкаталоги, апs не сломан, модули пустые — CMake просто пройдёт через них без таргетов).

- [ ] **Step 7: Закоммитить.**

```sh
git add conanfile.py CMakeLists.txt src/
git commit -m "build: add spdlog/fmt/stduuid deps + src/ aggregator for Stage 1a"
```

---

## Task 2: `coupecad_logging` — Level enum + Logger skeleton с одним работающим тестом

**Files:**
- Create: `src/coupecad/logging/log_level.h`
- Create: `src/coupecad/logging/logger.h`
- Create: `src/coupecad/logging/logger.cpp`
- Create: `src/coupecad/logging/CMakeLists.txt` (полное содержимое)
- Create: `tests/logging/CMakeLists.txt`
- Create: `tests/logging/logger_levels_test.cpp` (только первый тест)
- Modify: `tests/CMakeLists.txt` — добавить `add_subdirectory(logging)`

- [ ] **Step 1: Создать `src/coupecad/logging/log_level.h`.**

```cpp
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
```

- [ ] **Step 2: Создать `src/coupecad/logging/logger.h`.**

```cpp
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
```

- [ ] **Step 3: Создать `src/coupecad/logging/logger.cpp` (минимальный — только Level фильтр и заглушка sinks).**

```cpp
#include "coupecad/logging/logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

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
    std::filesystem::path file_path;  // заполнится в Task 4
    std::unordered_map<std::string, Level> category_levels;
    std::shared_ptr<spdlog::logger> spd;  // создаётся в rebuild()

    void rebuild() {
        std::vector<spdlog::sink_ptr> sinks;
        if (console_on) {
            sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
        }
        // File sink будет добавлен в Task 4.
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
    std::lock_guard lk(impl_->mu);
    impl_->spd->log(
        to_spd(level),
        "{:<14} {}:{}  {}",
        category,
        loc.file_name(),
        loc.line(),
        message);
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
```

- [ ] **Step 4: Заменить `src/coupecad/logging/CMakeLists.txt` на полное содержимое.**

```cmake
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_logging STATIC
    logger.cpp
)

target_include_directories(coupecad_logging
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_logging
    PUBLIC
        fmt::fmt
    PRIVATE
        spdlog::spdlog
)

target_compile_features(coupecad_logging PUBLIC cxx_std_20)
```

- [ ] **Step 5: Создать падающий тест `tests/logging/logger_levels_test.cpp`.**

```cpp
#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override { Logger::reset_for_test(); }
    void TearDown() override { Logger::reset_for_test(); }
};

TEST_F(LoggerTest, DefaultMinLevelIsInfo) {
    EXPECT_EQ(Logger::instance().min_level(), Level::Info);
}

TEST_F(LoggerTest, SetAndGetMinLevel) {
    Logger::instance().set_min_level(Level::Warning);
    EXPECT_EQ(Logger::instance().min_level(), Level::Warning);
}

TEST_F(LoggerTest, OffLevelStartsDisabled_AfterSet) {
    Logger::instance().set_min_level(Level::Off);
    EXPECT_EQ(Logger::instance().min_level(), Level::Off);
}
```

- [ ] **Step 6: Создать `tests/logging/CMakeLists.txt`.**

```cmake
add_executable(coupecad_logger_test
    logger_levels_test.cpp
)

target_link_libraries(coupecad_logger_test
    PRIVATE
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_logger_test)
```

- [ ] **Step 7: Подключить `tests/logging/` в `tests/CMakeLists.txt`.**

Заменить файл целиком на:

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
```

- [ ] **Step 8: Сконфигурировать, собрать тест, убедиться что все три кейса зелёные.**

```sh
export Qt6_DIR=$HOME/Qt/6.7.3/macos/lib/cmake/Qt6
cmake --preset default
cmake --build --preset default --target coupecad_logger_test
ctest --preset default -R LoggerTest --output-on-failure
```

Ожидается: `100% tests passed, 0 tests failed out of 3`.

- [ ] **Step 9: Закоммитить.**

```sh
git add src/coupecad/logging/ tests/logging/ tests/CMakeLists.txt
git commit -m "feat(logging): coupecad_logging skeleton with Level + min_level API"
```

---

## Task 3: `coupecad_logging` — file sink с rotating и платформенными путями

**Files:**
- Create: `src/coupecad/logging/log_paths.h`
- Create: `src/coupecad/logging/log_paths.cpp`
- Modify: `src/coupecad/logging/logger.cpp` (добавить file sink в `rebuild()`, дефолтный путь в конструкторе)
- Modify: `src/coupecad/logging/CMakeLists.txt` (добавить `log_paths.cpp`)
- Create: `tests/logging/logger_sinks_test.cpp`
- Create: `tests/logging/logger_file_rotation_test.cpp`
- Modify: `tests/logging/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/logging/log_paths.h`.**

```cpp
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
```

- [ ] **Step 2: Создать `src/coupecad/logging/log_paths.cpp`.**

```cpp
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
```

- [ ] **Step 3: Доработать `src/coupecad/logging/logger.cpp`** — заполнить дефолтный `file_path` в конструкторе и подключить file sink в `rebuild()`.

В верх файла добавить include:

```cpp
#include "coupecad/logging/log_paths.h"
#include <spdlog/sinks/rotating_file_sink.h>
```

В функции `Impl::rebuild()` (заменить целиком) — добавить file sink:

```cpp
    void rebuild() {
        std::vector<spdlog::sink_ptr> sinks;
        if (console_on) {
            sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
        }
        if (file_on && !file_path.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(file_path.parent_path(), ec);
            // 10 MB на файл, держим последние 7 файлов.
            constexpr std::size_t kMaxFileBytes = 10ull * 1024 * 1024;
            constexpr std::size_t kMaxFiles = 7;
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_path.string(), kMaxFileBytes, kMaxFiles));
        }
        spd = std::make_shared<spdlog::logger>(
            "coupecad", sinks.begin(), sinks.end());
        spd->set_level(to_spd(min_level));
        spd->set_pattern("%Y-%m-%dT%H:%M:%S.%e%z [%^%l%$] %v");
        spd->flush_on(spdlog::level::warn);
    }
```

В конструкторе `Logger::Logger()` (полностью заменить) — заполнить `file_path` дефолтом:

```cpp
Logger::Logger() : impl_(new Impl()) {
    impl_->file_path = default_log_file_path();
    impl_->rebuild();
}
```

В `Logger::reset_for_test()` (полностью заменить) — сбросить путь к дефолту, иначе тесты будут писать в файл из предыдущего теста:

```cpp
void Logger::reset_for_test() {
    auto& inst = instance();
    std::lock_guard lk(inst.impl_->mu);
    inst.impl_->min_level = Level::Info;
    inst.impl_->console_on = true;
    inst.impl_->file_on = true;
    inst.impl_->file_path = default_log_file_path();
    inst.impl_->category_levels.clear();
    inst.impl_->rebuild();
}
```

- [ ] **Step 4: Обновить `src/coupecad/logging/CMakeLists.txt` — добавить `log_paths.cpp`.**

```cmake
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_logging STATIC
    logger.cpp
    log_paths.cpp
)

target_include_directories(coupecad_logging
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_logging
    PUBLIC
        fmt::fmt
    PRIVATE
        spdlog::spdlog
)

target_compile_features(coupecad_logging PUBLIC cxx_std_20)
```

- [ ] **Step 5: Создать `tests/logging/logger_sinks_test.cpp`.**

```cpp
#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>

using coupecad::logging::Logger;

class LoggerSinksTest : public ::testing::Test {
protected:
    void SetUp() override { Logger::reset_for_test(); }
    void TearDown() override { Logger::reset_for_test(); }
};

TEST_F(LoggerSinksTest, ConsoleAndFileEnabledByDefault) {
    EXPECT_TRUE(Logger::instance().console_enabled());
    EXPECT_TRUE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, DisableConsole) {
    Logger::instance().enable_console(false);
    EXPECT_FALSE(Logger::instance().console_enabled());
    EXPECT_TRUE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, DisableFile) {
    Logger::instance().enable_file(false);
    EXPECT_TRUE(Logger::instance().console_enabled());
    EXPECT_FALSE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, SetCustomLogFilePath) {
    auto custom = std::filesystem::temp_directory_path() / "coupecad-test.log";
    Logger::instance().set_log_file_path(custom);
    EXPECT_EQ(Logger::instance().log_file_path(), custom);
}
```

- [ ] **Step 6: Создать `tests/logging/logger_file_rotation_test.cpp`.**

```cpp
#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_dir_ = std::filesystem::temp_directory_path() /
                   "coupecad_logger_file_test";
        std::filesystem::remove_all(tmp_dir_);
        std::filesystem::create_directories(tmp_dir_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove_all(tmp_dir_);
    }

    std::filesystem::path tmp_dir_;
};

TEST_F(LoggerFileTest, WritesMessageToFile) {
    auto log_file = tmp_dir_ / "out.log";
    Logger::instance().enable_console(false);
    Logger::instance().set_log_file_path(log_file);
    Logger::instance().info("test", "hello {}", 42);

    // spdlog rotating sink буферизует — заставляем сбросить через
    // выключение/включение файла (rebuild пересоздаст logger и сбросит).
    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    ASSERT_TRUE(std::filesystem::exists(log_file));
    std::ifstream in(log_file);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("hello 42"), std::string::npos)
        << "File contents: " << contents;
    EXPECT_NE(contents.find("INFO"), std::string::npos);
    EXPECT_NE(contents.find("test"), std::string::npos);
}

TEST_F(LoggerFileTest, RespectsMinLevel) {
    auto log_file = tmp_dir_ / "out.log";
    Logger::instance().enable_console(false);
    Logger::instance().set_log_file_path(log_file);
    Logger::instance().set_min_level(Level::Warning);

    Logger::instance().info("test", "should be filtered");
    Logger::instance().warn("test", "should be present");

    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    std::ifstream in(log_file);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(contents.find("should be filtered"), std::string::npos);
    EXPECT_NE(contents.find("should be present"), std::string::npos);
}
```

- [ ] **Step 7: Обновить `tests/logging/CMakeLists.txt` — добавить новые файлы.**

```cmake
add_executable(coupecad_logger_test
    logger_levels_test.cpp
    logger_sinks_test.cpp
    logger_file_rotation_test.cpp
)

target_link_libraries(coupecad_logger_test
    PRIVATE
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_logger_test)
```

- [ ] **Step 8: Собрать и прогнать все тесты.**

```sh
cmake --build --preset default --target coupecad_logger_test
ctest --preset default -R "Logger" --output-on-failure
```

Ожидается: 9 тестов passed (3 levels + 4 sinks + 2 file).

- [ ] **Step 9: Закоммитить.**

```sh
git add src/coupecad/logging/ tests/logging/
git commit -m "feat(logging): rotating file sink with platform-specific default path"
```

---

## Task 4: `coupecad_logging` — per-category overrides + concurrency test

**Files:**
- Create: `tests/logging/logger_categories_test.cpp`
- Create: `tests/logging/logger_concurrent_test.cpp`
- Modify: `tests/logging/CMakeLists.txt`

`Logger::set_category_level/clear_category_level/is_enabled` уже реализованы в Task 2 — здесь только тесты, чтобы зафиксировать поведение.

- [ ] **Step 1: Создать `tests/logging/logger_categories_test.cpp`.**

```cpp
#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerCategoriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_file_ = std::filesystem::temp_directory_path() /
                    "coupecad_logger_categories.log";
        std::filesystem::remove(tmp_file_);
        Logger::instance().enable_console(false);
        Logger::instance().set_log_file_path(tmp_file_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove(tmp_file_);
    }

    std::string read_log() {
        Logger::instance().enable_file(false);
        Logger::instance().enable_file(true);
        std::ifstream in(tmp_file_);
        return {std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>()};
    }

    std::filesystem::path tmp_file_;
};

TEST_F(LoggerCategoriesTest, CategoryLevelOverridesGlobal_LessVerbose) {
    Logger::instance().set_min_level(Level::Trace);
    Logger::instance().set_category_level("noisy", Level::Error);

    Logger::instance().info("regular", "regular-info");
    Logger::instance().info("noisy",   "noisy-info-should-be-suppressed");
    Logger::instance().error("noisy",  "noisy-error-should-pass");

    auto contents = read_log();
    EXPECT_NE(contents.find("regular-info"), std::string::npos);
    EXPECT_EQ(contents.find("noisy-info-should-be-suppressed"), std::string::npos);
    EXPECT_NE(contents.find("noisy-error-should-pass"), std::string::npos);
}

TEST_F(LoggerCategoriesTest, CategoryLevelOverridesGlobal_MoreVerbose) {
    Logger::instance().set_min_level(Level::Warning);
    Logger::instance().set_category_level("loud", Level::Debug);

    Logger::instance().debug("loud",    "loud-debug-should-pass");
    Logger::instance().debug("regular", "regular-debug-should-be-suppressed");

    auto contents = read_log();
    EXPECT_NE(contents.find("loud-debug-should-pass"), std::string::npos);
    EXPECT_EQ(contents.find("regular-debug-should-be-suppressed"), std::string::npos);
}

TEST_F(LoggerCategoriesTest, ClearCategoryLevelRestoresGlobal) {
    Logger::instance().set_min_level(Level::Warning);
    Logger::instance().set_category_level("temp", Level::Debug);
    Logger::instance().clear_category_level("temp");

    Logger::instance().debug("temp", "should-be-suppressed-after-clear");
    auto contents = read_log();
    EXPECT_EQ(contents.find("should-be-suppressed-after-clear"), std::string::npos);
}
```

- [ ] **Step 2: Создать `tests/logging/logger_concurrent_test.cpp`.**

```cpp
#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using coupecad::logging::Logger;

class LoggerConcurrentTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_file_ = std::filesystem::temp_directory_path() /
                    "coupecad_logger_concurrent.log";
        std::filesystem::remove(tmp_file_);
        Logger::instance().enable_console(false);
        Logger::instance().set_log_file_path(tmp_file_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove(tmp_file_);
    }

    std::filesystem::path tmp_file_;
};

TEST_F(LoggerConcurrentTest, EightThreadsThousandLinesEach) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 1000;

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([t]() {
            for (int i = 0; i < kPerThread; ++i) {
                Logger::instance().info("conc",
                                        "thread={} index={}",
                                        t,
                                        i);
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }

    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    std::ifstream in(tmp_file_);
    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        if (line.find("thread=") != std::string::npos) {
            ++count;
        }
    }
    EXPECT_EQ(count, kThreads * kPerThread);
}
```

- [ ] **Step 3: Обновить `tests/logging/CMakeLists.txt`.**

```cmake
add_executable(coupecad_logger_test
    logger_levels_test.cpp
    logger_sinks_test.cpp
    logger_file_rotation_test.cpp
    logger_categories_test.cpp
    logger_concurrent_test.cpp
)

target_link_libraries(coupecad_logger_test
    PRIVATE
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_logger_test)
```

- [ ] **Step 4: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_logger_test
ctest --preset default -R "Logger" --output-on-failure
```

Ожидается: 13 тестов passed (3 + 4 + 2 + 3 + 1).

- [ ] **Step 5: Закоммитить.**

```sh
git add tests/logging/
git commit -m "test(logging): per-category overrides + 8-thread concurrency"
```

---

## Task 5: `coupecad_core` — `units.h` (Millimeters, Vec3, Quat, Money, RGBA, Dimensions)

**Files:**
- Create: `src/coupecad/core/units.h`
- Create: `src/coupecad/core/CMakeLists.txt` (полное содержимое — пока только заголовки, без `.cpp`)
- Create: `tests/core/CMakeLists.txt`
- Create: `tests/core/units_test.cpp`
- Modify: `tests/CMakeLists.txt` (добавить `add_subdirectory(core)`)

- [ ] **Step 1: Создать `src/coupecad/core/units.h`.**

```cpp
#pragma once

#include <cstdint>
#include <compare>
#include <cstddef>

namespace coupecad::core {

// Strong-typed целочисленный миллиметр (см. spec §2.3).
// Значения хранятся как int32_t — диапазон ±2.1·10^9 мм заведомо
// больше любых разумных размеров мебели.
class Millimeters {
public:
    constexpr Millimeters() = default;
    constexpr explicit Millimeters(std::int32_t value) noexcept : v_(value) {}

    constexpr std::int32_t value() const noexcept { return v_; }

    constexpr auto operator<=>(const Millimeters&) const = default;

    constexpr Millimeters operator+(Millimeters rhs) const noexcept {
        return Millimeters{v_ + rhs.v_};
    }
    constexpr Millimeters operator-(Millimeters rhs) const noexcept {
        return Millimeters{v_ - rhs.v_};
    }
    constexpr Millimeters& operator+=(Millimeters rhs) noexcept {
        v_ += rhs.v_;
        return *this;
    }
    constexpr Millimeters& operator-=(Millimeters rhs) noexcept {
        v_ -= rhs.v_;
        return *this;
    }
    constexpr Millimeters operator-() const noexcept {
        return Millimeters{-v_};
    }
    constexpr Millimeters operator*(std::int32_t k) const noexcept {
        return Millimeters{v_ * k};
    }

private:
    std::int32_t v_ = 0;
};

constexpr Millimeters operator*(std::int32_t k, Millimeters m) noexcept {
    return m * k;
}

// Удобный литерал: 1500_mm
constexpr Millimeters operator""_mm(unsigned long long v) {
    return Millimeters{static_cast<std::int32_t>(v)};
}

// Внешние габариты шкафа (см. spec §2.1, §2.3).
struct Dimensions {
    Millimeters width{};
    Millimeters depth{};
    Millimeters height{};

    constexpr bool operator==(const Dimensions&) const = default;
};

// 3D-вектор для local_position и Custom-панелей. Целочисленный мм.
struct Vec3 {
    Millimeters x{};
    Millimeters y{};
    Millimeters z{};

    constexpr bool operator==(const Vec3&) const = default;
};

// Кватернион для ориентации (Custom + HardwareItem). w-first, double.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr bool operator==(const Quat&) const = default;
    static constexpr Quat identity() noexcept { return {1.0, 0.0, 0.0, 0.0}; }
};

// Деньги — целочисленный «мини-юнит» (копейки/центы), валюта отдельно.
// В v1 валюта — фиксированная RUB; полноценная многовалютность позже.
struct Money {
    std::int64_t minor_units = 0;     // копейки
    constexpr bool operator==(const Money&) const = default;
};

// Цвет в формате sRGB + альфа.
struct RGBA {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
    constexpr bool operator==(const RGBA&) const = default;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/CMakeLists.txt`.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    # .cpp-файлы добавляются по мере появления в следующих задачах.
    # Пока target существует только ради публичных заголовков.
    dummy.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
    PRIVATE
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

CMake требует хотя бы один source-файл в STATIC library. Создадим dummy:

```sh
printf '// Placeholder until real .cpp files arrive in Stage 1a Task 6+.\n' \
    > src/coupecad/core/dummy.cpp
```

(Удалим dummy.cpp в Task 6, когда появится первый реальный .cpp.)

- [ ] **Step 3: Создать `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 4: Создать `tests/core/units_test.cpp`.**

```cpp
#include "coupecad/core/units.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(Millimeters, DefaultIsZero) {
    EXPECT_EQ(Millimeters{}.value(), 0);
}

TEST(Millimeters, ConstructFromInt) {
    EXPECT_EQ(Millimeters{1500}.value(), 1500);
}

TEST(Millimeters, Comparison) {
    EXPECT_TRUE(Millimeters{100} < Millimeters{200});
    EXPECT_TRUE(Millimeters{200} > Millimeters{100});
    EXPECT_TRUE(Millimeters{100} == Millimeters{100});
}

TEST(Millimeters, ArithmeticAddSub) {
    EXPECT_EQ((Millimeters{100} + Millimeters{50}).value(), 150);
    EXPECT_EQ((Millimeters{100} - Millimeters{30}).value(), 70);
    auto m = Millimeters{100};
    m += Millimeters{5};
    EXPECT_EQ(m.value(), 105);
    m -= Millimeters{10};
    EXPECT_EQ(m.value(), 95);
}

TEST(Millimeters, MultiplyByScalar) {
    EXPECT_EQ((Millimeters{16} * 3).value(), 48);
    EXPECT_EQ((3 * Millimeters{16}).value(), 48);
}

TEST(Millimeters, Negation) {
    EXPECT_EQ((-Millimeters{50}).value(), -50);
}

TEST(Millimeters, UDLLiteral) {
    using namespace coupecad::core;
    EXPECT_EQ((1500_mm).value(), 1500);
}

TEST(Dimensions, EqualityValueSemantics) {
    Dimensions a{.width = Millimeters{2400}, .depth = Millimeters{600},
                 .height = Millimeters{2400}};
    Dimensions b = a;
    EXPECT_EQ(a, b);
    b.width = Millimeters{2500};
    EXPECT_NE(a, b);
}

TEST(Vec3, Equality) {
    Vec3 a{Millimeters{1}, Millimeters{2}, Millimeters{3}};
    Vec3 b{Millimeters{1}, Millimeters{2}, Millimeters{3}};
    EXPECT_EQ(a, b);
}

TEST(Quat, IdentityIsWaxis) {
    auto q = Quat::identity();
    EXPECT_DOUBLE_EQ(q.w, 1.0);
    EXPECT_DOUBLE_EQ(q.x, 0.0);
    EXPECT_DOUBLE_EQ(q.y, 0.0);
    EXPECT_DOUBLE_EQ(q.z, 0.0);
}

TEST(Money, MinorUnits) {
    Money m{.minor_units = 12345};
    EXPECT_EQ(m.minor_units, 12345);
}

TEST(RGBA, DefaultIsBlackOpaque) {
    RGBA c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}
```

- [ ] **Step 5: Подключить `tests/core/` в `tests/CMakeLists.txt`.**

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
```

- [ ] **Step 6: Собрать и прогнать unit-тесты.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Millimeters|Dimensions|Vec3|Quat|Money|RGBA" --output-on-failure
```

Ожидается: 12 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/ tests/core/ tests/CMakeLists.txt
git commit -m "feat(core): units.h with Millimeters, Vec3, Quat, Dimensions, Money, RGBA"
```

---

## Task 6: `coupecad_core` — `errors.h` (exception hierarchy) + `id.h` (UuidGenerator + strong-typed IDs)

**Files:**
- Create: `src/coupecad/core/errors.h`
- Create: `src/coupecad/core/id.h`
- Create: `src/coupecad/core/id.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt` (заменить `dummy.cpp` на `id.cpp`)
- Delete: `src/coupecad/core/dummy.cpp`
- Create: `tests/core/id_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/errors.h`.**

```cpp
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace coupecad::core {

// Базовый класс всех специфичных для CoupeCAD исключений (см. spec §3.4).
// what() — английский human-readable текст (UI делает локализацию по
// machine-readable code).
class CoupecadException : public std::runtime_error {
public:
    CoupecadException(std::string_view code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}
    const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

// Доменные нарушения инвариантов (отрицательные размеры, материал
// в использовании, hardware к несуществующему panel id, и т.п.).
class DomainError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

// Программные ошибки — нарушение протокола использования API (preview
// внутри макроса, execute во время preview). Stage 1b расширит.
class LogicError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

// Ошибки чтения/записи .ccad — Stage 1c.
class FileFormatError : public CoupecadException {
public:
    using CoupecadException::CoupecadException;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/id.h`.**

```cpp
#pragma once

#include <uuid.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace coupecad::core {

// Базовый wrapper. Не используется напрямую; используются строго
// типизированные алиасы (PanelId, CabinetId и т.п.) ниже через
// explicit-наследование.
template <class Tag>
class Id {
public:
    Id() = default;
    explicit Id(uuids::uuid u) noexcept : u_(u) {}

    static Id from_string(std::string_view s) {
        if (auto parsed = uuids::uuid::from_string(s)) {
            return Id{*parsed};
        }
        return Id{};   // invalid; callers should validate
    }

    bool is_valid() const noexcept { return !u_.is_nil(); }
    const uuids::uuid& raw() const noexcept { return u_; }
    std::string to_string() const { return uuids::to_string(u_); }

    // stduuid даёт ==, !=, <, но не <=> — поэтому пишем вручную.
    bool operator==(const Id& other) const noexcept { return u_ == other.u_; }
    bool operator!=(const Id& other) const noexcept { return u_ != other.u_; }
    bool operator<(const Id& other) const noexcept { return u_ < other.u_; }

private:
    uuids::uuid u_;
};

}  // namespace coupecad::core

// Hash support — нужно для использования Id в std::unordered_map.
namespace std {
template <class Tag>
struct hash<coupecad::core::Id<Tag>> {
    std::size_t operator()(const coupecad::core::Id<Tag>& id) const noexcept {
        return std::hash<uuids::uuid>{}(id.raw());
    }
};
}  // namespace std

namespace coupecad::core {

// Tag-структуры для Id<Tag>. Каждый Tag даёт независимый тип.
struct CabinetIdTag {};
struct PanelIdTag {};
struct MaterialIdTag {};
struct HardwareItemIdTag {};

using CabinetId      = Id<CabinetIdTag>;
using PanelId        = Id<PanelIdTag>;
using MaterialId     = Id<MaterialIdTag>;
using HardwareItemId = Id<HardwareItemIdTag>;

// HardwareRef — НЕ UUID, а строковый ключ карточки в каталоге
// (например "hinge.generic.straight"). Тоже sterk-typed для безопасности.
class HardwareRef {
public:
    HardwareRef() = default;
    explicit HardwareRef(std::string s) noexcept : s_(std::move(s)) {}
    const std::string& value() const noexcept { return s_; }
    bool operator==(const HardwareRef&) const = default;
    auto operator<=>(const HardwareRef&) const = default;

private:
    std::string s_;
};

// Интерфейс генератора UUID. Real implementation использует random;
// тесты используют seeded для воспроизводимости.
class UuidGenerator {
public:
    virtual ~UuidGenerator() = default;
    virtual uuids::uuid next() = 0;

    template <class Tag>
    Id<Tag> next_id() {
        return Id<Tag>{next()};
    }
};

// Production-default: cryptographically random.
std::unique_ptr<UuidGenerator> make_random_uuid_generator();

// Seeded: воспроизводимая последовательность для тестов.
std::unique_ptr<UuidGenerator> make_seeded_uuid_generator(std::uint64_t seed);

}  // namespace coupecad::core

namespace std {
template <>
struct hash<coupecad::core::HardwareRef> {
    std::size_t operator()(const coupecad::core::HardwareRef& r) const noexcept {
        return std::hash<std::string>{}(r.value());
    }
};
}  // namespace std
```

- [ ] **Step 3: Создать `src/coupecad/core/id.cpp` (реализации генераторов).**

```cpp
#include "coupecad/core/id.h"

#include <uuid.h>

#include <random>

namespace coupecad::core {

namespace {

class RandomUuidGenerator : public UuidGenerator {
public:
    RandomUuidGenerator()
        : rng_(std::random_device{}()),
          gen_(&rng_) {}

    uuids::uuid next() override { return gen_(); }

private:
    std::mt19937 rng_;
    uuids::uuid_random_generator gen_;
};

class SeededUuidGenerator : public UuidGenerator {
public:
    explicit SeededUuidGenerator(std::uint64_t seed)
        : rng_(seed), gen_(&rng_) {}

    uuids::uuid next() override { return gen_(); }

private:
    std::mt19937 rng_;
    uuids::uuid_random_generator gen_;
};

}  // namespace

std::unique_ptr<UuidGenerator> make_random_uuid_generator() {
    return std::make_unique<RandomUuidGenerator>();
}

std::unique_ptr<UuidGenerator> make_seeded_uuid_generator(std::uint64_t seed) {
    return std::make_unique<SeededUuidGenerator>(seed);
}

}  // namespace coupecad::core
```

- [ ] **Step 4: Заменить `src/coupecad/core/CMakeLists.txt` — убрать dummy, добавить id.cpp.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
    PRIVATE
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

(Изменилось: `stduuid` поднят в PUBLIC, поскольку `id.h` публично использует тип `uuids::uuid`. `dummy.cpp` удалён из списка.)

- [ ] **Step 5: Удалить `src/coupecad/core/dummy.cpp`.**

```sh
rm src/coupecad/core/dummy.cpp
```

- [ ] **Step 6: Создать `tests/core/id_test.cpp`.**

```cpp
#include "coupecad/core/id.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

#include <set>
#include <unordered_set>

using namespace coupecad::core;

TEST(IdTypes, AreDistinct) {
    // PanelId и CabinetId — разные типы, не присваиваются друг другу.
    static_assert(!std::is_assignable_v<PanelId&, CabinetId>);
    static_assert(!std::is_assignable_v<CabinetId&, MaterialId>);
}

TEST(Id, DefaultConstructedIsInvalid) {
    PanelId p;
    EXPECT_FALSE(p.is_valid());
}

TEST(Id, RoundTripStringFormat) {
    auto gen = make_seeded_uuid_generator(42);
    PanelId id{gen->next()};
    auto str = id.to_string();
    EXPECT_EQ(str.size(), 36);  // canonical UUID length
    auto parsed = PanelId::from_string(str);
    EXPECT_EQ(id, parsed);
}

TEST(Id, FromInvalidString) {
    auto bad = PanelId::from_string("not-a-uuid");
    EXPECT_FALSE(bad.is_valid());
}

TEST(Id, UseInUnorderedMap) {
    std::unordered_set<PanelId> seen;
    auto gen = make_seeded_uuid_generator(7);
    for (int i = 0; i < 10; ++i) {
        seen.insert(PanelId{gen->next()});
    }
    EXPECT_EQ(seen.size(), 10u);
}

TEST(SeededUuidGenerator, DeterministicSequenceForSameSeed) {
    auto a = make_seeded_uuid_generator(123);
    auto b = make_seeded_uuid_generator(123);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(a->next(), b->next());
    }
}

TEST(SeededUuidGenerator, DifferentSeedsProduceDifferentSequences) {
    auto a = make_seeded_uuid_generator(1);
    auto b = make_seeded_uuid_generator(2);
    EXPECT_NE(a->next(), b->next());
}

TEST(RandomUuidGenerator, ProducesDifferentValues) {
    auto gen = make_random_uuid_generator();
    auto x = gen->next();
    auto y = gen->next();
    EXPECT_NE(x, y);
}

TEST(HardwareRef, ConstructionAndEquality) {
    HardwareRef a{"hinge.generic.straight"};
    HardwareRef b{"hinge.generic.straight"};
    EXPECT_EQ(a, b);
    HardwareRef c{"slide.generic"};
    EXPECT_NE(a, c);
}

TEST(CoupecadException, CarriesCodeAndMessage) {
    DomainError e{"panel.invalid_role", "role/role_params mismatch"};
    EXPECT_STREQ(e.what(), "role/role_params mismatch");
    EXPECT_EQ(e.code(), "panel.invalid_role");
}
```

- [ ] **Step 7: Обновить `tests/core/CMakeLists.txt` — добавить id_test.cpp.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 8: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Id|UuidGenerator|HardwareRef|CoupecadException" --output-on-failure
```

Ожидается: 10 тестов passed.

- [ ] **Step 9: Закоммитить.**

```sh
git add src/coupecad/core/ tests/core/
git commit -m "feat(core): id.h with strong-typed UUIDs + UuidGenerator, errors.h hierarchy"
```

---

## Task 7: `coupecad_core` — `Material` (struct + invariants + tests)

**Files:**
- Create: `src/coupecad/core/material.h`
- Create: `src/coupecad/core/material.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/material_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/material.h`.**

```cpp
#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>

namespace coupecad::core {

enum class MaterialKind {
    ChipboardLaminated,  // ЛДСП
    Mdf,
    Hdf,
    Plywood,             // фанера
    SolidWood,           // массив дерева
    Glass,
    Metal,
    Other,
};

const char* material_kind_name(MaterialKind kind) noexcept;

// Plain-data материал. Инварианты валидируются Material::validate().
struct Material {
    MaterialId id;
    std::string name;
    MaterialKind kind = MaterialKind::ChipboardLaminated;
    Millimeters default_thickness{16};
    RGBA color_hint{};
    std::optional<std::string> texture_ref;
    std::optional<Money> price_per_sqm;

    bool operator==(const Material&) const = default;

    // Кидает DomainError при нарушении (см. spec §3.4):
    // - default_thickness <= 0
    // - name пустое
    // - id не valid
    void validate() const;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/material.cpp`.**

```cpp
#include "coupecad/core/material.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* material_kind_name(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::ChipboardLaminated: return "ChipboardLaminated";
        case MaterialKind::Mdf:                return "Mdf";
        case MaterialKind::Hdf:                return "Hdf";
        case MaterialKind::Plywood:            return "Plywood";
        case MaterialKind::SolidWood:          return "SolidWood";
        case MaterialKind::Glass:              return "Glass";
        case MaterialKind::Metal:              return "Metal";
        case MaterialKind::Other:              return "Other";
    }
    return "?";
}

void Material::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"material.invalid_id", "Material has invalid id"};
    }
    if (name.empty()) {
        throw DomainError{"material.empty_name", "Material name is empty"};
    }
    if (default_thickness.value() <= 0) {
        throw DomainError{"material.nonpositive_thickness",
                          "Material default_thickness must be > 0"};
    }
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt` — добавить material.cpp.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/material_test.cpp`.**

```cpp
#include "coupecad/core/material.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Material make_valid() {
    auto gen = make_seeded_uuid_generator(1);
    Material m;
    m.id = gen->next_id<MaterialIdTag>();
    m.name = "ЛДСП 16мм";
    m.kind = MaterialKind::ChipboardLaminated;
    m.default_thickness = Millimeters{16};
    m.color_hint = RGBA{240, 240, 240, 255};
    return m;
}
}  // namespace

TEST(Material, ValidPasses) {
    EXPECT_NO_THROW(make_valid().validate());
}

TEST(Material, InvalidIdThrows) {
    Material m = make_valid();
    m.id = MaterialId{};  // default = nil
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, EmptyNameThrows) {
    Material m = make_valid();
    m.name.clear();
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, ZeroThicknessThrows) {
    Material m = make_valid();
    m.default_thickness = Millimeters{0};
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, NegativeThicknessThrows) {
    Material m = make_valid();
    m.default_thickness = Millimeters{-5};
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, EqualityIsValueBased) {
    Material a = make_valid();
    Material b = a;
    EXPECT_EQ(a, b);
    b.name = "Different";
    EXPECT_NE(a, b);
}

TEST(MaterialKind, NamesAreStable) {
    EXPECT_STREQ(material_kind_name(MaterialKind::ChipboardLaminated),
                 "ChipboardLaminated");
    EXPECT_STREQ(material_kind_name(MaterialKind::Mdf), "Mdf");
    EXPECT_STREQ(material_kind_name(MaterialKind::Glass), "Glass");
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Material|MaterialKind" --output-on-failure
```

Ожидается: 7 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/material.* src/coupecad/core/CMakeLists.txt tests/core/material_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): Material struct with kinds and validate()"
```

---

## Task 8: `coupecad_core` — Hardware (HardwareKind + HardwareSpec + PanelAttachment + HardwareItem)

**Files:**
- Create: `src/coupecad/core/hardware.h`
- Create: `src/coupecad/core/hardware.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/hardware_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/hardware.h`.**

```cpp
#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>
#include <vector>

namespace coupecad::core {

enum class HardwareKind {
    Hinge,         // петля
    DrawerSlide,   // направляющая ящика
    Handle,        // ручка
    ShelfSupport,  // полкодержатель
    Connector,     // конфирмат / эксцентрик / минификс
    GasLift,       // газовая пружина
    Other,
};

const char* hardware_kind_name(HardwareKind kind) noexcept;

// Карточка фурнитуры в hardware_catalog проекта.
struct HardwareSpec {
    HardwareRef ref;
    HardwareKind kind = HardwareKind::Other;
    std::string name;
    std::optional<std::string> sku;
    Vec3 bbox{};                       // габариты (мм)
    std::optional<Money> price_each;

    bool operator==(const HardwareSpec&) const = default;

    // Бросает DomainError если:
    //  - ref пустой
    //  - name пустое
    //  - bbox имеет неположительное измерение
    void validate() const;
};

// Привязка фурнитуры к панели.
struct PanelAttachment {
    PanelId panel_id;
    Vec3 local_position{};
    Quat orientation = Quat::identity();

    bool operator==(const PanelAttachment&) const = default;
};

// Экземпляр фурнитуры в шкафу.
struct HardwareItem {
    HardwareItemId id;
    HardwareRef ref;
    std::vector<PanelAttachment> attachments;
    std::optional<std::string> label;

    bool operator==(const HardwareItem&) const = default;

    // Бросает DomainError если:
    //  - id или ref не valid
    //  - attachments пустой
    //  - какой-либо attachment.panel_id не valid
    void validate() const;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/hardware.cpp`.**

```cpp
#include "coupecad/core/hardware.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* hardware_kind_name(HardwareKind kind) noexcept {
    switch (kind) {
        case HardwareKind::Hinge:        return "Hinge";
        case HardwareKind::DrawerSlide:  return "DrawerSlide";
        case HardwareKind::Handle:       return "Handle";
        case HardwareKind::ShelfSupport: return "ShelfSupport";
        case HardwareKind::Connector:    return "Connector";
        case HardwareKind::GasLift:      return "GasLift";
        case HardwareKind::Other:        return "Other";
    }
    return "?";
}

void HardwareSpec::validate() const {
    if (ref.value().empty()) {
        throw DomainError{"hardware_spec.empty_ref",
                          "HardwareSpec ref is empty"};
    }
    if (name.empty()) {
        throw DomainError{"hardware_spec.empty_name",
                          "HardwareSpec name is empty"};
    }
    if (bbox.x.value() <= 0 || bbox.y.value() <= 0 || bbox.z.value() <= 0) {
        throw DomainError{"hardware_spec.nonpositive_bbox",
                          "HardwareSpec bbox dimensions must be > 0"};
    }
}

void HardwareItem::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"hardware_item.invalid_id",
                          "HardwareItem has invalid id"};
    }
    if (ref.value().empty()) {
        throw DomainError{"hardware_item.empty_ref",
                          "HardwareItem ref is empty"};
    }
    if (attachments.empty()) {
        throw DomainError{"hardware_item.no_attachments",
                          "HardwareItem must have at least one attachment"};
    }
    for (const auto& a : attachments) {
        if (!a.panel_id.is_valid()) {
            throw DomainError{"hardware_item.invalid_attachment_panel",
                              "PanelAttachment has invalid panel_id"};
        }
    }
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/hardware_test.cpp`.**

```cpp
#include "coupecad/core/hardware.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
HardwareSpec make_valid_spec() {
    return HardwareSpec{
        .ref = HardwareRef{"hinge.generic.straight"},
        .kind = HardwareKind::Hinge,
        .name = "Петля прямая 90°",
        .sku = std::nullopt,
        .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
        .price_each = std::nullopt,
    };
}

HardwareItem make_valid_item() {
    auto gen = make_seeded_uuid_generator(2);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = gen->next_id<PanelIdTag>(),
        .local_position = Vec3{Millimeters{10}, Millimeters{100}, Millimeters{8}},
        .orientation = Quat::identity(),
    });
    return h;
}
}  // namespace

TEST(HardwareKind, NamesAreStable) {
    EXPECT_STREQ(hardware_kind_name(HardwareKind::Hinge), "Hinge");
    EXPECT_STREQ(hardware_kind_name(HardwareKind::Connector), "Connector");
}

TEST(HardwareSpec, ValidPasses) {
    EXPECT_NO_THROW(make_valid_spec().validate());
}

TEST(HardwareSpec, EmptyRefThrows) {
    auto s = make_valid_spec();
    s.ref = HardwareRef{};
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareSpec, EmptyNameThrows) {
    auto s = make_valid_spec();
    s.name.clear();
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareSpec, NonPositiveBboxThrows) {
    auto s = make_valid_spec();
    s.bbox.y = Millimeters{0};
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareItem, ValidPasses) {
    EXPECT_NO_THROW(make_valid_item().validate());
}

TEST(HardwareItem, InvalidIdThrows) {
    auto h = make_valid_item();
    h.id = HardwareItemId{};
    EXPECT_THROW(h.validate(), DomainError);
}

TEST(HardwareItem, NoAttachmentsThrows) {
    auto h = make_valid_item();
    h.attachments.clear();
    EXPECT_THROW(h.validate(), DomainError);
}

TEST(HardwareItem, AttachmentWithoutPanelIdThrows) {
    auto h = make_valid_item();
    h.attachments.front().panel_id = PanelId{};
    EXPECT_THROW(h.validate(), DomainError);
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Hardware" --output-on-failure
```

Ожидается: 9 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/hardware.* src/coupecad/core/CMakeLists.txt tests/core/hardware_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): Hardware (HardwareSpec, HardwareItem, PanelAttachment)"
```

---

## Task 9: `coupecad_core` — `Panel` (PanelRole, RoleParams variant, EdgeBanding, Panel struct)

**Files:**
- Create: `src/coupecad/core/panel.h`
- Create: `src/coupecad/core/panel.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/panel_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/panel.h`.**

```cpp
#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>
#include <variant>

namespace coupecad::core {

// Все роли панелей (см. spec §2.2).
enum class PanelRole {
    Top,
    Bottom,
    SideLeft,
    SideRight,
    Back,
    Shelf,
    DividerVertical,
    DividerHorizontal,
    Facade,
    DrawerBottom,
    DrawerFront,
    DrawerSide,
    DrawerBack,
    Plinth,
    Custom,
};

const char* panel_role_name(PanelRole role) noexcept;

// --- RoleParams: variant per role ---

struct NoRoleParams {
    bool operator==(const NoRoleParams&) const = default;
};

// Полка может занимать всю ширину или быть зажата между
// двумя вертикальными перегородками (DividerVertical).
struct ShelfFullWidth {
    bool operator==(const ShelfFullWidth&) const = default;
};
struct ShelfBetweenDividers {
    PanelId from;
    PanelId to;
    bool operator==(const ShelfBetweenDividers&) const = default;
};
using ShelfExtent = std::variant<ShelfFullWidth, ShelfBetweenDividers>;

struct ShelfParams {
    Millimeters height_from_bottom{};
    ShelfExtent extent = ShelfFullWidth{};
    bool operator==(const ShelfParams&) const = default;
};

// Вертикальная перегородка: смещение от левой стенки + диапазон по высоте.
struct VerticalExtentFull {
    bool operator==(const VerticalExtentFull&) const = default;
};
struct VerticalExtentRange {
    Millimeters from_z{};
    Millimeters to_z{};
    bool operator==(const VerticalExtentRange&) const = default;
};
using VerticalExtent = std::variant<VerticalExtentFull, VerticalExtentRange>;

struct DividerVerticalParams {
    Millimeters offset_from_left{};
    VerticalExtent height_extent = VerticalExtentFull{};
    bool operator==(const DividerVerticalParams&) const = default;
};

// Горизонтальная перегородка: смещение от низа + диапазон по глубине.
struct DepthExtentFull {
    bool operator==(const DepthExtentFull&) const = default;
};
struct DepthExtentRange {
    Millimeters from_y{};
    Millimeters to_y{};
    bool operator==(const DepthExtentRange&) const = default;
};
using DepthExtent = std::variant<DepthExtentFull, DepthExtentRange>;

struct DividerHorizontalParams {
    Millimeters offset_from_bottom{};
    DepthExtent depth_extent = DepthExtentFull{};
    bool operator==(const DividerHorizontalParams&) const = default;
};

// Фасад: какую часть передней грани закрывает + сторона петель.
struct FacadeFullFront {
    bool operator==(const FacadeFullFront&) const = default;
};
struct FacadeRect {
    Millimeters from_x{};
    Millimeters to_x{};
    Millimeters from_z{};
    Millimeters to_z{};
    bool operator==(const FacadeRect&) const = default;
};
using FacadeExtent = std::variant<FacadeFullFront, FacadeRect>;

enum class HingeSide { Left, Right, Top, Bottom, None };
const char* hinge_side_name(HingeSide side) noexcept;

struct FacadeParams {
    FacadeExtent extent = FacadeFullFront{};
    HingeSide hinge_side = HingeSide::None;
    bool operator==(const FacadeParams&) const = default;
};

struct DrawerBottomParams {
    Millimeters height_from_bottom{};
    Millimeters depth{};
    bool operator==(const DrawerBottomParams&) const = default;
};
struct DrawerFrontParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    bool operator==(const DrawerFrontParams&) const = default;
};
struct DrawerSideParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    Millimeters depth{};
    enum class Side { Left, Right };
    Side side = Side::Left;
    bool operator==(const DrawerSideParams&) const = default;
};
struct DrawerBackParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    bool operator==(const DrawerBackParams&) const = default;
};

struct PlinthParams {
    Millimeters height{};
    Millimeters setback{};   // отступ от переднего края
    bool operator==(const PlinthParams&) const = default;
};

struct CustomParams {
    Vec3 position{};
    Vec3 size{};
    Quat orientation = Quat::identity();
    bool operator==(const CustomParams&) const = default;
};

using RoleParams = std::variant<
    NoRoleParams,
    ShelfParams,
    DividerVerticalParams,
    DividerHorizontalParams,
    FacadeParams,
    DrawerBottomParams,
    DrawerFrontParams,
    DrawerSideParams,
    DrawerBackParams,
    PlinthParams,
    CustomParams
>;

// --- EdgeBanding ---

struct EdgeBanding {
    MaterialId material_id;
    Millimeters thickness{2};

    bool operator==(const EdgeBanding&) const = default;
};

struct PanelEdgeBanding {
    std::optional<EdgeBanding> front;
    std::optional<EdgeBanding> back;
    std::optional<EdgeBanding> left;
    std::optional<EdgeBanding> right;

    bool operator==(const PanelEdgeBanding&) const = default;
};

enum class GrainDirection { Horizontal, Vertical, None };
const char* grain_direction_name(GrainDirection g) noexcept;

// --- Panel ---

struct Panel {
    PanelId id;
    PanelRole role = PanelRole::Top;
    RoleParams role_params = NoRoleParams{};
    std::optional<MaterialId> material_override;
    std::optional<Millimeters> thickness_override;
    PanelEdgeBanding edge_banding{};
    GrainDirection grain_direction = GrainDirection::None;
    std::optional<std::string> label;

    bool operator==(const Panel&) const = default;

    // Бросает DomainError если:
    //  - id не valid
    //  - role <-> role_params несовместимы (см. spec §3.4)
    //  - thickness_override <= 0
    //  - edge_banding имеет thickness <= 0 или material_id не valid
    void validate() const;
};

// Возвращает true если данная пара (role, role_params) допустима.
bool is_role_params_valid_for(PanelRole role, const RoleParams& params) noexcept;

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/panel.cpp`.**

```cpp
#include "coupecad/core/panel.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* panel_role_name(PanelRole role) noexcept {
    switch (role) {
        case PanelRole::Top:               return "Top";
        case PanelRole::Bottom:            return "Bottom";
        case PanelRole::SideLeft:          return "SideLeft";
        case PanelRole::SideRight:         return "SideRight";
        case PanelRole::Back:              return "Back";
        case PanelRole::Shelf:             return "Shelf";
        case PanelRole::DividerVertical:   return "DividerVertical";
        case PanelRole::DividerHorizontal: return "DividerHorizontal";
        case PanelRole::Facade:            return "Facade";
        case PanelRole::DrawerBottom:      return "DrawerBottom";
        case PanelRole::DrawerFront:       return "DrawerFront";
        case PanelRole::DrawerSide:        return "DrawerSide";
        case PanelRole::DrawerBack:        return "DrawerBack";
        case PanelRole::Plinth:            return "Plinth";
        case PanelRole::Custom:            return "Custom";
    }
    return "?";
}

const char* hinge_side_name(HingeSide side) noexcept {
    switch (side) {
        case HingeSide::Left:   return "Left";
        case HingeSide::Right:  return "Right";
        case HingeSide::Top:    return "Top";
        case HingeSide::Bottom: return "Bottom";
        case HingeSide::None:   return "None";
    }
    return "?";
}

const char* grain_direction_name(GrainDirection g) noexcept {
    switch (g) {
        case GrainDirection::Horizontal: return "Horizontal";
        case GrainDirection::Vertical:   return "Vertical";
        case GrainDirection::None:       return "None";
    }
    return "?";
}

bool is_role_params_valid_for(PanelRole role, const RoleParams& params) noexcept {
    switch (role) {
        case PanelRole::Top:
        case PanelRole::Bottom:
        case PanelRole::Back:
        case PanelRole::SideLeft:
        case PanelRole::SideRight:
            return std::holds_alternative<NoRoleParams>(params);
        case PanelRole::Shelf:
            return std::holds_alternative<ShelfParams>(params);
        case PanelRole::DividerVertical:
            return std::holds_alternative<DividerVerticalParams>(params);
        case PanelRole::DividerHorizontal:
            return std::holds_alternative<DividerHorizontalParams>(params);
        case PanelRole::Facade:
            return std::holds_alternative<FacadeParams>(params);
        case PanelRole::DrawerBottom:
            return std::holds_alternative<DrawerBottomParams>(params);
        case PanelRole::DrawerFront:
            return std::holds_alternative<DrawerFrontParams>(params);
        case PanelRole::DrawerSide:
            return std::holds_alternative<DrawerSideParams>(params);
        case PanelRole::DrawerBack:
            return std::holds_alternative<DrawerBackParams>(params);
        case PanelRole::Plinth:
            return std::holds_alternative<PlinthParams>(params);
        case PanelRole::Custom:
            return std::holds_alternative<CustomParams>(params);
    }
    return false;
}

void Panel::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"panel.invalid_id", "Panel has invalid id"};
    }
    if (!is_role_params_valid_for(role, role_params)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Panel role and role_params variant don't match"};
    }
    if (thickness_override && thickness_override->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "Panel thickness_override must be > 0"};
    }
    auto check_edge = [](const std::optional<EdgeBanding>& eb,
                         const char* code) {
        if (!eb) return;
        if (!eb->material_id.is_valid()) {
            throw DomainError{code, "EdgeBanding.material_id is invalid"};
        }
        if (eb->thickness.value() <= 0) {
            throw DomainError{code, "EdgeBanding.thickness must be > 0"};
        }
    };
    check_edge(edge_banding.front, "panel.edge_front_invalid");
    check_edge(edge_banding.back,  "panel.edge_back_invalid");
    check_edge(edge_banding.left,  "panel.edge_left_invalid");
    check_edge(edge_banding.right, "panel.edge_right_invalid");
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt` — добавить panel.cpp.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/panel_test.cpp`.**

```cpp
#include "coupecad/core/panel.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
PanelId next_panel_id(UuidGenerator& g) { return g.next_id<PanelIdTag>(); }
}  // namespace

TEST(PanelRole, NameForEveryRole) {
    EXPECT_STREQ(panel_role_name(PanelRole::Top), "Top");
    EXPECT_STREQ(panel_role_name(PanelRole::Shelf), "Shelf");
    EXPECT_STREQ(panel_role_name(PanelRole::Custom), "Custom");
}

TEST(IsRoleParamsValidFor, MatchedPairs) {
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Shelf, ShelfParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::DividerVertical,
                                         DividerVerticalParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Facade, FacadeParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Custom, CustomParams{}));
}

TEST(IsRoleParamsValidFor, MismatchedPairs) {
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Top, ShelfParams{}));
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Shelf, NoRoleParams{}));
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Facade, ShelfParams{}));
}

TEST(Panel, ValidShelfPasses) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Shelf;
    p.role_params = ShelfParams{
        .height_from_bottom = Millimeters{800},
        .extent = ShelfFullWidth{},
    };
    EXPECT_NO_THROW(p.validate());
}

TEST(Panel, InvalidIdThrows) {
    Panel p;
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, RoleParamsMismatchThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Shelf;
    p.role_params = NoRoleParams{};   // wrong variant
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, NonPositiveThicknessOverrideThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.thickness_override = Millimeters{0};
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EdgeBandingWithInvalidMaterialThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.edge_banding.front = EdgeBanding{
        .material_id = MaterialId{},   // invalid
        .thickness = Millimeters{2},
    };
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EdgeBandingWithZeroThicknessThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.edge_banding.left = EdgeBanding{
        .material_id = MaterialId{gen->next()},
        .thickness = Millimeters{0},
    };
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EqualityIsValueBased) {
    auto gen = make_seeded_uuid_generator(3);
    Panel a;
    a.id = next_panel_id(*gen);
    a.role = PanelRole::Top;
    Panel b = a;
    EXPECT_EQ(a, b);
    b.label = "Different";
    EXPECT_NE(a, b);
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Panel|IsRoleParamsValidFor|PanelRole" --output-on-failure
```

Ожидается: 10 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/panel.* src/coupecad/core/CMakeLists.txt tests/core/panel_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): Panel with PanelRole, RoleParams variant, EdgeBanding"
```

---

## Task 10: `coupecad_core` — `Cabinet` struct

**Files:**
- Create: `src/coupecad/core/cabinet.h`
- Create: `src/coupecad/core/cabinet.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/cabinet_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/cabinet.h`.**

```cpp
#pragma once

#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

#include <string>
#include <unordered_map>

namespace coupecad::core {

struct Cabinet {
    CabinetId id;
    std::string name = "Cabinet";
    Dimensions dimensions{};
    MaterialId default_panel_material;        // обязательно (см. validate)
    Millimeters default_panel_thickness{16};
    Millimeters default_back_thickness{4};
    std::unordered_map<PanelId, Panel> panels;
    std::unordered_map<HardwareItemId, HardwareItem> hardware;

    bool operator==(const Cabinet&) const = default;

    // Бросает DomainError если:
    //  - id не valid
    //  - dimensions имеет неположительное измерение
    //  - default_panel_material не valid
    //  - default_panel_thickness <= 0 или default_back_thickness <= 0
    //  - любая Panel или HardwareItem не проходят свой validate()
    //  - ключи map'ов не совпадают с id значений (внутренняя инвариантность)
    void validate() const;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/cabinet.cpp`.**

```cpp
#include "coupecad/core/cabinet.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

void Cabinet::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"cabinet.invalid_id", "Cabinet has invalid id"};
    }
    if (dimensions.width.value() <= 0 ||
        dimensions.depth.value() <= 0 ||
        dimensions.height.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_dimensions",
                          "Cabinet dimensions must all be > 0"};
    }
    if (!default_panel_material.is_valid()) {
        throw DomainError{"cabinet.invalid_default_material",
                          "Cabinet default_panel_material is invalid"};
    }
    if (default_panel_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_panel_thickness",
                          "Cabinet default_panel_thickness must be > 0"};
    }
    if (default_back_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_back_thickness",
                          "Cabinet default_back_thickness must be > 0"};
    }
    for (const auto& [key, panel] : panels) {
        if (key != panel.id) {
            throw DomainError{"cabinet.panel_key_mismatch",
                              "Cabinet.panels key does not match panel.id"};
        }
        panel.validate();
    }
    for (const auto& [key, hw] : hardware) {
        if (key != hw.id) {
            throw DomainError{"cabinet.hardware_key_mismatch",
                              "Cabinet.hardware key does not match hardware.id"};
        }
        hw.validate();
        // Дополнительная проверка: каждый attachment ссылается на
        // существующую в этом же шкафу панель.
        for (const auto& a : hw.attachments) {
            if (panels.find(a.panel_id) == panels.end()) {
                throw DomainError{"cabinet.hardware_unknown_panel",
                                  "HardwareItem references unknown PanelId"};
            }
        }
    }
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt` — добавить cabinet.cpp.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/cabinet_test.cpp`.**

```cpp
#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Cabinet make_valid_cabinet(UuidGenerator& gen) {
    Cabinet c;
    c.id = gen.next_id<CabinetIdTag>();
    c.dimensions = Dimensions{
        .width = Millimeters{2400},
        .depth = Millimeters{600},
        .height = Millimeters{2400},
    };
    c.default_panel_material = gen.next_id<MaterialIdTag>();
    return c;
}

PanelId add_top_panel(Cabinet& c, UuidGenerator& gen) {
    Panel p;
    p.id = gen.next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    auto id = p.id;
    c.panels[id] = std::move(p);
    return id;
}
}  // namespace

TEST(Cabinet, ValidEmptyPasses) {
    auto gen = make_seeded_uuid_generator(4);
    EXPECT_NO_THROW(make_valid_cabinet(*gen).validate());
}

TEST(Cabinet, InvalidIdThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.id = CabinetId{};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, ZeroDimensionThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.dimensions.depth = Millimeters{0};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, InvalidDefaultMaterialThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.default_panel_material = MaterialId{};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, NonPositivePanelThicknessThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.default_panel_thickness = Millimeters{0};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, PanelMapKeyMismatchThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    Panel p;
    p.id = gen->next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    PanelId wrong_key = gen->next_id<PanelIdTag>();
    c.panels[wrong_key] = std::move(p);
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, ValidPanelsPass) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    add_top_panel(c, *gen);
    EXPECT_NO_THROW(c.validate());
}

TEST(Cabinet, HardwareReferencingMissingPanelThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = gen->next_id<PanelIdTag>(),  // не в этом cabinet
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    c.hardware[h.id] = std::move(h);
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, HardwareReferencingExistingPanelPasses) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    auto pid = add_top_panel(c, *gen);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    c.hardware[h.id] = std::move(h);
    EXPECT_NO_THROW(c.validate());
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
    cabinet_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Cabinet" --output-on-failure
```

Ожидается: 9 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/cabinet.* src/coupecad/core/CMakeLists.txt tests/core/cabinet_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): Cabinet aggregate with cross-entity validation"
```

---

## Task 11: `coupecad_core` — `Project`, `IProjectObserver`, `ChangeSet`

**Files:**
- Create: `src/coupecad/core/project.h`
- Create: `src/coupecad/core/project.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/project_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/project.h`.**

```cpp
#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace coupecad::core {

// ChangeSet: какие сущности затронуты последней правкой. См. spec §3.5.
// В Stage 1a используется только для валидации формы; реальные команды,
// формирующие ChangeSet, появятся в Stage 1b.
struct ChangeSet {
    std::vector<PanelId> added_panels;
    std::vector<PanelId> removed_panels;
    std::vector<PanelId> updated_panels;

    std::vector<HardwareItemId> added_hardware;
    std::vector<HardwareItemId> removed_hardware;
    std::vector<HardwareItemId> updated_hardware;

    std::vector<MaterialId> added_materials;
    std::vector<MaterialId> removed_materials;
    std::vector<MaterialId> updated_materials;

    bool cabinet_changed = false;

    bool empty() const noexcept {
        return added_panels.empty() && removed_panels.empty() &&
               updated_panels.empty() && added_hardware.empty() &&
               removed_hardware.empty() && updated_hardware.empty() &&
               added_materials.empty() && removed_materials.empty() &&
               updated_materials.empty() && !cabinet_changed;
    }
};

// Метаданные проекта (имя, описание).
struct ProjectMeta {
    std::string name;
    std::string description;
    bool operator==(const ProjectMeta&) const = default;
};

class Project;

// Интерфейс наблюдателя за изменениями проекта.
class IProjectObserver {
public:
    virtual ~IProjectObserver() = default;
    virtual void on_changed(const Project& project, const ChangeSet& change) = 0;
};

class Project {
public:
    // Создаёт пустой проект с одним пустым Cabinet и одним базовым материалом.
    // Использует переданный UuidGenerator для всех новых id.
    static Project create_empty(std::string name,
                                std::unique_ptr<UuidGenerator> uuid_gen
                                    = make_random_uuid_generator());

    // Read-only доступ.
    const ProjectMeta& meta() const noexcept { return meta_; }
    const Cabinet& cabinet() const noexcept { return cabinet_; }
    const std::unordered_map<MaterialId, Material>& materials() const noexcept {
        return materials_;
    }
    const std::unordered_map<HardwareRef, HardwareSpec>&
        hardware_catalog() const noexcept { return hardware_catalog_; }

    UuidGenerator& uuid_gen() noexcept { return *uuid_gen_; }

    // Доступ к Cabinet для записи в Stage 1a (без команд) — возвращает
    // не-const ссылку. В Stage 1b эту лазейку закроют команды.
    Cabinet& mutable_cabinet() noexcept { return cabinet_; }
    std::unordered_map<MaterialId, Material>& mutable_materials() noexcept {
        return materials_;
    }
    std::unordered_map<HardwareRef, HardwareSpec>&
        mutable_hardware_catalog() noexcept { return hardware_catalog_; }
    ProjectMeta& mutable_meta() noexcept { return meta_; }

    // Полная валидация всего дерева. Бросает DomainError на первом
    // нарушении.
    void validate() const;

private:
    Project() = default;

    ProjectMeta meta_;
    Cabinet cabinet_;
    std::unordered_map<MaterialId, Material> materials_;
    std::unordered_map<HardwareRef, HardwareSpec> hardware_catalog_;
    std::unique_ptr<UuidGenerator> uuid_gen_;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/project.cpp`.**

```cpp
#include "coupecad/core/project.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

Project Project::create_empty(std::string name,
                              std::unique_ptr<UuidGenerator> uuid_gen) {
    Project p;
    p.uuid_gen_ = std::move(uuid_gen);
    p.meta_.name = std::move(name);

    // Один дефолтный материал — чтобы Cabinet прошёл validate().
    Material default_material{
        .id = p.uuid_gen_->next_id<MaterialIdTag>(),
        .name = "Default ChipboardLaminated 16mm",
        .kind = MaterialKind::ChipboardLaminated,
        .default_thickness = Millimeters{16},
        .color_hint = RGBA{220, 220, 220, 255},
    };
    auto material_id = default_material.id;
    p.materials_[material_id] = std::move(default_material);

    // Один пустой Cabinet с разумными дефолтами.
    p.cabinet_.id = p.uuid_gen_->next_id<CabinetIdTag>();
    p.cabinet_.name = "Cabinet";
    p.cabinet_.dimensions = Dimensions{
        .width = Millimeters{2400},
        .depth = Millimeters{600},
        .height = Millimeters{2400},
    };
    p.cabinet_.default_panel_material = material_id;
    p.cabinet_.default_panel_thickness = Millimeters{16};
    p.cabinet_.default_back_thickness = Millimeters{4};

    return p;
}

void Project::validate() const {
    cabinet_.validate();
    for (const auto& [key, m] : materials_) {
        if (key != m.id) {
            throw DomainError{"project.material_key_mismatch",
                              "materials map key != material.id"};
        }
        m.validate();
    }
    for (const auto& [key, spec] : hardware_catalog_) {
        if (key != spec.ref) {
            throw DomainError{"project.hardware_spec_key_mismatch",
                              "hardware_catalog key != spec.ref"};
        }
        spec.validate();
    }
    // Cross-aggregate проверка: cabinet.default_panel_material должен
    // существовать в materials_.
    if (materials_.find(cabinet_.default_panel_material) == materials_.end()) {
        throw DomainError{"project.default_material_missing",
                          "cabinet.default_panel_material is not in materials"};
    }
    // Каждый material_override и edge_banding в панелях должен ссылаться
    // на существующий материал.
    for (const auto& [_, p] : cabinet_.panels) {
        if (p.material_override &&
            materials_.find(*p.material_override) == materials_.end()) {
            throw DomainError{"project.panel_material_missing",
                              "panel.material_override references unknown material"};
        }
        auto check_eb = [&](const std::optional<EdgeBanding>& eb) {
            if (eb && materials_.find(eb->material_id) == materials_.end()) {
                throw DomainError{"project.edge_banding_material_missing",
                                  "panel.edge_banding references unknown material"};
            }
        };
        check_eb(p.edge_banding.front);
        check_eb(p.edge_banding.back);
        check_eb(p.edge_banding.left);
        check_eb(p.edge_banding.right);
    }
    // Каждый HardwareItem.ref должен быть в hardware_catalog.
    for (const auto& [_, h] : cabinet_.hardware) {
        if (hardware_catalog_.find(h.ref) == hardware_catalog_.end()) {
            throw DomainError{"project.hardware_ref_missing",
                              "HardwareItem.ref not found in hardware_catalog"};
        }
    }
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/project_test.cpp`.**

```cpp
#include "coupecad/core/project.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(Project, EmptyProjectValidates) {
    auto p = Project::create_empty("My wardrobe",
                                    make_seeded_uuid_generator(10));
    EXPECT_EQ(p.meta().name, "My wardrobe");
    EXPECT_EQ(p.materials().size(), 1u);
    EXPECT_TRUE(p.cabinet().id.is_valid());
    EXPECT_NO_THROW(p.validate());
}

TEST(Project, ChangeSetEmptyByDefault) {
    ChangeSet cs;
    EXPECT_TRUE(cs.empty());
}

TEST(Project, ChangeSetNotEmptyAfterAnyField) {
    ChangeSet cs;
    cs.added_panels.push_back(PanelId{});
    EXPECT_FALSE(cs.empty());
}

TEST(Project, AddingPanelDirectlyValidates) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(11));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    p.mutable_cabinet().panels[pan.id] = pan;
    EXPECT_NO_THROW(p.validate());
}

TEST(Project, EdgeBandingReferencingUnknownMaterialThrows) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(12));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    pan.edge_banding.front = EdgeBanding{
        .material_id = p.uuid_gen().next_id<MaterialIdTag>(),  // не в materials
        .thickness = Millimeters{2},
    };
    p.mutable_cabinet().panels[pan.id] = pan;
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Project, HardwareItemReferencingUnknownRefThrows) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(13));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    auto pid = pan.id;
    p.mutable_cabinet().panels[pid] = pan;

    HardwareItem h;
    h.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"unknown.ref"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    p.mutable_cabinet().hardware[h.id] = std::move(h);
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Project, AddingHardwareSpecAndItemThatReferenceItValidates) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(14));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    auto pid = pan.id;
    p.mutable_cabinet().panels[pid] = pan;

    HardwareSpec spec{
        .ref = HardwareRef{"hinge.generic.straight"},
        .kind = HardwareKind::Hinge,
        .name = "Петля",
        .sku = std::nullopt,
        .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
        .price_each = std::nullopt,
    };
    p.mutable_hardware_catalog()[spec.ref] = spec;

    HardwareItem h;
    h.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    h.ref = spec.ref;
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    p.mutable_cabinet().hardware[h.id] = std::move(h);

    EXPECT_NO_THROW(p.validate());
}

TEST(Project, DeterministicIdsForSeededGenerator) {
    auto a = Project::create_empty("X", make_seeded_uuid_generator(99));
    auto b = Project::create_empty("X", make_seeded_uuid_generator(99));
    EXPECT_EQ(a.cabinet().id, b.cabinet().id);
    EXPECT_EQ(a.cabinet().default_panel_material,
              b.cabinet().default_panel_material);
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
    cabinet_test.cpp
    project_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Project" --output-on-failure
```

Ожидается: 8 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/project.* src/coupecad/core/CMakeLists.txt tests/core/project_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): Project aggregate, ChangeSet, IProjectObserver interface"
```

---

## Task 12: `coupecad_core` — `geometry.h/.cpp` (`compute_panel_geometry`)

**Files:**
- Create: `src/coupecad/core/geometry.h`
- Create: `src/coupecad/core/geometry.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/geometry_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

Эта задача — самая объёмная по логике. Реализуем `compute_panel_geometry` для всех ролей, тестируем каждую роль отдельно.

**Соглашения координат (см. spec §2.3):** X вправо, Y вглубь (от зрителя к задней стенке), Z вверх. Origin — левый-нижний-передний угол. Cabinet занимает `[0..width] × [0..depth] × [0..height]`. Задняя стенка лежит в плоскости `y = depth`.

**Соглашение о толщинах в Stage 1a:** все корпусные панели (Top, Bottom, SideLeft, SideRight, Shelf, Dividers, Plinth) и Back имеют толщину `cabinet.default_panel_thickness` или `panel.thickness_override`, или `cabinet.default_back_thickness` для Back. Внутренний полезный объём — `[t_left..width-t_right] × [0..depth-t_back] × [t_bottom..height-t_top]`, где `t_X = thickness` соответствующей стенки. Для упрощения Stage 1a панели Top/Bottom предполагают полную ширину между `[0..width]`, а боковины — полную высоту `[0..height]`. (Альтернативная схема — «top/bottom между sides» — оставляется на Stage 2 при подключении OpenCASCADE; для read-only геометрии Stage 1a выбранная схема даёт корректные числа для тестов.)

- [ ] **Step 1: Создать `src/coupecad/core/geometry.h`.**

```cpp
#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

// Производная геометрия панели в локальной СК шкафа.
// origin — координаты левого-нижнего-переднего угла бокса панели;
// size  — габариты бокса (всегда положительные при валидной модели);
// orientation — для role-based панелей всегда identity, для Custom —
// из CustomParams.orientation.
struct PanelGeometry {
    Vec3 origin{};
    Vec3 size{};
    Quat orientation = Quat::identity();
};

// Возвращает фактические координаты и размеры панели.
// Бросает DomainError если role/role_params несовместимы с cabinet
// (например, Shelf.height_from_bottom выходит за высоту шкафа).
PanelGeometry compute_panel_geometry(const Cabinet& cabinet,
                                     const Panel& panel);

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/geometry.cpp`.**

```cpp
#include "coupecad/core/geometry.h"

#include "coupecad/core/errors.h"

#include <algorithm>

namespace coupecad::core {

namespace {

Millimeters effective_thickness(const Cabinet& c, const Panel& p) {
    if (p.thickness_override) return *p.thickness_override;
    if (p.role == PanelRole::Back) return c.default_back_thickness;
    return c.default_panel_thickness;
}

void require_positive_size(const Vec3& size, const char* code) {
    if (size.x.value() <= 0 || size.y.value() <= 0 || size.z.value() <= 0) {
        throw DomainError{code, "Computed panel size has non-positive dimension"};
    }
}

}  // namespace

PanelGeometry compute_panel_geometry(const Cabinet& c, const Panel& p) {
    const auto W = c.dimensions.width;
    const auto D = c.dimensions.depth;
    const auto H = c.dimensions.height;
    const auto t = effective_thickness(c, p);
    const auto t_back = c.default_back_thickness;

    PanelGeometry g{};
    switch (p.role) {
        case PanelRole::Top: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, H - t};
            g.size = Vec3{W, D - t_back, t};
            break;
        }
        case PanelRole::Bottom: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{W, D - t_back, t};
            break;
        }
        case PanelRole::SideLeft: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{t, D - t_back, H};
            break;
        }
        case PanelRole::SideRight: {
            g.origin = Vec3{W - t, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{t, D - t_back, H};
            break;
        }
        case PanelRole::Back: {
            g.origin = Vec3{Millimeters{0}, D - t, Millimeters{0}};
            g.size = Vec3{W, t, H};
            break;
        }
        case PanelRole::Shelf: {
            const auto& sp = std::get<ShelfParams>(p.role_params);
            if (sp.height_from_bottom.value() < 0 ||
                sp.height_from_bottom + t > H) {
                throw DomainError{"geometry.shelf_overflow",
                                  "Shelf height_from_bottom places shelf outside cabinet"};
            }
            // Side-thickness (использует default_panel_thickness, без оверрайдов
            // на side-панелях — упрощение Stage 1a).
            const auto side_t = c.default_panel_thickness;
            Millimeters from_x{0};
            Millimeters to_x{0};
            if (std::holds_alternative<ShelfFullWidth>(sp.extent)) {
                from_x = side_t;
                to_x = W - side_t;
            } else {
                const auto& bd = std::get<ShelfBetweenDividers>(sp.extent);
                auto fit = c.panels.find(bd.from);
                auto tit = c.panels.find(bd.to);
                if (fit == c.panels.end() || tit == c.panels.end()) {
                    throw DomainError{"geometry.shelf_divider_missing",
                                      "ShelfBetweenDividers references unknown PanelId"};
                }
                if (fit->second.role != PanelRole::DividerVertical ||
                    tit->second.role != PanelRole::DividerVertical) {
                    throw DomainError{"geometry.shelf_divider_role",
                                      "ShelfBetweenDividers references non-vertical-divider"};
                }
                const auto& f_dp = std::get<DividerVerticalParams>(fit->second.role_params);
                const auto& t_dp = std::get<DividerVerticalParams>(tit->second.role_params);
                from_x = f_dp.offset_from_left + side_t;  // правый край левого
                to_x   = t_dp.offset_from_left;           // левый край правого
            }
            g.origin = Vec3{from_x, Millimeters{0}, sp.height_from_bottom};
            g.size = Vec3{to_x - from_x, D - t_back, t};
            break;
        }
        case PanelRole::DividerVertical: {
            const auto& dp = std::get<DividerVerticalParams>(p.role_params);
            Millimeters from_z{0};
            Millimeters to_z = H;
            if (std::holds_alternative<VerticalExtentRange>(dp.height_extent)) {
                const auto& r = std::get<VerticalExtentRange>(dp.height_extent);
                from_z = r.from_z;
                to_z = r.to_z;
            }
            g.origin = Vec3{dp.offset_from_left, Millimeters{0}, from_z};
            g.size = Vec3{t, D - t_back, to_z - from_z};
            break;
        }
        case PanelRole::DividerHorizontal: {
            const auto& dp = std::get<DividerHorizontalParams>(p.role_params);
            Millimeters from_y{0};
            Millimeters to_y = D - t_back;
            if (std::holds_alternative<DepthExtentRange>(dp.depth_extent)) {
                const auto& r = std::get<DepthExtentRange>(dp.depth_extent);
                from_y = r.from_y;
                to_y = r.to_y;
            }
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, from_y, dp.offset_from_bottom};
            g.size = Vec3{W - 2 * side_t.value(), to_y - from_y, t};
            break;
        }
        case PanelRole::Facade: {
            const auto& fp = std::get<FacadeParams>(p.role_params);
            Millimeters from_x{0};
            Millimeters to_x = W;
            Millimeters from_z{0};
            Millimeters to_z = H;
            if (std::holds_alternative<FacadeRect>(fp.extent)) {
                const auto& r = std::get<FacadeRect>(fp.extent);
                from_x = r.from_x;
                to_x = r.to_x;
                from_z = r.from_z;
                to_z = r.to_z;
            }
            // Фасад лежит ПЕРЕД корпусом (y < 0): в Stage 1a условно
            // помещаем его в y = -t..0 (gap=0).
            g.origin = Vec3{from_x, -t, from_z};
            g.size = Vec3{to_x - from_x, t, to_z - from_z};
            break;
        }
        case PanelRole::DrawerBottom: {
            const auto& dp = std::get<DrawerBottomParams>(p.role_params);
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, Millimeters{0}, dp.height_from_bottom};
            g.size = Vec3{W - 2 * side_t.value(), dp.depth, t};
            break;
        }
        case PanelRole::DrawerFront: {
            const auto& dp = std::get<DrawerFrontParams>(p.role_params);
            g.origin = Vec3{Millimeters{0}, -t, dp.height_from_bottom};
            g.size = Vec3{W, t, dp.height};
            break;
        }
        case PanelRole::DrawerSide: {
            const auto& dp = std::get<DrawerSideParams>(p.role_params);
            const auto side_t = c.default_panel_thickness;
            Millimeters x = (dp.side == DrawerSideParams::Side::Left)
                                ? side_t
                                : W - side_t - t;
            g.origin = Vec3{x, Millimeters{0}, dp.height_from_bottom};
            g.size = Vec3{t, dp.depth, dp.height};
            break;
        }
        case PanelRole::DrawerBack: {
            const auto& dp = std::get<DrawerBackParams>(p.role_params);
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, D - t_back - t, dp.height_from_bottom};
            g.size = Vec3{W - 2 * side_t.value(), t, dp.height};
            break;
        }
        case PanelRole::Plinth: {
            const auto& pp = std::get<PlinthParams>(p.role_params);
            g.origin = Vec3{Millimeters{0}, pp.setback, Millimeters{0}};
            g.size = Vec3{W, t, pp.height};
            break;
        }
        case PanelRole::Custom: {
            const auto& cp = std::get<CustomParams>(p.role_params);
            g.origin = cp.position;
            g.size = cp.size;
            g.orientation = cp.orientation;
            break;
        }
    }
    require_positive_size(g.size, "geometry.computed_negative_size");
    return g;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt` — добавить geometry.cpp.**

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
    geometry.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/geometry_test.cpp`.**

```cpp
#include "coupecad/core/geometry.h"
#include "coupecad/core/project.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {

// Тестовый шкаф 2400×600×2400 с толщиной корпуса 16 и спинки 4.
struct TestProject {
    Project project = Project::create_empty(
        "T", make_seeded_uuid_generator(100));
    UuidGenerator& gen() { return project.uuid_gen(); }
    Cabinet& cab() { return project.mutable_cabinet(); }
};

PanelId add_panel(Cabinet& c, UuidGenerator& gen,
                  PanelRole role, RoleParams params) {
    Panel p;
    p.id = gen.next_id<PanelIdTag>();
    p.role = role;
    p.role_params = std::move(params);
    auto id = p.id;
    c.panels[id] = std::move(p);
    return id;
}

}  // namespace

TEST(Geometry, TopPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Top, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.z, Millimeters{2384});       // 2400 - 16
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.y, Millimeters{596});          // 600 - 4 (back)
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, BottomPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Bottom, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.z, Millimeters{0});
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, SideLeftPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::SideLeft, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{0});
    EXPECT_EQ(g.size.x, Millimeters{16});
    EXPECT_EQ(g.size.z, Millimeters{2400});
}

TEST(Geometry, SideRightPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::SideRight, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{2384});       // 2400 - 16
    EXPECT_EQ(g.size.x, Millimeters{16});
}

TEST(Geometry, BackPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Back, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.y, Millimeters{596});        // 600 - 4
    EXPECT_EQ(g.size.y, Millimeters{4});
}

TEST(Geometry, ShelfFullWidth) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                        ShelfParams{
                            .height_from_bottom = Millimeters{800},
                            .extent = ShelfFullWidth{},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{16});         // вычисление от side
    EXPECT_EQ(g.size.x, Millimeters{2368});         // 2400 - 2*16
    EXPECT_EQ(g.origin.z, Millimeters{800});
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, ShelfBetweenDividers) {
    TestProject t;
    auto d1 = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{600},
                        });
    auto d2 = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{1800},
                        });
    auto sid = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                         ShelfParams{
                             .height_from_bottom = Millimeters{1000},
                             .extent = ShelfBetweenDividers{.from = d1,
                                                            .to = d2},
                         });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(sid));
    EXPECT_EQ(g.origin.x, Millimeters{616});        // 600 + 16 (правый край левого divider)
    EXPECT_EQ(g.size.x, Millimeters{1184});         // 1800 - 616
}

TEST(Geometry, ShelfOverflowThrows) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                        ShelfParams{
                            .height_from_bottom = Millimeters{2400},
                            .extent = ShelfFullWidth{},
                        });
    EXPECT_THROW(compute_panel_geometry(t.cab(), t.cab().panels.at(id)),
                 DomainError);
}

TEST(Geometry, DividerVerticalFullHeight) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{500},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{500});
    EXPECT_EQ(g.size.z, Millimeters{2400});
}

TEST(Geometry, DividerHorizontal) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::DividerHorizontal,
                        DividerHorizontalParams{
                            .offset_from_bottom = Millimeters{1200},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{16});
    EXPECT_EQ(g.size.x, Millimeters{2368});
    EXPECT_EQ(g.origin.z, Millimeters{1200});
}

TEST(Geometry, FacadeFullFront) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Facade,
                        FacadeParams{
                            .extent = FacadeFullFront{},
                            .hinge_side = HingeSide::Left,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{0});
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.z, Millimeters{2400});
    EXPECT_EQ(g.origin.y, Millimeters{-16});        // фасад перед корпусом
}

TEST(Geometry, FacadeRect) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Facade,
                        FacadeParams{
                            .extent = FacadeRect{
                                .from_x = Millimeters{100},
                                .to_x = Millimeters{700},
                                .from_z = Millimeters{200},
                                .to_z = Millimeters{1200},
                            },
                            .hinge_side = HingeSide::Right,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.size.x, Millimeters{600});
    EXPECT_EQ(g.size.z, Millimeters{1000});
}

TEST(Geometry, Plinth) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Plinth,
                        PlinthParams{
                            .height = Millimeters{100},
                            .setback = Millimeters{50},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.y, Millimeters{50});
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.z, Millimeters{100});
}

TEST(Geometry, CustomPanelKeepsOrientation) {
    TestProject t;
    Quat q{0.7071, 0.0, 0.7071, 0.0};
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Custom,
                        CustomParams{
                            .position = Vec3{Millimeters{100},
                                             Millimeters{200},
                                             Millimeters{300}},
                            .size = Vec3{Millimeters{400},
                                         Millimeters{16},
                                         Millimeters{500}},
                            .orientation = q,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin, (Vec3{Millimeters{100}, Millimeters{200}, Millimeters{300}}));
    EXPECT_EQ(g.size, (Vec3{Millimeters{400}, Millimeters{16}, Millimeters{500}}));
    EXPECT_DOUBLE_EQ(g.orientation.w, 0.7071);
    EXPECT_DOUBLE_EQ(g.orientation.y, 0.7071);
}

TEST(Geometry, DrawerBottomFront) {
    TestProject t;
    auto db = add_panel(t.cab(), t.gen(), PanelRole::DrawerBottom,
                        DrawerBottomParams{
                            .height_from_bottom = Millimeters{200},
                            .depth = Millimeters{500},
                        });
    auto df = add_panel(t.cab(), t.gen(), PanelRole::DrawerFront,
                        DrawerFrontParams{
                            .height_from_bottom = Millimeters{200},
                            .height = Millimeters{180},
                        });
    auto gdb = compute_panel_geometry(t.cab(), t.cab().panels.at(db));
    EXPECT_EQ(gdb.size.x, Millimeters{2368});
    EXPECT_EQ(gdb.size.y, Millimeters{500});
    auto gdf = compute_panel_geometry(t.cab(), t.cab().panels.at(df));
    EXPECT_EQ(gdf.origin.y, Millimeters{-16});
    EXPECT_EQ(gdf.size.z, Millimeters{180});
}

TEST(Geometry, ThicknessOverrideUsed) {
    TestProject t;
    Panel p;
    p.id = t.gen().next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.thickness_override = Millimeters{18};
    auto id = p.id;
    t.cab().panels[id] = std::move(p);
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.size.z, Millimeters{18});
    EXPECT_EQ(g.origin.z, Millimeters{2382});       // 2400 - 18
}
```

- [ ] **Step 5: Обновить `tests/core/CMakeLists.txt`.**

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
    cabinet_test.cpp
    project_test.cpp
    geometry_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "Geometry" --output-on-failure
```

Ожидается: 17 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/geometry.* src/coupecad/core/CMakeLists.txt tests/core/geometry_test.cpp tests/core/CMakeLists.txt
git commit -m "feat(core): compute_panel_geometry for all PanelRole variants"
```

---

## Task 13: Финальная сводка — полный прогон тестов + push

**Files:** ничего нового, только верификация и push.

- [ ] **Step 1: Прогнать ВСЕ тесты подряд.**

```sh
cmake --build --preset default
ctest --preset default --output-on-failure
```

Ожидается: ВСЕ тесты passed (smoke + AppVersion + Logger* + Millimeters/Dimensions/Vec3/Quat/Money/RGBA + Id/UuidGenerator/HardwareRef/CoupecadException + Material* + Hardware* + Panel*/IsRoleParamsValidFor + Cabinet* + Project* + Geometry*).

- [ ] **Step 2: Запушить.**

```sh
git push
```

Ожидается: workflow CI зелёный (Linux + Windows). macOS остаётся отключён (см. Task 9 Stage 0).

- [ ] **Step 3: Дождаться зелёного CI.**

```sh
gh run list --limit 1 --workflow=CI
gh run watch
```

Ожидается: `conclusion=success`. Если красное — проверить `gh run view <id> --log-failed`, починить, перезапушить.

- [ ] **Step 4: Финальная контрольная точка.**

После зелёного CI должно быть истинно:

- [ ] `cmake --list-presets` показывает default + release.
- [ ] Локально `ctest --preset default` зелёный полностью.
- [ ] CI зелёный на Linux и Windows.
- [ ] Файлы `src/coupecad/logging/{logger.h, logger.cpp, log_paths.h, log_paths.cpp, log_level.h, CMakeLists.txt}` существуют.
- [ ] Файлы `src/coupecad/core/{units.h, id.h, id.cpp, errors.h, material.h, material.cpp, hardware.h, hardware.cpp, panel.h, panel.cpp, cabinet.h, cabinet.cpp, project.h, project.cpp, geometry.h, geometry.cpp, CMakeLists.txt}` существуют.
- [ ] Стек тестов: `Logger*` (≥13 кейсов), `Millimeters/Dimensions/...` (12), `Id/...` (10), `Material*` (7), `Hardware*` (9), `Panel*/IsRoleParamsValidFor` (10), `Cabinet*` (9), `Project*` (8), `Geometry*` (17). Всего ≥ 95 кейсов.

После этого можно переходить к плану **Stage 1b** (commands + UndoStack с discrete и preview-режимами): новый brainstorm если нужно, новый план.
