# Stage 1 — Core domain (CoupeCAD)

Дата: 2026-04-20
Статус: черновик v1, согласован с владельцем продукта в ходе brainstorming.
Место в общем видении: см. `2026-04-18-coupecad-design.md` (общий спек) и `../plans/2026-04-18-stage-0-bootstrap.md` (предыдущий стейдж).

Этот документ описывает **доменную модель CoupeCAD**: структуры данных, команды изменения с undo/redo, формат файла проекта `.ccad`, публичный API и тестирование. Результат Stage 1 — работающий модуль `src/coupecad/core/`, полностью Qt-free, покрытый GoogleTest-ами, служащий фундаментом для Stage 2 (Geometry), Stage 4 (UI) и далее.

---

## 1. Цель и границы Stage 1

**Что Stage 1 производит.** Доменную модель в `src/coupecad/core/` (структуры данных Project → Cabinet → Panels + Hardware + Materials, команды изменения с undo/redo, чтение/запись `.ccad`) **и** инфраструктурный модуль логирования в `src/coupecad/logging/` (см. §6). Оба Qt-free, покрыты unit-тестами.

**Что НЕ входит.** Геометрия (OpenCASCADE — Stage 2), рендер (Stage 3+), UI (Stage 4+), реальные каталоги материалов и фурнитуры (Stage 6). Core оперирует только «логической» моделью: роль панели, её параметры, материал, кромка. Никакого реального 3D.

**Definition of Done.**

- Программно создаётся `Project` с одним `Cabinet`, добавляются панели по ролям (top/bottom/side/back/shelf/facade/...), назначаются материалы и кромка.
- Любая дискретная правка оборачивается в `Command`; интерактивная — в `PreviewableCommand` с `begin/update/commit/cancel`. И то и другое участвует в undo/redo (одна запись на интерактивную сессию).
- Сохранение в `.ccad`-файл и открытие из него дают байт-в-байт тот же проект (при детерминированных UUID и timestamp, как в тестах).
- Логгер `coupecad::logging::Logger` пишет в console и в файл с ротацией; уровень/sinks конфигурируются программно; покрыт тестами.
- Покрытие тестами `coupecad_core` ≥ 70% (lines), `coupecad_logging` ≥ 70%.

**Scope.** Stage 1 плотный (оценочно 5-6 недель с учётом логирования). Концерны (entities, commands/undo, serialization) тесно связаны, поэтому всё описано **одним спеком**. Имплементационный план разделит работу на подстейджи: **Stage 1a** — `coupecad_logging` + entities + неизменяющие read API; **Stage 1b** — commands + undo stack (discrete + preview/commit); **Stage 1c** — `.ccad` I/O. Логирование вперёд, чтобы Core и тесты сразу могли его использовать. Каждый подстейдж заканчивается набором зелёных тестов.

---

## 2. Доменная модель

### 2.1 Корневые сущности

```
Project
├── meta: {name, description}
├── cabinet: Cabinet                             (ровно один в v1)
├── materials: Map<MaterialId, Material>         (пользовательские)
└── hardware_catalog: Map<HardwareRef, HardwareSpec>
```

`Project` — aggregate root. Все мутации идут через `UndoStack`, ассоциированный с конкретным `Project`. Вне команд `Project` предоставляет только const-accessors для чтения.

**Cabinet.**

| Поле | Тип | Описание |
|---|---|---|
| `id` | `CabinetId` | UUID |
| `name` | `string` | Человекочитаемое имя |
| `dimensions` | `Dimensions` = `{width, depth, height: Millimeters}` | Внешние габариты (см. §2.3 о маппинге на оси X/Y/Z) |
| `default_panel_material` | `MaterialId` | Материал корпусных деталей по умолчанию |
| `default_panel_thickness` | `Millimeters` | Обычно 16 или 18 мм |
| `default_back_thickness` | `Millimeters` | Обычно 3-4 мм для ХДФ |
| `panels` | `Map<PanelId, Panel>` | Все панели шкафа |
| `hardware` | `Map<HardwareItemId, HardwareItem>` | Фурнитура |

**Panel.**

| Поле | Тип | Описание |
|---|---|---|
| `id` | `PanelId` | UUID |
| `role` | `PanelRole` (enum) | См. §2.2 |
| `role_params` | `RoleParams` (variant) | Параметры, зависящие от роли |
| `material_override` | `optional<MaterialId>` | Если пусто — `cabinet.default_panel_material` |
| `thickness_override` | `optional<Millimeters>` | Если пусто — `cabinet.default_panel_thickness` |
| `edge_banding` | `{front, back, left, right: optional<EdgeBanding>}` | Кромка по четырём сторонам |
| `grain_direction` | `GrainDirection` (enum: `Horizontal`, `Vertical`, `None`) | Ориентация текстуры |
| `label` | `optional<string>` | Ярлык для спецификации |

**Material.**

| Поле | Тип | Описание |
|---|---|---|
| `id` | `MaterialId` | UUID |
| `name` | `string` | «ЛДСП Egger H3331 ST10 Thermopal Cognac» |
| `kind` | `MaterialKind` (enum: `ChipboardLaminated`, `Mdf`, `Hdf`, `Plywood`, `SolidWood`, `Glass`, `Metal`, `Other`) | Тип плиты |
| `default_thickness` | `Millimeters` | Обычная толщина этого материала |
| `color_hint` | `RGBA` | Для Stage 3+ визуализации; в Core хранится |
| `texture_ref` | `optional<string>` | Путь внутри `assets/` в `.ccad`, опционально |
| `price_per_sqm` | `optional<Money>` | Для отчётов; опционально |

