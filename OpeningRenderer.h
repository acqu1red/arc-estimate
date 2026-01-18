#pragma once

#include "pch.h"
#include "Opening.h"
#include "Camera.h"

namespace winrt::estimate1
{
    // =====================================================
    // РЕНДЕРЕР ДВЕРЕЙ И ОКОН
    // =====================================================

    class OpeningRenderer
    {
    public:
        OpeningRenderer() = default;

        // Отрисовка всех дверей
        void DrawDoors(
            Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& canvas,
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const std::vector<std::shared_ptr<Door>>& doors,
            const std::vector<std::shared_ptr<Wall>>& walls,
            uint64_t selectedId = 0,
            uint64_t hoverId = 0)
        {
            for (const auto& door : doors)
            {
                // Найти хост-стену
                Wall* hostWall = nullptr;
                for (const auto& wall : walls)
                {
                    if (wall->GetId() == door->GetHostWallId())
                    {
                        hostWall = wall.get();
                        break;
                    }
                }

                if (hostWall)
                {
                    bool isSelected = (door->GetId() == selectedId);
                    bool isHovered = (door->GetId() == hoverId);
                    DrawDoor(ds, camera, *door, *hostWall, isSelected, isHovered);
                }
            }
        }

        // Отрисовка всех окон
        void DrawWindows(
            Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& canvas,
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const std::vector<std::shared_ptr<Window>>& windows,
            const std::vector<std::shared_ptr<Wall>>& walls,
            uint64_t selectedId = 0,
            uint64_t hoverId = 0)
        {
            for (const auto& window : windows)
            {
                // Найти хост-стену
                Wall* hostWall = nullptr;
                for (const auto& wall : walls)
                {
                    if (wall->GetId() == window->GetHostWallId())
                    {
                        hostWall = wall.get();
                        break;
                    }
                }

                if (hostWall)
                {
                    bool isSelected = (window->GetId() == selectedId);
                    bool isHovered = (window->GetId() == hoverId);
                    DrawWindow(ds, camera, *window, *hostWall, isSelected, isHovered);
                }
            }
        }

