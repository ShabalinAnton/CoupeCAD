# Stage 1 — Core domain (CoupeCAD)

Дата: 2026-04-20
Статус: черновик v1, согласован с владельцем продукта в ходе brainstorming.
Место в общем видении: см. `2026-04-18-coupecad-design.md` (общий спек) и `../plans/2026-04-18-stage-0-bootstrap.md` (предыдущий стейдж).

Этот документ описывает **доменную модель CoupeCAD**: структуры данных, команды изменения с undo/redo, формат файла проекта `.ccad`, публичный API и тестирование. Результат Stage 1 — работающий модуль `src/coupecad/core/`, полностью Qt-free, покрытый GoogleTest-ами, служащий фундаментом для Stage 2 (Geometry), Stage 4 (UI) и далее.

---

## 1. Цель и границы Stage 1

**Что Stage 1 производит.** Доменную модель в `src/coupecad/core/`: структуры данных (Project → Cabinet → Panels + Hardware + Materials), команды изменения с undo/redo, чтение/запись `.ccad`. Всё Qt-free, покрыто unit-тестами.

**Что НЕ входит.** Геометрия (OpenCASCADE — Stage 2), рендер (Stage 3+), UI (Stage 4+), реальные каталоги материалов и фурнитуры (Stage 6). Core оперирует только «логической» моделью: роль панели, её параметры, материал, кромка. Никакого реального 3D.

**Definition of Done.**

- Программно создаётся `Project` с одним `Cabinet`, добавляются панели по ролям (top/bottom/side/back/shelf/facade/...), назначаются материалы и кромка.
- Любая правка оборачивается в `Command`; выполненное можно отменить (undo), затем повторить (redo).
- Сохранение в `.ccad`-файл и открытие из него дают байт-в-байт тот же проект (при детерминированных UUID и timestamp, как в тестах).
- Покрытие тестами Core ≥ 70% (lines).

**Scope.** Stage 1 плотный (оценочно 4-5 недель). Концерны (entities, commands/undo, serialization) тесно связаны, поэтому всё описано **одним спеком**. Имплементационный план разделит работу на подстейджи: **Stage 1a** — entities + неизменяющие read API; **Stage 1b** — commands + undo stack; **Stage 1c** — `.ccad` I/O. Каждый подстейдж заканчивается набором зелёных тестов.

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
| `dimensions` | `Dimensions` = `{width, height, depth: Millimeters}` | Внешние габариты |
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

Cabinet-local, **правосторонняя**: **X** вправо, **Y** вверх, **Z** к зрителю (от задней стенки к фасаду). Origin — **левый-нижний-задний угол** внешних габаритов.

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
    virtual bool try_merge(const Command& other) { return false; }  // default: не мержится
};
```

`ChangeSet` описывает, какие сущности затронуты (см. §3.5). Возвращается и из `apply`, и из `revert`, чтобы `UndoStack` мог транслировать его наблюдателям.

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

```cpp
class UndoStack {
public:
    explicit UndoStack(Project& project);

    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    bool can_undo() const;
    bool can_redo() const;
    std::span<const std::string_view> undo_labels() const;

    void clear();
    void begin_macro(std::string_view label);
    void end_macro();

    void add_observer(IProjectObserver*);
    void remove_observer(IProjectObserver*);

private:
    Project& project_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::optional<MacroBuilder> macro_;
    std::vector<IProjectObserver*> observers_;
};
```

`execute(cmd)` делает `cmd->apply(project_)`, пушит в `undo_`, очищает `redo_`. `undo()` поп-ит из `undo_`, вызывает `revert()`, пушит в `redo_`. И наоборот для `redo()`.

**Merge.** Чтобы крутящий слайдер ширины не раздувал стек до сотни записей, команды того же `kind()` на одной и той же цели в пределах временнОго окна (по умолчанию 500 мс) **сливаются**: новая команда принимает `old_value` первой и заменяет её. Реализуется через `Command::try_merge(const Command& other) -> bool`. Сливаются: `SetCabinetDimensions`, `UpdatePanelRoleParams`, `SetPanelMaterial`, `SetPanelThickness`. Остальные (add/remove) — нет.

**Макросы.** `begin_macro(label)` открывает накопление команд. Все последующие `execute()` до `end_macro()` добавляются в строящийся `MacroCommand`, который по закрытию кладётся в стек как единое целое.

**Persistence.** В v1 undo-стек **не сохраняется** в `.ccad`. Открытие файла → пустой стек. Это упрощает формат и соответствует поведению большинства CAD.

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
        "dimensions_mm": {"width": 2400, "height": 2400, "depth": 600},
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
    PRIVATE
        libzip::libzip
        stduuid::stduuid
)
```

**`coupecad_core` не линкуется ни с Qt, ни с OpenCASCADE.** Зависит только от `nlohmann_json` (публично), `libzip` и `stduuid` (приватно — детали реализации io). Новые Conan-зависимости добавятся в `conanfile.py` в Stage 1c.

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
    Dimensions{2400, 2400, 600}));
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

## 6. Открытые вопросы

Решения, сознательно отложенные до implementation-плана или последующих стейджей:

1. **Политика `copy` vs `move` для Command:** все ли команды запрещают copy? (Вероятно да — команды non-copyable, non-movable после `execute`, чтобы избежать случайного повторного apply.) Детализируется в Stage 1b.
2. **Точный tolerance merge-окна:** 500 мс — грубая оценка. Отточится при подключении UI в Stage 4, когда появится реальный слайдер и можно замерить UX.
3. **Локализация ошибок:** `DomainError::what()` — английский текст + machine-readable `code: std::string_view`. UI в Stage 4+ маппит код на локализованный текст. Фиксируется при реализации в Stage 1a.
4. **Профиль `IProjectObserver`:** синхронные вызовы в рамках `execute()` — дёшево, но потенциально блокирует правки «изнутри» обсервера. Альтернатива — deferred-очередь. Решается в Stage 4, когда появится реальный consumer.
5. **Максимальный размер undo-стека:** безлимит или порог (напр. 1000 команд)? Для v1 — безлимит; добавить лимит, только если проявится проблема памяти.
