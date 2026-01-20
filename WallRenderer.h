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
    // 2. Outline stroke thickness stays CONSTANT on screen (screen-space lineweight)
    // 3. Lineweight is controlled by VIEW SCALE (1:50 vs 1:100), not camera zoom
    // 4. Thin Lines toggle: when ON, all strokes are hairline width
    // 5. At extreme zoom, you see boundary strokes, NOT a solid fill covering viewport
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
            
            // Get stroke width from lineweight table - INDEPENDENT OF ZOOM
            float strokeWidth = m_lineWeightTable.GetCategoryStrokeWidth(
                LineCategory::WallCut, viewSettings);
            
            // Add extra width for selection
            if (wall.IsSelected())
            {
                strokeWidth += m_lineWeightTable.GetSelectionStrokeAddition();
            }

            // Configure stroke style with Fixed transform behavior
            // This keeps the stroke width constant in screen pixels regardless of zoom
            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.LineJoin(Microsoft::Graphics::Canvas::Geometry::CanvasLineJoin::Miter);
            strokeStyle.StartCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Square);
            strokeStyle.EndCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Square);
            strokeStyle.TransformBehavior(
                Microsoft::Graphics::Canvas::Geometry::CanvasStrokeTransformBehavior::Fixed);

            // Apply dashed style for demolish state
            WorkStateNative state = wall.GetWorkState();
            if (state == WorkStateNative::Demolish)
            {
                strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);
                strokeColor.A = 180;
            }

            // =================================================================
            // DRAW WALL BOUNDARY AS A CLOSED CONTOUR (NOT SEPARATE PIECES)
            // This ensures clean joints and consistent screen-space lineweight.
            // =================================================================
            
            // Draw the single closed geometric contour
            DrawPolyline(session, camera, geometry.BoundaryPath, strokeColor, strokeWidth, strokeStyle);

            // Draw layer boundaries for Fine detail level
            if (viewSettings.GetDetailLevel() == DetailLevel::Fine)
            {
                float layerStrokeWidth = m_lineWeightTable.GetCategoryStrokeWidth(
                    LineCategory::WallLayerBoundary, viewSettings);
                
                Windows::UI::Color layerColor = m_lineWeightTable.GetStyle(
                    LineCategory::WallLayerBoundary).Color;

                for (const auto& layerBoundary : geometry.LayerBoundaries)
                {
                    DrawPolyline(session, camera, layerBoundary, layerColor, layerStrokeWidth, strokeStyle);
                }
            }

            // Optional: draw semi-transparent fill for selection/hover feedback
            if ((wall.IsSelected() || isHovered) && m_showSelectionFill)
            {
                DrawWallFill(session, camera, geometry, wall.IsSelected(), isHovered);
            }
        }

        // Draw a world-space polyline with screen-space constant stroke width
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
            
            // Transform geometry to screen space
            auto screenGeometry = worldGeometry.Transform(transform);
            
            // Draw with Fixed stroke width (will not scale with transformation)
            session.DrawGeometry(screenGeometry, color, strokeWidth, strokeStyle);
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
    };
}
