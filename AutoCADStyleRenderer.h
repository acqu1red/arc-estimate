#pragma once
#include "pch.h"
#include "Camera.h"
#include "Models.h"

namespace winrt::estimate1
{
    // =============================================================================
    // Рендерер размеров и аннотаций в стиле AutoCAD/nanoCAD
    // =============================================================================
    class AutoCADStyleRenderer
    {
    public:
        // Рисовать размер стены (оранжевый текст НА стене)
        static void DrawWallDimension(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& start,
            const WorldPoint& end,
            double length)
        {
            // Средняя точка
            WorldPoint mid((start.X + end.X) / 2, (start.Y + end.Y) / 2);
            
            // Направление стены
            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.01) return;
            
            dx /= len;
            dy /= len;
            
            // Перпендикуляр (вправо от направления)
            double perpX = -dy;
            double perpY = dx;
            
            // Смещение текста от оси (120мм)
            WorldPoint textPos(mid.X + perpX * 120.0, mid.Y + perpY * 120.0);
            ScreenPoint textScreen = camera.WorldToScreen(textPos);

            // Текст
            wchar_t text[32];
            swprintf_s(text, L"%.1f", length);

            // Размер шрифта - МАСШТАБИРУЕМЫЙ
            float fontSize = static_cast<float>(70.0 * camera.GetZoom());
            fontSize = (std::max)(10.0f, (std::min)(20.0f, fontSize));

            auto format = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            format.FontSize(fontSize);
            format.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

            // Цвет: оранжевый
            Windows::UI::Color color = Windows::UI::ColorHelper::FromArgb(255, 255, 160, 50);

            session.DrawText(text, Windows::Foundation::Numerics::float2(textScreen.X, textScreen.Y), color, format);
        }

        // Рисовать угол между стенами (дуга + текст)
        static void DrawAngleBetweenWalls(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& vertex,
            const WorldPoint& dir1End,
            const WorldPoint& dir2End,
            double angleDegrees)
        {
            ScreenPoint vertexScreen = camera.WorldToScreen(vertex);
            ScreenPoint dir1Screen = camera.WorldToScreen(dir1End);
            ScreenPoint dir2Screen = camera.WorldToScreen(dir2End);

            // Радиус дуги (в экранных координатах)
            float arcRadius = 60.0f;

            // Углы направлений
            float angle1 = std::atan2(dir1Screen.Y - vertexScreen.Y, dir1Screen.X - vertexScreen.X);
            float angle2 = std::atan2(dir2Screen.Y - vertexScreen.Y, dir2Screen.X - vertexScreen.X);

            // Нормализация углов
            if (angle2 < angle1) angle2 += 2.0f * 3.14159265f;

            // Цвет дуги: оранжевый
            Windows::UI::Color arcColor = Windows::UI::ColorHelper::FromArgb(180, 255, 160, 50);

            // Рисуем дугу
            Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder pathBuilder(session);
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(
                vertexScreen.X + arcRadius * std::cos(angle1),
                vertexScreen.Y + arcRadius * std::sin(angle1)));
            
            pathBuilder.AddArc(
                Windows::Foundation::Numerics::float2(vertexScreen.X, vertexScreen.Y),
                arcRadius, arcRadius,
                angle2 - angle1,
                Microsoft::Graphics::Canvas::Geometry::CanvasSweepDirection::Clockwise,
                Microsoft::Graphics::Canvas::Geometry::CanvasArcSize::Small);
            
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Open);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);
            session.DrawGeometry(geometry, arcColor, 1.5f);

            // Текст угла (в середине дуги)
            float midAngle = (angle1 + angle2) / 2.0f;
            float textRadius = arcRadius + 15.0f;
            ScreenPoint textPos(
                vertexScreen.X + textRadius * std::cos(midAngle),
                vertexScreen.Y + textRadius * std::sin(midAngle));

            wchar_t angleText[32];
            swprintf_s(angleText, L"%.2f°", angleDegrees);

            auto format = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            format.FontSize(11);
            format.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

            session.DrawText(angleText, Windows::Foundation::Numerics::float2(textPos.X, textPos.Y), arcColor, format);
        }

        // Рисовать пунктирную линию привязки (через всю стену)
        static void DrawAttachmentLine(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& start,
            const WorldPoint& end,
            bool isHorizontal,
            bool isVertical)
        {
            ScreenPoint s1 = camera.WorldToScreen(start);
            ScreenPoint s2 = camera.WorldToScreen(end);

            // Цвет: голубой пунктир
            Windows::UI::Color lineColor = Windows::UI::ColorHelper::FromArgb(120, 100, 150, 200);

            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);

            session.DrawLine(
                Windows::Foundation::Numerics::float2(s1.X, s1.Y),
                Windows::Foundation::Numerics::float2(s2.X, s2.Y),
                lineColor, 1.0f, strokeStyle);
        }

        // Рисовать подсказку режима ("Вертикаль", "Горизонталь")
        static void DrawModeHint(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& position,
            const std::wstring& hintText)
        {
            ScreenPoint pos = camera.WorldToScreen(position);
            pos.X += 15.0f;
            pos.Y += 15.0f;

            auto format = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            format.FontSize(12);

            // Фон подсказки (белый прямоугольник)
            auto layout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session, hintText.c_str(), format, 0.0f, 0.0f);
            
            auto bounds = layout.LayoutBounds();
            Windows::Foundation::Rect bgRect(
                pos.X - 2,
                pos.Y - 2,
                bounds.Width + 4,
                bounds.Height + 4);

            session.FillRectangle(bgRect, Windows::UI::ColorHelper::FromArgb(220, 255, 255, 255));
            session.DrawRectangle(bgRect, Windows::UI::ColorHelper::FromArgb(255, 100, 100, 100), 1.0f);

            session.DrawText(
                hintText.c_str(),
                Windows::Foundation::Numerics::float2(pos.X, pos.Y),
                Windows::UI::ColorHelper::FromArgb(255, 50, 50, 50),
                format);
        }

        // Рисовать маркеры привязки (квадратики на концах стен)
        static void DrawSnapMarker(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WorldPoint& point,
            bool isActive)
        {
            ScreenPoint screen = camera.WorldToScreen(point);
            float size = isActive ? 8.0f : 5.0f;

            Windows::UI::Color color = isActive
                ? Windows::UI::ColorHelper::FromArgb(255, 0, 200, 255)
                : Windows::UI::ColorHelper::FromArgb(180, 100, 150, 200);

            // Квадратик
            Windows::Foundation::Rect rect(
                screen.X - size / 2,
                screen.Y - size / 2,
                size, size);

            session.FillRectangle(rect, color);
            session.DrawRectangle(rect, Windows::UI::Colors::White(), 1.0f);
        }
    };
}
