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
