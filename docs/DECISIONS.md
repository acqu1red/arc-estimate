# ARC-Estimate Architecture Decision Records

> Документ архитектурных решений / Architecture Decision Records (ADR)

---

## ADR-001: Technology Stack Selection

**Date:** 2025-01-17  
**Status:** Accepted

### Context
We needed to choose a technology stack for building a Windows desktop application with:
- High-performance 2D graphics for CAD-like functionality
- Modern Windows 10/11 support
- Maintainable codebase for a commercial product

### Options Considered

1. **WinUI 3 + C++/WinRT + Win2D** (Selected)
   - Native performance
   - Modern Fluent Design
   - GPU-accelerated 2D graphics via Win2D
   
2. **WPF + C# + SkiaSharp**
   - Easier development
   - Less performant for complex graphics
   
3. **Qt + C++**
   - Cross-platform
   - Different UI paradigm, licensing concerns

4. **Electron + JavaScript**
   - Web technologies
   - Poor performance for CAD applications

### Decision
Selected **WinUI 3 + C++/WinRT + Win2D** because:
- Win2D provides GPU-accelerated immediate-mode 2D graphics
- C++/WinRT offers best performance for geometry calculations
- WinUI 3 is Microsoft's modern UI framework
- Native Windows integration for file pickers, printing, etc.

### Consequences
- Steeper learning curve for C++/WinRT
- Need to manage memory manually in some cases
- Win2D integration requires additional NuGet package

---

## ADR-002: MVVM Architecture Pattern

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Need a software architecture pattern for separating UI from business logic.

### Decision
Use **MVVM (Model-View-ViewModel)** pattern because:
- Natural fit for XAML-based UI
- Enables data binding
- Improves testability
- Clear separation of concerns

### Consequences
- ViewModels must implement `INotifyPropertyChanged`
- Commands used for user actions
- Models remain UI-agnostic

---

## ADR-003: Coordinate System (Millimeter-Based)

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Architectural drawings require precise measurements. Need to choose internal units.

### Decision
Use **millimeters** as the internal coordinate unit because:
- Standard in architectural drawing (especially for renovation work)
- Integer millimeters often sufficient (though we use doubles for precision)
- Easy conversion to meters for display/export
- Matches common DXF/DWG units

### Consequences
- All internal geometry stored in mm
- UI must convert for display (e.g., "1500 мм" or "1.5 м")
- Camera transform handles screen pixel conversion

---

## ADR-004: Project File Format (JSON)

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Need a format for saving/loading project files.

### Options Considered
1. **JSON** (Selected) — Human-readable, easy to debug, widely supported
2. **Binary** — Smaller files, faster I/O, but harder to debug
3. **XML** — Verbose, more complex parsing

### Decision
Use **JSON** for project serialization because:
- Human-readable for debugging
- Easy to parse with libraries (nlohmann/json, RapidJSON)
- Future-proof and extensible
- Familiar format for developers

### Consequences
- Slightly larger file sizes than binary
- Need to include schema version for migrations
- Library dependency for JSON parsing

---

## ADR-005: Documentation Language Strategy

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Team includes Russian-speaking developers; product targets Russian market.

### Decision
Multi-language documentation approach:
- **UI text**: Russian (all user-facing strings)
- **Code identifiers**: English (classes, methods, variables)
- **Code comments**: Russian (for team comprehension)
- **High-level docs**: English (ROADMAP, PROGRESS, DECISIONS)
- **Technical docs**: Russian (`Русская_документация.md`)

### Consequences
- UI localization built-in from start
- Code readable by international audience
- Internal team has Russian technical reference

---

## ADR-006: Win2D for 2D Graphics Rendering

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Need a 2D graphics library for rendering floor plans with:
- GPU acceleration for smooth pan/zoom
- Vector graphics for crisp lines at any scale
- Integration with WinUI 3 / C++/WinRT

### Options Considered
1. **Win2D** (Selected) — Microsoft's WinRT wrapper for Direct2D
2. **Direct2D directly** — More control but complex API
3. **SkiaSharp** — Cross-platform but less WinRT integration

### Decision
Use **Win2D (Microsoft.Graphics.Win2D v1.3.0)** because:
- GPU-accelerated immediate-mode 2D graphics
- Native WinRT component works seamlessly with WinUI 3
- CanvasControl integrates directly into XAML
- Simpler API than raw Direct2D
- Supports all needed primitives (lines, text, geometries)

### Consequences
- Additional NuGet package dependency
- Need to manage CanvasControl lifecycle (CreateResources event)
- Drawing code in C++ uses Win2D drawing session

---

## ADR-007: Camera-Based Coordinate Transform

