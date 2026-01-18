#pragma once

#include "pch.h"
#include "Structure.h"
#include "Camera.h"
#include "Layer.h"
#include <vector>
#include <memory>
#include <algorithm>

using namespace winrt::Microsoft::Graphics::Canvas;
using namespace winrt::Microsoft::Graphics::Canvas::Geometry;
using namespace winrt::Microsoft::UI;
using winrt::Windows::UI::Color;

namespace winrt::estimate1
{
    class StructureRenderer
    {
    public:
        // Рендеринг колонн
        static void DrawColumns(
            CanvasDrawingSession const& session,
            const Camera& camera,
            const std::vector<std::shared_ptr<Column>>& columns,
            uint64_t hoverId = 0)
        {
            for (const auto& col : columns)
            {
                if (!col) continue;

                bool isSelected = col->IsSelected();
                bool isHovered = (col->GetId() == hoverId);
                
                // Получаем контур
                auto points = col->GetContour();
                if (points.size() < 3) continue;

                // Создаем геометрию
                CanvasPathBuilder builder(session);
                ScreenPoint p0 = camera.WorldToScreen(points[0]);
                builder.BeginFigure(p0.X, p0.Y);
                for (size_t i = 1; i < points.size(); ++i)
                {
                    ScreenPoint p = camera.WorldToScreen(points[i]);
                    builder.AddLine(p.X, p.Y);
                }
                builder.EndFigure(CanvasFigureLoop::Closed);
                auto geometry = CanvasGeometry::CreatePath(builder);

                // Цвета
                Color fillColor = isSelected ? Colors::Orange() : Colors::LightGray();
                if (isHovered && !isSelected) fillColor = Colors::LightSlateGray();
                
                Color strokeColor = Colors::Black();
                float strokeWidth = isSelected ? 2.0f : 1.0f;

                // Заливка и обводка
                session.FillGeometry(geometry, fillColor);
                session.DrawGeometry(geometry, strokeColor, strokeWidth);
                
                // Если круглая, рисуем крест в центре (условное обозначение)
                if (col->GetShape() == ColumnShape::Circular)
                {
                    WorldPoint center = col->GetPosition();
                    ScreenPoint cs = camera.WorldToScreen(center);
                    float r = 5.0f;
                    session.DrawLine(cs.X - r, cs.Y, cs.X + r, cs.Y, strokeColor, 1.0f);
                    session.DrawLine(cs.X, cs.Y - r, cs.X, cs.Y + r, strokeColor, 1.0f);
                }
            }
        }

        // Рендеринг перекрытий
        static void DrawSlabs(
            CanvasDrawingSession const& session,
            const Camera& camera,
            const std::vector<std::shared_ptr<Slab>>& slabs,
            uint64_t hoverId = 0)
        {
            for (const auto& slab : slabs)
            {
                if (!slab) continue;

                auto contour = slab->GetContour();
                if (contour.size() < 3) continue;

                bool isSelected = slab->IsSelected();
                bool isHovered = (slab->GetId() == hoverId);

                CanvasPathBuilder builder(session);
                ScreenPoint p0 = camera.WorldToScreen(contour[0]);
                builder.BeginFigure(p0.X, p0.Y);
                for (size_t i = 1; i < contour.size(); ++i)
                {
                    ScreenPoint p = camera.WorldToScreen(contour[i]);
                    builder.AddLine(p.X, p.Y);
                }
                builder.EndFigure(CanvasFigureLoop::Closed);
                auto geometry = CanvasGeometry::CreatePath(builder);

                // Цвет
                Color fillColor = Colors::LightBlue();
                fillColor.A = 50; // Прозрачный синий
                if (isSelected) fillColor = Colors::Orange(), fillColor.A = 100;
                else if (isHovered) fillColor = Colors::LightCyan(), fillColor.A = 80;

                session.FillGeometry(geometry, fillColor);
                session.DrawGeometry(geometry, Colors::Gray(), 1.0f);
            }
        }

        // Рендеринг балок
        static void DrawBeams(
            CanvasDrawingSession const& session,
            const Camera& camera,
            const std::vector<std::shared_ptr<Beam>>& beams,
            uint64_t hoverId = 0)
        {
            for (const auto& beam : beams)
            {
                if (!beam) continue;

                bool isSelected = beam->IsSelected();
                bool isHovered = (beam->GetId() == hoverId);

                // Рисуем как линию с толщиной
                WorldPoint start = beam->GetStartPoint();
                WorldPoint end = beam->GetEndPoint();
                ScreenPoint s = camera.WorldToScreen(start);
                ScreenPoint e = camera.WorldToScreen(end);

                float width = static_cast<float>(beam->GetWidth() * camera.GetZoom());
                
                Color color = Colors::DarkGray();
                if (isSelected) color = Colors::Orange();
                else if (isHovered) color = Colors::LightSlateGray(); // Note: LightSlateGray might not be in Colors enum in WinUI, check or use helper
                
                // Draw centerline
                CanvasStrokeStyle style;
                // style.DashStyle(CanvasDashStyle::Solid); 
                
                session.DrawLine(s.X, s.Y, e.X, e.Y, color, width);

                // Optional: draw centerline axis
                session.DrawLine(s.X, s.Y, e.X, e.Y, Colors::Black(), 1.0f);
            }
        }
    };
}