**HardwareItem** (экземпляр фурнитуры в шкафу):

| Поле | Тип | Описание |
|---|---|---|
| `id` | `HardwareItemId` | UUID |
| `ref` | `HardwareRef` | Ключ карточки в `project.hardware_catalog` |
| `attachments` | `list<PanelAttachment>` | К каким панелям крепится |
| `label` | `optional<string>` | Ярлык |

`PanelAttachment = {panel_id: PanelId, local_position: Vec3, orientation: Quat}`. Локальная СК панели — см. §2.3.

**HardwareSpec** (карточка из каталога):

| Поле | Тип | Описание |
|---|---|---|
| `ref` | `HardwareRef` | Ключ (строка, напр. `"hinge.generic.straight"`) |
| `kind` | `HardwareKind` (enum: `Hinge`, `DrawerSlide`, `Handle`, `ShelfSupport`, `Connector`, `GasLift`, `Other`) | Категория |
| `name` | `string` | Человекочитаемое имя |
| `sku` | `optional<string>` | Артикул производителя (пусто для generic) |
| `bbox` | `Vec3` | Габариты для визуализации и collision-check |
| `price_each` | `optional<Money>` | Цена за единицу для отчётов |

В v1 `hardware_catalog` заполняется generic-записями, не привязанными к брендам. Брендированные каталоги — платная фича roadmap (Stage 6).

### 2.2 Роли панелей и `RoleParams`

`PanelRole` — enum ролей: `Top`, `Bottom`, `SideLeft`, `SideRight`, `Back`, `Shelf`, `DividerVertical`, `DividerHorizontal`, `Facade`, `DrawerBottom`, `DrawerFront`, `DrawerSide`, `DrawerBack`, `Plinth`, `Custom`.

`RoleParams` — `std::variant` один-на-роль:

- `Top`, `Bottom`, `Back`, `SideLeft`, `SideRight` — без параметров. Позиция и размеры выводятся из `Cabinet.dimensions` и толщин.
- `Shelf`: `{height_from_bottom: Millimeters, extent: ShelfExtent}` где `ShelfExtent = FullWidth | BetweenDividers{from: PanelId, to: PanelId}` (референсы на панели с ролью `DividerVertical`).
- `DividerVertical`: `{offset_from_left: Millimeters, height_extent: VerticalExtent}` (может быть Full или From/To).
- `DividerHorizontal`: `{offset_from_bottom: Millimeters, depth_extent: DepthExtent}`.
- `Facade`: `{extent: FacadeExtent, hinge_side: HingeSide}` где `HingeSide = Left | Right | Top | Bottom | None`.
- `DrawerBottom`, `DrawerFront`, `DrawerSide`, `DrawerBack`: параметры ящика (глубина, высота, смещение).
- `Plinth`: `{height: Millimeters, setback: Millimeters}`.
- `Custom`: `{position: Vec3, size: Vec3, orientation: Quat}` — free-form opt-out.

**Реактивность.** Производные величины (фактические x/y/z/w/h/d панели) **не хранятся**. Функция `compute_panel_geometry(const Cabinet&, const Panel&) -> PanelGeometry` пересчитывает их по запросу из `role + role_params + cabinet.dimensions + толщин`. Изменение `cabinet.width` автоматически меняет производную геометрию всех зависимых панелей при следующем вызове.

### 2.3 Система координат

Cabinet-local, **правосторонняя, Z-up** (CAD-конвенция, как в AutoCAD/Blender):

- **X** — вправо (вдоль ширины шкафа).
- **Y** — вглубь, от зрителя к задней стенке (вдоль глубины шкафа).
- **Z** — вверх (вдоль высоты шкафа).

Origin — **левый-нижний-передний угол** внешних габаритов (`x=0, y=0, z=0` — точка, ближайшая к зрителю в левом нижнем углу). Задняя стенка лежит в плоскости `y = cabinet.depth`.

В `Cabinet.dimensions = {width, depth, height}` поля по-прежнему семантические (а не «X/Y/Z»); маппинг: `width → X`, `depth → Y`, `height → Z`.

Единицы — **миллиметры, целочисленно** (`Millimeters` = newtype над `int32_t`). Плавающая точка в Core не используется — избавляет от накопления ошибок при undo/redo и сериализации. Geometry-модуль в Stage 2 при необходимости переводит в double для OpenCASCADE.

Кватернионы `Quat = {w, x, y, z: double}` используются только в `Custom`-панели и `HardwareItem.orientation`. Для role-based панелей произвольное вращение не требуется.

---

## 3. Команды и undo/redo

### 3.1 Command-интерфейс

```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual ChangeSet apply(Project& project) = 0;
    virtual ChangeSet revert(Project& project) = 0;
    virtual std::string_view label() const noexcept = 0;
    virtual CommandKind kind() const noexcept = 0;
};

class PreviewableCommand : public Command {
public:
    // Изменить значение «на лету» в рамках активного preview-сеанса.
    // См. §3.3.2.
    virtual ChangeSet update(Project& project, const std::any& new_value) = 0;
};
```

