#pragma once

#include "pch.h"
#include "Camera.h"
#include "WallSnapSystem.h"
#include <winrt/Microsoft.Graphics.Canvas.h>
#include <winrt/Microsoft.Graphics.Canvas.Geometry.h>
#include <winrt/Microsoft.Graphics.Canvas.Text.h>

namespace winrt::estimate1
{
    // ============================================================================
    // Wall Snap Renderer (Рендерер привязок стен)
    // ============================================================================

    class WallSnapRenderer
    {
    public:
        // ============================================================
        // Отрисовка индикатора привязки
        // ============================================================

        static void DrawSnapIndicator(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WallSnapCandidate& snap)
        {
            if (!snap.IsValid)
                return;

            ScreenPoint screenPos = camera.WorldToScreen(snap.ProjectedPoint);
            Windows::UI::Color color = WallSnapSystem::GetSnapPlaneColor(snap.Plane);

            // Размер индикатора
            float size = snap.IsEndpoint ? 8.0f : 6.0f;
            float halfSize = size / 2.0f;

            // Рисуем разные формы в зависимости от типа привязки
            switch (snap.Plane)
            {
            case WallSnapPlane::Endpoint:
                // Квадрат с заливкой для конечных точек
                DrawEndpointMarker(session, screenPos, color, size);
                break;

            case WallSnapPlane::Midpoint:
                // Треугольник для середины
                DrawMidpointMarker(session, screenPos, color, size);
                break;

            case WallSnapPlane::Centerline:
            case WallSnapPlane::CoreCenterline:
                // Круг для оси
                DrawCenterlineMarker(session, screenPos, color, size);
                break;

            case WallSnapPlane::FinishFaceExterior:
            case WallSnapPlane::FinishFaceInterior:
            case WallSnapPlane::CoreFaceExterior:
            case WallSnapPlane::CoreFaceInterior:
                // Ромб для граней
                DrawFaceMarker(session, screenPos, color, size);
                break;

            default:
                // Круг по умолчанию
                session.DrawCircle(
                    Windows::Foundation::Numerics::float2(screenPos.X, screenPos.Y),
                    halfSize, color, 2.0f);
                break;
            }
        }

        // ============================================================
        // Отрисовка линий выравнивания (пунктир и размеры как в AutoCAD)
        // ============================================================

        static void DrawAlignmentLines(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WallSnapCandidate& snap)
        {
            if (!snap.IsValid || !snap.IsAlignment)
                return;

            Windows::UI::Color color = Windows::UI::ColorHelper::FromArgb(200, 0, 255, 255); // Cyan
            
            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);

            ScreenPoint p1 = camera.WorldToScreen(snap.AlignmentSource);
            ScreenPoint p2 = camera.WorldToScreen(snap.ProjectedPoint);

            // 1. Пунктирная линия вдоль оси стены (с выходом за края)
            float dx = p2.X - p1.X;
            float dy = p2.Y - p1.Y;
            float len = std::sqrt(dx * dx + dy * dy);
            
            if (len > 1.0f)
            {
                float ux = dx / len;
                float uy = dy / len;
                float extension = 40.0f; // px

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(p1.X - ux * extension, p1.Y - uy * extension),
                    Windows::Foundation::Numerics::float2(p2.X + ux * extension, p2.Y + uy * extension),
                    color, 0.8f, strokeStyle);

                // 2. Временный размер над стеной
                double distanceMM = snap.AlignmentSource.Distance(snap.ProjectedPoint);
                if (distanceMM > 10.0)
                {
                    DrawAutoCadDimension(session, camera, snap.AlignmentSource, snap.ProjectedPoint, distanceMM);
                }
                
