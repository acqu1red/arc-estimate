#pragma once

#include "pch.h"
#include "Camera.h"
#include "DxfReference.h"
#include <cmath>

namespace winrt::estimate1
{
    // ============================================================================
    // DXF Reference Renderer (рендеринг импортированных DXF-подложек)
    // ============================================================================

    class DxfReferenceRenderer
    {
    public:
        // Цвета AutoCAD по индексу (основные)
        static Windows::UI::Color GetColorByIndex(int colorIndex, Windows::UI::Color defaultColor)
        {
            // Основные цвета AutoCAD ACI
            switch (colorIndex)
            {
            case 1: return Windows::UI::ColorHelper::FromArgb(255, 255, 0, 0);      // Red
            case 2: return Windows::UI::ColorHelper::FromArgb(255, 255, 255, 0);    // Yellow
            case 3: return Windows::UI::ColorHelper::FromArgb(255, 0, 255, 0);      // Green
            case 4: return Windows::UI::ColorHelper::FromArgb(255, 0, 255, 255);    // Cyan
            case 5: return Windows::UI::ColorHelper::FromArgb(255, 0, 0, 255);      // Blue
            case 6: return Windows::UI::ColorHelper::FromArgb(255, 255, 0, 255);    // Magenta
            case 7: return Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255);  // White/Black
            case 8: return Windows::UI::ColorHelper::FromArgb(255, 128, 128, 128);  // Gray
            case 9: return Windows::UI::ColorHelper::FromArgb(255, 192, 192, 192);  // Light Gray
            default: return defaultColor;
            }
        }

        // Отрисовка всех DXF-подложек
        static void Draw(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfReferenceManager& manager)
        {
            for (const auto& layer : manager.GetLayers())
            {
                if (layer && layer->IsVisible())
                {
                    DrawLayer(session, camera, *layer);
                }
            }
        }

        // Отрисовка одного слоя
        static void DrawLayer(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfReferenceLayer& layer)
        {
            auto baseColor = layer.GetColor();
            uint8_t alpha = layer.GetOpacity();
            float lineWidth = layer.GetLineWidth();
            bool useOriginalColors = layer.UseOriginalColors();

            for (const auto& entity : layer.GetEntities())
            {
                if (!entity)
                    continue;

                // Определяем цвет
                Windows::UI::Color color = baseColor;
                color.A = alpha;

                if (useOriginalColors && entity->ColorIndex != 256)
                {
                    color = GetColorByIndex(entity->ColorIndex, baseColor);
                    color.A = alpha;
                }

                // Рисуем по типу
                switch (entity->Type)
                {
                case DxfEntityType::Line:
                    DrawLine(session, camera, static_cast<const DxfLine&>(*entity), color, lineWidth);
                    break;
                case DxfEntityType::LWPolyline:
                case DxfEntityType::Polyline:
                    DrawPolyline(session, camera, static_cast<const DxfPolyline&>(*entity), color, lineWidth);
                    break;
                case DxfEntityType::Circle:
                    DrawCircle(session, camera, static_cast<const DxfCircle&>(*entity), color, lineWidth);
                    break;
                case DxfEntityType::Arc:
                    DrawArc(session, camera, static_cast<const DxfArc&>(*entity), color, lineWidth);
                    break;
                case DxfEntityType::Text:
                case DxfEntityType::MText:
                    DrawText(session, camera, static_cast<const DxfText&>(*entity), color);
                    break;
                default:
                    break;
                }
            }
        }

    private:
        // Отрисовка линии
        static void DrawLine(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfLine& line,
            Windows::UI::Color color,
            float lineWidth)
        {
            auto p1 = camera.WorldToScreen(line.Start);
            auto p2 = camera.WorldToScreen(line.End);

            session.DrawLine(
                static_cast<float>(p1.X), static_cast<float>(p1.Y),
                static_cast<float>(p2.X), static_cast<float>(p2.Y),
                color, lineWidth);
        }

        // Отрисовка полилинии
        static void DrawPolyline(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfPolyline& poly,
            Windows::UI::Color color,
            float lineWidth)
        {
            if (poly.Vertices.size() < 2)
                return;

            // Рисуем сегменты
            for (size_t i = 0; i < poly.Vertices.size() - 1; ++i)
            {
                const auto& v1 = poly.Vertices[i];
                const auto& v2 = poly.Vertices[i + 1];

                auto p1 = camera.WorldToScreen(v1.Point);
                auto p2 = camera.WorldToScreen(v2.Point);

                if (std::abs(v1.Bulge) < 0.0001)
                {
                    // Прямой сегмент
                    session.DrawLine(
                        static_cast<float>(p1.X), static_cast<float>(p1.Y),
                        static_cast<float>(p2.X), static_cast<float>(p2.Y),
                        color, lineWidth);
                }
                else
                {
                    // Дуговой сегмент (bulge)
                    DrawBulgeArc(session, camera, v1.Point, v2.Point, v1.Bulge, color, lineWidth);
                }
            }

            // Замыкаем, если нужно
            if (poly.IsClosed && poly.Vertices.size() >= 2)
            {
                const auto& vLast = poly.Vertices.back();
                const auto& vFirst = poly.Vertices.front();

                auto pLast = camera.WorldToScreen(vLast.Point);
                auto pFirst = camera.WorldToScreen(vFirst.Point);

                if (std::abs(vLast.Bulge) < 0.0001)
                {
                    session.DrawLine(
                        static_cast<float>(pLast.X), static_cast<float>(pLast.Y),
                        static_cast<float>(pFirst.X), static_cast<float>(pFirst.Y),
                        color, lineWidth);
                }
                else
                {
                    DrawBulgeArc(session, camera, vLast.Point, vFirst.Point, vLast.Bulge, color, lineWidth);
                }
            }
        }

