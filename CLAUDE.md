# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**ARC-Estimate** is a 2D architectural CAD application for renovation projects, built with WinUI 3, C++/WinRT, and Win2D. It enables drawing floor plans, placing architectural elements (walls, doors, windows), and generating cost estimates and construction drawings.

**Technology Stack:**
- WinUI 3 + C++/WinRT (Windows App SDK 1.8.x)
- Win2D (Microsoft.Graphics.Win2D 1.3.2) for GPU-accelerated 2D rendering
- C++17/C++20 with header-only architecture
- MVVM pattern (ViewModels for data binding)
- JSON for project serialization (nlohmann/json)

## Build Commands

### Build the project
```bash
msbuild estimate1.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Clean build
```bash
msbuild estimate1.vcxproj /t:Clean /p:Configuration=Debug /p:Platform=x64
msbuild estimate1.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Build configurations
- **Debug|x64** - Primary development configuration
- **Release|x64** - Production build (not actively used)
- Win32 and ARM64 configurations exist but are not primary targets

### Run the application
After building, run from Visual Studio or execute the generated .exe from:
```
estimate1\x64\Debug\estimate1.exe
```

## Architecture Overview

### Core Coordinate System
- **World coordinates**: Millimeters (double precision)
- **Screen coordinates**: Canvas pixels
- **Camera class** handles all coordinate transformations (world ↔ screen)
- Pan with offset (mm), zoom with pixels-per-mm ratio
- Zoom limits: 0.01 (zoomed out) to 10.0 (zoomed in)

### Document Model Architecture

**DocumentModel** (`Element.h`) is the central data store:
- Owns all elements: walls, doors, windows, dimensions, rooms, columns, slabs, beams
- Manages catalogs: materials, wall types, door types, window types
- Handles element lifecycle: add/remove/find/select
- Triggers rebuilds: auto-dimensions, wall attachment joins, room detection

**Element hierarchy** (in `Models.h` and related headers):
```
Element (base)
├─ Wall (start/end points, thickness, location line mode)
├─ Opening (base for doors/windows, hostWallId, position 0.0-1.0)
│  ├─ Door (swing type, swing arc)
│  └─ Window (sill height, pane type)
├─ Dimension (linear, angular)
├─ Room (polygon contour, area, category)
├─ Column (position, cross-section)
├─ Slab (polygon, thickness)
└─ Beam (line, cross-section)
```

### Wall Attachment System

**WallAttachmentSystem.h** manages wall corner connections using 3 attachment modes:
- Core (centerline)
- FinishExterior (outer finish face)
- FinishInterior (inner finish face)

**Critical**: Wall rendering must use `WallAttachmentSystem::BuildWallContour()` (via `WallRenderer`) for correct corner geometry. Never render walls as simple rectangles.

### Dimension System

**Auto-dimensions** (`AutoDimensionManager.h`):
- Automatically generated for each wall when geometry changes
- Dimension chains for continuous measurements
- Offset and lock state persist across rebuilds
- Triggered by `RebuildAutoDimensions()` after wall changes

**Manual dimensions** (`DimensionTool` in `DrawingTools.h`):
- User-placed linear measurements
- Stored separately from auto-dimensions

**Angular dimensions** (`AngularDimension` in `Dimension.h`):
- Shows angles between walls during drawing
- Rendered with arc and degree text

### Room Detection

**RoomDetector.h** automatically finds enclosed spaces:
- Builds graph of wall connections (5mm snap tolerance)
- Searches for minimal cycles using DFS (depth limit: 12)
- Creates Room objects with polygon contours
- Filters out areas < 1 m²
- Triggered automatically after wall changes via `RebuildRooms()`

**Room.h** stores room data:
- Polygon contour, area, perimeter, volume
- Category (Living, Wet, Service, Circulation, Balcony, Office)
- Finish types (floor, ceiling, walls)
- Bounding walls tracking

**Zone.h** groups rooms by function:
- Auto-generates zones from RoomCategory
- Calculates zone totals (area, cost)
- Used in estimation engine for finish cost calculations

### Import/Export Systems

**DXF Import** (`DxfParser.h`, `DxfReference.h`):
- Custom lightweight parser (no external dependencies)
- Supports LINE, POLYLINE, LWPOLYLINE, CIRCLE, ARC, TEXT
- Renders as reference underlay with opacity control
- Can convert to native Wall elements

**IFC Import** (`IfcParser.h`, `IfcReference.h`):
- Custom STEP/IFC parser (no IfcOpenShell dependency)
- Parses IFCWALL, IFCDOOR, IFCWINDOW, IFCSPACE
- Unit detection (mm, cm, m, feet, inches)
- Renders as reference underlay
- Can convert to native elements