        // Отрисовка одной двери
        void DrawDoor(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const Door& door,
            const Wall& hostWall,
            bool isSelected = false,
            bool isHovered = false)
        {
            using namespace Windows::UI;
            using namespace Microsoft::Graphics::Canvas::Geometry;

            // Вычислить позицию и направления
            WorldPoint center = door.GetCenterPoint(hostWall);
            WorldPoint dir = door.GetWallDirection(hostWall);
            WorldPoint normal = door.GetWallNormal(hostWall);

            double width = door.GetWidth();
            double halfW = width / 2;
            double thickness = hostWall.GetThickness();

            // Конвертировать в экранные координаты
            ScreenPoint screenCenter = camera.WorldToScreen(center);
            float screenWidth = static_cast<float>(width * camera.GetZoom());
            float screenThickness = static_cast<float>(thickness * camera.GetZoom());
            float screenHalfW = screenWidth / 2;

            // Угол стены
            double angle = std::atan2(dir.Y, dir.X);

            // Цвета
            Color fillColor = isHovered ? Colors::LightCyan() : Colors::White();
            Color strokeColor = isSelected ? Colors::Blue() : Colors::Black();
            float strokeWidth = isSelected ? 2.0f : 1.0f;

            // Сохранить состояние и применить трансформацию
            auto transform = ds.Transform();
            
            // Создать матрицу трансформации
            auto translateToCenter = Windows::Foundation::Numerics::make_float3x2_translation(
                screenCenter.X, screenCenter.Y);
            auto rotate = Windows::Foundation::Numerics::make_float3x2_rotation(
                static_cast<float>(angle));
            auto translateBack = Windows::Foundation::Numerics::make_float3x2_translation(
                -screenCenter.X, -screenCenter.Y);

            ds.Transform(transform * translateToCenter * rotate * translateBack);

            // Прямоугольник проёма (заливка белым — "вырез" в стене)
            Windows::Foundation::Rect openingRect(
                screenCenter.X - screenHalfW,
                screenCenter.Y - screenThickness / 2,
                screenWidth,
                screenThickness
            );
            ds.FillRectangle(openingRect, fillColor);
            ds.DrawRectangle(openingRect, strokeColor, strokeWidth);

            // Полотно двери
            float leafWidth = screenWidth - 4;  // Немного меньше проёма
            float leafThick = 4.0f;

            // Определить сторону петель и направление открывания
            bool isLeft = door.IsLeftHanded();
            bool isOutward = door.IsOutward();
            float hingeSide = isLeft ? -screenHalfW + 2 : screenHalfW - 2;
            float openSide = isLeft ? screenHalfW - 2 : -screenHalfW + 2;
            float swingDir = (isOutward != door.IsFlipped()) ? 1.0f : -1.0f;

            // Позиция петель
            float hingeX = screenCenter.X + hingeSide;
            float hingeY = screenCenter.Y + swingDir * screenThickness / 2;

            // Угол открывания
            double swingAngle = door.GetSwingAngle() * 3.14159265 / 180.0;
            if (!isLeft) swingAngle = -swingAngle;

            // Отрисовка полотна (в закрытом положении)
            float leafX1 = screenCenter.X - screenHalfW + 2;
            float leafX2 = screenCenter.X + screenHalfW - 2;
            float leafY = screenCenter.Y + swingDir * screenThickness / 2;
            
            ds.DrawLine(leafX1, leafY, leafX2, leafY, strokeColor, strokeWidth + 1);

            // Дуга открывания
            float arcRadius = leafWidth;
            float startAngle = swingDir > 0 ? 0 : (float)M_PI;
            float sweepAngle = (float)(swingAngle);

            if (arcRadius > 5)
            {
                // Рисуем дугу сегментами
                int segments = 16;
                float arcCenterX = hingeX;
                float arcCenterY = hingeY;

                std::vector<Windows::Foundation::Numerics::float2> arcPoints;
                for (int i = 0; i <= segments; ++i)
                {
                    float t = (float)i / segments;
                    float a = startAngle + sweepAngle * t;
                    float x = arcCenterX + arcRadius * std::cos(a);
                    float y = arcCenterY - arcRadius * std::sin(a) * swingDir;
                    arcPoints.push_back({ x, y });
                }

                for (size_t i = 1; i < arcPoints.size(); ++i)
                {
                    ds.DrawLine(
                        arcPoints[i - 1].x, arcPoints[i - 1].y,
                        arcPoints[i].x, arcPoints[i].y,
                        Colors::Gray(), 0.5f
                    );
                }

                // Линия открытой двери
                if (arcPoints.size() > 1)
                {
                    ds.DrawLine(
                        hingeX, hingeY,
                        arcPoints.back().x, arcPoints.back().y,
                        Colors::Gray(), 1.0f
                    );
                }
            }

            // Восстановить трансформацию
            ds.Transform(transform);

            // Ручки выделения
            if (isSelected)
            {
                DrawSelectionHandle(ds, screenCenter.X - screenHalfW, screenCenter.Y);
                DrawSelectionHandle(ds, screenCenter.X + screenHalfW, screenCenter.Y);
                DrawSelectionHandle(ds, screenCenter.X, screenCenter.Y);
            }
        }

