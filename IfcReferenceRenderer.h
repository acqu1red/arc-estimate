#pragma once

#include "pch.h"
#include "Camera.h"
#include "IfcReference.h"
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.Geometry.h>
#include <winrt/Microsoft.Graphics.Canvas.Text.h>

namespace winrt::estimate1
{
    // ============================================================================
    // IFC Reference Renderer (рендерер импортированных IFC)
    // ============================================================================

    class IfcReferenceRenderer
    {
    public:
        // Отрисовка всех IFC-подложек
        static void Draw(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcReferenceManager& manager)
        {
            for (const auto& layer : manager.GetLayers())
            {
                if (layer && layer->IsVisible())
                {
                    DrawLayer(session, camera, *layer);
                }
            }
        }

        // Отрисовка одного слоя IFC
        static void DrawLayer(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcReferenceLayer& layer)
        {
            const IfcDocument* doc = layer.GetDocument();
            if (!doc)
                return;

            uint8_t opacity = layer.GetOpacity();
            float lineWidth = layer.GetLineWidth();

            // Рисуем помещения (под стенами)
            if (layer.GetShowSpaces())
            {
                DrawSpaces(session, camera, *doc, layer, opacity);
            }

            // Рисуем плиты перекрытия (как контуры)
            DrawSlabs(session, camera, *doc, layer, opacity);

            // Рисуем стены
            DrawWalls(session, camera, *doc, layer, opacity, lineWidth);

            // Рисуем двери
            DrawDoors(session, camera, *doc, layer, opacity, lineWidth);

            // Рисуем окна
            DrawWindows(session, camera, *doc, layer, opacity, lineWidth);

            // Рисуем названия помещений
            if (layer.GetShowNames() && layer.GetShowSpaces())
            {
                DrawSpaceNames(session, camera, *doc, layer, opacity);
            }
        }

    private:
        // ============================================================
        // Отрисовка стен
        // ============================================================
        
        static void DrawWalls(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity,
            float lineWidth)
        {
            Windows::UI::Color wallColor = layer.GetWallColor();
            wallColor.A = static_cast<uint8_t>((wallColor.A * opacity) / 255);

            for (const auto& wall : doc.Walls)
            {
                if (!wall)
                    continue;

                // Если есть контуры — рисуем их
                if (!wall->Contours.empty())
                {
                    for (const auto& contour : wall->Contours)
                    {
                        DrawPolyline(session, camera, contour.Points, contour.IsClosed, 
                                    wallColor, lineWidth);
                    }
                }
                // Иначе рисуем осевую линию
                else if (wall->StartPoint.X != 0 || wall->StartPoint.Y != 0 ||
                         wall->EndPoint.X != 0 || wall->EndPoint.Y != 0)
                {
                    ScreenPoint sp1 = camera.WorldToScreen(WorldPoint(wall->StartPoint.X, wall->StartPoint.Y));
                    ScreenPoint sp2 = camera.WorldToScreen(WorldPoint(wall->EndPoint.X, wall->EndPoint.Y));
                    
                    session.DrawLine(
                        Windows::Foundation::Numerics::float2(sp1.X, sp1.Y),
                        Windows::Foundation::Numerics::float2(sp2.X, sp2.Y),
                        wallColor,
                        lineWidth * 2.0f);
                }
            }
        }

        // ============================================================
        // Отрисовка дверей
        // ============================================================
        