`ChangeSet` описывает, какие сущности затронуты (см. §3.5). Возвращается и из `apply`, и из `revert`, и из `update`, чтобы `UndoStack` мог транслировать его наблюдателям.

Команды, которым имеет смысл интерактивная правка (слайдеры, drag), наследуются от `PreviewableCommand`. Дискретные операции (add/remove) — от обычного `Command`.

Команды хранят только **UUIDs и дельты** (before/after), а не ссылки на сущности. Это делает их:

- устойчивыми к пересозданию `Project` (важно при undo после reload);
- потенциально сериализуемыми (задел для сохранения undo-стека в будущем).

### 3.2 Набор команд v1

**Cabinet:** `SetCabinetDimensions`, `SetCabinetDefaults`.

**Panel:** `AddPanel(role, role_params, material_override?)`, `RemovePanel(id)`, `UpdatePanelRoleParams(id, new_params)`, `SetPanelMaterial(id, material_id?)`, `SetPanelThickness(id, thickness?)`, `SetPanelEdgeBanding(id, side, banding?)`, `SetPanelLabel(id, label?)`, `SetPanelGrain(id, direction)`.

**Hardware:** `AddHardware(ref, attachments)`, `RemoveHardware(id)`, `UpdateHardwareAttachments(id, new_attachments)`.

**Material:** `AddMaterial`, `UpdateMaterial`, `RemoveMaterial` (с проверкой, что материал не используется).

**Hardware catalog:** `AddHardwareSpec`, `UpdateHardwareSpec`, `RemoveHardwareSpec`.

**Composite:** `MacroCommand` — упорядоченный список команд, применяется и откатывается атомарно. Используется для шаблонов и групповых операций («добавить 5 полок разом»).

Каждая команда снимает дамп старого состояния в своём конструкторе или в первом `apply()`, чтобы `revert()` был однозначным. Для `Remove*`-команд дамп удаляемого объекта снимается перед удалением.

Команды создания (`Add*`) назначают новые UUID детерминированно через `UuidGenerator`, параметризуемый извне — это позволяет тестам использовать seed-based генератор и получать воспроизводимые ID. ID доступен через `cmd.assigned_id()` после конструктора.

### 3.3 UndoStack

Две парадигмы правки:

- **Discrete** — команда применяется атомарно и сразу попадает в стек. Подходит для дискретных действий: «добавить полку», «удалить панель», «выбрать материал из списка», «нажать +1 у счётчика ящиков».
- **Live (preview)** — пользователь интерактивно крутит значение (слайдер, drag), модель меняется на лету для визуальной обратной связи, но **в стек ничего не попадает**. Только когда пользователь явно подтверждает результат («apply», release слайдера, Enter), на стек кладётся **одна** запись с дельтой `initial → final`. Если пользователь отменяет правку, изменение откатывается без записи в стек.

```cpp
class UndoStack {
public:
    explicit UndoStack(Project& project);

    // Discrete-режим.
    void execute(std::unique_ptr<Command> cmd);

    // Live-режим (preview/commit).
    PreviewHandle begin_preview(std::unique_ptr<PreviewableCommand> cmd);
    void          update_preview(PreviewHandle&, std::any new_value);
    void          commit_preview(PreviewHandle&);   // → одна запись в стеке
    void          cancel_preview(PreviewHandle&);   // → откат, стек не трогается

    // Стандартные операции.
    void undo();
    void redo();
    bool can_undo() const;
    bool can_redo() const;
    std::span<const std::string_view> undo_labels() const;
    void clear();

    // Макросы (для шаблонов и групповых дискретных операций).
    void begin_macro(std::string_view label);
    void end_macro();

    // Наблюдатели.
    void add_observer(IProjectObserver*);
    void remove_observer(IProjectObserver*);

private:
    Project& project_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::optional<MacroBuilder> macro_;
    std::optional<ActivePreview> active_preview_;
    std::vector<IProjectObserver*> observers_;
};
```

#### 3.3.1 Discrete

`execute(cmd)`:
1. Запрещено, если активен preview (`active_preview_.has_value()`) — бросает `LogicError::PreviewActive` (вызывающий код должен сначала commit или cancel).
2. Вызывает `cmd->apply(project_)`, получает `ChangeSet`.
3. Пушит `cmd` в `undo_`, очищает `redo_`.
4. Транслирует `ChangeSet` наблюдателям.

`undo()` — поп из `undo_`, `revert(project_)`, пуш в `redo_`, события. `redo()` — симметрично.

#### 3.3.2 Live (preview/commit)

Подмножество команд реализует расширенный интерфейс:

```cpp
class PreviewableCommand : public Command {
public:
    // Применить начальное значение (= зафиксированное на момент begin_preview)
    // и запомнить «initial» снимок для отката/коммита.
    ChangeSet apply(Project&) override = 0;

    // Изменить значение «на лету»: мутирует Project в новое состояние,
    // не трогая зафиксированный «initial». Возвращает ChangeSet.
    virtual ChangeSet update(Project&, const std::any& new_value) = 0;

    // Откатить Project обратно в «initial».
    ChangeSet revert(Project&) override = 0;
};
```

Сценарий:

1. `begin_preview(cmd)`:
   - Запрещено, если уже активен preview.
   - `cmd->apply(project_)` — переводит Project в начальное состояние правки, фиксирует «initial» внутри команды (это и есть `old_value` для будущей записи в стеке).
   - Возвращает `PreviewHandle` (opaque, привязан к UndoStack).
   - События наблюдателям.

2. `update_preview(handle, new_value)` (вызывается многократно):
   - `cmd->update(project_, new_value)` — Project мутируется в текущее «live» значение.
   - События наблюдателям.
   - **В стек ничего не пишется.**

3. `commit_preview(handle)`:
   - Текущее состояние Project — это «final value». Команда уже знает свой `initial`.
   - Команда (с уже зафиксированными initial и final) кладётся в `undo_`.
   - `redo_` очищается.
   - **Один пуш в стек на всю серию update'ов.**

4. `cancel_preview(handle)`:
   - `cmd->revert(project_)` — Project возвращается в «initial».
   - Команда уничтожается.
   - **В стек ничего не пишется.**

Какие команды должны быть `PreviewableCommand` в v1: `SetCabinetDimensions`, `UpdatePanelRoleParams`, `SetPanelThickness`. Остальные (add/remove material, add/remove panel, add/remove hardware, edge banding по сторонам) — дискретные: их интерактивно «крутить» нечего.

Активный preview — **строго один** в моменте (одна интерактивная сессия). Это исключает гонки в Stage 4 (UI-сторона должна сама гарантировать завершение текущего preview перед началом следующего).

#### 3.3.3 Макросы

`begin_macro(label)` открывает накопление дискретных команд. Все `execute()` между `begin_macro/end_macro` добавляются в строящийся `MacroCommand`, который по закрытию кладётся в стек как единое целое. Live preview внутри макроса запрещён (бросает `LogicError::PreviewInsideMacro`) — макрос для атомарных композиций, не для интерактивных правок.

#### 3.3.4 Persistence

В v1 undo/redo стеки **не сохраняются** в `.ccad`. Открытие файла → пустой стек. Это упрощает формат и соответствует поведению большинства CAD. Активный preview в `.ccad` тоже не сохраняется (если пользователь сохранит во время preview — сохранится текущее «live» состояние Project как обычный snapshot, без записи в стек).

### 3.4 Валидация

Проверки — **внутри `Command::apply()`**, до мутации. При нарушении бросается `DomainError` (подкласс `CoupecadException`). Типичные проверки:

- Отрицательные или нулевые размеры/толщины.
- Панель, чей bounding box не помещается в `cabinet.dimensions` после пересчёта геометрии.
- Удаление `Material`, ещё используемого какой-либо `Panel` или `Cabinet.default_*`.
- Добавление hardware attachment к несуществующему `PanelId`.
- Неверная комбинация `role + role_params` (напр. `Shelf` без `role_params` типа `ShelfParams`).

Команды атомарны: либо полностью применяются, либо совсем не применяются. При исключении в `apply()` состояние `Project` не должно быть частично изменённым — это достигается тем, что команды сначала валидируют всё, потом мутируют.

### 3.5 События изменения (`IProjectObserver`)

Для Stage 4+ UI:

```cpp
class IProjectObserver {
public:
    virtual ~IProjectObserver() = default;
    virtual void on_changed(const Project&, const ChangeSet&) = 0;
};

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
};
```

`UndoStack` вызывает `on_changed()` после успешного `apply`/`revert`, передавая `ChangeSet`, возвращённый командой.

В v1 ChangeSet не детализирует, какие именно поля панели изменились — только факт. Углубление — по мере необходимости в Stage 4+.

---

## 4. Формат `.ccad`

### 4.1 Контейнер

ZIP-архив (deflate), расширение `.ccad`. Внутренняя структура:

```
project.ccad
├── meta.json                # версия формата, encoding, checksums
├── project.json             # вся доменная модель
├── assets/                  # пользовательские текстуры, content-addressable
│   └── <sha256>.<ext>
└── drawings/                # (будущее) цеховые DXF, см. §4.9
```

ZIP выбран не ради сжатия (JSON компактный), а ради инкапсуляции binary-ассетов в один переносимый файл.

### 4.2 `meta.json`

```json
{
    "schema_version": 1,
    "content_encoding": "json",
    "app_version": "0.1.0",
    "created_at": "2026-04-20T10:15:00Z",
    "modified_at": "2026-04-20T12:34:56Z",
    "checksum_project_data": "sha256:..."
}
```

- `schema_version`: целое число, монотонно растущее. Миграции — линейная цепочка `N → N+1` (§4.4).
- `content_encoding`: `"json"` в v1. Задел на бинарные форматы — §4.8.
- `checksum_project_data`: SHA-256 от содержимого файла данных (`project.json` в v1); детектор повреждения архива.

### 4.3 `project.json` (схема v1)

UTF-8 без BOM, отступ 2 пробела (git-friendly diff). Все UUID — canonical hex, строчные, без фигурных скобок. Пустые `optional`-поля — **явные `null`**, не опускаются.