        // Отрисовка одного окна
        void DrawWindow(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const Window& window,
            const Wall& hostWall,
            bool isSelected = false,
            bool isHovered = false)
        {
            using namespace Windows::UI;

            // Вычислить позицию и направления
            WorldPoint center = window.GetCenterPoint(hostWall);
            WorldPoint dir = window.GetWallDirection(hostWall);
            WorldPoint normal = window.GetWallNormal(hostWall);

            double width = window.GetWidth();
            double halfW = width / 2;
            double thickness = hostWall.GetThickness();
            double frameWidth = window.GetFrameWidth();

            // Конвертировать в экранные координаты
            ScreenPoint screenCenter = camera.WorldToScreen(center);
            float screenWidth = static_cast<float>(width * camera.GetZoom());
            float screenThickness = static_cast<float>(thickness * camera.GetZoom());
            float screenFrame = static_cast<float>(frameWidth * camera.GetZoom());
            float screenHalfW = screenWidth / 2;

            // Угол стены
            double angle = std::atan2(dir.Y, dir.X);

            // Цвета
            Color fillColor = isHovered ? Color{ 200, 200, 220, 255 } : Color{ 230, 240, 255, 255 };
            Color frameColor = Colors::DarkGray();
            Color glassColor = Color{ 100, 180, 220, 255 };
            Color strokeColor = isSelected ? Colors::Blue() : Colors::Black();
            float strokeWidth = isSelected ? 2.0f : 1.0f;

            // Сохранить состояние
            auto transform = ds.Transform();
            
            // Применить вращение
            auto translateToCenter = Windows::Foundation::Numerics::make_float3x2_translation(
                screenCenter.X, screenCenter.Y);
            auto rotate = Windows::Foundation::Numerics::make_float3x2_rotation(
                static_cast<float>(angle));
            auto translateBack = Windows::Foundation::Numerics::make_float3x2_translation(
                -screenCenter.X, -screenCenter.Y);

            ds.Transform(transform * translateToCenter * rotate * translateBack);

            // Внешний прямоугольник (проём)
            Windows::Foundation::Rect openingRect(
                screenCenter.X - screenHalfW,
                screenCenter.Y - screenThickness / 2,
                screenWidth,
                screenThickness
            );
            ds.FillRectangle(openingRect, fillColor);
            ds.DrawRectangle(openingRect, strokeColor, strokeWidth);

            // Рама окна
            float innerMargin = (std::min)(screenFrame, screenWidth * 0.1f);
            Windows::Foundation::Rect glassRect(
                screenCenter.X - screenHalfW + innerMargin,
                screenCenter.Y - screenThickness / 2 + innerMargin * 0.5f,
                screenWidth - 2 * innerMargin,
                screenThickness - innerMargin
            );

            if (glassRect.Width > 0 && glassRect.Height > 0)
            {
                ds.FillRectangle(glassRect, glassColor);
                ds.DrawRectangle(glassRect, frameColor, 1.0f);
            }

            // Импост (вертикальная перегородка) для двустворчатых окон
            int panes = window.GetPaneCount();
            if (panes >= 2 && screenWidth > 20)
            {
                float paneWidth = (screenWidth - 2 * innerMargin) / panes;
                for (int i = 1; i < panes; ++i)
                {
                    float divX = screenCenter.X - screenHalfW + innerMargin + paneWidth * i;
                    ds.DrawLine(
                        divX, screenCenter.Y - screenThickness / 2 + innerMargin * 0.5f,
                        divX, screenCenter.Y + screenThickness / 2 - innerMargin * 0.5f,
                        frameColor, 2.0f
                    );
                }
            }

            // Двойная линия стекла (вид сверху)
            float glassLineY1 = screenCenter.Y - 1;
            float glassLineY2 = screenCenter.Y + 1;
            ds.DrawLine(
                screenCenter.X - screenHalfW + innerMargin,
                glassLineY1,
                screenCenter.X + screenHalfW - innerMargin,
                glassLineY1,
                Colors::LightBlue(), 1.0f
            );
            ds.DrawLine(
                screenCenter.X - screenHalfW + innerMargin,
                glassLineY2,
                screenCenter.X + screenHalfW - innerMargin,
                glassLineY2,
                Colors::LightBlue(), 1.0f
            );

            // Восстановить трансформацию
            ds.Transform(transform);

            // Ручки выделения
            if (isSelected)
            {
                DrawSelectionHandle(ds, screenCenter.X - screenHalfW, screenCenter.Y);
                DrawSelectionHandle(ds, screenCenter.X + screenHalfW, screenCenter.Y);
                DrawSelectionHandle(ds, screenCenter.X, screenCenter.Y);
            }
        }