**Date:** 2025-01-17  
**Status:** Accepted

### Context
Need to support pan and zoom while maintaining millimeter-accurate geometry.

### Decision
Implement a Camera class that:
- Stores pan offset in world units (mm)
- Stores zoom as pixels-per-mm ratio
- Centers zoom operations at cursor position
- Provides bidirectional coordinate transformation

### Implementation Details
```cpp
// Zoom limits
constexpr double minZoom = 0.01;  // 1px = 100mm (zoomed out)
constexpr double maxZoom = 10.0;  // 10px = 1mm (zoomed in)

// Zoom centered at cursor
void ZoomAt(ScreenPoint cursor, double factor) {
    WorldPoint worldUnderCursor = ScreenToWorld(cursor);
    m_zoom = clamp(m_zoom * factor, minZoom, maxZoom);
    // Adjust offset so worldUnderCursor stays under cursor
    m_offset = ... // recalculate
}
```

### Consequences
- All drawing operations must use camera transform
- Mouse coordinates must be converted to world coords for interaction
- Grid rendering adapts to visible area

---

## ADR-008: Layer System Architecture

**Date:** 2025-01-17  
**Status:** Proposed

### Context
Need a layer system to control visibility and editing of drawing elements based on:
- View context (Measure, Demolition, New Construction)

---

## ADR-009: Wall Types, Layers and Materials (M3.1)

**Date:** 2026-01-17  
**Status:** Accepted

### Context
Walls must support:
- parametric thickness based on a type definition
- material assignment for future cost estimation
- Revit-like location line modes (core/finish faces)

### Decision
Introduce the following model:
- `Material` — basic catalog entity (name/code/cost, optional display color)
- `WallLayer` — a named layer with thickness (mm) and optional `Material` reference
- `WallType` — a named collection of `WallLayer` with computed total thickness and computed core thickness

Walls (`Wall`) hold an optional `WallType` reference (`std::shared_ptr<WallType>`). When a type is assigned:
- wall thickness is synchronized from `WallType::GetTotalThickness()`.

If the user manually edits thickness, the wall is detached from its type to avoid conflicting sources of truth.

### Implementation Notes
- `DocumentModel` owns in-memory catalogs of materials and wall types and creates a few defaults on initialization.
- `Core*` vs `Finish*` offsets are implemented using a first-pass heuristic:
  - `WallType::GetCoreThickness()` treats layers containing "Отдел" in the name as finish layers.
  - finish thickness is `total - core`.

### Consequences
- Enables immediate UI selection of wall types and future cost/hatch pipelines.
- Current core/finish detection is heuristic and should be replaced by an explicit flag in `WallLayer` later.
- Using `shared_ptr` simplifies referencing wall types from multiple walls; persistence will require stable IDs.
- WorkState of elements (Existing, Demolish, New)
- User preferences for layer visibility

### Decision
Implement a Layer system with:
- `Layer` class holding visibility, lock state, and optional WorkState link
- `LayerManager` for managing layer collection
- Automatic layer-to-view mapping:
  - Обмерный view: shows Existing layer
  - Демонтаж view: shows Existing + Demolish layers
  - Новый view: shows Existing + New layers

### Consequences
- Elements must reference a Layer
- Rendering must filter elements by layer visibility
- View switching updates layer visibility automatically

---

## ADR-009: Wall Type System with Material Layers

**Date:** 2025-01-17  
**Status:** Proposed

### Context
Walls need to support multiple materials (brick, drywall, insulation) for:
- Accurate visual representation with hatching
- Correct quantity takeoff by material
- Revit-like wall layer structure

### Decision
Implement a hierarchical wall type system:
```
Material (name, hatch, cost) 
  ?
WallLayer (material, thickness, function)
  ?
WallType (collection of WallLayer)
  ?
Wall (references WallType)
```

### Consequences
- Wall rendering becomes more complex (multiple fills)
- Estimation can break down by material type
- Need a Wall Type Editor UI

---

## ADR-010: Auto-Dimension Engine Design

**Date:** 2025-01-17  
**Status:** Proposed

### Context
Auto-dimensions are critical for architectural drawings:
- Overall wall lengths
- Individual segment lengths
- Opening positions and widths
- Must update automatically when geometry changes

### Decision
Implement an `AutoDimensionManager` that:
- Observes element changes (add/move/delete)
- Generates dimension objects automatically
- Maintains dimension chains (linked dimensions)
- Supports "Anchor Lock" to freeze specific dimensions

### Key Features
1. **Reflow behavior**: dimensions adjust when geometry moves
2. **Anchor Lock**: user can lock dimensions in place
3. **Toggle auto/manual**: user can disable auto-dimensions
4. **Dimension chains**: linked series for continuous dimensions