```json
{
    "meta": {
        "name": "Мой шкаф-купе",
        "description": ""
    },
    "cabinet": {
        "id": "a3f8e2c1-...",
        "name": "Cabinet",
        "dimensions_mm": {"width": 2400, "depth": 600, "height": 2400},
        "default_panel_material": "...",
        "default_panel_thickness_mm": 16,
        "default_back_thickness_mm": 4,
        "panels": [
            {
                "id": "...",
                "role": "Shelf",
                "role_params": {
                    "height_from_bottom_mm": 800,
                    "extent": {"kind": "FullWidth"}
                },
                "material_override": null,
                "thickness_override": null,
                "edge_banding": {
                    "front": {"material_id": "...", "thickness_mm": 2},
                    "back": null,
                    "left": null,
                    "right": null
                },
                "grain_direction": "Horizontal",
                "label": null
            }
        ],
        "hardware": [
            {
                "id": "...",
                "ref": "hinge.generic.straight",
                "attachments": [
                    {
                        "panel_id": "...",
                        "local_position_mm": [10, 100, 8],
                        "orientation": [1.0, 0.0, 0.0, 0.0]
                    }
                ],
                "label": null
            }
        ]
    },
    "materials": [
        {
            "id": "...",
            "name": "ЛДСП 16мм белый",
            "kind": "ChipboardLaminated",
            "default_thickness_mm": 16,
            "color_hint": [240, 240, 240, 255],
            "texture_ref": null,
            "price_per_sqm": null
        }
    ],
    "hardware_catalog": [
        {
            "ref": "hinge.generic.straight",
            "kind": "Hinge",
            "name": "Петля прямая 90°",
            "sku": null,
            "bbox_mm": [35, 14, 60],
            "price_each": null
        }
    ]
}
```

Сериализация **детерминированная**: массивы сортируются по `id`/`ref`, ключи объектов упорядочены по фиксированной схеме. Это нужно для воспроизводимых тестов round-trip и для осмысленных diff'ов.

### 4.4 Миграции схемы

```cpp
class SchemaMigration {
public:
    virtual int from_version() const = 0;
    virtual int to_version() const = 0;     // обычно from+1
    virtual nlohmann::json apply(nlohmann::json) = 0;
};
```

Процесс загрузки:

1. Прочитать `meta.schema_version`.
2. `> current` → `FileFormatError::UnsupportedVersion` («файл от более новой версии приложения»).
3. `== current` → парсить напрямую.
4. `< current` → прогнать цепочку `[v → v+1 → ... → current]` над JSON'ом, затем парсить.

Миграции пишутся **раз** при bump-е схемы и никогда не изменяются. Тесты фиксируют входы/выходы каждой миграции.

В v1 `schema_version = 1`, миграций нет — это заготовка.

### 4.5 JSON-библиотека

**`nlohmann::json`** через Conan (`nlohmann_json/3.11.3+`). Причины: header-only, не тянет Qt, отличная ergonomика, нативная поддержка CBOR/MessagePack для будущего (§4.8). На Conan Center есть прекомпилированный пакет (0 сборки).

### 4.6 Обработка ошибок при чтении

Все — subclasses `FileFormatError` (подкласс `CoupecadException`):

| Код | Когда |
|---|---|
| `CorruptedArchive` | ZIP битый |
| `MissingManifest` | Нет `meta.json` или `project.json` |
| `ChecksumMismatch` | `checksum_project_data` не совпал (warning, не блокер) |
| `UnsupportedVersion` | `schema_version > current` |
| `UnsupportedEncoding` | `content_encoding` не в известном списке |
| `InvalidData` | JSON невалиден или не соответствует схеме; содержит путь (`$.cabinet.panels[3].role`) |

UI в Stage 4+ маппит их в человекопонятные сообщения.

### 4.7 Assets

Пользовательские текстуры (если пользователь загрузит PNG/JPG для своего материала) хранятся в `assets/<sha256>.<ext>`. `Material.texture_ref` содержит имя файла внутри архива. Content-addressable — один и тот же файл не задваивается.

Garbage collection на save: ассеты, не упомянутые ни одним `texture_ref`, при сохранении не переносятся в новый архив.

### 4.8 Бинарный формат (задел на будущее)

Для крупных проектов JSON-сериализация может стать узким местом по времени. В будущей версии схемы планируется альтернативный бинарный формат (кандидаты: CBOR, MessagePack, FlatBuffers). **В v1 не реализуем — YAGNI.** Заложенные в v1 точки, которые сделают переход безболезненным:

- Поле `meta.content_encoding` — в v1 всегда `"json"`, явно записывается при сохранении. Незнакомое значение → `UnsupportedEncoding`.
- Интерфейс `ProjectSerializer` с парой методов `serialize(const Project&) -> bytes` / `deserialize(bytes) -> Project`. В v1 одна реализация `JsonProjectSerializer`. Альтернативная реализация добавляется соседним классом — `Project`, `Command`, `UndoStack` не трогаются.
- Миграционная цепочка (§4.4) — schema-level, независимая от encoding. Бинарный формат при чтении приводится к in-memory JSON-like структуре (`nlohmann::json::from_cbor` и т. п.), затем прогоняется через те же миграции.
- Расширение имени файла остаётся `.ccad`; encoding определяется `content_encoding` в `meta.json`, не именем файла.

### 4.9 Встроенные DXF-чертежи (задел на будущее)

