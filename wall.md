Please analyze this text prompt, update the roadmap by adding stage M5.6, complete missing docs, provide code examples, sequence diagrams, tests, and integrate with existing ARC‑Estimate project.


ARC‑Estimate — Stage M5.6: Advanced Wall Snap & Alignment System  
Mega Prompt for Claude Opus 4.5 (English)

Task:
Implement a new roadmap stage M5.6 called "Advanced Wall Snap & Alignment System".  
This stage must deliver precise, Revit‑like wall placement and snapping logic.

Context:
The current implementation in ARC‑Estimate only supports simple endpoint snap.  
Revit’s wall placement supports **dynamic snapping to multiple wall references** based on cursor location:
  • Wall centerline (axis)
  • Exterior finish face
  • Interior finish face
  • Core face (if multi‑layer wall)
  • Any point along the face (not just endpoints)

Goals:
1. **Dynamic Cursor‑Based Snap Target Selection**
   • When user moves cursor near an existing wall, the system should detect the closest relevant wall reference plane:
     - If near the centerline → suggest snapping to center
     - If near exterior finish face → suggest that face
     - If near interior finish face → suggest that face
   • The snap indicator should visually highlight (color/underline/marker)
   • If closer to an endpoint, prioritize endpoint snap

2. **Snap Preview & Visual Feedback**
   • While placing/moving a wall, show a preview line connected to the chosen snap target
   • Show small tooltip text like “Centerline”, “Outer Face”, “Inner Face”

3. **Reference Modes UI and Shortcuts**
   • Provide UI toggle/buttons to choose default reference mode:
     - Centerline
     - Exterior Face
     - Interior Face
   • Hotkey toggle (e.g., Tab) to cycle through modes during placement
   • Indicator in status bar showing selected snap reference mode

4. **Support for Multi‑Layer Walls**
   • Walls have layers (core + finishes); snapping must respect chosen layer surfaces
   • Integrate with existing WallType/WallLayer structures

5. **Integration with Current Model & Dimensions**
   • When a wall is placed using advanced snap, update wall model coordinates in mm
   • Auto‑dimension engine should reflect changes in real time
   • Dimensions should reflow based on new snap point

6. **Priority & Rules**
   • Priority: endpoints > face references > centerline when cursor is near multiple
   • If Ctrl (or Alt) is pressed, force snap to specific reference mode
   • If disallowed joins are set (wall end toggles), ignore snap on that segment

Detailed Behavior:
• On mouse move during wall placement:
  - Compute distance from cursor to:
    * Wall centerline
    * Finish faces (inner/exterior)
  - If within threshold (e.g., 10px) show snap highlight
  - Allow continuous placement (chain) with snapping like Revit

• On wall hover (select mode):
  - Show different handles for different snap targets (center, inner, outer)
  - Clicking on a handle starts a new segment from that reference

Pseudo‑Logic Example:
if distance_to_face < distance_to_centerline:
active_snap = face_reference
elif distance_to_centerline < threshold:
active_snap = centerline
else:
active_snap = endpoint if near endpoint

sql
Копировать код

Requirements:
• Must be added as new roadmap stage `M5.6` with proper acceptance criteria  
• Include UI specification (hotkeys, visual feedback design)  
• Provide unit tests for:
  - Snap target detection
  - Cursor threshold behavior
  - Snap priority logic
• Provide sequence diagrams for:
  - User starts placing wall → cursor movement → snap detection → wall placement
• Provide example code stubs for:
  - Snap detection algorithm
  - UI feedback rendering (Win2D)
• Provide DataBinding spec for WinUI controls related to snap mode

UI Controls Spec:
• Snap Mode Dropdown or Icons (Centerline | Outer | Inner)
• Snap Preview highlight overlays
• Tooltip during placement (“Snap: Exterior Face”)
• Config panel: snap threshold (mm or pixels)

Acceptance Criteria:
• Cursor dynamically snaps to proper wall faces like Revit
• Snap preview visible during placement
• UI mode toggles work in placement and edit tools
• All combinations of wall geometry tested with unit tests
• Documentation updated (English + Russian)


You must implement and extend a new stage in the ARC‑Estimate roadmap:  
**M5.6 — Advanced Wall Snap & Alignment System**.

This stage introduces a **Revit‑like wall snapping and alignment system** that ensures wall placement and editing behaves like Revit’s real architect CAD/BIM experience — snapping to centerlines and faces of walls, not just endpoints.

Revit’s aligned dimension and wall placement tools have a feature where you can snap to:
• Wall centerline
• Core centerline
• Exterior finish face
• Interior finish face
• Faces of core
These reference types are visible when placing or dimensioning walls. :contentReference[oaicite:1]{index=1}

---

# Objective

Implement a **generalized wall snap system** for ARC‑Estimate that supports:

1. **Snap to multiple wall reference surfaces**:
   - **Centerline**
   - **Core centerline**
   - **Finish face exterior**
   - **Finish face interior**
   - **Core face exterior**
   - **Core face interior**

2. **Dynamic cursor‑based snapping**:
   - Automatically detect nearest reference surface based on cursor proximity
   - Provide visual feedback of snap location
   - Allow cycling snap target via hotkey (e.g., Tab)

3. **Update wall placement and editing behavior**:
   - New and existing walls should adhere to this snap logic
   - Snapping should work in all drawing/editing modes

4. **Integration with current architecture**:
   - Work with existing wall model (WallType, layers, thickness, WorkState)
   - Provide hooks for auto‑dimension reflow when walls are moved/snapped

5. **User interface improvements**:
   - Buttons/controls to select default snap preference
   - Tooltips showing current snap target
   - Visual indicators near walls (e.g., small markers on faces/center)

