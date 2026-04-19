# CoupeCAD

Кроссплатформенное приложение для проектирования корпусной мебели (шкафов, шкафов-купе, гардеробов, кухонных гарнитуров). См. видение и архитектуру в [`docs/superpowers/specs/2026-04-18-coupecad-design.md`](docs/superpowers/specs/2026-04-18-coupecad-design.md).

## Статус

Stage 0 (Bootstrap). Проект только начат. Работающего функционала пока нет — в репозитории каркас сборки, который собирает и запускает пустое окно.

## Требования к окружению

- CMake **3.25+** (требуется для CMakePresets v6)
- C++20 компилятор: MSVC 2022, Clang 14+, GCC 11+
- Ninja
- Conan **2.x**

Qt 6 и все остальные C++-зависимости подтягиваются через Conan (см. `conanfile.py`), отдельно устанавливать Qt не требуется.

## Первая сборка (важно)

Qt в Conan Center скомпилирован только для части профилей. Если ваш профиль совпадает с предсобранным — `conan install` завершится за секунды. Если нет — Conan соберёт Qt из исходников (2–4 часа, потребуется ~30 ГБ свободного места на диске). Результат ляжет в `~/.conan2` и переиспользуется для всех последующих сборок.

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
