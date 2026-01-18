#pragma once

#include "pch.h"
#include "Camera.h"
#include "Dimension.h"
#include "Layer.h"
#include "Models.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace winrt::estimate1
{
    class DimensionRenderer
    {
    public:
        DimensionRenderer() = default;

        void SetHover(uint64_t dimensionId, DimensionHandle handle)
        {
            m_hoverDimensionId = dimensionId;
            m_hoverHandle = handle;
        }

        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            const LayerManager& layerManager)
        {
            (void)layerManager;

            // Рисуем авторазмеры
            for (const auto& dim : document.GetDimensions())
            {
                DrawDimension(session, camera, *dim, false);
            }

            // Рисуем ручные размеры (другим цветом)
            for (const auto& dim : document.GetManualDimensions())
            {
                DrawDimension(session, camera, *dim, true);
            }
        }

        // Рисуем превью размера при ручном размещении
        void DrawPreview(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& p1,
            const WorldPoint& p2,
            double offset)
        {
            if (p1.Distance(p2) < 10.0)
                return;

            // Создаём временный размер для отрисовки
            Dimension preview(p1, p2);
            preview.SetOffset(offset);

            DrawDimension(session, camera, preview, true, true);
        }

        // =====================================================
        // R3 — ОТРИСОВКА УГЛА ПРИ РИСОВАНИИ СТЕНЫ
        // =====================================================

        // Отрисовка угла между существующей и рисуемой стенами
        void DrawAnglePreview(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& center,
            const WorldPoint& direction1,
            const WorldPoint& direction2,
            double arcRadius = 50.0)
        {
            double angle1 = std::atan2(direction1.Y, direction1.X);
            double angle2 = std::atan2(direction2.Y, direction2.X);

            double angleDiff = angle2 - angle1;
            while (angleDiff > M_PI) angleDiff -= 2 * M_PI;
            while (angleDiff < -M_PI) angleDiff += 2 * M_PI;

            double angleDeg = std::abs(angleDiff) * 180.0 / M_PI;

            ScreenPoint screenCenter = camera.WorldToScreen(center);
            float screenRadius = static_cast<float>(arcRadius * camera.GetZoom());

            if (screenRadius < 20.0f) screenRadius = 20.0f;
            if (screenRadius > 80.0f) screenRadius = 80.0f;

            Windows::UI::Color arcColor = (std::abs(angleDeg - 90.0) < 1.0)
                ? Windows::UI::ColorHelper::FromArgb(200, 0, 180, 0)
                : Windows::UI::ColorHelper::FromArgb(200, 255, 140, 0);

            DrawArc(session, screenCenter, screenRadius,
                    static_cast<float>(angle1), static_cast<float>(angleDiff), arcColor);

            wchar_t angleText[32];
            swprintf_s(angleText, L"%.1f°", angleDeg);

            double midAngle = angle1 + angleDiff / 2;
            float textRadius = screenRadius + 15.0f;
            float textX = screenCenter.X + textRadius * static_cast<float>(std::cos(midAngle));
            float textY = screenCenter.Y + textRadius * static_cast<float>(std::sin(midAngle));

            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(11);

            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session.Device(), angleText, textFormat, 100, 30);

            float textWidth = textLayout.LayoutBounds().Width;
            float textHeight = textLayout.LayoutBounds().Height;

            session.FillRoundedRectangle(
                Windows::Foundation::Rect(textX - textWidth / 2 - 3, textY - textHeight / 2 - 2,
                                          textWidth + 6, textHeight + 4),
                3.0f, 3.0f,
                Windows::UI::ColorHelper::FromArgb(220, 255, 255, 230));

            session.DrawText(angleText, textX - textWidth / 2, textY - textHeight / 2, arcColor);
        }

        // Отрисовка угла от горизонтали (при рисовании стены)
        void DrawAngleFromHorizontal(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& startPoint,
            const WorldPoint& endPoint,
            double arcRadius = 80.0)
        {
            double dx = endPoint.X - startPoint.X;
            double dy = endPoint.Y - startPoint.Y;
            double length = std::sqrt(dx * dx + dy * dy);

            if (length < 10.0)
                return;

            double angle = std::atan2(dy, dx);
            double angleDeg = angle * 180.0 / M_PI;
            if (angleDeg < 0) angleDeg += 360.0;

            ScreenPoint screenStart = camera.WorldToScreen(startPoint);
            float screenRadius = static_cast<float>(arcRadius * camera.GetZoom());

            if (screenRadius < 25.0f) screenRadius = 25.0f;
            if (screenRadius > 100.0f) screenRadius = 100.0f;

            bool isNiceAngle = false;
            for (double nice : {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0, 360.0})
            {
                if (std::abs(angleDeg - nice) < 1.0)
                {
                    isNiceAngle = true;
                    break;
                }
            }

            Windows::UI::Color arcColor = isNiceAngle
                ? Windows::UI::ColorHelper::FromArgb(180, 0, 150, 0)
                : Windows::UI::ColorHelper::FromArgb(180, 200, 100, 0);

            DrawArc(session, screenStart, screenRadius, 0.0f, static_cast<float>(angle), arcColor);

            auto dashStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            dashStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dot);

            session.DrawLine(
                Windows::Foundation::Numerics::float2(screenStart.X, screenStart.Y),
                Windows::Foundation::Numerics::float2(screenStart.X + screenRadius * 1.5f, screenStart.Y),
                Windows::UI::ColorHelper::FromArgb(100, 100, 100, 100),
                1.0f, dashStyle);

            wchar_t angleText[32];
            swprintf_s(angleText, L"%.2f°", angleDeg);

            float midAngle = static_cast<float>(angle / 2);
            float textRadius = screenRadius + 18.0f;
            float textX = screenStart.X + textRadius * std::cos(midAngle);
            float textY = screenStart.Y + textRadius * std::sin(midAngle);

            session.DrawText(angleText, textX - 20.0f, textY - 8.0f, arcColor);
        }

    private:
        uint64_t m_hoverDimensionId{ 0 };
        DimensionHandle m_hoverHandle{ DimensionHandle::None };

        void DrawArc(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const ScreenPoint& center,
            float radius,
            float startAngle,
            float sweepAngle,
            Windows::UI::Color color)
        {
            const int segments = 32;
            float step = sweepAngle / segments;

            for (int i = 0; i < segments; ++i)
            {
                float a1 = startAngle + i * step;
                float a2 = startAngle + (i + 1) * step;

                float x1 = center.X + radius * std::cos(a1);
                float y1 = center.Y + radius * std::sin(a1);
                float x2 = center.X + radius * std::cos(a2);
                float y2 = center.Y + radius * std::sin(a2);

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(x1, y1),
                    Windows::Foundation::Numerics::float2(x2, y2),
                    color, 1.5f);
            }
        }
        
        static Windows::UI::Color BaseColor(bool selected, bool isManual)
        {
            if (isManual)
            {
                return selected
                    ? Windows::UI::ColorHelper::FromArgb(230, 0, 150, 100)   // Зелёный для ручных
                    : Windows::UI::ColorHelper::FromArgb(180, 0, 120, 80);
            }
            return selected
                ? Windows::UI::ColorHelper::FromArgb(230, 0, 120, 215)
                : Windows::UI::ColorHelper::FromArgb(180, 80, 80, 80);
        }

        void DrawDimension(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
                const Dimension& dim,
                bool isManual = false,
                bool isPreview = false)
            {
                WorldPoint p1 = dim.GetP1();
                WorldPoint p2 = dim.GetP2();

                // Нормаль
                double dx = p2.X - p1.X;
                double dy = p2.Y - p1.Y;
                double len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.001)
                    return;

                double nx = -dy / len;
                double ny = dx / len;

                double offset = dim.GetOffset();
                WorldPoint a1(p1.X + nx * offset, p1.Y + ny * offset);
                WorldPoint a2(p2.X + nx * offset, p2.Y + ny * offset);

                auto s1 = camera.WorldToScreen(p1);
                auto s2 = camera.WorldToScreen(p2);
                auto sa1 = camera.WorldToScreen(a1);
                auto sa2 = camera.WorldToScreen(a2);

                Windows::UI::Color color = BaseColor(dim.IsSelected(), isManual || dim.IsManual());
            
                // Для превью делаем полупрозрачным
                if (isPreview)
                {
                    color.A = 150;
                }

                // Выносные линии
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(s1.X, s1.Y),
                    Windows::Foundation::Numerics::float2(sa1.X, sa1.Y),
                    color,
                    1.0f);

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(s2.X, s2.Y),
                    Windows::Foundation::Numerics::float2(sa2.X, sa2.Y),
                    color,
                    1.0f);

                // Размерная линия
                float lineWidth = isPreview ? 1.0f : 1.5f;
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(sa1.X, sa1.Y),
                    Windows::Foundation::Numerics::float2(sa2.X, sa2.Y),
                    color,
                    lineWidth);

                // Засечки
                DimensionTickType tickType = dim.GetTickType();
                
                // Вектор линии размера (от p1 к p2)
                double dimDirX = ny; 
                double dimDirY = -nx;
                
                // Для Arrow передаем направление "наружу" (от центра к краю)
                // Для sa1 (начало) это вектор (p1 - p2) -> против (nx,-ny) -> т.е. (-ny, nx)
                // Но у нас nx,ny это нормаль К стене.
                // Вектор стены (dx, dy). Нормаль (-dy, dx).
                // Вектор размера параллелен стене.
                
                // Вычислим вектор на экране для точности
                float sdx = sa2.X - sa1.X;
                float sdy = sa2.Y - sa1.Y;
                float slen = std::sqrt(sdx*sdx + sdy*sdy);
                float dirX = (slen > 0.001f) ? sdx / slen : 0.0f;
                float dirY = (slen > 0.001f) ? sdy / slen : 0.0f;

                // Рисуем тики
                // Для sa1 (Start): направление линии ВПРАВО (к sa2). Стрелка должна указывать ВЛЕВО (наружу) или ВПРАВО (внутрь)?
                // Стандарт "Filled Arrow": острие касается выносной линии.
                // То есть на левом конце стрелка смотрит ВЛЕВО (<).
                // На правом конце стрелка смотрит ВПРАВО (>).
                
                DrawTick(session, sa1, -dirX, -dirY, color, tickType);
                DrawTick(session, sa2, dirX, dirY, color, tickType);

                // Текст
                double value = dim.GetValueMm();
                wchar_t text[64];
                swprintf_s(text, L"%.0f мм", value);

                // Позиция текста - середина размерной линии
                ScreenPoint mid((sa1.X + sa2.X) * 0.5f, (sa1.Y + sa2.Y) * 0.5f);
                session.DrawText(
                text,
                mid.X + 6.0f,
                mid.Y - 10.0f,
                Windows::UI::ColorHelper::FromArgb(220, 40, 40, 40));

            // Индикатор lock
            if (dim.IsLocked())
            {
                session.DrawText(
                    L"??",
                    mid.X - 14.0f,
                    mid.Y - 14.0f,
                    Windows::UI::ColorHelper::FromArgb(180, 120, 120, 120));
            }

            // CAD-ручки (рисуем только если размер выбран)
            bool hoverThis = (m_hoverDimensionId != 0 && dim.GetId() == m_hoverDimensionId);
            if (dim.IsSelected() || hoverThis)
            {
                WorldPoint h1w, hmw, h2w;
                dim.GetHandlePoints(h1w, hmw, h2w);

                DrawHandle(session, camera.WorldToScreen(h1w), color,
                    hoverThis && m_hoverHandle == DimensionHandle::Start);
                DrawHandle(session, camera.WorldToScreen(hmw), color,
                    hoverThis && m_hoverHandle == DimensionHandle::Middle);
                DrawHandle(session, camera.WorldToScreen(h2w), color,
                    hoverThis && m_hoverHandle == DimensionHandle::End);
            }
        }

        void DrawHandle(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const ScreenPoint& at,
            Windows::UI::Color color,
            bool isHot = false)
        {
            float size = isHot ? 9.0f : 7.0f;
            Windows::UI::Color fill = isHot
                ? Windows::UI::ColorHelper::FromArgb(255, 255, 240, 200)
                : Windows::UI::ColorHelper::FromArgb(230, 255, 255, 255);

            session.FillRectangle(
                Windows::Foundation::Rect(at.X - size / 2, at.Y - size / 2, size, size),
                fill);
            session.DrawRectangle(
                Windows::Foundation::Rect(at.X - size / 2, at.Y - size / 2, size, size),
                color,
                isHot ? 2.0f : 1.5f);
        }

        void DrawTick(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const ScreenPoint& at,
            float dirX, // Направление острия стрелки
            float dirY, 
            Windows::UI::Color color,
            DimensionTickType type = DimensionTickType::Tick)
        {
            if (type == DimensionTickType::Arrow)
            {
                // Рисуем стрелку. Острие в точке at.
                // Направление (dirX, dirY) - это куда указывает стрелка.
                // Хвост стрелки находится в направлении -(dirX, dirY).
                
                float arrowLen = 12.0f; // Длина стрелки
                float arrowWidth = 4.0f; // Половина ширины основания

                // База стрелки
                float baseX = at.X - dirX * arrowLen;
                float baseY = at.Y - dirY * arrowLen;

                // Перпендикуляр к направлению для ширины
                float perpX = -dirY;
                float perpY = dirX;

                // Точки основания
                float p1x = baseX + perpX * arrowWidth;
                float p1y = baseY + perpY * arrowWidth;
                float p2x = baseX - perpX * arrowWidth;
                float p2y = baseY - perpY * arrowWidth;

                // Рисуем заполненный треугольник
                // Используем GeometryBuilder или просто линии для начала (нет FillTriangle в session)
                // Можно использовать FillGeometry с массивом точек
                
                auto builder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session);
                builder.BeginFigure(Windows::Foundation::Numerics::float2(at.X, at.Y));
                builder.AddLine(Windows::Foundation::Numerics::float2(p1x, p1y));
                builder.AddLine(Windows::Foundation::Numerics::float2(p2x, p2y));
                builder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);
                
                auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(builder);
                session.FillGeometry(geometry, color);
            }
            else if (type == DimensionTickType::Dot)
            {
                session.FillCircle(
                    Windows::Foundation::Numerics::float2(at.X, at.Y),
                    3.0f, color);
            }
            else // Tick default
            {
                // Архитектурная засечка: диагональ экрана под 45 градусов
                float tickLen = 5.0f;
                
                // Фиксированный угол на экране (45 градусов)
                // (1, -1)
                float tx = 1.0f / 1.414f;
                float ty = -1.0f / 1.414f;
                
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(at.X - tx * tickLen, at.Y - ty * tickLen),
                    Windows::Foundation::Numerics::float2(at.X + tx * tickLen, at.Y + ty * tickLen),
                    color,
                    2.0f);
            }
        }
    };
}