        // Превью двери при размещении
        void DrawDoorPreview(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const WorldPoint& position,
            const WorldPoint& wallDir,
            double width,
            double wallThickness,
            bool isValid)
        {
            using namespace Windows::UI;

            ScreenPoint screenPos = camera.WorldToScreen(position);
            float screenWidth = static_cast<float>(width * camera.GetZoom());
            float screenThickness = static_cast<float>(wallThickness * camera.GetZoom());
            float screenHalfW = screenWidth / 2;

            double angle = std::atan2(wallDir.Y, wallDir.X);

            Color fillColor = isValid ? Color{ 200, 255, 200, 128 } : Color{ 255, 200, 200, 128 };
            Color strokeColor = isValid ? Colors::Green() : Colors::Red();

            auto transform = ds.Transform();
            auto translateToCenter = Windows::Foundation::Numerics::make_float3x2_translation(
                screenPos.X, screenPos.Y);
            auto rotate = Windows::Foundation::Numerics::make_float3x2_rotation(
                static_cast<float>(angle));
            auto translateBack = Windows::Foundation::Numerics::make_float3x2_translation(
                -screenPos.X, -screenPos.Y);

            ds.Transform(transform * translateToCenter * rotate * translateBack);

            Windows::Foundation::Rect previewRect(
                screenPos.X - screenHalfW,
                screenPos.Y - screenThickness / 2,
                screenWidth,
                screenThickness
            );
            ds.FillRectangle(previewRect, fillColor);
            ds.DrawRectangle(previewRect, strokeColor, 2.0f);

            // Дуга открывания (упрощённая)
            float arcRadius = screenWidth - 4;
            ds.DrawLine(
                screenPos.X + screenHalfW - 2, screenPos.Y + screenThickness / 2,
                screenPos.X + screenHalfW - 2 + arcRadius * 0.7f, screenPos.Y + screenThickness / 2 + arcRadius * 0.7f,
                strokeColor, 1.0f
            );

            ds.Transform(transform);
        }

        // Превью окна при размещении
        void DrawWindowPreview(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            Camera& camera,
            const WorldPoint& position,
            const WorldPoint& wallDir,
            double width,
            double wallThickness,
            bool isValid)
        {
            using namespace Windows::UI;

            ScreenPoint screenPos = camera.WorldToScreen(position);
            float screenWidth = static_cast<float>(width * camera.GetZoom());
            float screenThickness = static_cast<float>(wallThickness * camera.GetZoom());
            float screenHalfW = screenWidth / 2;

            double angle = std::atan2(wallDir.Y, wallDir.X);

            Color fillColor = isValid ? Color{ 200, 220, 255, 128 } : Color{ 255, 200, 200, 128 };
            Color strokeColor = isValid ? Colors::Blue() : Colors::Red();

            auto transform = ds.Transform();
            auto translateToCenter = Windows::Foundation::Numerics::make_float3x2_translation(
                screenPos.X, screenPos.Y);
            auto rotate = Windows::Foundation::Numerics::make_float3x2_rotation(
                static_cast<float>(angle));
            auto translateBack = Windows::Foundation::Numerics::make_float3x2_translation(
                -screenPos.X, -screenPos.Y);

            ds.Transform(transform * translateToCenter * rotate * translateBack);

            Windows::Foundation::Rect previewRect(
                screenPos.X - screenHalfW,
                screenPos.Y - screenThickness / 2,
                screenWidth,
                screenThickness
            );
            ds.FillRectangle(previewRect, fillColor);
            ds.DrawRectangle(previewRect, strokeColor, 2.0f);

            // Двойная линия стекла
            ds.DrawLine(
                screenPos.X - screenHalfW + 4, screenPos.Y - 1,
                screenPos.X + screenHalfW - 4, screenPos.Y - 1,
                strokeColor, 1.0f
            );
            ds.DrawLine(
                screenPos.X - screenHalfW + 4, screenPos.Y + 1,
                screenPos.X + screenHalfW - 4, screenPos.Y + 1,
                strokeColor, 1.0f
            );

            ds.Transform(transform);
        }

    private:
        void DrawSelectionHandle(
            Microsoft::Graphics::Canvas::CanvasDrawingSession const& ds,
            float x, float y)
        {
            const float size = 6.0f;
            ds.FillRectangle(
                x - size / 2, y - size / 2,
                size, size,
                Windows::UI::Colors::White()
            );
            ds.DrawRectangle(
                x - size / 2, y - size / 2,
                size, size,
                Windows::UI::Colors::Blue(), 1.0f
            );
        }
    };
}
