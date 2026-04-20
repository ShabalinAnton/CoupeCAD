# Stage 0 — Bootstrap репозитория

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Получить рабочий каркас C++/Qt-проекта CoupeCAD, который собирается и проходит тесты на Windows, macOS и Linux через GitHub Actions, и запускает пустое QML-окно. Это фундамент для всех последующих стейджей.

**Architecture:** Один CMake-проект с CMakePresets и Conan 2 для зависимостей (GoogleTest и в будущем OpenCASCADE и т.п.). **Qt ставится отдельно**, через `aqtinstall` на машине разработчика и через `jurplel/install-qt-action@v4` в CI — см. примечание ниже. Один исполняемый таргет `coupecad` (apps/coupecad/) и каталог тестов `tests/` на GoogleTest. Структура `src/coupecad/<module>` подготовлена под будущие модули (Core, Geometry и т.д.), но пустая в Stage 0.

**Tech Stack:** C++20, CMake **3.25+** (требуется для CMakePresets v6), Conan 2, GoogleTest, Qt 6.5+ (Quick + Gui). Ninja генератор. GitHub Actions с runner-ами ubuntu-22.04, macos-13, windows-2022. Qt в CI ставится через `jurplel/install-qt-action@v4`; локально — через `aqtinstall`.

> **Почему Qt не через Conan (проверено 2026-04-19, окончательно).** Прошли две итерации обсуждения. Прямой путь Qt через Conan Center не работает на нескольких уровнях: (1) нет прекомпилированных бинарей с включённым `qtdeclarative` ни для одной из 6.5.x — 6.10.x версий; (2) даже если строить из исходников, на свежем macOS (Xcode 26 / apple-clang 21) каскад транзитивных recipe-ошибок (`bzip2`, `libffi`, ...) делает сборку нестабильной; (3) двух-четырёхчасовой компилировать Qt — слишком тяжёлая боль для первого Stage. Поэтому Qt берём прекомпилированным от Qt Company через aqtinstall/install-qt-action. Conan остаётся менеджером всех ОСТАЛЬНЫХ C++-зависимостей. Спецификация говорит "Conan — пакетный менеджер" — это по-прежнему верно, Qt задокументированное исключение. Подробное обоснование — в `.claude` memory `feedback_qt_via_conan.md`.

**Definition of Done:**
- На свежей машине: `conan install . --build=missing && cmake --preset default && cmake --build --preset default && ctest --preset default` отрабатывает зелёным на трёх ОС.
- Запуск собранного `coupecad --version` печатает версию и выходит с кодом 0.
- Запуск `coupecad` без аргументов открывает пустое QML-окно с заголовком "CoupeCAD".
- GitHub Actions workflow `ci.yml` зелёный на push в main и на PR, на трёх ОС.
- README объясняет, как собрать локально и как запустить тесты.

---

## File Structure

После Stage 0 структура репозитория должна быть:

```
CoupeCAD/
├── .clang-format
├── .gitignore
├── .github/
│   └── workflows/
│       └── ci.yml
├── CMakeLists.txt
├── CMakePresets.json
├── conanfile.py
├── README.md
├── docs/
│   ├── archive/initial_instructions.txt        (уже есть)
│   ├── ARCHITECTURE.md                         (новый, скелет)
│   └── superpowers/
│       ├── specs/2026-04-18-coupecad-design.md (уже есть)
│       └── plans/2026-04-18-stage-0-bootstrap.md (этот план)
├── apps/
│   └── coupecad/
│       ├── CMakeLists.txt
│       ├── main.cpp
│       └── qml/
│           └── Main.qml
├── src/
│   └── coupecad/
│       └── (пусто; модули появятся в следующих стейджах)
└── tests/
    ├── CMakeLists.txt
    └── smoke/
        ├── CMakeLists.txt
        └── smoke_test.cpp
```

**Ответственности файлов:**

