#pragma once

#include "pch.h"
#include "Camera.h"

namespace winrt::estimate1
{
    // Класс для отрисовки сетки на холсте
    class GridRenderer
    {
    public:
        GridRenderer() = default;

        // Отрисовка сетки
        void Draw(
            const Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs& args,
            const Camera& camera)
        {
            auto session = args.DrawingSession();
            
            // Получаем видимую область
            WorldPoint topLeft, bottomRight;
            camera.GetVisibleBounds(topLeft, bottomRight);

            // Определяем шаг сетки в зависимости от масштаба
            double zoom = camera.GetZoom();
            GridSpacing spacing = CalculateGridSpacing(zoom);

            // Рисуем минорную сетку
            DrawGridLines(session, camera, topLeft, bottomRight, 
                spacing.minorSpacing, m_minorGridColor, m_minorLineWidth);

            // Рисуем мажорную сетку
            DrawGridLines(session, camera, topLeft, bottomRight, 
                spacing.majorSpacing, m_majorGridColor, m_majorLineWidth);

            // Рисуем оси координат
            if (m_showAxes)
            {
                DrawAxes(session, camera);
            }
        }

        // Настройки сетки
        void SetShowAxes(bool show) { m_showAxes = show; }
        bool GetShowAxes() const { return m_showAxes; }

    private:
        struct GridSpacing
        {
            double minorSpacing;  // Шаг минорной сетки в мм
            double majorSpacing;  // Шаг мажорной сетки в мм
        };

        // Расчёт оптимального шага сетки в зависимости от масштаба
        GridSpacing CalculateGridSpacing(double zoom) const
        {
            // Целевое расстояние между линиями сетки на экране (пиксели)
            constexpr double targetMinorPixels = 20.0;
            constexpr double targetMajorPixels = 100.0;

            // Вычисляем шаг в мировых единицах
            double rawMinorSpacing = targetMinorPixels / zoom;
            double rawMajorSpacing = targetMajorPixels / zoom;

            // Округляем до "красивых" значений (10, 50, 100, 500, 1000 мм и т.д.)
            double minorSpacing = RoundToNiceValue(rawMinorSpacing);
            double majorSpacing = RoundToNiceValue(rawMajorSpacing);

            // Мажорная сетка должна быть кратна минорной
            if (majorSpacing <= minorSpacing)
            {
                majorSpacing = minorSpacing * 5;
            }

            return { minorSpacing, majorSpacing };
        }

        // Округление до "красивого" значения
        double RoundToNiceValue(double value) const
        {
            // Возможные базовые значения: 1, 2, 5
            static const double bases[] = { 1.0, 2.0, 5.0 };

            // Находим порядок величины
            double magnitude = std::pow(10.0, std::floor(std::log10(value)));
            double normalized = value / magnitude;

            // Выбираем ближайшее базовое значение
            double best = bases[0];
            double bestDiff = std::abs(normalized - bases[0]);

            for (double base : bases)
            {
                double diff = std::abs(normalized - base);
                if (diff < bestDiff)
                {
                    bestDiff = diff;
                    best = base;
                }
            }

            return best * magnitude;
        }

        // Отрисовка линий сетки
        void DrawGridLines(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& topLeft,
            const WorldPoint& bottomRight,
            double spacing,
            const Windows::UI::Color& color,
            float lineWidth)
        {
            if (spacing <= 0) return;

            // Вертикальные линии
            double startX = std::floor(topLeft.X / spacing) * spacing;
            double endX = std::ceil(bottomRight.X / spacing) * spacing;

            for (double x = startX; x <= endX; x += spacing)
            {
                ScreenPoint top = camera.WorldToScreen(WorldPoint(x, topLeft.Y));
                ScreenPoint bottom = camera.WorldToScreen(WorldPoint(x, bottomRight.Y));
                
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(top.X, top.Y),
                    Windows::Foundation::Numerics::float2(bottom.X, bottom.Y),
                    color, lineWidth);
            }

            // Горизонтальные линии
            double startY = std::floor(topLeft.Y / spacing) * spacing;
            double endY = std::ceil(bottomRight.Y / spacing) * spacing;

            for (double y = startY; y <= endY; y += spacing)
            {
                ScreenPoint left = camera.WorldToScreen(WorldPoint(topLeft.X, y));
                ScreenPoint right = camera.WorldToScreen(WorldPoint(bottomRight.X, y));
                
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(left.X, left.Y),
                    Windows::Foundation::Numerics::float2(right.X, right.Y),
                    color, lineWidth);
            }
        }

        // Отрисовка осей координат
        void DrawAxes(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera)
        {
            float canvasWidth = camera.GetCanvasWidth();
            float canvasHeight = camera.GetCanvasHeight();

            // Находим позицию осей на экране
            ScreenPoint origin = camera.WorldToScreen(WorldPoint(0, 0));

            // Ось X (горизонтальная)
            if (origin.Y >= 0 && origin.Y <= canvasHeight)
            {
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(0, origin.Y),
                    Windows::Foundation::Numerics::float2(canvasWidth, origin.Y),
                    m_axisColor, m_axisLineWidth);
            }

            // Ось Y (вертикальная)
            if (origin.X >= 0 && origin.X <= canvasWidth)
            {
                session.DrawLine(
                    Windows::Foundation::Numerics::float2(origin.X, 0),
                    Windows::Foundation::Numerics::float2(origin.X, canvasHeight),
                    m_axisColor, m_axisLineWidth);
            }
        }

        // Цвета и толщины линий
        Windows::UI::Color m_minorGridColor{ Windows::UI::ColorHelper::FromArgb(40, 128, 128, 128) };
        Windows::UI::Color m_majorGridColor{ Windows::UI::ColorHelper::FromArgb(80, 128, 128, 128) };
        Windows::UI::Color m_axisColor{ Windows::UI::ColorHelper::FromArgb(150, 100, 100, 200) };

        float m_minorLineWidth{ 0.5f };
        float m_majorLineWidth{ 1.0f };
        float m_axisLineWidth{ 1.5f };

        bool m_showAxes{ true };
    };
}