В будущем `.ccad` может содержать **цеховые DXF-чертежи** (развёртки панелей, карты присадки, сборочные виды), сгенерированные Specs & Reports (Stage 7-9) или импортированные пользователем как контекст (платная фича). Структура архива заложена на это уже в v1, но Core их не пишет и не читает:

- Подкаталог **`drawings/`** внутри архива, параллельно `assets/`. Ассеты и чертежи разведены намеренно: у них разная модель версионирования (ассет — переиспользуемый content-addressable; чертёж — производный артефакт конкретной панели, перегенерируется при изменении её геометрии).
- Именование будущих чертежей: `drawings/panels/<panel_id>/flat.dxf` (развёртка), `drawings/panels/<panel_id>/drilling.dxf` (карта присадки), `drawings/assembly.dxf` (сборочный чертёж).
- У `Panel` в `project.json` будет optional `drawings: {flat: path, drilling: path, ...}`. В v1 поле отсутствует; Reader обязан принять его отсутствие как валидное.
- Импортированные пользователем сторонние DXF-файлы — `drawings/imports/<user_filename>.dxf`, с записью о них в `project.json`.
- Garbage collection на save (§4.7) применяется и к `drawings/`: удаление панели → удаление её чертежей.

---

## 5. API Core и тестирование

### 5.1 Структура namespace и заголовков

Всё в `coupecad::core::`. Файловое разделение по ответственности (не по типам):

```
src/coupecad/core/
├── CMakeLists.txt
├── id.h                  # strong-type wrappers для UUID
├── units.h               # Millimeters, Money, RGBA, Vec3, Quat
├── material.h/.cpp
├── hardware.h/.cpp
├── panel.h/.cpp
├── cabinet.h/.cpp
├── project.h/.cpp        # Project + IProjectObserver
├── geometry.h/.cpp       # compute_panel_geometry() и derived-величины
├── commands/
│   ├── command.h
│   ├── cabinet_commands.h/.cpp
│   ├── panel_commands.h/.cpp
│   ├── hardware_commands.h/.cpp
│   ├── material_commands.h/.cpp
│   └── macro_command.h/.cpp
├── undo_stack.h/.cpp
├── errors.h              # CoupecadException hierarchy
└── io/
    ├── project_serializer.h      # interface ProjectSerializer
    ├── json_project_serializer.h/.cpp
    ├── schema_migration.h/.cpp   # (в v1 без миграций)
    └── ccad_archive.h/.cpp       # ZIP read/write, meta.json
```

Каждый файл — одна ответственность, компактный и удобный для независимой правки. Границы внутри Core мягкие (всё публично для коллег-файлов в `core/`); между Core и внешним миром (Geometry, UI, Plugins) — строгие: внешний код работает только через `project.h` + `commands/*.h` + `io/*.h`.

### 5.2 CMake target

```cmake
# src/coupecad/core/CMakeLists.txt
add_library(coupecad_core STATIC
    project.cpp
    cabinet.cpp
    panel.cpp
    material.cpp
    hardware.cpp
    geometry.cpp
    undo_stack.cpp
    commands/cabinet_commands.cpp
    commands/panel_commands.cpp
    commands/hardware_commands.cpp
    commands/material_commands.cpp
    commands/macro_command.cpp
    io/json_project_serializer.cpp
    io/schema_migration.cpp
    io/ccad_archive.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        nlohmann_json::nlohmann_json
        coupecad_logging
    PRIVATE
        libzip::libzip
        stduuid::stduuid
)
```

**`coupecad_core` не линкуется ни с Qt, ни с OpenCASCADE.** Публичные зависимости — `nlohmann_json` (типы из API сериализации) и `coupecad_logging` (любой пользователь Core может логгировать через ту же подсистему). Приватные — `libzip` и `stduuid` (детали реализации io). Новые Conan-зависимости добавятся в `conanfile.py` по подстейджам.

### 5.3 Пример клиентского кода

```cpp
using namespace coupecad::core;

Project project = Project::create_empty("My wardrobe");
UndoStack undo{project};

// Материал.
auto mat_cmd = std::make_unique<AddMaterial>(Material{
    .name = "ЛДСП 16мм",
    .kind = MaterialKind::ChipboardLaminated,
    .default_thickness = Millimeters{16},
    .color_hint = RGBA{240, 240, 240, 255}});
MaterialId mat_id = mat_cmd->assigned_id();
undo.execute(std::move(mat_cmd));

// Cabinet.
undo.execute(std::make_unique<SetCabinetDimensions>(
    project.cabinet().id(),
    Dimensions{.width = 2400, .depth = 600, .height = 2400}));
undo.execute(std::make_unique<SetCabinetDefaults>(
    project.cabinet().id(), mat_id, Millimeters{16}, Millimeters{4}));

// Полка.
auto add_shelf = std::make_unique<AddPanel>(
    PanelRole::Shelf,
    ShelfParams{.height_from_bottom = Millimeters{800}, .extent = FullWidth{}});
PanelId shelf_id = add_shelf->assigned_id();
undo.execute(std::move(add_shelf));

// Производная геометрия.
auto geom = compute_panel_geometry(project.cabinet(), project.cabinet().panel(shelf_id));
// geom.size.width == 2368 мм (2400 - 2*16)

// Сохранить / открыть.
JsonProjectSerializer s;
CcadArchive::save(project, "my_wardrobe.ccad", s);
Project loaded = CcadArchive::load("my_wardrobe.ccad", s);
```

