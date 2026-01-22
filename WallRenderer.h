#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "Layer.h"
#include "ViewSettings.h"
#include "LineWeightTable.h"
#include "WallPlanGeometry.h"
#include <unordered_map>

namespace winrt::estimate1
{
    // =================================================================
    // REVIT-LIKE WALL RENDERER
    // =================================================================
    // Key behaviors:
    // 1. Wall body geometry grows with zoom (viewport zoom)
    // 2. Outline is GEOMETRY, not stroke - grows with zoom like in Revit
    // 3. Lineweight is controlled by VIEW SCALE (1:50 vs 1:100)
    // 4. At extreme zoom, you can zoom INTO the outline itself
    // 5. Outline is rendered as filled geometry band, not as stroke
    // =================================================================

    class WallRenderer
    {
    public:
        WallRenderer() = default;

        // Main draw method with ViewSettings support
        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            const LayerManager& layerManager,
            uint64_t hoverWallId = 0)
        {
            // Use default view settings if none provided
            Draw(session, camera, document, layerManager, hoverWallId, m_defaultViewSettings);
        }

        // Full draw method with explicit ViewSettings
        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            const LayerManager& layerManager,
            uint64_t hoverWallId,
            const ViewSettings& viewSettings)
        {
            const auto& walls = document.GetWalls();

            for (const auto& wall : walls)
            {
                if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                    continue;

                bool isHovered = (hoverWallId != 0 && wall->GetId() == hoverWallId);

                DrawWall(session, camera, *wall, viewSettings, false, isHovered);
            }
        }

        // Draw wall preview during creation
        void DrawPreview(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& startPoint,
            const WorldPoint& endPoint,
            double thickness,
            WorkStateNative workState,
            const LayerManager& layerManager,
            bool isFlipped = false,
            LocationLineMode locationLine = LocationLineMode::WallCenterline)
        {
            (void)layerManager;
            (void)isFlipped;
            
            Wall previewWall(startPoint, endPoint, thickness);
            previewWall.SetWorkState(workState);
            previewWall.SetLocationLineMode(locationLine);

            DrawWall(session, camera, previewWall, m_defaultViewSettings, true, false);
        }

        // Draw snap point indicator
        void DrawSnapPoint(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& point,
            bool isActive)
        {
            ScreenPoint screenPoint = camera.WorldToScreen(point);
            float radius = isActive ? 6.0f : 4.0f;

            Windows::UI::Color fillColor = isActive
                ? Windows::UI::ColorHelper::FromArgb(200, 255, 200, 0)
                : Windows::UI::ColorHelper::FromArgb(150, 100, 200, 100);

            Windows::UI::Color strokeColor = isActive
                ? Windows::UI::ColorHelper::FromArgb(255, 200, 150, 0)
                : Windows::UI::ColorHelper::FromArgb(200, 50, 150, 50);

            session.FillCircle(
                Windows::Foundation::Numerics::float2(screenPoint.X, screenPoint.Y),
                radius,
                fillColor);

            session.DrawCircle(
                Windows::Foundation::Numerics::float2(screenPoint.X, screenPoint.Y),
                radius,
                strokeColor,
                1.5f);
        }

        // Access to view settings for external modification
        ViewSettings& GetViewSettings() { return m_defaultViewSettings; }
        const ViewSettings& GetViewSettings() const { return m_defaultViewSettings; }

        // Access to lineweight table for customization
        LineWeightTable& GetLineWeightTable() { return m_lineWeightTable; }
        const LineWeightTable& GetLineWeightTable() const { return m_lineWeightTable; }

        // Clear geometry cache (call when walls are modified)
        void InvalidateCache() { m_geometryCache.clear(); }
        void InvalidateCache(uint64_t wallId) { m_geometryCache.erase(wallId); }

    private:
        // =================================================================
        // CORE WALL DRAWING - REVIT-LIKE BEHAVIOR
        // =================================================================
        void DrawWall(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall,
            const ViewSettings& viewSettings,
            bool isPreview = false,
            bool isHovered = false)
        {
            // Build or retrieve cached geometry
            WallPlanGeometry geometry = GetOrBuildGeometry(wall, viewSettings.GetDetailLevel());
            
            if (!geometry.IsValid())
                return;

            // Determine colors based on state
            Windows::UI::Color strokeColor = DetermineStrokeColor(wall, isPreview, isHovered);

            // REVIT-STYLE: Fixed thickness in WORLD coordinates (millimeters)
            // This thickness scales with zoom - at higher zoom, outline appears thicker
            // This allows zooming "into" the outline itself, seeing it as a real band
            double outlineWorldThickness = m_fixedOutlineThicknessMm;

            // Add extra width for selection (in world units)
            if (wall.IsSelected())
            {
                outlineWorldThickness += 1.0; // Add 1mm for selection highlight
            }

            // Apply dashed style for demolish state
            WorkStateNative state = wall.GetWorkState();
            if (state == WorkStateNative::Demolish)
            {
                strokeColor.A = 180;
            }

            // =================================================================
            // REVIT-STYLE RENDERING: Outline as GEOMETRY, not stroke
            // Draw the outline as a filled band around the wall boundary
            // This makes it part of world geometry - it scales with zoom!
            // =================================================================
            
            DrawWallWithGeometricOutline(session, camera, geometry, strokeColor, outlineWorldThickness);

            // Draw layer boundaries for Fine detail level
            if (viewSettings.GetDetailLevel() == DetailLevel::Fine)
            {
                // Layer boundaries use thinner outline (half of main outline)
                double layerWorldThickness = m_fixedOutlineThicknessMm * 0.5;

                Windows::UI::Color layerColor = m_lineWeightTable.GetStyle(
                    LineCategory::WallLayerBoundary).Color;

                for (const auto& layerBoundary : geometry.LayerBoundaries)
                {
                    DrawPolylineAsGeometry(session, camera, layerBoundary, layerColor, layerWorldThickness);
                }
            }

            // Optional: draw semi-transparent fill for selection/hover feedback
            if ((wall.IsSelected() || isHovered) && m_showSelectionFill)
            {
                DrawWallFill(session, camera, geometry, wall.IsSelected(), isHovered);
            }
        }

        // =================================================================
        // REVIT-STYLE: Draw wall with GEOMETRIC outline (not stroke)
        // The outline is rendered as a filled band that is part of world geometry
        // This makes it scale with zoom naturally
        // =================================================================
        void DrawWallWithGeometricOutline(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WallPlanGeometry& geometry,
            Windows::UI::Color outlineColor,
            double outlineWorldThickness)
        {
            if (geometry.BoundaryPath.Points.size() < 2)
                return;

            // Create the geometry in WORLD coordinates
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            
            const auto& points = geometry.BoundaryPath.Points;
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(
                static_cast<float>(points[0].X),
                static_cast<float>(points[0].Y)));
            
            for (size_t i = 1; i < points.size(); ++i)
            {
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(
                    static_cast<float>(points[i].X),
                    static_cast<float>(points[i].Y)));
            }
            
            pathBuilder.EndFigure(geometry.BoundaryPath.IsClosed 
                ? Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed
                : Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Open);

            auto worldGeometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
            
            // Create camera transformation
            float centerX = camera.GetCanvasWidth() / 2.0f;
            float centerY = camera.GetCanvasHeight() / 2.0f;
            float zoom = static_cast<float>(camera.GetZoom());
            float offsetX = static_cast<float>(-camera.GetOffset().X);
            float offsetY = static_cast<float>(-camera.GetOffset().Y);
            
            auto transform = Windows::Foundation::Numerics::make_float3x2_translation(-offsetX, -offsetY) *
                           Windows::Foundation::Numerics::make_float3x2_scale(zoom) *
                           Windows::Foundation::Numerics::make_float3x2_translation(centerX, centerY);
            
            // CRITICAL: Use hairline stroke with Normal behavior to draw the outline
            // The key is that outlineWorldThickness is in WORLD units (mm)
            // When transformed by zoom, it becomes proportionally larger on screen
            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.LineJoin(Microsoft::Graphics::Canvas::Geometry::CanvasLineJoin::Miter);
            strokeStyle.StartCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Flat);
            strokeStyle.EndCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Flat);
            strokeStyle.TransformBehavior(
                Microsoft::Graphics::Canvas::Geometry::CanvasStrokeTransformBehavior::Normal);

            auto oldTransform = session.Transform();
            session.Transform(transform);
            
            // Draw outline with world-space thickness
            session.DrawGeometry(worldGeometry, outlineColor, 
                static_cast<float>(outlineWorldThickness), strokeStyle);
            
            session.Transform(oldTransform);
        }

        // Draw polyline as geometric outline (for layer boundaries)
        void DrawPolylineAsGeometry(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPolyline& polyline,
            Windows::UI::Color color,
            double worldThickness)
        {
            if (polyline.Points.size() < 2)
                return;

            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(
                static_cast<float>(polyline.Points[0].X),
                static_cast<float>(polyline.Points[0].Y)));
            
            for (size_t i = 1; i < polyline.Points.size(); ++i)
            {
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(
                    static_cast<float>(polyline.Points[i].X),
                    static_cast<float>(polyline.Points[i].Y)));
            }
            
            pathBuilder.EndFigure(polyline.IsClosed 
                ? Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed
                : Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Open);

            auto worldGeometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
            
            float centerX = camera.GetCanvasWidth() / 2.0f;
            float centerY = camera.GetCanvasHeight() / 2.0f;
            float zoom = static_cast<float>(camera.GetZoom());
            float offsetX = static_cast<float>(-camera.GetOffset().X);
            float offsetY = static_cast<float>(-camera.GetOffset().Y);
            
            auto transform = Windows::Foundation::Numerics::make_float3x2_translation(-offsetX, -offsetY) *
                           Windows::Foundation::Numerics::make_float3x2_scale(zoom) *
                           Windows::Foundation::Numerics::make_float3x2_translation(centerX, centerY);

            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.LineJoin(Microsoft::Graphics::Canvas::Geometry::CanvasLineJoin::Miter);
            strokeStyle.TransformBehavior(
                Microsoft::Graphics::Canvas::Geometry::CanvasStrokeTransformBehavior::Normal);

            auto oldTransform = session.Transform();
            session.Transform(transform);
            
            session.DrawGeometry(worldGeometry, color, 
                static_cast<float>(worldThickness), strokeStyle);
            
            session.Transform(oldTransform);
        }

        // Legacy method - can be removed if not used elsewhere
        void DrawPolyline(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPolyline& polyline,
            Windows::UI::Color color,
            float strokeWidth,
            const Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle& strokeStyle)
        {
            if (polyline.Points.size() < 2)
                return;

            // Create geometry in WORLD coordinates first
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(
                static_cast<float>(polyline.Points[0].X),
                static_cast<float>(polyline.Points[0].Y)));
            
            for (size_t i = 1; i < polyline.Points.size(); ++i)
            {
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(
                    static_cast<float>(polyline.Points[i].X),
                    static_cast<float>(polyline.Points[i].Y)));
            }
            
            pathBuilder.EndFigure(polyline.IsClosed 
                ? Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed
                : Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Open);

            auto worldGeometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
            
            // Create camera transformation matrix
            float centerX = camera.GetCanvasWidth() / 2.0f;
            float centerY = camera.GetCanvasHeight() / 2.0f;
            float zoom = static_cast<float>(camera.GetZoom());
            float offsetX = static_cast<float>(-camera.GetOffset().X);
            float offsetY = static_cast<float>(-camera.GetOffset().Y);
            
            // Transformation: Translate to camera center, scale by zoom, translate to canvas center
            auto transform = Windows::Foundation::Numerics::make_float3x2_translation(-offsetX, -offsetY) *
                           Windows::Foundation::Numerics::make_float3x2_scale(zoom) *
                           Windows::Foundation::Numerics::make_float3x2_translation(centerX, centerY);
            
            // REVIT-LIKE BEHAVIOR: Apply transform to DrawingSession
            // With Normal stroke behavior, the outline SCALES WITH ZOOM
            // User can zoom in and see the outline grow, eventually zooming "into" the line itself
            auto oldTransform = session.Transform();
            session.Transform(transform);
            
            // Draw geometry in WORLD coordinates - stroke scales with zoom
            session.DrawGeometry(worldGeometry, color, strokeWidth, strokeStyle);
            
            // Restore original transform
            session.Transform(oldTransform);
        }

        // Draw optional selection/hover fill
        void DrawWallFill(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WallPlanGeometry& geometry,
            bool isSelected,
            bool isHovered)
        {
            if (geometry.BoundaryPath.Points.size() < 3)
                return;

            // Build closed polygon from boundary path
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            
            ScreenPoint first = camera.WorldToScreen(geometry.BoundaryPath.Points[0]);
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(first.X, first.Y));
            
            for (size_t i = 1; i < geometry.BoundaryPath.Points.size(); ++i)
            {
                ScreenPoint screen = camera.WorldToScreen(geometry.BoundaryPath.Points[i]);
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(screen.X, screen.Y));
            }
            
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);
            
            auto fillGeometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            // Very subtle fill color
            Windows::UI::Color fillColor;
            if (isSelected)
            {
                fillColor = Windows::UI::ColorHelper::FromArgb(40, 90, 180, 255);
            }
            else if (isHovered)
            {
                fillColor = Windows::UI::ColorHelper::FromArgb(25, 120, 210, 255);
            }
            else
            {
                return;
            }

            session.FillGeometry(fillGeometry, fillColor);
        }

        // Determine stroke color based on wall state
        Windows::UI::Color DetermineStrokeColor(const Wall& wall, bool isPreview, bool isHovered)
        {
            if (isPreview)
            {
                return Windows::UI::ColorHelper::FromArgb(179, 255, 255, 255);
            }
            
            if (wall.IsSelected())
            {
                return Windows::UI::ColorHelper::FromArgb(255, 90, 180, 255);
            }
            
            if (isHovered)
            {
                return Windows::UI::ColorHelper::FromArgb(255, 120, 210, 255);
            }
            
            // Default wall color (light for dark theme)
            return Windows::UI::Colors::White();
        }

        // Get cached geometry or build new
        WallPlanGeometry GetOrBuildGeometry(const Wall& wall, DetailLevel detailLevel)
        {
            uint64_t wallId = wall.GetId();
            
            auto it = m_geometryCache.find(wallId);
            if (it != m_geometryCache.end())
            {
                // Check if cache is still valid
                if (m_geometryBuilder.IsCacheValid(wall, it->second))
                {
                    return it->second;
                }
            }
            
            // Build new geometry
            WallPlanGeometry geometry = m_geometryBuilder.BuildBasic(wall, detailLevel);
            
            // Cache it (for non-preview walls)
            if (wallId != 0)
            {
                m_geometryCache[wallId] = geometry;
            }
            
            return geometry;
        }

    private:
        // Default view settings
        ViewSettings m_defaultViewSettings;
        
        // Lineweight table (maps weight indices to screen stroke widths)
        LineWeightTable m_lineWeightTable;
        
        // Geometry builder
        WallPlanGeometryBuilder m_geometryBuilder;
        
        // Geometry cache (per wall ID)
        std::unordered_map<uint64_t, WallPlanGeometry> m_geometryCache;
        
        // Whether to show selection fill (subtle fill for selected/hovered walls)
        bool m_showSelectionFill{ true };

        // Fixed outline thickness in world coordinates (millimeters)
        // This makes the outline scale with zoom, allowing zoom "into" the outline
        double m_fixedOutlineThicknessMm{ 8.0 };
    };
}