**Excel Export** (`ExcelExporter.h`):
- Generates XLSX files (custom ZIP/XML writer, no external libs)
- Cost estimation by WorkState (Existing, Demolish, New)
- Material quantities and pricing
- Zone-based finish cost calculations
- Also supports simple CSV export

**PDF Export** (`PdfExporter.h`):
- Custom PDF 1.4 writer (no external dependencies)
- Scaled architectural drawings (1:50, 1:100, etc.)
- Paper sizes: A4, A3, A2, A1, A0
- GOST-style title blocks
- Renders walls, dimensions, rooms

### Undo/Redo System

**UndoManager.h** implements command pattern:
- Commands: AddWall, RemoveWall, ModifyWall
- Ctrl+Z / Ctrl+Y shortcuts
- Integrated into MainWindow event handlers

### Rendering Pipeline

**MainWindow.xaml.cpp** orchestrates rendering in `OnCanvasDraw()`:
1. Grid (GridRenderer)
2. DXF/IFC reference underlays
3. Rooms (RoomRenderer) - semi-transparent fills
4. Walls (WallRenderer) - with join geometry
5. Columns, Slabs, Beams (StructureRenderer)
6. Doors, Windows (OpeningRenderer)
7. Dimensions (DimensionRenderer)
8. Snap indicators, preview geometry

**Rendering order matters**: rooms under walls, walls under doors/windows, dimensions on top.

## Code Organization

### Header-Only Architecture
All business logic is in `.h` files:
- `Models.h` - Wall, Material, Layer
- `Opening.h` - Door, Window, DoorType, WindowType_
- `Element.h` - DocumentModel, base Element class
- `Room.h` - Room model with area calculations
- `Structure.h` - Column, Slab, Beam
- `Zone.h` - Zone grouping and management

### Tools
Drawing and editing tools in separate headers:
- `DrawingTools.h` - WallTool, SelectTool, DimensionTool
- `OpeningTools.h` - DoorPlacementTool, WindowPlacementTool
- `StructureTools.h` - ColumnTool, SlabTool, BeamTool
- `EditTools.h` - TrimExtendTool, SplitTool

### Renderers
Separate renderer classes for each element type:
- `WallRenderer.h` - Walls with join geometry
- `OpeningRenderer.h` - Doors with swing arcs, windows with frames
- `DimensionRenderer.h` - Extension lines, dimension lines, text
- `RoomRenderer.h` - Room fills with labels
- `StructureRenderer.h` - Columns, slabs, beams

### XAML and WinRT
- `MainWindow.xaml` - UI layout (Ribbon-style menu, canvas, properties panel)
- `MainWindow.xaml.h/.cpp` - Code-behind with event handlers
- `MainViewModel.idl/.h/.cpp` - MVVM pattern for data binding

## Project File Format

**File extension**: `.arcp` (ARC-Estimate Project)

**Format**: JSON with schema versioning

**Serialization**: `ProjectSerializer.h`
- Saves all elements, dimensions, catalogs, settings
- Room user data (names, finishes) persists
- Camera state (position, zoom) persists
- Auto-dimension state (offsets) persists

## Common Development Patterns

### Adding a New Element Type
1. Define model class in a new `.h` file (inherit from Element)
2. Add storage to DocumentModel (`m_elements`, Add/Remove/Get methods)
3. Create renderer class (e.g., `ElementRenderer.h`)
4. Create tool class if needed (e.g., `ElementTool.h`)
5. Add serialization in `ProjectSerializer.h`
6. Integrate into MainWindow:
   - Add renderer instance
   - Call renderer in OnCanvasDraw()
   - Handle tool in HandleToolClick()
   - Add HitTest logic

### Working with Wall Joins
Always use `WallJoinSystem::CalculateWallContour()` to get wall geometry:
```cpp
WallJoinSystem joinSys(...);
auto contour = joinSys.CalculateWallContour(wall, joins);
// contour is std::vector<WorldPoint> with proper corner geometry
```

Never render walls as simple offset rectangles from start/end points.

### Triggering Rebuilds
After modifying walls:
```cpp
documentModel.RebuildAutoDimensions();  // Updates dimensions AND rooms
```

After modifying rooms manually:
```cpp
documentModel.RebuildZones();  // Updates zone groupings
```

### Working with Coordinates
Always use Camera for transformations:
```cpp
// Screen to world (for mouse input)
WorldPoint worldPt = camera.ScreenToWorld(ScreenPoint{x, y});

// World to screen (for rendering)
ScreenPoint screenPt = camera.WorldToScreen(worldPt);
```

## Language and Localization