### 5.4 Тестирование

Все тесты — GoogleTest, живут в `tests/core/<subsystem>/`.

**Уровни тестов:**

- **Unit-тесты модели** (`tests/core/model/`). Конструкторы, инварианты, операции на отдельных сущностях. Примеры: «`Material` с отрицательной толщиной бросает», «`ShelfExtent::BetweenDividers` с несуществующими id бросает при валидации».
- **Unit-тесты команд** (`tests/core/commands/`). Для каждой команды: `apply` меняет состояние так-то, `revert` возвращает точно в прежнее состояние (сравнение через round-trip сериализацию снимка), ошибки валидации бросают правильный `DomainError`.
- **Undo-stack тесты** (`tests/core/undo/`). Сценарии: 10 execute → 10 undo → модель равна исходной; execute → undo → execute нового → redo пустой; merge-поведение с mockable clock; вложенные макросы.
- **Geometry compute тесты** (`tests/core/geometry/`). `compute_panel_geometry` для каждой роли, edge cases: shelf у верха/низа, divider на всю высоту, custom-панель с rotation, cabinet с zero-depth.
- **Serializer round-trip тесты** (`tests/core/io/`). `Project → serialize → deserialize → equal`. Golden JSON-файлы в `tests/core/io/golden/` фиксируют формат; изменение формата требует осознанного обновления golden-файла.
- **Archive тесты** (`tests/core/io/archive/`). ZIP корректно пишется/читается, assets (dummy PNG) кладутся/извлекаются, checksum-детекция. Миграционный пайплайн проверяется синтетическим тестом: фейковая миграция `v0 → v1`, прописанная только в тестовом коде, прогоняется над тестовым JSON'ом с `schema_version=0` и даёт ожидаемый v1. В production-коде v1 цепочка миграций пустая — реальный тест для реальной миграции появится при bump'е схемы.

**Детерминизм.** Для воспроизводимости:

- `UuidGenerator` — параметр класса `Project` и команд. В production — `stduuid::uuid_random_generator`. В тестах — seed-based.
- Timestamps в `meta.json` — через mockable `IClock`.
- Порядок сериализации — стабильный (sort by UUID).

**Coverage.** Целевое покрытие `coupecad_core` ≥ 70% (lines) по `llvm-cov`. Метрика прогоняется в CI как отдельный job (warning only, без блокировки PR). Этот job добавляется в Stage 1c.

---

## 6. Логирование

CoupeCAD-у нужен унифицированный логгер: ошибки, варнинги, диагностика реактивных пересчётов, события open/save файла, аномалии в данных. Используется и Core'ом (для DomainError-выбросов и валидационных предупреждений), и в будущем — UI/рендером/I/O. Поэтому логгер — **отдельный модуль `src/coupecad/logging/`**, не внутри Core, и Core зависит от него (а не наоборот).

### 6.1 Уровни

Шесть стандартных уровней (от тихого к шумному):

| Уровень | Когда |
|---|---|
| `Off` | Логирование выключено целиком (для тестов, перфоманс-режима). |
| `Critical` | Невосстановимая ошибка: повреждён файл, недоступный ресурс, программная инвариантная ошибка. После такого вызова часто следует exception. |
| `Error` | Восстановимая ошибка: операция отвалилась, но приложение продолжает работать (валидация команды, сетевая ошибка обновления подписки). |
| `Warning` | Подозрительное состояние, не блокирующее: устаревший формат, потенциальная потеря данных, чек-сумма не сошлась. |
| `Info` | Существенное событие нормального хода: «открыт проект X», «сохранён `.ccad`», «применена команда такая-то». |
| `Debug` | Тонкая диагностика: входы/выходы команд, размеры структур, переходы состояний. По умолчанию выключено. |
| `Trace` | Очень шумно: каждый пересчёт геометрии, каждое событие observer'а. Для разработки конкретной фичи. |

`Off < Critical < Error < Warning < Info < Debug < Trace`.

### 6.2 API

```cpp
namespace coupecad::logging {

enum class Level { Off, Critical, Error, Warning, Info, Debug, Trace };

class Logger {
public:
    static Logger& instance();   // process-wide singleton (см. §6.5 о тестах)

    void log(Level, std::string_view category, std::string_view message,
             std::source_location loc = std::source_location::current());

    // Удобные шорткаты.
    template<class... Args>
    void critical(std::string_view category, fmt::format_string<Args...> fmt, Args&&...);
    template<class... Args>
    void error(   std::string_view category, fmt::format_string<Args...> fmt, Args&&...);
    template<class... Args>
    void warn(    std::string_view category, fmt::format_string<Args...> fmt, Args&&...);
    template<class... Args>
    void info(    std::string_view category, fmt::format_string<Args...> fmt, Args&&...);
    template<class... Args>
    void debug(   std::string_view category, fmt::format_string<Args...> fmt, Args&&...);
    template<class... Args>
    void trace(   std::string_view category, fmt::format_string<Args...> fmt, Args&&...);

    // Конфигурация.
    void set_min_level(Level);
    Level min_level() const;

    // Управление выводами (sinks).
    void enable_console(bool);
    void enable_file(bool);
    void set_log_file_path(std::filesystem::path);   // меняет файл на лету
    std::filesystem::path log_file_path() const;

    // Категорийные фильтры (опционально, поверх min_level).
    void set_category_level(std::string_view category, Level);
};

}  // namespace coupecad::logging
```