---

# High‑Level Requirements

## Snap Detection System

You must design a system that:
• Computes distance from cursor to each relevant wall reference 
• Chooses the nearest valid snap target within threshold  
• Prioritizes endpoint snap when very close

## Snap Target Priorities

The following priority rules should apply:
1. Endpoint snap
2. Closest wall reference face/centerline
3. If multiple near — nearest wins
4. If user forces mode via hotkey, override priority

---

# Data Structures

You must define a general **SnapCandidate** struct:

```cpp
enum class WallSnapPlane {
    CENTERLINE,
    CORE_CENTERLINE,
    FINISH_FACE_EXT,
    FINISH_FACE_INT,
    CORE_FACE_EXT,
    CORE_FACE_INT,
    ENDPOINT
};

struct SnapCandidate {
    WallID wallID;
    WallSnapPlane plane;
    WorldPoint projectedPoint;
    double distance; // mm
};
Wall Reference Computation
For each wall segment, compute its reference surfaces:

cpp
Копировать код
WorldPoint getCenterLineProjection(const Wall& wall, WorldPoint cursor);
WorldPoint getFaceProjection(const Wall& wall, WorldPoint cursor, WallSnapPlane face);
double computeDistance(WorldPoint a, WorldPoint b);
For each wall you should generate candidates:

cpp
Копировать код
SnapCandidate candidate;
candidate.plane = WallSnapPlane::CENTERLINE;
candidate.projectedPoint = getCenterLineProjection(wall, cursor);
candidate.distance = computeDistance(cursor, candidate.projectedPoint);
candidates.push_back(candidate);

// Repeat for all face types
Snap Algorithm
Pseudo‑logic:

pseudo
Копировать код
function findBestSnap(cursor):
    candidates = []
    foreach wall in visibleWalls:
        for each referencePlane in wall.referencePlanes:
            project = projectCursorToPlane(cursor, referencePlane)
            dist = distance(cursor, project)
            if dist < snapThreshold:
                candidates.push({ wall, referencePlane, project, dist })

    sort candidates by distance
    return first candidate
Win2D Rendering Hook
When snap is found, show indicator:

cpp
Копировать код
void DrawSnapIndicator(CanvasDrawingSession^ ds, SnapCandidate snap) {
    ds->DrawCircle(snap.projectedPoint, 4, Colors::Red);
    ds->DrawText(getSnapPlaneName(snap.plane), snap.projectedPoint + Vector2(6, -6),
                 Colors::Black, textFormat);
}
Hotkeys & UI
Hotkey Mapping
Key	Action
Tab	Cycle snap mode (Centerline → Face → Core)
Esc	Cancel placement
Shift	Lock current snap

UI Spec
Snap Mode dropdown showing current mode

Snap Indicators near walls

Tooltip “Snap: <PlaneName> (e.g., FINISH_FACE_EXT)”

XAML Example (WinUI 3 Bindings)
xml
Копировать код
<ComboBox x:Name="SnapModeCombo" SelectedItem="{x:Bind ViewModel.SnapMode, Mode=TwoWay}">
  <ComboBoxItem Content="Centerline"/>
  <ComboBoxItem Content="Finish Ext"/>
  <ComboBoxItem Content="Finish Int"/>
  <ComboBoxItem Content="Core Ext"/>
  <ComboBoxItem Content="Core Int"/>
</ComboBox>
Bind SnapMode enum to viewmodel.

Sequence Diagram (text UML)
rust
Копировать код
User -> Canvas: Mouse_Move(cursorPosition)
Canvas -> SnapSystem: findBestSnap(cursorPosition)
SnapSystem -> WallModel: evaluate reference projections
WallModel -> SnapSystem: return candidates
SnapSystem -> Canvas: return best snap
Canvas -> Render: showSnapIndicator()
Unit Tests
Use a testing framework (GoogleTest/Catch2):

Snap to Centerline Test
cpp
Копировать код
TEST(WallSnapTests, SnapToCenterline) {
    Wall w(...); // 1000mm length
    WorldPoint cursor = {500, 0}; // near center
    SnapCandidate result = findBestSnap(cursor);
    ASSERT_EQ(result.plane, WallSnapPlane::CENTERLINE);
}
Snap to Exterior Face Test
cpp
Копировать код
TEST(WallSnapTests, SnapToExteriorFace) {
    Wall w(...); // 200mm thickness
    WorldPoint cursor = {1000, 100}; // near face
    SnapCandidate result = findBestSnap(cursor);
    ASSERT_EQ(result.plane, WallSnapPlane::FINISH_FACE_EXT);
}
Integration Checklist
Add stage M5.6 – Advanced Wall Snap System to roadmap with acceptance criteria.

Update Wall class to expose multiple reference planes.

Implement SnapSystem with distance evaluation.

Hook into Win2D rendering for visual feedback.

Bind SnapMode to UI controls.

Add hotkey support (Tab/Shift) in input handler.

Write unit tests and integration tests for snapping logic.

Document behavior in PROGRESS.md and Russian docs.

Update sequence diagrams and API docs.

⚠️ Notes:
• Use libdxfrw for DXF import (parsing line/polylines for wall geometry). 
GitHub

• Snap thresholds and behavior should be configurable in settings.
• Ensure performance by culling far walls.

Acceptance Criteria
✔ Cursor snaps to correct plane when within threshold
✔ Visual indicator updates in real time
✔ Hotkeys cycle modes without errors
✔ Unit tests cover all planes
✔ UI reflects snap mode and status
✔ Documentation and roadmap updated