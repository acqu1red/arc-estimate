# Wall Rendering System Refactor - Revit-Like Behavior

## Summary

This document describes the architectural changes made to implement Revit-like wall rendering behavior in ARC-Estimate.

## Key Behavioral Changes

### Before (Old System)
- Wall was drawn as a **filled screen-space rectangle**
- Screen thickness = `realThickness * camera.GetZoom()` - **scaled with zoom**
- Stroke width was **hardcoded** (5px/6px) - independent of view scale
- At extreme zoom, walls became giant solid rectangles covering the viewport

### After (New System - Revit-Like)
1. **Wall geometry grows with zoom** (natural viewport zoom behavior)
2. **Outline stroke thickness stays constant on screen** (screen-space lineweight)
3. **Lineweight controlled by VIEW SCALE** (1:50 vs 1:100), not camera zoom
4. **Thin Lines toggle** - when ON, all strokes render as hairline
5. **At extreme zoom**, you see two boundary strokes, NOT solid fill

## New Files Created

### `ViewSettings.h`
Contains view-level display settings:
- `viewScaleDenominator` - View scale (50 for 1:50, 100 for 1:100)
- `DetailLevel` enum - Coarse/Medium/Fine
- `thinLinesEnabled` - Thin Lines toggle
- `ViewRange` struct - Stub for future cut plane implementation

### `LineWeightTable.h`
Revit-like lineweight system:
- 16 weight indices (1-16) mapped to printed thickness in mm
- Converts printed mm to screen DIPs based on DPI
- **Does NOT use camera zoom** in calculations
- View scale affects stroke thickness (larger scale = thicker lines)
- `LineCategory` enum for semantic line types (WallCut, WallProjection, etc.)
- `GraphicStyle` struct for category-specific styling

### `WallPlanGeometry.h`
World-space wall geometry representation:
- `WorldPolyline` - Vector of world points
- `WallPlanGeometry` - Contains left/right boundaries, end caps, layer boundaries
- `WallPlanGeometryBuilder` - Computes geometry from Wall objects
- Supports location line modes (centerline, core face, finish face)
- Caching with dirty-check via geometry hash

## Modified Files

### `WallRenderer.h` (Complete Rewrite)
- **Removed**: Filled rectangle body as primary representation
- **Removed**: `screenThickness = realThickness * camera.GetZoom()`
- **Removed**: Hardcoded stroke widths
- **Added**: Draws boundary polylines with screen-space strokes
- **Added**: Uses `LineWeightTable` for stroke width calculation
- **Added**: Optional selection fill (very subtle, only when selected/hovered)
- **Added**: Geometry caching per wall

### `MainWindow.xaml.h`
- Added `ViewSettings.h` include
- Added `m_viewSettings` member variable
- Added handler declarations for `OnThinLinesToggleClick` and `OnViewScaleChanged`

### `MainWindow.xaml.cpp`
- Updated `m_wallRenderer.Draw()` call to pass `m_viewSettings`
- Added `OnThinLinesToggleClick` handler
- Added `OnViewScaleChanged` handler

### `Tests.h`
- Added `ViewSettings` tests
- Added `LineWeight` tests (including zoom invariance verification)
- Added `WallPlanGeometry` tests

## Acceptance Criteria Verification

### 1. Zoom Invariance ?
Changing camera zoom from 1x to 1000x does NOT change outline stroke thickness.
- `LineWeightTable::GetScreenStrokeWidth()` does not take zoom as parameter
- Tested in `LineWeight_ZoomInvariance` test

### 2. View Scale Effect ?
Changing `ViewSettings.viewScaleDenominator` (100 -> 50) changes stroke thickness.
- Formula: `scaleFactor = 100.0 / viewScaleDenominator`
- At 1:50, lines are thicker than at 1:100
- Tested in `LineWeight_ViewScaleEffect` test