                // 4. Двойная стрелочка (magenta) на конце как на фото
                DrawDoubleArrow(session, camera, snap.ProjectedPoint, !snap.AlignmentHorizontal);
            }
        }

        // ============================================================
        // Отрисовка тултипа с названием привязки
        // ============================================================

        static void DrawSnapTooltip(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WallSnapCandidate& snap)
        {
            if (!snap.IsValid)
                return;

            ScreenPoint screenPos = camera.WorldToScreen(snap.ProjectedPoint);
            std::wstring text = WallSnapSystem::GetSnapPlaneShortName(snap.Plane);

            if (text.empty())
                return;

            // Позиция тултипа (справа сверху от точки)
            float tooltipX = screenPos.X + 12.0f;
            float tooltipY = screenPos.Y - 16.0f;

            // Создаём формат текста
            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(11.0f);

            // Измеряем текст для фона
            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session.Device(),
                winrt::hstring(text),
                textFormat,
                200.0f, 50.0f);

            float textWidth = static_cast<float>(textLayout.LayoutBounds().Width);
            float textHeight = static_cast<float>(textLayout.LayoutBounds().Height);

            // Фон тултипа
            Windows::Foundation::Rect bgRect(
                tooltipX - 3.0f,
                tooltipY - 1.0f,
                textWidth + 6.0f,
                textHeight + 2.0f);

            session.FillRoundedRectangle(
                bgRect,
                3.0f, 3.0f,
                Windows::UI::ColorHelper::FromArgb(220, 40, 40, 40));

            // Текст
            session.DrawText(
                winrt::hstring(text),
                tooltipX, tooltipY,
                Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255),
                textFormat);
        }

        // ============================================================
        // Отрисовка линий привязки стены (при наведении)
        // ============================================================

        static void DrawWallReferenceLines(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const Wall& wall,
            const WallSnapSystem& snapSystem,
            bool showAllLines = false)
        {
            auto refLines = snapSystem.GetWallReferenceLines(wall);

            for (const auto& line : refLines)
            {
                // Показываем только некоторые линии если не showAllLines
                if (!showAllLines)
                {
                    if (line.Plane != WallSnapPlane::Centerline &&
                        line.Plane != WallSnapPlane::FinishFaceExterior &&
                        line.Plane != WallSnapPlane::FinishFaceInterior)
                    {
                        continue;
                    }
                }

                ScreenPoint sp1 = camera.WorldToScreen(line.Start);
                ScreenPoint sp2 = camera.WorldToScreen(line.End);

                Windows::UI::Color color = WallSnapSystem::GetSnapPlaneColor(line.Plane);
                color.A = 100; // Полупрозрачность

                float lineWidth = (line.Plane == WallSnapPlane::Centerline) ? 1.0f : 0.5f;

                // Стиль линии
                auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
                
                if (line.Plane == WallSnapPlane::Centerline ||
                    line.Plane == WallSnapPlane::CoreCenterline)
                {
                    // Штриховая линия для осей
                    strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);
                }
                else
                {
                    // Пунктирная линия для граней
                    strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dot);
                }

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(sp1.X, sp1.Y),
                    Windows::Foundation::Numerics::float2(sp2.X, sp2.Y),
                    color,
                    lineWidth,
                    strokeStyle);
            }
        }

        // ============================================================
        // Отрисовка линии соединения (preview line от курсора к привязке)
        // ============================================================

        static void DrawSnapConnectionLine(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WorldPoint& fromPoint,
            const WallSnapCandidate& toSnap)
        {
            if (!toSnap.IsValid)
                return;

            ScreenPoint sp1 = camera.WorldToScreen(fromPoint);
            ScreenPoint sp2 = camera.WorldToScreen(toSnap.ProjectedPoint);

            Windows::UI::Color color = WallSnapSystem::GetSnapPlaneColor(toSnap.Plane);
            color.A = 150;

            // Тонкая линия от точки к привязке
            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);

            session.DrawLine(
                Windows::Foundation::Numerics::float2(sp1.X, sp1.Y),
                Windows::Foundation::Numerics::float2(sp2.X, sp2.Y),
                color,
                1.0f,
                strokeStyle);
        }

        // ============================================================
        // Отрисовка индикатора режима привязки в статусной строке
        // ============================================================

        static void DrawSnapModeIndicator(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            SnapReferenceMode mode,
            float x, float y)
        {
            std::wstring modeText = L"Привязка: " + WallSnapSystem::GetReferenceModeName(mode);

            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(11.0f);

            session.DrawText(
                winrt::hstring(modeText),
                x, y,
                Windows::UI::ColorHelper::FromArgb(200, 80, 80, 80),
                textFormat);
        }

    private:
        static void DrawAutoCadDimension(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WorldPoint& start,
            const WorldPoint& end,
            double distanceMM)
        {
            ScreenPoint p1 = camera.WorldToScreen(start);
            ScreenPoint p2 = camera.WorldToScreen(end);

            float dx = p2.X - p1.X;
            float dy = p2.Y - p1.Y;
            float len = std::sqrt(dx * dx + dy * dy);
            float ux = dx / len;
            float uy = dy / len;
            float px = -uy; // Perpendicular
            float py = ux;

            float offset = -40.0f; // Смещение вверх (в Win2D Y уменьшается вверх)
            
            ScreenPoint d1(p1.X + px * offset, p1.Y + py * offset);
            ScreenPoint d2(p2.X + px * offset, p2.Y + py * offset);

            Windows::UI::Color dimColor = Windows::UI::ColorHelper::FromArgb(255, 120, 120, 140);
            Windows::UI::Color textColor = Windows::UI::ColorHelper::FromArgb(255, 255, 180, 100); // Оранжевый из фото

            // Размерная линия
            session.DrawLine(d1.X, d1.Y, d2.X, d2.Y, dimColor, 1.2f);

            // Короткие выносные линии (перпендикулярные к стене)
            float ext = 10.0f;
            session.DrawLine(d1.X - px * ext, d1.Y - py * ext, d1.X + px * ext, d1.Y + py * ext, dimColor, 1.0f);
            session.DrawLine(d2.X - px * ext, d2.Y - py * ext, d2.X + px * ext, d2.Y + py * ext, dimColor, 1.0f);

            // Засечки (?) - под углом 45 градусов
            float tickSize = 6.0f;
            float tx = (ux + px) * tickSize;
            float ty = (uy + py) * tickSize;
            session.DrawLine(d1.X - tx, d1.Y - ty, d1.X + tx, d1.Y + ty, dimColor, 2.0f);
            session.DrawLine(d2.X - tx, d2.Y - ty, d2.X + tx, d2.Y + ty, dimColor, 2.0f);

            // Текст
            wchar_t distText[32];
            swprintf_s(distText, L"%.1f", distanceMM);
            
            auto format = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            format.FontSize(11.0f);
            format.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

            session.DrawText(distText, 
                Windows::Foundation::Numerics::float2((d1.X + d2.X) / 2, (d1.Y + d2.Y) / 2 - 16.0f),
                textColor, format);
        }

        static void DrawDoubleArrow(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WorldPoint& point,
            bool vertical)
        {
            ScreenPoint sp = camera.WorldToScreen(point);
            float s = 6.0f;
            Windows::UI::Color color = Windows::UI::ColorHelper::FromArgb(255, 255, 0, 255); // Magenta

            if (vertical)
            {
                // Вертикальная двойная стрелочка
                session.DrawLine(sp.X, sp.Y - s, sp.X, sp.Y + s, color, 2.0f);
                session.DrawLine(sp.X - 3, sp.Y - s + 3, sp.X, sp.Y - s, color, 2.0f);
                session.DrawLine(sp.X + 3, sp.Y - s + 3, sp.X, sp.Y - s, color, 2.0f);
                session.DrawLine(sp.X - 3, sp.Y + s - 3, sp.X, sp.Y + s, color, 2.0f);
                session.DrawLine(sp.X + 3, sp.Y + s - 3, sp.X, sp.Y + s, color, 2.0f);
            }
            else
            {
                // Горизонтальная двойная стрелочка
                session.DrawLine(sp.X - s, sp.Y, sp.X + s, sp.Y, color, 2.0f);
                session.DrawLine(sp.X - s + 3, sp.Y - 3, sp.X - s, sp.Y, color, 2.0f);
                session.DrawLine(sp.X - s + 3, sp.Y + 3, sp.X - s, sp.Y, color, 2.0f);
                session.DrawLine(sp.X + s - 3, sp.Y - 3, sp.X + s, sp.Y, color, 2.0f);
                session.DrawLine(sp.X + s - 3, sp.Y + 3, sp.X + s, sp.Y, color, 2.0f);
            }
        }

    private:
        // ============================================================
        // Маркеры разных типов
        // ============================================================

        static void DrawEndpointMarker(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            ScreenPoint pos,
            Windows::UI::Color color,
            float size)
        {
            float halfSize = size / 2.0f;

            // Квадрат с заливкой
            Windows::Foundation::Rect rect(
                pos.X - halfSize,
                pos.Y - halfSize,
                size,
                size);

            // Белая заливка
            session.FillRectangle(rect, Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255));
            
            // Цветная рамка
            session.DrawRectangle(rect, color, 2.0f);
        }

        static void DrawMidpointMarker(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            ScreenPoint pos,
            Windows::UI::Color color,
            float size)
        {
            float halfSize = size / 2.0f;

            // Треугольник направленный вниз
            auto device = session.Device();
            Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder pathBuilder(device);

            pathBuilder.BeginFigure(pos.X, pos.Y - halfSize);
            pathBuilder.AddLine(pos.X + halfSize, pos.Y + halfSize);
            pathBuilder.AddLine(pos.X - halfSize, pos.Y + halfSize);
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            session.FillGeometry(geometry, Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255));
            session.DrawGeometry(geometry, color, 2.0f);
        }

        static void DrawCenterlineMarker(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            ScreenPoint pos,
            Windows::UI::Color color,
            float size)
        {
            float radius = size / 2.0f;

            // Круг с заливкой
            Windows::Foundation::Numerics::float2 center(pos.X, pos.Y);

            session.FillCircle(center, radius, Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255));
            session.DrawCircle(center, radius, color, 2.0f);

            // Перекрестие внутри
            float crossSize = radius * 0.6f;
            session.DrawLine(
                Windows::Foundation::Numerics::float2(pos.X - crossSize, pos.Y),
                Windows::Foundation::Numerics::float2(pos.X + crossSize, pos.Y),
                color, 1.0f);
            session.DrawLine(
                Windows::Foundation::Numerics::float2(pos.X, pos.Y - crossSize),
                Windows::Foundation::Numerics::float2(pos.X, pos.Y + crossSize),
                color, 1.0f);
        }

        static void DrawFaceMarker(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            ScreenPoint pos,
            Windows::UI::Color color,
            float size)
        {
            float halfSize = size / 2.0f;

            // Ромб
            auto device = session.Device();
            Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder pathBuilder(device);

            pathBuilder.BeginFigure(pos.X, pos.Y - halfSize);
            pathBuilder.AddLine(pos.X + halfSize, pos.Y);
            pathBuilder.AddLine(pos.X, pos.Y + halfSize);
            pathBuilder.AddLine(pos.X - halfSize, pos.Y);
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            session.FillGeometry(geometry, Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255));
            session.DrawGeometry(geometry, color, 2.0f);
        }
    };
}