        static void DrawDoors(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity,
            float lineWidth)
        {
            Windows::UI::Color doorColor = layer.GetDoorColor();
            doorColor.A = static_cast<uint8_t>((doorColor.A * opacity) / 255);

            for (const auto& door : doc.Doors)
            {
                if (!door)
                    continue;

                // Рисуем дверь как прямоугольник в позиции
                double halfWidth = door->Width / 2.0;
                double halfHeight = door->Height / 2.0;

                ScreenPoint pos = camera.WorldToScreen(
                    WorldPoint(door->Position.X, door->Position.Y));

                float screenHalfWidth = static_cast<float>(halfWidth * camera.GetZoom());
                float screenHalfHeight = static_cast<float>(halfHeight * camera.GetZoom());

                // Простой прямоугольник для обозначения двери
                Windows::Foundation::Rect doorRect(
                    pos.X - screenHalfWidth,
                    pos.Y - screenHalfHeight,
                    screenHalfWidth * 2,
                    screenHalfHeight * 2);

                // Заливка
                Windows::UI::Color fillColor = doorColor;
                fillColor.A = static_cast<uint8_t>(fillColor.A / 3);
                session.FillRectangle(doorRect, fillColor);

                // Контур
                session.DrawRectangle(doorRect, doorColor, lineWidth);

                // Дуга открывания двери (упрощённо)
                if (door->Width > 0)
                {
                    float arcRadius = screenHalfWidth;
                    // Рисуем дугу 90°
                    DrawArc90(session, pos, arcRadius, doorColor, lineWidth);
                }
            }
        }

        // ============================================================
        // Отрисовка окон
        // ============================================================
        
        static void DrawWindows(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity,
            float lineWidth)
        {
            Windows::UI::Color windowColor = layer.GetWindowColor();
            windowColor.A = static_cast<uint8_t>((windowColor.A * opacity) / 255);

            for (const auto& window : doc.Windows)
            {
                if (!window)
                    continue;

                double halfWidth = window->Width / 2.0;

                ScreenPoint pos = camera.WorldToScreen(
                    WorldPoint(window->Position.X, window->Position.Y));

                float screenHalfWidth = static_cast<float>(halfWidth * camera.GetZoom());
                float thickness = static_cast<float>(100.0 * camera.GetZoom()); // Условная толщина стены

                // Рисуем окно как две параллельные линии (стекло)
                Windows::Foundation::Rect windowRect(
                    pos.X - screenHalfWidth,
                    pos.Y - thickness / 2,
                    screenHalfWidth * 2,
                    thickness);

                // Заливка (голубая для стекла)
                Windows::UI::Color glassColor = windowColor;
                glassColor.A = static_cast<uint8_t>(glassColor.A / 2);
                session.FillRectangle(windowRect, glassColor);

                // Контур
                session.DrawRectangle(windowRect, windowColor, lineWidth);

                // Средняя линия (переплёт)
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(pos.X, pos.Y - thickness / 2),
                    Windows::Foundation::Numerics::float2(pos.X, pos.Y + thickness / 2),
                    windowColor,
                    lineWidth);
            }
        }

        // ============================================================
        // Отрисовка помещений
        // ============================================================
        