### Consequences
- Need observer pattern for element changes
- Dimensions stored separately from geometry
- Performance consideration for many dimensions

---

## ADR-011: DXF Import with Custom Parser

**Date:** 2025-01-18  
**Status:** Accepted

### Context
DXF (Drawing Exchange Format) import is essential for:
- Importing existing floor plans
- Using AutoCAD drawings as reference underlays
- Tracing geometry from architectural surveys

### Options Considered

1. **libdxfrw** — Open-source DXF library
   - Full DXF support
   - Requires building C++ library
   - Additional dependency

2. **dxflib** — Simple DXF parser
   - Lightweight
   - Limited to older DXF versions
   - External dependency

3. **Custom parser** (Selected)
   - No external dependencies
   - Full control over parsing
   - Header-only implementation

### Decision
Implement a custom lightweight DXF parser because:
- ASCII DXF format is well-documented
- Only need basic entities (LINE, POLYLINE, CIRCLE, ARC, TEXT)
- Avoids external library integration complexity
- Header-only fits project architecture

### Implementation Details
- `DxfParser.h` — State machine parser for DXF group codes
- Supports HEADER (INSUNITS), TABLES (layers), ENTITIES sections
- Handles LWPOLYLINE bulge arcs (curved segments)
- Automatic unit conversion (inches, feet, cm, m ? mm)

### Consequences
- Limited to ASCII DXF (binary DXF not supported)
- May need extensions for advanced entities (SPLINE, HATCH)
- No DWG support (proprietary format)

---

## ADR-012: IFC Import Strategy

**Date:** 2025-01-18  
**Status:** Accepted (Updated)

### Context
IFC (Industry Foundation Classes) import enables interoperability with:
- Autodesk Revit
- ArchiCAD
- Other BIM software

### Options Considered

1. **IfcOpenShell** — Full-featured IFC library
   - Complete IFC support (geometry, properties, relationships)
   - Large dependency, complex build
   - Overkill for reference underlay

2. **Custom STEP Parser** (Selected)
   - IFC files are STEP format (ISO-10303-21)
   - No external dependencies
   - Header-only implementation
   - Sufficient for basic elements

### Decision
Implement a custom lightweight IFC/STEP parser because:
- STEP format is text-based and parseable with regex
- Only need basic elements (wall, door, window, space)
- Avoids complex library integration
- Consistent with DXF approach

### Implementation Details

**Parsing Strategy:**
- Regex-based entity extraction: `#(\d+)\s*=\s*([A-Z0-9_]+)\s*\(([^;]*)\)\s*;`
- Two-pass parsing: entities first, then relationships

---

## ADR-013: Алгоритм распознавания помещений (R5.1)

**Date:** 2025-01-23  
**Status:** Accepted

### Context
- Нужно автоматически определять помещения по замкнутым контурам стен.
- Требования: простота (без тяжёлой топологии), устойчивость к неточностям (смещение узлов), возможность перерасчёта после любой правки стен.

### Decision
- Строим граф стен: вершины — узлы стен, объединённые по допуску 5 мм; рёбра — сегменты стен.
- Ищем минимальные циклы DFS с ограничением глубины (12) и канонизируем порядок обхода для дедупликации (минимальный индекс, сравнение с разворотом).
- Отбрасываем контуры площадью < 1 м²; считаем площадь/периметр/центроид при создании `Room`.
- Храним результат в `DocumentModel::m_rooms`, пересчитываем в `RebuildRooms()` при каждом `RebuildAutoDimensions()`.

### Consequences
- Помещения автоматически актуализируются при добавлении/удалении/редактировании стен и при загрузке проекта.
- Предоставлен геттер `DocumentModel::GetRooms()` для визуализации/маркировок в R5.2/R5.3.
- Пока не учитываем проёмы и перекрытия; доработка запланирована в следующих R5 задачах.
- Unit detection from IFCSIUNIT and IFCCONVERSIONBASEDUNIT

**Entity Mapping:**

| IFC Entity | Parser Structure |
|------------|-----------------|
| IFCWALL, IFCWALLSTANDARDCASE | IfcWall |
| IFCDOOR | IfcDoor |
| IFCWINDOW | IfcWindow |
| IFCSPACE | IfcSpace |
| IFCSLAB | IfcSlab |
| IFCBUILDINGSTOREY | IfcBuildingStorey |
| IFCCARTESIANPOINT | IfcPoint3D |
| IFCPOLYLINE | Points vector |

