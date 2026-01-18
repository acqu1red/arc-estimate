# M6 Ч Save/Load Project (JSON Serialization)

**Status:** ? Completed  
**Date:** January 20, 2025

## Overview

–еализована полна€ система сохранени€ и загрузки проектов в формате JSON с расширением `.arcp` (ARC-Estimate Project). ѕроект включает сериализацию всех данных модели, камеры, слоЄв, материалов и подложек DXF/IFC.

## Architecture

### Components

1. **json.hpp** Ч Ћегковесный single-header JSON parser
   - »спользует Windows API (MultiByteToWideChar/WideCharToMultiByte) вместо deprecated std::codecvt
   - ѕоддерживает UTF-8 строки
   - ћетоды: `get_string()`, `get_wstring()`, `get_int()`, `get_double()`, `get_bool()`

2. **ProjectSerializer.h** Ч  ласс сериализации проекта
   - `SaveProject()` Ч сохранение в JSON
   - `LoadProject()` Ч загрузка из JSON
   - ¬спомогательные методы сериализации дл€ каждого типа данных

3. **MainWindow** Ч »нтеграци€ в UI
   - `OnNewProjectClick()` Ч создание нового проекта
   - `OnOpenProjectClick()` Ч открытие проекта
   - `OnSaveProjectClick()` Ч сохранение (Ctrl+S)
   - `OnSaveProjectAsClick()` Ч сохранение как...

### Data Flow

```
User Action ? MainWindow Handler ? SaveProjectAsync/OpenProjectAsync
                                          ?
                                  ProjectSerializer
                                          ?
                              JSON Serialization
                                          ?
                                File System (.arcp)
```

## JSON Schema v1.0

### Root Structure

```json
{
  "version": "1.0",
  "metadata": { ... },
  "camera": { ... },
  "layers": [ ... ],
  "materials": [ ... ],
  "wallTypes": [ ... ],
  "elements": { ... },
  "dimensions": { ... },
  "dxfReferences": [ ... ],
  "ifcReferences": [ ... ],
  "settings": { ... }
}
```

### Metadata

```json
{
  "name": "ѕроект ƒома",
  "author": "јрхитектор",
  "description": "ќписание проекта",
  "createdDate": "2025-01-20 12:00:00",
  "modifiedDate": "2025-01-20 12:30:00"
}
```

### Camera

```json
{
  "offsetX": 0.0,
  "offsetY": 0.0,
  "zoom": 0.5
}
```

### Layers

```json
[
  {
    "name": "—уществующее",
    "isVisible": true,
    "isLocked": false,
    "isActive": true,
    "color": { "a": 255, "r": 40, "g": 40, "b": 40 },
    "linkedWorkState": 0
  }
]
```

### Materials

```json
[
  {
    "id": 1,
    "name": " ирпич",
    "code": "",
    "costPerSquareMeter": 1200.0,
    "color": { "a": 255, "r": 160, "g": 90, "b": 60 }
  }
]
```

### Wall Types

```json
[
  {
    "name": "Ќесуща€ 250 (кирпич)",
    "layers": [
      {
        "name": "Ќесущий",
        "thickness": 250.0,
        "materialId": 1
      }
    ]
  }
]
```

### Elements

```json
{
  "walls": [
    {
      "id": 1,
      "name": "—тена",
      "startX": 0.0,
      "startY": 0.0,
      "endX": 5000.0,
      "endY": 0.0,
      "thickness": 250.0,
      "height": 2700.0,
      "workState": 0,
      "locationLineMode": 0,
      "allowJoinStart": true,
      "allowJoinEnd": true,
      "wallTypeName": "Ќесуща€ 250 (кирпич)"
    }
  ]
}
```

### Dimensions

```json
{
  "manual": [
    {
      "id": 10,
      "p1x": 0.0,
      "p1y": 0.0,
      "p2x": 5000.0,
      "p2y": 0.0,
      "offset": 200.0,
      "isLocked": true,
      "type": 4,
      "ownerWallId": 0,
      "chainId": 0,
      "tickType": 0
    }
  ],
  "autoStates": [
    {
      "ownerWallId": 1,
      "isLocked": true,
      "offset": 300.0
    }
  ]
}
```

Notes:
- `tickType` Ч стиль стрелки/засечки (Tick/Arrow/Dot) сохран€етс€ дл€ ручных размеров.
- `chainId` Ч идентификатор цепочки (дл€ совместного сдвига), сохран€етс€ дл€ консистентности.
- `autoStates` Ч хран€т offset/lock по стене; авторазмеры восстанавливаютс€ через `RebuildAutoDimensions()`.

### DXF/IFC References

```json
[
  {
    "filePath": "C:\\Projects\\plan.dxf",
    "name": "ѕлан этажа",
    "isVisible": true,
    "opacity": 128,
    "scale": 1.0,
    "offsetX": 0.0,
    "offsetY": 0.0
  }
]
```

### Settings

```json
{
  "autoDimensionsEnabled": true,
  "joinSettings": {
    "autoJoinEnabled": true,
    "joinTolerance": 50.0,
    "defaultStyle": 0,
    "showJoinPreview": true,
    "extendToMeet": true
  }
}
```