- **UI strings**: Russian (all user-facing text)
- **Code identifiers**: English (classes, methods, variables)
- **Code comments**: Mixed (Russian in older code, English acceptable)
- **Documentation**: ROADMAP/PROGRESS/DECISIONS are in Russian with English code examples

## Testing

**Tests.h** contains unit tests:
- 33 tests covering core algorithms
- Test suites: Camera, Wall, WallType, Dimension, Estimation, Document
- Run via menu: "Справка" → "Запустить тесты"

## Important Constraints

### Performance Considerations
- No spatial indexing yet (R9 milestone deferred)
- All elements are linear-searched for HitTest
- Target: 100+ walls without lag (not yet optimized)

### Limitations
- Windows-only (WinUI 3 is Windows-specific)
- No 3D views (2D plan only)
- Binary DXF not supported (ASCII only)
- DWG files not supported (proprietary format)

## Current Development Status

**Completed Milestones** (see `docs/ROADMAP.md`):
- M0-M9.5: All core functionality through quality assurance
- R1: Revit-like ribbon interface
- R2: Wall join system with miter corners
- R3: Professional dimension lines
- R4: Doors and windows
- R5: Room detection and zones
- R6: Columns, slabs, beams (partial - editing UI pending)

**Known Issues**:
- Wall Type Editor exists but UI is basic
- Room label drag not implemented (position calculated automatically)
- Performance optimization deferred (R9)
- Animations and polish deferred (R7)

## Debugging Tips

- Set breakpoints in `MainWindow.xaml.cpp` event handlers
- Check `DocumentModel` state via debugger (m_walls, m_rooms, etc.)
- Enable Win2D debug layer for graphics issues
- Room detection issues: check `RoomDetector::DetectRooms()` cycle search
- Wall join issues: check `WallAttachmentSystem::FindJoins()` tolerance

## Helpful Commands

### Find element by ID
```cpp
Wall* wall = documentModel.GetWall(id);
if (wall) { /* use wall */ }
```

### Get all rooms
```cpp
const auto& rooms = documentModel.GetRooms();
for (const auto& room : rooms) {
    double area = room.GetAreaSqM();
}
```

### Calculate estimation
```cpp
EstimationEngine engine;
auto result = engine.Calculate(documentModel);
// result.GrandTotal, result.Items, etc.
```

## Recent Changes (2026-01-19)

### WallAttachmentSystem Integration

**Status:** ✅ FULLY INTEGRATED AND WORKING

A new wall attachment system has replaced the old `WallJoinSystem`. The legacy system is archived and no longer used.

#### Key Files:
- `WallAttachmentSystem.h` - New attachment system (750+ lines)
- `DrawingTools.h` - WallTool integration with attachment modes
- `MainWindow.xaml` - UI ComboBox for attachment mode selection
- `MainWindow.xaml.cpp` - Event handlers for mode switching
- `Tests.h` - 15 unit tests for WallAttachmentSystem

#### Attachment Modes:
```cpp
enum class WallAttachmentMode
{
    Core,               // Centerline (default)
    FinishExterior,     // Outer finish face
    FinishInterior      // Inner finish face
};
```

#### Usage:

**Setting attachment mode in WallTool:**
```cpp
m_wallTool.SetAttachmentMode(WallAttachmentMode::FinishExterior);
// This automatically syncs with LocationLineMode for rendering
```

**UI Integration:**
- ComboBox: `AttachmentModeComboBox` in MainWindow.xaml (line 523)
- Handler: `OnWallAttachmentModeChanged` in MainWindow.xaml.cpp (line 1315)
- The UI immediately updates the tool and any selected wall

**Synchronization with LocationLineMode:**
When `SetAttachmentMode()` is called, it automatically converts to the corresponding `LocationLineMode`:
- `Core` → `WallCenterline`
- `FinishExterior` → `FinishFaceExterior`
- `FinishInterior` → `FinishFaceInterior`

This ensures compatibility with the existing rendering system while using the new simplified API.

#### Testing:
Run tests via menu: **Справка** → **Запустить тесты**
- Test suite: "WallAttachmentSystem Tests"
- 15 tests covering offsets, geometry, joins, and conversions

#### Important Notes:
- `WallAttachmentSystem` is used exclusively in `WallRenderer`
- The legacy join system is archived and should not be used
- All new walls use `WallAttachmentSystem` by default

#### Known Issues Fixed:
- ✅ std::max macro conflicts (4 instances fixed with extra parentheses)
- ✅ UI not connected to tool (fixed with sync in SetAttachmentMode)
- ✅ Attachment mode not applied to drawing (fixed with LocationLineMode sync)

#### Migration Path:
Future work can:
1. Simplify `LocationLineMode` to only 3 modes
2. Expand attachment joins (T/X/collinear)