**Relationships Parsed:**
- IFCRELCONTAINEDINSPATIALSTRUCTURE → element-to-storey
- IFCRELVOIDSELEMENT → opening-to-wall
- IFCRELFILLSELEMENT → door/window-to-opening

**Rendering:**
- Walls: contour polygons or axis lines
- Doors: rectangle with 90° swing arc
- Windows: rectangle with glass fill
- Spaces: filled polygons with labels

### Consequences
- Limited to architectural elements (no MEP, structural details)
- Geometry extraction simplified (no boolean operations)
- Fast parsing, small code footprint
- Can be extended for PropertySet parsing later

---

## ADR-012: JSON Project File Format

**Date:** 2025-01-17  
**Status:** Proposed

### Context
Need to save and load project files with all model data.

### Decision
Use JSON format with schema versioning:
```json
{
  "version": "1.0",
  "metadata": { "name": "...", "author": "...", "date": "..." },
  "camera": { "offsetX": 0, "offsetY": 0, "zoom": 0.5 },
  "layers": [...],
  "materials": [...],
  "wallTypes": [...],
  "elements": {
    "walls": [...],
    "doors": [...],
    "windows": [...]
  },
  "dimensions": [...]
}
```

### File Extension
`.arcp` (ARC-Estimate Project)

### Consequences
- Human-readable for debugging
- Easy to add new fields with backward compatibility
- Need JSON library (nlohmann/json recommended)

---

## ADR-014: Room Model and Visualization (R5.2)

**Date:** 2025-01-24  
**Status:** Accepted

### Context
Помещения должны:
- Автоматически определяться по замкнутым контурам стен
- Иметь расширенные свойства (отделка, категория)
- Визуализироваться с цветовой схемой по типу
- Сохраняться и восстанавливаться в проекте

### Decision

1. **Расширенная модель Room**
   - `RoomCategory` enum для типизации (Living, Wet, Service, Circulation, Balcony, Office)
   - `RoomBoundary` структура для связи с ограждающими стенами
   - Отделка раздельно: пол, потолок, стены
   - Автоопределение категории по названию помещения

2. **RoomRenderer**
   - Отдельный рендерер для помещений (как WallRenderer, OpeningRenderer)
   - Цветовая схема на основе типа/названия помещения
   - Метки с фоном и рамкой для читаемости
   - RoomDisplaySettings для настройки отображения

3. **Сериализация**
   - Помещения автогенерируются из стен при загрузке
   - Сохраняются только пользовательские данные (название, номер, отделка)
   - Сопоставление при загрузке по центроиду с допуском 500 мм

### Implementation
```cpp
// Цветовая схема по названию
if (ContainsAny(name, { L"кухня", L"Кухня" }))
    return Orange;
if (ContainsAny(name, { L"ванная", L"санузел" }))
    return Blue;
// ...
```

### Consequences
- Помещения рисуются под стенами (z-order)
- При изменении стен помещения пересчитываются
- Пользовательские данные восстанавливаются по центроиду

---

## ADR-015: Zone System Architecture (R5.5)

**Date:** 2025-01-24  
**Status:** Accepted

### Context
Для сметы и анализа помещений необходима группировка по функциональным зонам:
- Жилая зона (спальни, гостиные)
- Мокрые помещения (кухня, ванная, санузел)
- Технические (кладовые, гардеробные)
- Коридоры и проходные зоны

### Decision

1. **Автоматическая группировка**
   - Зоны создаются автоматически из RoomCategory
   - ZoneManager пересобирается при каждом RebuildRooms()
   - Пользователь может создавать дополнительные зоны

2. **Расчёт стоимости отделки**
   - Цены отделки зависят от типа зоны
   - Мокрые помещения: плитка (дороже)
   - Жилые: ламинат, обои (средне)
   - Технические: минимальная отделка

3. **Структура данных**
```cpp
enum class ZoneType {
    Living,      // Жилая
    Wet,         // Мокрые
    Service,     // Технические
    Circulation, // Коридоры
    Outdoor,     // Открытые (балконы)
    Custom       // Пользовательские
};

class Zone {
    std::set<uint64_t> m_roomIds;  // ID помещений
    // Кэшируемые суммы площадей, периметров, объёмов
};

class ZoneManager {
    std::vector<Zone> m_autoZones;   // Автоматические
    std::vector<Zone> m_customZones; // Пользовательские
};
```

### Implementation Notes
- Zone не хранит указатели на Room — только ID
- При изменении помещений зоны пересобираются
- Кэширование вычислений для производительности

### Consequences
- Смета может группировать работы по зонам
- Автоматический расчёт стоимости отделки
- Пользовательские зоны сохраняются в проекте

---

*Additional decisions will be recorded as development progresses.*