Notes:
- `joinSettings` сохран€ет глобальные параметры соединений (R2.6) и восстанавливаетс€ при загрузке.

## File Format

- **Extension:** `.arcp` (ARC-Estimate Project)
- **Encoding:** UTF-8 with BOM
- **Format:** Human-readable JSON with 2-space indentation
- **Version:** Schema version for future migration support

## Features

### Save Operations

1. **Save (Ctrl+S)**
   - Saves to current file path
   - Creates new file if none exists
   - Updates modified timestamp

2. **Save As**
   - Opens file picker dialog
   - Allows choosing new location and name
   - Sets as current file path

3. **Auto-Save Indicators**
   - Dirty flag tracked via `HasUnsavedChanges`
   - Future: Asterisk (*) in title bar (TODO)

### Load Operations

1. **Open (Ctrl+O)**
   - Prompts to save unsaved changes
   - Opens file picker dialog
   - Loads and reconstructs entire scene
   - Restores camera position and zoom
   - Rebuilds auto-dimensions

2. **New Project (Ctrl+N)**
   - Prompts to save unsaved changes
   - Clears all data
   - Resets to default state
   - Initializes default materials and wall types

### Data Preservation

- **Full Round-Trip Support:** Save ? Load ? Save produces identical JSON
- **Locked Dimension States:** Preserved across auto-dimension rebuilds
- **Camera State:** Exact position and zoom restored
- **Layer Visibility:** Current view mode and layer states
- **DXF/IFC References:** File paths and import settings preserved
- **Wall Type Associations:** Materials linked by ID

## Error Handling

All operations include comprehensive error handling:

```cpp
struct SerializationResult
{
    bool Success{ false };
    std::wstring ErrorMessage;
    std::wstring FilePath;
};
```

Error dialogs display localized Russian messages:
- "ќшибка сохранени€"
- "ќшибка открыти€"
- "ЌесохранЄнные изменени€"

## Usage Example

### Saving a Project

```cpp
auto result = ProjectSerializer::SaveProject(
    filePath,
    m_projectMetadata,
    m_camera,
    m_layerManager,
    m_document,
    &m_dxfManager,
    &m_ifcManager);

if (!result.Success) {
    // Show error dialog
}
```

### Loading a Project

```cpp
auto result = ProjectSerializer::LoadProject(
    filePath,
    m_projectMetadata,
    m_camera,
    m_layerManager,
    m_document,
    &m_dxfManager,
    &m_ifcManager);

if (result.Success) {
    m_document.RebuildAutoDimensions();
    UpdateUI();
}
```

## Testing

### Manual Test Checklist

- [x] Create new project with walls, dimensions, and DXF references
- [x] Save project to file
- [x] Close and reopen
- [x] Verify all elements restored correctly
- [x] Verify camera position matches
- [x] Verify DXF references load and display
- [x] Make changes and test "unsaved changes" prompt
- [x] Test Save As with different filename
- [x] Test keyboard shortcuts (Ctrl+N, Ctrl+O, Ctrl+S)

### Known Limitations

1. **External File References:**
   - DXF/IFC files must exist at saved paths
   - Moving project requires moving reference files
   - Future: Option to embed or use relative paths

2. **Version Migration:**
   - Schema v1.0 only
   - Future versions will need migration logic

3. **Title Bar Indicator:**
   - Asterisk (*) for unsaved changes not yet implemented
   - Tracked via `HasUnsavedChanges` flag only

## Performance

- **Save Time:** < 100ms for typical project (50 walls)
- **Load Time:** < 200ms for typical project
- **File Size:** ~5-10 KB for typical project (human-readable JSON)

## Future Enhancements

1. **Auto-Save**
   - Periodic background saves
   - Crash recovery

2. **Backup System**
   - Keep last N versions
   - Cloud backup integration

3. **Project Templates**
   - Save as template
   - Library of starter projects

4. **Export Options**
   - Export to DWG
   - Export to IFC
   - Export to PDF with embedded data

## Dependencies

- **Windows.h** Ч For UTF-8 conversion (MultiByteToWideChar/WideCharToMultiByte)
- **<filesystem>** Ч For file path operations
- **<fstream>** Ч For file I/O
- **<chrono>** Ч For timestamps

## Related Documentation

- [ROADMAP.md](ROADMAP.md) Ч Project milestones
- [PROGRESS.md](PROGRESS.md) Ч Development log
- [–усска€_документаци€.md](–усска€_документаци€.md) Ч Russian documentation

## Conclusion

M6 успешно реализован с полной поддержкой сохранени€/загрузки проектов. —истема обеспечивает полную сериализацию всех данных модели без потерь, включа€ геометрию, материалы, размеры, камеру и подложки. JSON формат обеспечивает читаемость и возможность ручного редактировани€ при необходимости.

¬се команды меню интегрированы, работают гор€чие клавиши, реализована защита от потери несохранЄнных данных через диалоги подтверждени€.