`category` — короткий строковый ключ модуля-источника (`"core.commands"`, `"core.io"`, `"renderer"`, `"ui"`, `"licensing"`). По нему фильтруются выводы и по нему же оператор поддержки или разработчик быстро находит нужные строки.

`std::source_location` фиксируется автоматически и попадает в строку лога (`file:line`).

### 6.3 Поведение по умолчанию

- Уровень: `Info`.
- Sinks: **console (stderr)** и **файл** — оба включены.
- Файл: `<platform-specific user log dir>/CoupeCAD/log/coupecad-YYYY-MM-DD.log`. Платформенные пути:
  - Linux: `$XDG_STATE_HOME/CoupeCAD/log/` (фолбэк `~/.local/state/CoupeCAD/log/`).
  - macOS: `~/Library/Logs/CoupeCAD/`.
  - Windows: `%LOCALAPPDATA%\CoupeCAD\log\`.
- Файл **ротируется по дате** (новый файл каждый день) и **по размеру** (≥ 10 МБ → ротация в `coupecad-...-1.log`); сохраняется последние 7 файлов.
- Формат строки: `2026-04-21T12:34:56.789Z [INFO ] core.commands  panel_commands.cpp:142  AddPanel(role=Shelf) applied → PanelId=...`.

### 6.4 Отключение

- `Logger::instance().set_min_level(Level::Off)` — глушит всё.
- `Logger::instance().enable_console(false)` / `enable_file(false)` — раздельно отключают каждый sink.
- Эти настройки переопределяются при следующем запуске (хранение конфигурации — задача Stage 4 UI или CLI-флага; в Stage 1 — только программный API).
- В тестах: фикстура `LoggingFixture` в `tests/logging/` ставит `Off` перед каждым тестом, восстанавливает после.

### 6.5 Реализация

- Под капотом — **`spdlog`** через Conan (`spdlog/1.13.0+`, header-only режим). Библиотека зрелая, потокобезопасная, быстрая, MIT-лицензия. Совместима с `fmt` (тоже Conan, transitive).
- Наш `coupecad::logging::Logger` — **тонкая обёртка** над spdlog: один `spdlog::logger` под капотом с двумя sinks (`spdlog::sinks::stderr_color_sink_mt` и `spdlog::sinks::daily_file_sink_mt` + size-rotating адаптер). Обёртка нужна, чтобы (а) не тащить `spdlog` в публичные заголовки Core, (б) можно было легко подменить движок в будущем.
- Logger — process-wide singleton с lazy-init. Для тестов есть `Logger::reset_for_test()` (доступен только из тестового заголовка), пересоздающий singleton чистым.
- Потокобезопасность: все методы `Logger` — thread-safe (спасибо spdlog `_mt`-сink'ам). Core пока однопоточный, но UI и будущий рендер — нет.

### 6.6 CMake target

```cmake
# src/coupecad/logging/CMakeLists.txt
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
```

`coupecad_core` линкуется с `coupecad_logging` (PUBLIC, чтобы пользователи Core могли логгировать своими категориями) и использует через `coupecad::logging::Logger::instance()`. Logger Qt-free.

### 6.7 Тестирование

В `tests/logging/`:

- Уровни: установка min_level скрывает нижестоящие уровни.
- Категорийные фильтры: per-category level переопределяет глобальный.
- Sinks: отключение console / file независимо.
- Файловый sink: создаётся в указанной директории, ротация по размеру и дате (с mockable clock — переиспользуем `IClock` из Core).
- Корректность форматирования (timestamp, level, category, source location).
- Концурренция: 8 потоков одновременно пишут 1000 сообщений каждый — все строки попадают в файл целиком (без перемешивания символов).

Покрытие тестами `coupecad_logging` — также ≥ 70%.

---

## 7. Открытые вопросы

Решения, сознательно отложенные до implementation-плана или последующих стейджей:

1. **Политика `copy` vs `move` для Command:** все ли команды запрещают copy? (Вероятно да — команды non-copyable, non-movable после `execute`, чтобы избежать случайного повторного apply.) Детализируется в Stage 1b.
2. **Локализация ошибок:** `DomainError::what()` — английский текст + machine-readable `code: std::string_view`. UI в Stage 4+ маппит код на локализованный текст. Фиксируется при реализации в Stage 1a.
3. **Профиль `IProjectObserver`:** синхронные вызовы в рамках `execute()` — дёшево, но потенциально блокирует правки «изнутри» обсервера. Альтернатива — deferred-очередь. Решается в Stage 4, когда появится реальный consumer.
4. **Максимальный размер undo-стека:** безлимит или порог (напр. 1000 команд)? Для v1 — безлимит; добавить лимит, только если проявится проблема памяти.
5. **Тип `new_value` в preview:** `std::any` — простой и универсальный, но без type-safety. Альтернатива — шаблонный `PreviewableCommand<NewValueT>` или variant с фиксированным набором. Для v1 идём через `std::any` (минимум boilerplate); рефакторим на typed, если в реальной интеграции с UI окажется неудобно.