        // Отрисовка дуги по bulge
        static void DrawBulgeArc(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const WorldPoint& start,
            const WorldPoint& end,
            double bulge,
            Windows::UI::Color color,
            float lineWidth)
        {
            // Булж = tan(угол/4)
            // Положительный булж = против часовой, отрицательный = по часовой

            constexpr double PI = 3.14159265358979323846;

            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double chordLen = std::sqrt(dx * dx + dy * dy);

            if (chordLen < 0.001)
                return;

            double theta = 4.0 * std::atan(bulge);
            double radius = chordLen / (2.0 * std::sin(std::abs(theta) / 2.0));

            // Середина хорды
            double midX = (start.X + end.X) / 2.0;
            double midY = (start.Y + end.Y) / 2.0;

            // Перпендикуляр к хорде
            double perpX = -dy / chordLen;
            double perpY = dx / chordLen;

            // Расстояние от середины хорды до центра
            double sagitta = radius * (1.0 - std::cos(std::abs(theta) / 2.0));
            double dist = radius - sagitta;

            // Направление к центру зависит от знака bulge
            if (bulge > 0)
            {
                perpX = -perpX;
                perpY = -perpY;
            }

            double centerX = midX + perpX * dist;
            double centerY = midY + perpY * dist;

            // Углы начала и конца
            double startAngle = std::atan2(start.Y - centerY, start.X - centerX);
            double endAngle = std::atan2(end.Y - centerY, end.X - centerX);

            // Аппроксимируем дугу линейными сегментами
            int segments = static_cast<int>(std::abs(theta) / (PI / 18.0));  // ~10° на сегмент
            segments = (std::max)(segments, 8);

            double angleStep = theta / segments;

            WorldPoint prevPt = start;
            for (int i = 1; i <= segments; ++i)
            {
                double angle = startAngle + angleStep * i;
                WorldPoint pt{
                    centerX + radius * std::cos(angle),
                    centerY + radius * std::sin(angle)
                };

                auto screenPrev = camera.WorldToScreen(prevPt);
                auto screenCur = camera.WorldToScreen(pt);

                session.DrawLine(
                    static_cast<float>(screenPrev.X), static_cast<float>(screenPrev.Y),
                    static_cast<float>(screenCur.X), static_cast<float>(screenCur.Y),
                    color, lineWidth);

                prevPt = pt;
            }
        }

        // Отрисовка окружности
        static void DrawCircle(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfCircle& circle,
            Windows::UI::Color color,
            float lineWidth)
        {
            auto center = camera.WorldToScreen(circle.Center);
            float screenRadius = static_cast<float>(circle.Radius * camera.GetZoom());

            session.DrawCircle(
                static_cast<float>(center.X),
                static_cast<float>(center.Y),
                screenRadius,
                color, lineWidth);
        }

        // Отрисовка дуги
        static void DrawArc(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfArc& arc,
            Windows::UI::Color color,
            float lineWidth)
        {
            constexpr double PI = 3.14159265358979323846;

            // Конвертируем углы в радианы
            double startRad = arc.StartAngle * PI / 180.0;
            double endRad = arc.EndAngle * PI / 180.0;

            // Нормализуем углы
            while (endRad < startRad)
                endRad += 2.0 * PI;

            double sweepAngle = endRad - startRad;

            // Аппроксимируем дугу линейными сегментами
            int segments = static_cast<int>(sweepAngle / (PI / 18.0));
            segments = (std::max)(segments, 8);

            double angleStep = sweepAngle / segments;

            WorldPoint prevPt{
                arc.Center.X + arc.Radius * std::cos(startRad),
                arc.Center.Y + arc.Radius * std::sin(startRad)
            };

            for (int i = 1; i <= segments; ++i)
            {
                double angle = startRad + angleStep * i;
                WorldPoint pt{
                    arc.Center.X + arc.Radius * std::cos(angle),
                    arc.Center.Y + arc.Radius * std::sin(angle)
                };

                auto screenPrev = camera.WorldToScreen(prevPt);
                auto screenCur = camera.WorldToScreen(pt);

                session.DrawLine(
                    static_cast<float>(screenPrev.X), static_cast<float>(screenPrev.Y),
                    static_cast<float>(screenCur.X), static_cast<float>(screenCur.Y),
                    color, lineWidth);

                prevPt = pt;
            }
        }

        // Отрисовка текста
        static void DrawText(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& session,
            const Camera& camera,
            const DxfText& text,
            Windows::UI::Color color)
        {
            if (text.Content.empty())
                return;

            auto pos = camera.WorldToScreen(text.Position);
            float fontSize = static_cast<float>(text.Height * camera.GetZoom());

            // Минимальный и максимальный размер шрифта
            fontSize = (std::max)(fontSize, 6.0f);
            fontSize = (std::min)(fontSize, 72.0f);

            // Создаём формат текста
            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(fontSize);
            textFormat.FontFamily(L"Consolas");

            // Рисуем текст
            // DXF Y-координата вверх, а на экране вниз — текст может быть отзеркален
            // Пока рисуем как есть
            session.DrawText(
                winrt::hstring(text.Content),
                static_cast<float>(pos.X),
                static_cast<float>(pos.Y),
                color,
                textFormat);
        }
    };
}