### 3. Thin Lines ?
When `thinLinesEnabled = true`, all strokes render at hairline width.
- Override in `GetScreenStrokeWidth()` returns `m_thinLineWidth`
- Tested in `LineWeight_ThinLinesOverride` test

### 4. Extreme Zoom ?
At extreme zoom, viewport shows two boundary strokes, not a full-screen fill.
- Wall drawn as polylines, not filled rectangle
- Optional subtle fill only for selection/hover feedback

### 5. Demolish Style ?
Dashed style works with new polylines.
- Preserved from old system, applied to polyline strokes

### 6. Performance ?
- Geometry caching per wall ID
- Cache invalidation on wall modification
- No per-frame heap allocations for unchanged walls

## Future Extensions (Prepared)

### View Range / Cut Plane
- `ViewRange` struct exists in `ViewSettings.h`
- `LineCategory::WallCut` vs `LineCategory::WallProjection` prepared
- Architecture ready for cut vs projection line styles

### Detail Levels
- `DetailLevel::Fine` triggers layer boundary rendering
- `WallPlanGeometry::LayerBoundaries` populated for compound walls
- `LineCategory::WallLayerBoundary` style defined

### Compound Wall Layers
- `WallPlanGeometryBuilder::BuildLayerBoundaries()` implemented
- Layer boundaries computed from `WallType::GetLayers()`

## Usage

### Toggle Thin Lines
```cpp
m_viewSettings.SetThinLinesEnabled(true);
m_wallRenderer.InvalidateCache();
InvalidateCanvas();
```

### Change View Scale
```cpp
m_viewSettings.SetViewScaleDenominator(100); // 1:100
m_wallRenderer.InvalidateCache();
InvalidateCanvas();
```

### Change Detail Level
```cpp
m_viewSettings.SetDetailLevel(DetailLevel::Fine);
m_wallRenderer.InvalidateCache();
InvalidateCanvas();
```

## Architecture Diagram

```
                     ???????????????????
                     ?  DocumentModel  ?
                     ?   GetWalls()    ?
                     ???????????????????
                              ?
                              ?
                     ???????????????????
                     ?      Wall       ?
                     ? start, end,     ?
                     ? thickness, type ?
                     ???????????????????
                              ?
                              ?
              ?????????????????????????????????
              ?    WallPlanGeometryBuilder    ?
              ?  BuildBasic(wall, detailLvl)  ?
              ?????????????????????????????????
                              ?
                              ?
              ?????????????????????????????????
              ?      WallPlanGeometry         ?
              ?  LeftBoundary, RightBoundary  ?
              ?  StartCap, EndCap, Layers     ?
              ?      (WORLD SPACE mm)         ?
              ?????????????????????????????????
                              ?
         ???????????????????????????????????????????
         ?                    ?                    ?
         ?                    ?                    ?
???????????????????  ???????????????????  ???????????????????
?   ViewSettings  ?  ? LineWeightTable ?  ?     Camera      ?
? viewScale, thin ?  ?  weights 1-16   ?  ? WorldToScreen() ?
?   lines, DPI    ?  ? category styles ?  ?   (pan/zoom)    ?
???????????????????  ???????????????????  ???????????????????
         ?                    ?                    ?
         ???????????????????????????????????????????
                              ?
                              ?
                     ???????????????????
                     ?   WallRenderer  ?
                     ?  DrawPolyline() ?
                     ? screen strokes  ?
                     ? (zoom-invariant)?
                     ???????????????????
                              ?
                              ?
                     ???????????????????
                     ?  Win2D Session  ?
                     ? DrawGeometry()  ?
                     ???????????????????
```

## Commit History (Recommended)

1. **Commit 1**: Add `ViewSettings.h` + `LineWeightTable.h`
2. **Commit 2**: Add `WallPlanGeometry.h` (basic geometry builder)
3. **Commit 3**: Rewrite `WallRenderer.h` (polylines + lineweights)
4. **Commit 4**: Integrate into `MainWindow` (handlers, Draw call)
5. **Commit 5**: Add tests + documentation
