You are Claude Opus 4.5 Thinking. You have already received the initial roadmap and have started building the ARC-Estimate MVP. This prompt will provide additional details and instructions to fully complete the roadmap and enhance the project.

Please make sure to:

Update the roadmap with the missing tasks and sub-tasks.

Add detailed logic to each feature and specify how each part should work, especially in cases where the initial instructions were vague.

Update the documentation files (PROGRESS.md, ROADMAP.md, –усска€_документаци€.md) after each milestone, marking them as completed and noting any adjustments made.

Ensure clarity in the workflow, especially for tasks that were missing in the original roadmap, such as Auto-Dimensions, IFC Import, and Layer System.

Roadmap Updates
M2.5 Ч Layer System (—истема слоЄв)

Description: The system for managing layers in ARC-Estimate is crucial for controlling visibility and editing of different parts of the drawing. Layers will correspond to different views, allowing users to toggle visibility based on the current work context (e.g., demolition, new walls, etc.).

Tasks:

Create Layer class to manage individual layer properties (visibility, assigned objects, etc.).

Integrate Layer system with views: Each plan view (Measure, Demolition, etc.) must have its own layer configuration.

Implement Layer visibility toggle: Users should be able to toggle the visibility of layers for each view.

Link layers to WorkState: Certain layers (e.g., Demolition) should automatically be assigned WorkState based on the selected mode.

Acceptance Criteria:

Users can turn layers on and off via a toolbar or settings panel.

Layers are associated with specific WorkStates, such as Demolition or New.

The visibility of walls, windows, doors, and other objects should be controlled by the active layer.

M3.5 Ч Auto-Dimension Engine (јвторазмеры)

Description: Auto-dimensions are a critical part of ARC-Estimate. This feature should automatically generate dimensions for wall lengths, openings, and offsets when elements are added to the plan. Dimensions should also be updated when any element changes.

Tasks:

Create Auto-Dimension Manager: Develop a class that tracks elements and automatically adds dimensions when a new object (wall, window, door) is drawn.

Implement dimension chain updates: Ensure dimensions are automatically adjusted when the geometry is changed (e.g., resizing a wall or moving a window).

Allow toggle between auto and manual dimensions: Users should be able to turn off auto-dimensions and use the manual "Dimension" tool when needed.

Add Anchor Lock feature: Implement a way for users to lock the position of dimensions so that they are not automatically adjusted when objects are modified.

Acceptance Criteria:

Auto-dimensions are added automatically when a new object is drawn.

Dimensions include overall length, segment lengths, and distances to openings.

Reflow behavior works: dimensions update automatically when objects move or resize.

The Anchor Lock option is available and allows users to lock dimensions in place.

M5.5 Ч IFC Import (»мпорт IFC)

Description: Importing IFC files is essential for ARC-Estimate, as it allows users to bring in entire architectural models from Revit and other BIM software. This will help ARC-Estimate stay compatible with other platforms while preserving the integrity of wall types, work states, and material data.

Tasks:

Integrate IFC parsing library (e.g., IfcOpenShell or similar) into ARC-Estimate.

Map IFC elements to ARC-Estimate model: Walls, doors, windows, rooms, and other relevant objects should be translated into the internal data model.

Map IFC WorkStates: Imported objects should be automatically assigned to the correct WorkState (e.g., Existing, Demolish, New).

Handle layers in IFC: If the IFC file contains layer information, it must be respected and mapped correctly to ARC-Estimate layers.

Implement layer-based import options: Allow the user to choose whether to import all layers or specific ones.

Acceptance Criteria:

User can import an IFC file into ARC-Estimate.

The imported elements are properly mapped to ARC-Estimate's data model (walls, doors, rooms).

WorkStates (Existing, Demolish, New) are properly assigned based on the IFC file data.

Imported geometry can be traced over or modified within ARC-Estimate.

Layers and WorkStates are correctly mapped from the IFC file.

M3.1 Ч Wall Types & Materials (“ипы стен и материалы)

Description: Walls in ARC-Estimate must support multiple materials and types. Each wall will have layers with different materials (e.g., brick, drywall), and the materials must be reflected visually and in the estimation.

Tasks:

Create Wall Type Editor: Users must be able to select the material and thickness for each layer of the wall.

Apply material textures: Different materials should have distinct visual representations (e.g., brick, drywall).

Allow modification of wall layers: Users should be able to modify the thickness of each layer and swap materials.

Support material hatching for estimation: Different materials should be visualized using different hatches for clarity.

Acceptance Criteria:

Walls have multiple layers with different materials.

Materials are visually represented (e.g., brick, drywall) and correctly hatch in the 2D view.

Users can modify wall layers (thickness, material) through the properties panel.

Wall layers are mapped correctly to the estimate (quantity calculation by material type).

M9.5 Ч Quality Assurance (“естирование и QA)

Description: Before final release, comprehensive testing must be performed to ensure that the app is stable, performs well with many elements, and has no critical bugs. This includes both automated and manual testing.

Tasks:

Write Unit Tests: For key algorithms like wall joins, snapping, and dimensioning.

Create integration tests for full workflows (e.g., importing, drawing walls, generating estimates).

Test performance with a large number of walls (e.g., 50+).

Perform UI testing: Ensure all buttons, panels, and views are interactive and responsive.

Stability testing: Run stress tests and ensure no crashes in typical usage scenarios.

Acceptance Criteria:

Unit tests cover all critical algorithms with 90% coverage.

Regression tests are complete and pass.

Performance tests confirm the app performs well with many walls (50+).

UI testing confirms that the app is fully interactive, responsive, and stable.

Documentation is updated with test results and test coverage details.

Conclusion and Next Steps for Opus

After reviewing and updating the roadmap with all the missing points, Opus 4.5 should:

Complete all tasks and update ROADMAP.md with detailed progress and completed milestones.

For each feature, ensure the Acceptance Criteria is met and documented.

Update PROGRESS.md after each milestone, and make sure that the Russian documentation is also kept up to date.

After each task, Opus should ensure that the final documentation reflects all changes and decisions made during the implementation process.

Please proceed to update the roadmap and tasks according to this Mega Prompt.