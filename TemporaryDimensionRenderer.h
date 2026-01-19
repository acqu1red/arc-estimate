#pragma once
#include "pch.h"
#include "Camera.h"
#include "Models.h"
#include <vector>

namespace winrt::estimate1
{
    // =====================================================
    // Временный размер (показывается после клика)
    // =====================================================
    struct TemporaryDimension
    {
        WorldPoint Point1;          // Начальная точка
        WorldPoint Point2;          // Конечная точка
        double Distance;            // Расстояние
        bool IsHorizontal;          // Горизонтальный размер
        bool IsVertical;            // Вертикальный размер
        double Offset;              // Смещение от линии (мм)
    };

    // =====================================================
    // Рендерер временных размеров (стиль AutoCAD)
    // =====================================================
    class TemporaryDimensionRenderer
    {
    public:
        // Рисовать временные размеры после клика
        static void DrawTemporaryDimensions(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const std::vector<TemporaryDimension>& dimensions)
        {
            for (const auto& dim : dimensions)
            {
                DrawTemporaryDimension(session, camera, dim);
            }
        }

        // Рисовать один временный размер
        static void DrawTemporaryDimension(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const TemporaryDimension& dim)
        {
            ScreenPoint p1 = camera.WorldToScreen(dim.Point1);
            ScreenPoint p2 = camera.WorldToScreen(dim.Point2);

            // Цвет: голубой как в AutoCAD
            Windows::UI::Color lineColor = Windows::UI::ColorHelper::FromArgb(255, 100, 150, 200);
            Windows::UI::Color textColor = Windows::UI::ColorHelper::FromArgb(255, 100, 150, 255);

            // Вычисляем направление и перпендикуляр
            float dx = p2.X - p1.X;
            float dy = p2.Y - p1.Y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01f) return;

            float dirX = dx / len;
            float dirY = dy / len;
            float perpX = -dirY;
            float perpY = dirX;

            // Смещение в экранных координатах
            float offsetScreen = static_cast<float>(dim.Offset) * camera.GetZoom();

            // Точки размерной линии со смещением
            ScreenPoint lineP1(p1.X + perpX * offsetScreen, p1.Y + perpY * offsetScreen);
            ScreenPoint lineP2(p2.X + perpX * offsetScreen, p2.Y + perpY * offsetScreen);

            // Extension lines (выносные линии)
            session.DrawLine(
                Windows::Foundation::Numerics::float2(p1.X, p1.Y),
                Windows::Foundation::Numerics::float2(lineP1.X, lineP1.Y),
                lineColor, 1.0f);
            session.DrawLine(
                Windows::Foundation::Numerics::float2(p2.X, p2.Y),
                Windows::Foundation::Numerics::float2(lineP2.X, lineP2.Y),
                lineColor, 1.0f);

            // Размерная линия
            session.DrawLine(
                Windows::Foundation::Numerics::float2(lineP1.X, lineP1.Y),
                Windows::Foundation::Numerics::float2(lineP2.X, lineP2.Y),
                lineColor, 1.0f);

            // Стрелки на концах (маленькие треугольники)
            DrawArrowHead(session, lineP1, lineP2, lineColor);
            DrawArrowHead(session, lineP2, lineP1, lineColor);

            // Текст размера (в середине)
            wchar_t text[64];
            swprintf_s(text, L"%.1f", dim.Distance);

            ScreenPoint textPos(
                (lineP1.X + lineP2.X) / 2.0f,
                (lineP1.Y + lineP2.Y) / 2.0f - 10.0f);

            // Фон для текста (белый прямоугольник)
            Microsoft::Graphics::Canvas::Text::CanvasTextFormat format;
            format.FontSize(12);
            format.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session, text, format, 0.0f, 0.0f);
            
            auto bounds = textLayout.LayoutBounds();
            Windows::Foundation::Rect bgRect(
                textPos.X - bounds.Width / 2 - 2,
                textPos.Y - bounds.Height / 2 - 1,
                bounds.Width + 4,
                bounds.Height + 2);

            session.FillRectangle(bgRect, Windows::UI::Colors::White());
            session.DrawText(
                text,
                Windows::Foundation::Numerics::float2(textPos.X, textPos.Y - bounds.Height / 2),
                textColor,
                format);
        }

    private:
        // Рисовать стрелку на конце размерной линии
        static void DrawArrowHead(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const ScreenPoint& tip,
            const ScreenPoint& base,
            const Windows::UI::Color& color)
        {
            float dx = tip.X - base.X;
            float dy = tip.Y - base.Y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01f) return;

            float dirX = dx / len;
            float dirY = dy / len;

            float arrowSize = 8.0f;
            float arrowWidth = 3.0f;

            // Точки треугольника стрелки
            ScreenPoint p1 = tip;
            ScreenPoint p2(
                tip.X - dirX * arrowSize + dirY * arrowWidth,
                tip.Y - dirY * arrowSize - dirX * arrowWidth);
            ScreenPoint p3(
                tip.X - dirX * arrowSize - dirY * arrowWidth,
                tip.Y - dirY * arrowSize + dirX * arrowWidth);

            // Рисуем закрашенный треугольник
            Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder pathBuilder(session);
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(p1.X, p1.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(p2.X, p2.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(p3.X, p3.Y));
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
            session.FillGeometry(geometry, color);
        }
    };

    // =====================================================
    // Менеджер временных размеров
    // =====================================================
    class TemporaryDimensionManager
    {
    private:
        std::vector<TemporaryDimension> m_dimensions;
        bool m_isVisible{ false };

    public:
        // Генерировать временные размеры от точки до всех стен
        void GenerateFromPoint(
            const WorldPoint& point,
            const std::vector<std::unique_ptr<Wall>>& walls,
            double maxDistance = 5000.0)
        {
            m_dimensions.clear();

            for (const auto& wall : walls)
            {
                if (!wall) continue;

                // Находим ближайшую точку на стене
                WorldPoint closest = FindClosestPointOnWall(*wall, point);
                double distance = point.Distance(closest);

                if (distance < maxDistance && distance > 10.0)
                {
                    TemporaryDimension dim;
                    dim.Point1 = point;
                    dim.Point2 = closest;
                    dim.Distance = distance;
                    
                    // Определяем ориентацию
                    double dx = std::abs(closest.X - point.X);
                    double dy = std::abs(closest.Y - point.Y);
                    dim.IsHorizontal = (dx > dy * 2);
                    dim.IsVertical = (dy > dx * 2);
                    
                    // Смещение размера
                    dim.Offset = dim.IsHorizontal ? -300.0 : 300.0;

                    m_dimensions.push_back(dim);
                }
            }

            m_isVisible = true;
        }

        // Показать временные размеры
        void Show() { m_isVisible = true; }

        // Скрыть временные размеры
        void Hide() 
        { 
            m_isVisible = false;
            m_dimensions.clear();
        }

        // Получить размеры для рисования
        const std::vector<TemporaryDimension>& GetDimensions() const { return m_dimensions; }

        // Видимы ли размеры
        bool IsVisible() const { return m_isVisible; }

    private:
        // Найти ближайшую точку на стене к заданной точке
        static WorldPoint FindClosestPointOnWall(const Wall& wall, const WorldPoint& point)
        {
            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double lenSq = dx * dx + dy * dy;

            if (lenSq < 0.001)
                return start;

            // Параметр t на отрезке [0, 1]
            double t = ((point.X - start.X) * dx + (point.Y - start.Y) * dy) / lenSq;
            t = (std::max)(0.0, (std::min)(1.0, t));

            return WorldPoint(
                start.X + t * dx,
                start.Y + t * dy);
        }
    };
}