- `CMakeLists.txt` — корневой проект, опции, поиск Qt, подключение поддиректорий.
- `CMakePresets.json` — пресеты конфигурации/сборки/тестирования; единая команда для всех ОС.
- `conanfile.py` — зависимости через Conan (только GoogleTest на Stage 0; Qt — снаружи через aqtinstall/install-qt-action).
- `apps/coupecad/main.cpp` — точка входа: парсит `--version`, иначе запускает `QQmlApplicationEngine` с `Main.qml`.
- `apps/coupecad/qml/Main.qml` — пустое окно с заголовком и фиксированным размером.
- `tests/smoke/smoke_test.cpp` — тривиальный тест (`EXPECT_EQ(2 + 2, 4)`), доказывающий, что цепочка test-discovery работает.
- `.github/workflows/ci.yml` — установка Qt, установка Conan-зависимостей, конфигурация, сборка, тесты на трёх ОС.
- `docs/ARCHITECTURE.md` — скелет с разделами под будущие модули.

---

## Task 1: Repo hygiene

**Files:**
- Create: `.gitignore`
- Create: `.clang-format`
- Create: `README.md`

- [ ] **Step 1: Создать `.gitignore`**

Содержимое:

```gitignore
# Build directories
build/
build-*/
out/
cmake-build-*/

# Conan
.conan2/
CMakeUserPresets.json

# IDE
.idea/
.vscode/
.qtcreator/
*.user
*.user.*
.cache/

# OS
.DS_Store
Thumbs.db

# Compiled / generated
*.o
*.obj
*.so
*.dylib
*.dll
*.a
*.lib
*.exe
*.app
*.exp
*.pdb
*.ilk

# Qt artefacts
moc_*.cpp
moc_*.h
qrc_*.cpp
ui_*.h
*.qm

# Test artefacts
Testing/
CTestTestfile.cmake
```

- [ ] **Step 2: Создать `.clang-format`**

Берём LLVM-стиль за основу с минимальными правками под Qt-конвенции:

```yaml
---
BasedOnStyle: LLVM
Language: Cpp
Standard: c++20
ColumnLimit: 100
IndentWidth: 4
TabWidth: 4
UseTab: Never
AccessModifierOffset: -4
NamespaceIndentation: None
PointerAlignment: Left
AlignAfterOpenBracket: Align
AllowShortFunctionsOnASingleLine: Inline
BreakBeforeBraces: Attach
SortIncludes: CaseInsensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<Q.*>$'
    Priority: 3
  - Regex: '^<.*\.h>$'
    Priority: 2
  - Regex: '^<.*>$'
    Priority: 1
  - Regex: '.*'
    Priority: 4
```

- [ ] **Step 3: Создать `README.md` (скелет)**

```markdown
# CoupeCAD

Кроссплатформенное приложение для проектирования корпусной мебели (шкафов, шкафов-купе, гардеробов, кухонных гарнитуров). См. видение и архитектуру в [`docs/superpowers/specs/2026-04-18-coupecad-design.md`](docs/superpowers/specs/2026-04-18-coupecad-design.md).

## Статус

Stage 0 (Bootstrap). Проект только начат. Работающего функционала пока нет — в репозитории каркас сборки, который собирает и запускает пустое окно.

## Требования к окружению

- CMake **3.25+** (требуется для CMakePresets v6)
- C++20 компилятор: MSVC 2022, Clang 14+, GCC 11+
- Ninja
- Conan **2.x**
- Qt **6.5 LTS или новее**, компоненты Core, Gui, Quick

## Установка Qt

Qt ставится отдельно от Conan — рассуждение в плане Stage 0. Самый простой способ для разработки — `aqtinstall`:

```sh
pipx install aqtinstall
aqt install-qt mac desktop 6.7.3 clang_64 -m qtshadertools         # macOS
# aqt install-qt linux desktop 6.7.3 gcc_64 -m qtshadertools       # Linux
# aqt install-qt windows desktop 6.7.3 win64_msvc2022_64 -m qtshadertools  # Windows
export Qt6_DIR=$PWD/6.7.3/macos/lib/cmake/Qt6                      # путь зависит от ОС
```

В CI Qt ставится через `jurplel/install-qt-action`.

## Сборка и запуск

```sh
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
./build/default/bin/coupecad        # Linux/macOS
./build/default/bin/coupecad.exe    # Windows
```

## Структура проекта

См. `docs/ARCHITECTURE.md`.

## Лицензия

Проприетарная. Конкретные условия лицензирования будут добавлены в LICENSE до публичного релиза.
```

