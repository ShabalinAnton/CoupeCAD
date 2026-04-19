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

Qt ставится отдельно от Conan (причина — в плане Stage 0). Самый простой способ для разработки — `aqtinstall`:

```sh
pipx install aqtinstall
aqt install-qt mac desktop 6.7.3 clang_64 -m qtshadertools           # macOS
# aqt install-qt linux desktop 6.7.3 gcc_64 -m qtshadertools         # Linux
# aqt install-qt windows desktop 6.7.3 win64_msvc2022_64 -m qtshadertools  # Windows
export Qt6_DIR=$PWD/6.7.3/macos/lib/cmake/Qt6                        # путь зависит от ОС
```

В CI Qt ставится через `jurplel/install-qt-action`.

## Сборка и запуск

```sh
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
./build/default/apps/coupecad/coupecad        # Linux/macOS
./build/default/apps/coupecad/coupecad.exe    # Windows
```

## Структура проекта

См. `docs/ARCHITECTURE.md`.

## Лицензия

Проприетарная. Конкретные условия лицензирования будут добавлены в LICENSE до публичного релиза.