        static void DrawSpaces(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity)
        {
            Windows::UI::Color spaceColor = layer.GetSpaceColor();
            spaceColor.A = static_cast<uint8_t>((spaceColor.A * opacity) / 255 / 4); // Очень прозрачная заливка

            for (const auto& space : doc.Spaces)
            {
                if (!space)
                    continue;

                for (const auto& contour : space->BoundaryContours)
                {
                    if (contour.Points.size() < 3)
                        continue;

                    // Создаём геометрию для заливки
                    auto device = session.Device();
                    
                    Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder pathBuilder(device);
                    
                    ScreenPoint first = camera.WorldToScreen(
                        WorldPoint(contour.Points[0].X, contour.Points[0].Y));
                    pathBuilder.BeginFigure(first.X, first.Y);

                    for (size_t i = 1; i < contour.Points.size(); ++i)
                    {
                        ScreenPoint pt = camera.WorldToScreen(
                            WorldPoint(contour.Points[i].X, contour.Points[i].Y));
                        pathBuilder.AddLine(pt.X, pt.Y);
                    }

                    pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);
                    
                    auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
                    session.FillGeometry(geometry, spaceColor);
                }
            }
        }

        // ============================================================
        // Отрисовка названий помещений
        // ============================================================
        
        static void DrawSpaceNames(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity)
        {
            Windows::UI::Color textColor = Windows::UI::ColorHelper::FromArgb(
                static_cast<uint8_t>(200 * opacity / 255), 60, 60, 60);

            for (const auto& space : doc.Spaces)
            {
                if (!space || space->Name.empty())
                    continue;

                // Находим центр помещения
                WorldPoint center = CalculateContourCenter(space->BoundaryContours);
                ScreenPoint screenCenter = camera.WorldToScreen(center);

                // Формируем текст
                std::wstring displayText = space->Name;
                if (space->Area > 0)
                {
                    wchar_t areaText[32];
                    swprintf_s(areaText, L" (%.1f м?)", space->Area);
                    displayText += areaText;
                }

                // Рисуем текст
                auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
                textFormat.FontSize(12.0f);
                textFormat.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

                session.DrawText(
                    winrt::hstring(displayText),
                    screenCenter.X, screenCenter.Y,
                    textColor,
                    textFormat);
            }
        }

        // ============================================================
        // Отрисовка плит
        // ============================================================
        
        static void DrawSlabs(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const IfcDocument& doc,
            const IfcReferenceLayer& layer,
            uint8_t opacity)
        {
            Windows::UI::Color slabColor = Windows::UI::ColorHelper::FromArgb(
                static_cast<uint8_t>(80 * opacity / 255), 150, 150, 150);

            for (const auto& slab : doc.Slabs)
            {
                if (!slab)
                    continue;

                for (const auto& contour : slab->Contours)
                {
                    DrawPolyline(session, camera, contour.Points, contour.IsClosed,
                                slabColor, 0.5f);
                }
            }
        }

        // ============================================================
        // Вспомогательные функции
        // ============================================================

        static void DrawPolyline(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const std::vector<IfcPoint2D>& points,
            bool isClosed,
            Windows::UI::Color color,
            float lineWidth)
        {
            if (points.size() < 2)
                return;

            for (size_t i = 0; i < points.size() - 1; ++i)
            {
                ScreenPoint sp1 = camera.WorldToScreen(WorldPoint(points[i].X, points[i].Y));
                ScreenPoint sp2 = camera.WorldToScreen(WorldPoint(points[i + 1].X, points[i + 1].Y));

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(sp1.X, sp1.Y),
                    Windows::Foundation::Numerics::float2(sp2.X, sp2.Y),
                    color,
                    lineWidth);
            }

            // Замыкаем, если нужно
            if (isClosed && points.size() > 2)
            {
                ScreenPoint sp1 = camera.WorldToScreen(
                    WorldPoint(points.back().X, points.back().Y));
                ScreenPoint sp2 = camera.WorldToScreen(
                    WorldPoint(points.front().X, points.front().Y));

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(sp1.X, sp1.Y),
                    Windows::Foundation::Numerics::float2(sp2.X, sp2.Y),
                    color,
                    lineWidth);
            }
        }

        static void DrawArc90(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            ScreenPoint center,
            float radius,
            Windows::UI::Color color,
            float lineWidth)
        {
            // Упрощённая дуга 90° (четверть окружности)
            const int segments = 8;
            const float startAngle = 0.0f;
            const float endAngle = 3.14159f / 2.0f; // 90°

            float prevX = center.X + radius * std::cos(startAngle);
            float prevY = center.Y + radius * std::sin(startAngle);

            for (int i = 1; i <= segments; ++i)
            {
                float angle = startAngle + (endAngle - startAngle) * i / segments;
                float x = center.X + radius * std::cos(angle);
                float y = center.Y + radius * std::sin(angle);

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(prevX, prevY),
                    Windows::Foundation::Numerics::float2(x, y),
                    color,
                    lineWidth);

                prevX = x;
                prevY = y;
            }
        }

        static WorldPoint CalculateContourCenter(const std::vector<IfcPolyline>& contours)
        {
            double sumX = 0, sumY = 0;
            size_t count = 0;

            for (const auto& contour : contours)
            {
                for (const auto& pt : contour.Points)
                {
                    sumX += pt.X;
                    sumY += pt.Y;
                    count++;
                }
            }

            if (count == 0)
                return WorldPoint(0, 0);

            return WorldPoint(sumX / count, sumY / count);
        }
    };
}