- [ ] **Step 4: Закоммитить**

```sh
git add .gitignore .clang-format README.md
git commit -m "chore: add .gitignore, .clang-format, README skeleton"
```

---

## Task 2: Top-level CMakeLists + CMakePresets

**Files:**
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`

- [ ] **Step 1: Создать корневой `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)

project(CoupeCAD
    VERSION 0.1.0
    DESCRIPTION "Cross-platform CAD for cabinet furniture"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(COUPECAD_BUILD_TESTS "Build CoupeCAD test suites" ON)

# Renderer backend selector — зарезервировано для Stage 3+ (renderer skeleton).
# В Stage 0 ни одна реализация ещё не подключена; флаг определён, чтобы
# CMake-конфигурация уже понимала это имя.
set(COUPECAD_RENDERER "occt" CACHE STRING "Renderer backend: occt or qq3d")
set_property(CACHE COUPECAD_RENDERER PROPERTY STRINGS occt qq3d)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# В Stage 0 ещё нет ни OpenCASCADE, ни Qt Quick 3D — только Quick + Gui.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(apps/coupecad)

if(COUPECAD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Создать `CMakePresets.json`**

```json
{
    "version": 6,
    "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
    "configurePresets": [
        {
            "name": "default",
            "displayName": "Default (Ninja, Debug)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/default",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/build/default/conan_toolchain.cmake"
            }
        },
        {
            "name": "release",
            "inherits": "default",
            "displayName": "Release (Ninja, Release)",
            "binaryDir": "${sourceDir}/build/release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/build/release/conan_toolchain.cmake"
            }
        }
    ],
    "buildPresets": [
        { "name": "default", "configurePreset": "default" },
        { "name": "release", "configurePreset": "release" }
    ],
    "testPresets": [
        {
            "name": "default",
            "configurePreset": "default",
            "output": { "outputOnFailure": true },
            "execution": { "stopOnFailure": false }
        }
    ]
}
```

- [ ] **Step 3: Проверить, что CMake может прочитать пресеты**

```sh
cmake --list-presets
```

Ожидается: вывод со списком `default` и `release`. Конфигурация ещё не запускается (нет conan toolchain) — это нормально на этом шаге.

- [ ] **Step 4: Закоммитить**

```sh
git add CMakeLists.txt CMakePresets.json
git commit -m "build: add top-level CMakeLists and CMakePresets"
```

---

## Task 3: Conanfile с GoogleTest

**Files:**
- Create: `conanfile.py`

> **Предпосылка:** Qt в Conan не используется (см. начало плана и `feedback_qt_via_conan.md`). Conanfile содержит только GoogleTest и в будущем добавит OpenCASCADE/etc. Qt находится через `find_package(Qt6)` из внешнего источника: `Qt6_DIR` экспортирует `install-qt-action` в CI и пользователь сам после `aqtinstall` локально.

- [ ] **Step 1: Создать `conanfile.py`**

```python
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class CoupeCADConan(ConanFile):
    name = "coupecad"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    # Stage 0: только GoogleTest. Qt подключается извне (aqtinstall/install-qt-action).
    # OpenCASCADE добавится в Stage 2 (Geometry layer).
    def requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        # Принудительно используем build/default/ как на Linux/macOS,
        # так и на Windows, чтобы CMakePresets.json совпадал с conan layout.
        self.folders.build = "build/default"
        self.folders.generators = "build/default"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()
```

- [ ] **Step 2: Установить зависимости через Conan**

```sh
conan profile detect --force
conan install . --build=missing -s build_type=Debug
```

> **macOS, Xcode 26+ (apple-clang 21+):** Conan 2.27.x ещё не знает об этой версии Apple Clang. После `conan profile detect` отредактируйте `~/.conan2/profiles/default` и выставьте `compiler.version=17` (последняя известная Conan; ABI-совместимо). Иначе любая операция упадёт с `Invalid setting`.

Ожидается: создаётся `build/default/conan_toolchain.cmake` и `build/default/*-GTest*.cmake` (или похожие). Команда заканчивается без ошибок и быстро (gtest маленький, скачивается за секунды).

- [ ] **Step 3: Проверить, что CMake конфигурируется**

Сначала экспортируй путь к локальной установке Qt (`aqtinstall` обычно ставит сюда):

```sh
export Qt6_DIR=$HOME/Qt/6.7.3/macos/lib/cmake/Qt6     # macOS
# export Qt6_DIR=$HOME/Qt/6.7.3/gcc_64/lib/cmake/Qt6  # Linux
# %Qt6_DIR%=%USERPROFILE%\Qt\6.7.3\msvc2022_64\lib\cmake\Qt6  # Windows
cmake --preset default
```

Ожидается: CMake находит Qt6 (печатает что-то вроде `Found Qt6Config: .../Qt6Config.cmake`), затем падает с ошибкой про отсутствующий каталог `tests/` или таргет `coupecad`. Эту ошибку решат Task 4 и Task 5 — в этом таске она ожидаема и означает, что Qt6 + Conan настроены корректно.

- [ ] **Step 4: Закоммитить**

```sh
git add conanfile.py
git commit -m "build: add conanfile with GoogleTest dependency"
```

---

## Task 4: tests scaffold + first GoogleTest

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/smoke/CMakeLists.txt`
- Create: `tests/smoke/smoke_test.cpp`

- [ ] **Step 1: Написать падающий тест в `tests/smoke/smoke_test.cpp`**

Сначала пишем намеренно падающий ассерт, чтобы убедиться, что инфраструктура запуска тестов работает:

```cpp
#include <gtest/gtest.h>

TEST(SmokeTest, ArithmeticIsBroken) {
    // Намеренно неверно — будет исправлено на Step 4.
    EXPECT_EQ(2 + 2, 5);
}
```

- [ ] **Step 2: Создать `tests/smoke/CMakeLists.txt`**

```cmake
add_executable(coupecad_smoke_test smoke_test.cpp)

target_link_libraries(coupecad_smoke_test
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_smoke_test)
```

- [ ] **Step 3: Создать `tests/CMakeLists.txt`**

```cmake
find_package(GTest REQUIRED)
add_subdirectory(smoke)
```

- [ ] **Step 4: Сконфигурировать и собрать тест, убедиться, что он падает**

```sh
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default --target coupecad_smoke_test
ctest --preset default -R SmokeTest --output-on-failure
```

Ожидается: 1 тест запущен, **1 fail**. Сообщение содержит `Expected equality of these values: 2 + 2 5`.

- [ ] **Step 5: Исправить тест — теперь он должен пройти**

Заменить тело теста:

```cpp
#include <gtest/gtest.h>

TEST(SmokeTest, ArithmeticWorks) {
    EXPECT_EQ(2 + 2, 4);
}
```

- [ ] **Step 6: Перезапустить и убедиться в зелёном**

```sh
cmake --build --preset default --target coupecad_smoke_test
ctest --preset default -R SmokeTest --output-on-failure
```

Ожидается: 1 тест, 1 pass, exit code 0.

- [ ] **Step 7: Закоммитить**

```sh
git add tests/
git commit -m "test: add GoogleTest scaffold with smoke test"
```

---

## Task 5: Минимальное Qt-приложение `coupecad`

**Files:**
- Create: `apps/coupecad/CMakeLists.txt`
- Create: `apps/coupecad/main.cpp`
- Create: `apps/coupecad/qml/Main.qml`

- [ ] **Step 1: Создать `apps/coupecad/main.cpp`**

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include <iostream>

namespace {
constexpr const char* kAppVersion = "0.1.0";
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CoupeCAD"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setOrganizationName(QStringLiteral("CoupeCAD"));
    app.setOrganizationDomain(QStringLiteral("coupecad.app"));

    // Простой ручной разбор `--version`/`-v` без QCommandLineParser ради
    // минимума зависимостей в Stage 0.
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
            std::cout << "CoupeCAD " << kAppVersion << std::endl;
            return 0;
        }
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    engine.loadFromModule("coupecad", "Main");
    return app.exec();
}
```

- [ ] **Step 2: Создать `apps/coupecad/qml/Main.qml`**

```qml
import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1024
    height: 720
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"
    }
}
```

- [ ] **Step 3: Создать `apps/coupecad/CMakeLists.txt`**

```cmake
qt_add_executable(coupecad
    WIN32
    MACOSX_BUNDLE
    main.cpp
)

qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(coupecad
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

set_target_properties(coupecad PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.coupecad.coupecad"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    # На Linux qt_add_qml_module создаёт подкаталог с именем URI
    # ("coupecad/") в CMAKE_CURRENT_BINARY_DIR. Исполняемый файл легёт
    # по тому же пути, давая link error "Is a directory". Поэтому
    # выносим бинарь на уровень выше, в build/<preset>/bin/.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
```

- [ ] **Step 4: Сконфигурировать и собрать приложение**

```sh
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default --target coupecad
```

Ожидается: бинарь `build/default/bin/coupecad` (на macOS: `build/default/bin/coupecad.app/Contents/MacOS/coupecad`; на Windows: `build/default/bin/coupecad.exe`).

- [ ] **Step 5: Проверить `--version`**

```sh
./build/default/bin/coupecad --version                              # Linux
./build/default/bin/coupecad.app/Contents/MacOS/coupecad --version  # macOS
./build/default/bin/coupecad.exe --version                          # Windows
```

Ожидается: вывод `CoupeCAD 0.1.0`, exit code 0.

- [ ] **Step 6: Проверить запуск окна (визуально)**

```sh
./build/default/bin/coupecad                                          # Linux
open ./build/default/bin/coupecad.app                                 # macOS
./build/default/bin/coupecad.exe                                      # Windows
```

Ожидается: открывается окно 1024×720 с тёмно-серым фоном и заголовком «CoupeCAD». Закрытие окна — exit code 0.

- [ ] **Step 7: Закоммитить**

```sh
git add apps/
git commit -m "feat(app): add minimal Qt Quick application with empty window"
```

---

## Task 6: Smoke-тест запуска приложения через `--version`

**Files:**
- Create: `tests/app/CMakeLists.txt`
- Create: `tests/app/app_version_test.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Написать падающий тест в `tests/app/app_version_test.cpp`**

```cpp
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

#ifdef COUPECAD_BINARY_PATH
constexpr const char* kCoupeCadBinary = COUPECAD_BINARY_PATH;
#else
constexpr const char* kCoupeCadBinary = "coupecad";
#endif

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
};

ProcessResult run(const std::string& command) {
    ProcessResult result;
    std::array<char, 256> buffer{};
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return result;
    }
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.stdout_output.append(buffer.data());
    }
#ifdef _WIN32
    result.exit_code = _pclose(pipe);
#else
    result.exit_code = pclose(pipe);
#endif
    return result;
}

}  // namespace

TEST(AppVersion, PrintsVersionAndExitsZero) {
    const std::string command = std::string("\"") + kCoupeCadBinary + "\" --version";
    const auto result = run(command);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("CoupeCAD 0.1.0"), std::string::npos)
        << "Got: '" << result.stdout_output << "'";
}
```

- [ ] **Step 2: Создать `tests/app/CMakeLists.txt`**

```cmake
add_executable(coupecad_app_version_test app_version_test.cpp)

target_link_libraries(coupecad_app_version_test
    PRIVATE
        GTest::gtest
)

target_compile_definitions(coupecad_app_version_test
    PRIVATE
        COUPECAD_BINARY_PATH="$<TARGET_FILE:coupecad>"
)

add_dependencies(coupecad_app_version_test coupecad)

include(GoogleTest)
gtest_discover_tests(coupecad_app_version_test
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 3: Подключить новый каталог в `tests/CMakeLists.txt`**

Заменить весь файл на:

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
```

- [ ] **Step 4: Собрать и запустить тесты**

```sh
cmake --build --preset default
ctest --preset default --output-on-failure
```

Ожидается: 2 теста запущено (`SmokeTest.ArithmeticWorks` и `AppVersion.PrintsVersionAndExitsZero`), оба зелёные. Exit code 0.

- [ ] **Step 5: Закоммитить**

```sh
git add tests/app/ tests/CMakeLists.txt
git commit -m "test: add smoke test that runs coupecad --version and checks output"
```

---

## Task 7: ARCHITECTURE.md скелет

**Files:**
- Create: `docs/ARCHITECTURE.md`

- [ ] **Step 1: Создать `docs/ARCHITECTURE.md`**

```markdown
# Архитектура CoupeCAD

Этот документ — карта репозитория и логических компонентов. Подробное проектное описание системы см. в [`superpowers/specs/2026-04-18-coupecad-design.md`](superpowers/specs/2026-04-18-coupecad-design.md).

## Структура репозитория

```
CoupeCAD/
├── apps/coupecad/         # Точка входа GUI-приложения; main.cpp + QML.
├── src/coupecad/          # Доменные/инфраструктурные модули (Stage 1+).
│   ├── core/              # Доменная модель проекта (Stage 1).
│   ├── geometry/          # Обёртка над OpenCASCADE (Stage 2).
│   ├── renderer/          # Интерфейс IRenderer + реализации (Stage 3, 11).
│   ├── catalogs/          # Материалы и фурнитура (Stage 6).
│   ├── specs/             # Спецификации, раскрой, присадка (Stage 7-9).
│   ├── io/                # Импорт/экспорт форматов (Stage 10).
│   ├── plugins/           # Plugin host (Stage 12).
│   ├── licensing/         # Лицензионный клиент (Stage 13).
│   └── telemetry/         # Крэш-репорты, опциональная телеметрия (Stage 14).
├── tests/                 # GoogleTest-наборы по модулям.
├── docs/                  # Документация, спеки, планы, архив.
└── .github/workflows/     # CI на GitHub Actions.
```

В Stage 0 каталоги внутри `src/coupecad/` ещё пустые. Они появляются по мере реализации соответствующих стейджей.

## Компонентная карта (по спеке)

| Компонент | Где живёт | Стейдж |
|---|---|---|
| Core (Project, Cabinet, Panel, Hardware, params, undo/redo) | `src/coupecad/core/` | Stage 1 |
| Geometry (OpenCASCADE wrapper, IGeometry) | `src/coupecad/geometry/` | Stage 2 |
| Renderer (IRenderer + OCCT impl) | `src/coupecad/renderer/` | Stage 3 |
| UI (QML, panels, viewport) | `apps/coupecad/qml/` | Stage 4 |
| Templates | `src/coupecad/core/templates/` | Stage 5 |
| Catalogs | `src/coupecad/catalogs/` | Stage 6 |
| Specs & Reports | `src/coupecad/specs/` | Stage 7 |
| Cut-list optimization | `src/coupecad/specs/cutlist/` | Stage 8 |
| Drilling map | `src/coupecad/specs/drilling/` | Stage 9 |
| I/O (.obj, .stl, .dxf, .ccad) | `src/coupecad/io/` | Stage 10 |
| Renderer (Qt Quick 3D impl) | `src/coupecad/renderer/qq3d/` | Stage 11 |
| Plugin host (C++ + Python) | `src/coupecad/plugins/` | Stage 12 |
| Licensing client + backend | `src/coupecad/licensing/` + отдельный репозиторий | Stage 13 |
| Telemetry & crash reporting | `src/coupecad/telemetry/` | Stage 14 |

## Renderer: флаг сборки

Выбор реализации рендера управляется CMake-кешем `COUPECAD_RENDERER` (`occt` | `qq3d`). По умолчанию — `occt`. Реализации появляются в Stage 3 (OCCT) и Stage 11 (Qt Quick 3D); в Stage 0 опция объявлена в корневом `CMakeLists.txt`, но никакая реализация ещё не подключена.

## Тесты

- Unit-тесты модулей живут рядом с модулями (например, `tests/core/`, `tests/geometry/`).
- Smoke-тесты приложения — в `tests/app/`.
- Целевое покрытие Core ≥ 70% (метрика появляется со Stage 1).
```

- [ ] **Step 2: Закоммитить**

```sh
git add docs/ARCHITECTURE.md
git commit -m "docs: add ARCHITECTURE.md skeleton mapping repo layout to spec components"
```

---

## Task 8: GitHub Actions — Linux

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Создать `.github/workflows/ci.yml` с одним job для Linux**

```yaml
name: CI

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-linux:
    name: Linux (Ubuntu 22.04, GCC, Qt 6.7.3)
    runs-on: ubuntu-22.04
    timeout-minutes: 60

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install system packages
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build libgl1-mesa-dev libxkbcommon-dev \
              libxcb-cursor0 libdbus-1-3 xvfb

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install Conan
        run: |
          pip install --upgrade pip
          pip install "conan>=2.0,<3"
          conan profile detect --force

      - name: Cache Conan home
        uses: actions/cache@v4
        with:
          path: ~/.conan2
          key: conan-linux-${{ hashFiles('conanfile.py') }}
          restore-keys: |
            conan-linux-

      - name: Install Qt
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.3'
          host: linux
          target: desktop
          arch: linux_gcc_64
          modules: 'qtshadertools'
          cache: true

      - name: Conan install
        run: conan install . --build=missing -s build_type=Debug

      - name: Configure
        run: cmake --preset default

      - name: Build
        run: cmake --build --preset default

      - name: Test
        env:
          QT_QPA_PLATFORM: offscreen
        run: ctest --preset default --output-on-failure
```

- [ ] **Step 2: Закоммитить и запушить, дождаться зелёного**

```sh
git add .github/workflows/ci.yml
git commit -m "ci: add GitHub Actions workflow for Linux build and tests"
git push -u origin main
```

Ожидается: workflow «CI» запускается, job `build-linux` зелёный (обычно 5–10 минут).

> **Troubleshooting:**
> - **`install-qt-action` не нашёл версию** → проверить, что `6.7.3` доступна для linux/gcc_64 (список: https://ddalcino.github.io/aqt-list-server/).
> - **`find_package(Qt6)` не отработал** → проверить, что `install-qt-action` экспортировал `Qt6_DIR`/`QT_ROOT_DIR` в окружение последующих шагов.
> - **`gtest_discover_tests` не нашёл бинари** → проверить, что `cmake --build` собрал и `coupecad`, и `coupecad_*_test`.

---

## Task 9: GitHub Actions — расширение на macOS

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Добавить job `build-macos` в `.github/workflows/ci.yml`**

После блока `build-linux` (на том же уровне отступа) добавить:

```yaml
  build-macos:
    name: macOS (macos-13, AppleClang, Qt 6.7.3)
    runs-on: macos-13
    timeout-minutes: 60

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install system packages
        run: brew install ninja

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install Conan
        run: |
          pip install --upgrade pip
          pip install "conan>=2.0,<3"
          conan profile detect --force

      - name: Cache Conan home
        uses: actions/cache@v4
        with:
          path: ~/.conan2
          key: conan-macos-${{ hashFiles('conanfile.py') }}
          restore-keys: |
            conan-macos-

      - name: Install Qt
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.3'
          host: mac
          target: desktop
          arch: clang_64
          modules: 'qtshadertools'
          cache: true

      - name: Conan install
        run: conan install . --build=missing -s build_type=Debug

      - name: Configure
        run: cmake --preset default

      - name: Build
        run: cmake --build --preset default

      - name: Test
        env:
          QT_QPA_PLATFORM: offscreen
        run: ctest --preset default --output-on-failure
```

- [ ] **Step 2: Закоммитить и запушить, дождаться зелёного**

```sh
git add .github/workflows/ci.yml
git commit -m "ci: add macOS job to CI workflow"
git push
```

Ожидается: оба job-а (`build-linux`, `build-macos`) зелёные. Troubleshooting — как в Task 8.

---

## Task 10: GitHub Actions — расширение на Windows

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Добавить job `build-windows` в `.github/workflows/ci.yml`**

После блока `build-macos`:

```yaml
  build-windows:
    name: Windows (windows-2022, MSVC 2022, Qt 6.7.3)
    runs-on: windows-2022
    timeout-minutes: 60

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install Conan and Ninja
        run: |
          pip install --upgrade pip
          pip install "conan>=2.0,<3"
          choco install ninja -y
          conan profile detect --force

      - name: Cache Conan home
        uses: actions/cache@v4
        with:
          path: ~/.conan2
          key: conan-windows-${{ hashFiles('conanfile.py') }}
          restore-keys: |
            conan-windows-

      - name: Install Qt
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.3'
          host: windows
          target: desktop
          arch: win64_msvc2019_64
          modules: 'qtshadertools'
          cache: true

      - name: Configure MSVC environment
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64

      - name: Conan install
        run: conan install . --build=missing -s build_type=Debug

      - name: Configure
        run: cmake --preset default

      - name: Build
        run: cmake --build --preset default

      - name: Test
        env:
          QT_QPA_PLATFORM: offscreen
        run: ctest --preset default --output-on-failure
```

- [ ] **Step 2: Закоммитить и запушить, дождаться зелёного**

```sh
git add .github/workflows/ci.yml
git commit -m "ci: add Windows job to CI workflow"
git push
```

Ожидается: все три job-а (`build-linux`, `build-macos`, `build-windows`) зелёные. Это финальная контрольная точка Stage 0 — Definition of Done выполнен. Troubleshooting — как в Task 8.

---

## Финальная проверка Stage 0

После всех задач должно быть истинно:

- [ ] `git log --oneline | wc -l` ≥ 9 коммитов (по одному на задачу + первоначальные).
- [ ] `cmake --list-presets` показывает `default` и `release`.
- [ ] Локально на хост-ОС: `conan install . --build=missing && cmake --preset default && cmake --build --preset default && ctest --preset default` — зелёный.
- [ ] `./build/default/bin/coupecad --version` печатает `CoupeCAD 0.1.0`.
- [ ] Запуск `coupecad` без аргументов открывает пустое тёмное окно.
- [ ] GitHub Actions: workflow `CI` зелёный на трёх ОС.
- [ ] `docs/ARCHITECTURE.md` описывает структуру и стейджи.

После этого можно переходить к Stage 1 (Core domain): новый brainstorming → новый mini-spec → новый план.
