#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "Layer.h"
#include "WallJoinSystem.h"

namespace winrt::estimate1
{
    // Класс для отрисовки стен на холсте Win2D
    class WallRenderer
    {
    public:
        WallRenderer() = default;

        // Установить систему соединений для рендеринга митра-углов
        void SetJoinSystem(WallJoinSystem* joinSystem) { m_joinSystem = joinSystem; }

        // Отрисовка всех стен с учётом соединений
        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            const LayerManager& layerManager,
            uint64_t hoverWallId = 0)
        {
            const auto& walls = document.GetWalls();

            // Если есть система соединений, сначала найдём все соединения
            std::vector<JoinInfo> allJoins;
            if (m_joinSystem)
            {
                for (const auto& wall : walls)
                {
                    if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                        continue;
                    auto joins = m_joinSystem->FindJoins(*wall, walls);
                    for (auto& j : joins)
                        allJoins.push_back(j);
                }
            }

            for (const auto& wall : walls)
            {
                // Проверяем видимость слоя
                if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                    continue;

                bool isHovered = (hoverWallId != 0 && wall->GetId() == hoverWallId);
                
                // Находим соединения для этой стены
                std::vector<JoinInfo> wallJoins;
                for (const auto& j : allJoins)
                {
                    if (j.WallId1 == wall->GetId() || j.WallId2 == wall->GetId())
                        wallJoins.push_back(j);
                }
                
                DrawWallWithJoins(session, camera, *wall, layerManager, wallJoins, false, isHovered);
            }
        }

        // Отрисовка превью стены (при рисовании)

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
                    // Создаём временную стену для превью
                    Wall previewWall(startPoint, endPoint, thickness);
                    previewWall.SetWorkState(workState);
                    previewWall.SetLocationLineMode(locationLine);

                    // Рисуем с полупрозрачностью
                    DrawWall(session, camera, previewWall, layerManager, true);

                    // Рисуем длину стены
                    double length = previewWall.GetLength();
                    if (length > 10.0) // Показываем только если длина > 10мм
                    {
                        DrawDimensionLabel(session, camera, previewWall, length);
                    }

                    // Рисуем индикатор flip (стрелка направления)
                    if (isFlipped && length > 50.0)
                    {
                        DrawFlipIndicator(session, camera, startPoint, endPoint);
                    }

                    // Рисуем индикатор Location Line
                    if (locationLine != LocationLineMode::WallCenterline && length > 50.0)
                    {
                        DrawLocationLineIndicator(session, camera, previewWall, locationLine);
                    }
                }

                // Отрисовка точек привязки
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

    private:
        WallJoinSystem* m_joinSystem{ nullptr };

        // Отрисовка стены с учётом соединений (митра-углы)
        void DrawWallWithJoins(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall,
            const LayerManager& layerManager,
            const std::vector<JoinInfo>& joins,
            bool isPreview = false,
            bool isHovered = false)
        {
            Windows::UI::Color baseColor = Windows::UI::Colors::White();

            // Если есть соединения с митра-углами, строим сложный контур
            if (m_joinSystem && !joins.empty())
            {
                auto contour = m_joinSystem->CalculateWallContour(wall, joins);
                if (contour.Points.size() >= 3)
                {
                    DrawContour(session, camera, contour, baseColor, wall.GetWorkState(), 
                               isPreview, wall.IsSelected());
                    
                    if (wall.IsSelected())
                        DrawSelectionHandles(session, camera, wall);
                    
                    if (wall.IsSelected() || isPreview)
                        DrawAxisLine(session, camera, wall);
                    
                    return;
                }
            }

            // Стандартная отрисовка (без митра)
            DrawWall(session, camera, wall, layerManager, isPreview, isHovered);
        }

        // Отрисовка контура стены
        void DrawContour(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const WallContour& contour,
            Windows::UI::Color baseColor,
            WorkStateNative workState,
            bool isPreview,
            bool isSelected)
        {
            (void)baseColor;
            if (contour.Points.size() < 3)
                return;

            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            
            ScreenPoint first = camera.WorldToScreen(contour.Points[0]);
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(first.X, first.Y));
            
            for (size_t i = 1; i < contour.Points.size(); ++i)
            {
                ScreenPoint screen = camera.WorldToScreen(contour.Points[i]);
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(screen.X, screen.Y));
            }
            
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);
            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            // Заливка (непрозрачный белый)
            Windows::UI::Color fillColor = Windows::UI::Colors::White();
            fillColor.A = 255;
            session.FillGeometry(geometry, fillColor);

            // Контур: чёрный, в превью — серый
            float strokeWidth = isSelected ? 2.5f : 1.5f;
            Windows::UI::Color strokeColor = isPreview
                ? Windows::UI::ColorHelper::FromArgb(255, 140, 140, 140)
                : Windows::UI::Colors::Black();

            if (workState == WorkStateNative::Demolish)
            {
                auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
                strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);
                session.DrawGeometry(geometry, strokeColor, strokeWidth, strokeStyle);
            }
            else
            {
                session.DrawGeometry(geometry, strokeColor, strokeWidth);
            }
        }

        // Отрисовка осевой линии
        void DrawAxisLine(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall)
        {
            ScreenPoint startScreen = camera.WorldToScreen(wall.GetStartPoint());
            ScreenPoint endScreen = camera.WorldToScreen(wall.GetEndPoint());

            auto axisStrokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            axisStrokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::DashDot);

            session.DrawLine(
                Windows::Foundation::Numerics::float2(startScreen.X, startScreen.Y),
                Windows::Foundation::Numerics::float2(endScreen.X, endScreen.Y),
                Windows::UI::ColorHelper::FromArgb(150, 100, 100, 100),
                1.0f,
                axisStrokeStyle);
        }

        // Отрисовка одной стены (базовая, без соединений)
        void DrawWall(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall,
            const LayerManager& layerManager,
            bool isPreview = false,
            bool isHovered = false)
        {
            (void)layerManager;
            (void)isHovered;
            // Получаем угловые точки стены
            WorldPoint p1, p2, p3, p4;
            wall.GetCornerPoints(p1, p2, p3, p4);

            // Преобразуем в экранные координаты
            ScreenPoint s1 = camera.WorldToScreen(p1);
            ScreenPoint s2 = camera.WorldToScreen(p2);
            ScreenPoint s3 = camera.WorldToScreen(p3);
            ScreenPoint s4 = camera.WorldToScreen(p4);

            // Цвета стены: непрозрачный белый фон и чёрный контур (серый в превью)
            Windows::UI::Color fillColor = Windows::UI::Colors::White();
            fillColor.A = 255;

            Windows::UI::Color strokeColor = isPreview
                ? Windows::UI::ColorHelper::FromArgb(255, 140, 140, 140)
                : Windows::UI::Colors::Black();

            // Создаём путь для заливки
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(s1.X, s1.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s2.X, s2.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s3.X, s3.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s4.X, s4.Y));
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            // Заливка стены (непрозрачный белый)
            session.FillGeometry(geometry, fillColor);

            // Контур стены
            float strokeWidth = wall.IsSelected() ? 2.5f : 1.5f;
            
            WorkStateNative state = wall.GetWorkState();
            if (state == WorkStateNative::Demolish)
            {
                // Для демонтажа — пунктирная линия
                auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
                strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);
                session.DrawGeometry(geometry, strokeColor, strokeWidth, strokeStyle);
            }
            else
            {
                session.DrawGeometry(geometry, strokeColor, strokeWidth);
            }

            // Если стена выбрана, рисуем маркеры
            if (wall.IsSelected())
            {
                DrawSelectionHandles(session, camera, wall);
            }

            // Рисуем осевую линию (центр стены)
            if (wall.IsSelected() || isPreview)
            {
                ScreenPoint startScreen = camera.WorldToScreen(wall.GetStartPoint());
                ScreenPoint endScreen = camera.WorldToScreen(wall.GetEndPoint());

                auto axisStrokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
                axisStrokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::DashDot);

                session.DrawLine(
                    Windows::Foundation::Numerics::float2(startScreen.X, startScreen.Y),
                    Windows::Foundation::Numerics::float2(endScreen.X, endScreen.Y),
                    Windows::UI::ColorHelper::FromArgb(150, 100, 100, 100),
                    1.0f,
                    axisStrokeStyle);
            }
        }

        // Отрисовка маркеров выделения
        void DrawSelectionHandles(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall)
        {
            float handleSize = 8.0f;

            // Маркер в начальной точке
            ScreenPoint startScreen = camera.WorldToScreen(wall.GetStartPoint());
            DrawHandle(session, startScreen, handleSize, true);

            // Маркер в конечной точке
            ScreenPoint endScreen = camera.WorldToScreen(wall.GetEndPoint());
            DrawHandle(session, endScreen, handleSize, true);

            // Маркер в середине
            WorldPoint midPoint(
                (wall.GetStartPoint().X + wall.GetEndPoint().X) / 2,
                (wall.GetStartPoint().Y + wall.GetEndPoint().Y) / 2
            );
            ScreenPoint midScreen = camera.WorldToScreen(midPoint);
            DrawHandle(session, midScreen, handleSize * 0.7f, false);
        }

        // Отрисовка одного маркера
        void DrawHandle(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const ScreenPoint& position,
            float size,
            bool isEndpoint)
        {
            Windows::UI::Color fillColor = Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255);
            Windows::UI::Color strokeColor = Windows::UI::ColorHelper::FromArgb(255, 0, 120, 215);

            if (isEndpoint)
            {
                // Квадратный маркер для конечных точек
                session.FillRectangle(
                    Windows::Foundation::Rect(
                        position.X - size / 2,
                        position.Y - size / 2,
                        size, size),
                    fillColor);
                session.DrawRectangle(
                    Windows::Foundation::Rect(
                        position.X - size / 2,
                        position.Y - size / 2,
                        size, size),
                    strokeColor,
                    1.5f);
            }
            else
            {
                // Круглый маркер для средней точки
                session.FillCircle(
                    Windows::Foundation::Numerics::float2(position.X, position.Y),
                    size / 2,
                    fillColor);
                session.DrawCircle(
                    Windows::Foundation::Numerics::float2(position.X, position.Y),
                    size / 2,
                    strokeColor,
                    1.5f);
            }
        }

        // Отрисовка метки с размером
        void DrawDimensionLabel(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Wall& wall,
            double length)
        {
            // Позиция метки — середина стены, смещённая перпендикулярно
            WorldPoint midWorld(
                (wall.GetStartPoint().X + wall.GetEndPoint().X) / 2,
                (wall.GetStartPoint().Y + wall.GetEndPoint().Y) / 2
            );
            
            WorldPoint perp = wall.GetPerpendicular();
            double offset = wall.GetThickness() / 2 + 50; // Смещение от стены
            
            WorldPoint labelWorld(
                            midWorld.X + perp.X * offset,
                            midWorld.Y + perp.Y * offset
                        );

                        ScreenPoint labelScreen = camera.WorldToScreen(labelWorld);

                        // Форматируем текст
                        wchar_t text[32];
                        if (length >= 1000)
                        {
                            swprintf_s(text, L"%.2f м", length / 1000.0);
                        }
                        else
                        {
                            swprintf_s(text, L"%.0f мм", length);
                        }

                        // Фон для текста
                        auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
                        textFormat.FontSize(12);
            
                        auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                            session.Device(),
                            text,
                            textFormat,
                            200, 50);

                        float textWidth = textLayout.LayoutBounds().Width;
                        float textHeight = textLayout.LayoutBounds().Height;

                        session.FillRoundedRectangle(
                            Windows::Foundation::Rect(
                                labelScreen.X - textWidth / 2 - 4,
                                labelScreen.Y - textHeight / 2 - 2,
                                textWidth + 8,
                                textHeight + 4),
                            3.0f, 3.0f,
                            Windows::UI::ColorHelper::FromArgb(200, 255, 255, 200));

                        session.DrawText(
                            text,
                            labelScreen.X - textWidth / 2,
                            labelScreen.Y - textHeight / 2,
                            Windows::UI::ColorHelper::FromArgb(255, 80, 80, 80));
                    }

                    // Индикатор flip (стрелка)
                    void DrawFlipIndicator(
                        const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
                        const Camera& camera,
                        const WorldPoint& startPoint,
                        const WorldPoint& endPoint)
                    {
                        // Рисуем стрелку в направлении от start к end
                        WorldPoint mid(
                            (startPoint.X + endPoint.X) / 2,
                            (startPoint.Y + endPoint.Y) / 2);
            
                        double dx = endPoint.X - startPoint.X;
                        double dy = endPoint.Y - startPoint.Y;
                        double len = std::sqrt(dx * dx + dy * dy);
                        if (len < 0.001) return;

                        // Нормализованное направление
                        double dirX = dx / len;
                        double dirY = dy / len;

                        // Стрелка на середине
                        ScreenPoint midScreen = camera.WorldToScreen(mid);
                        float arrowLen = 20.0f;
                        float arrowWidth = 8.0f;

                        // Направление в экранных координатах
                        ScreenPoint endScreen = camera.WorldToScreen(endPoint);
                        float sdx = endScreen.X - midScreen.X;
                        float sdy = endScreen.Y - midScreen.Y;
                        float slen = std::sqrt(sdx * sdx + sdy * sdy);
                        if (slen < 0.001f) return;
                        sdx /= slen;
                        sdy /= slen;

                        // Конец стрелки
                        float tipX = midScreen.X + sdx * arrowLen;
                        float tipY = midScreen.Y + sdy * arrowLen;

                        // Основание стрелки
                        float baseX = midScreen.X - sdx * arrowLen * 0.3f;
                        float baseY = midScreen.Y - sdy * arrowLen * 0.3f;

                        // Перпендикуляр
                        float perpX = -sdy;
                        float perpY = sdx;

                        Windows::UI::Color arrowColor = Windows::UI::ColorHelper::FromArgb(200, 255, 150, 0);

                        // Рисуем стрелку
                        session.DrawLine(
                            Windows::Foundation::Numerics::float2(baseX, baseY),
                            Windows::Foundation::Numerics::float2(tipX, tipY),
                            arrowColor, 2.0f);

                        session.DrawLine(
                            Windows::Foundation::Numerics::float2(tipX, tipY),
                            Windows::Foundation::Numerics::float2(tipX - sdx * arrowWidth + perpX * arrowWidth * 0.5f,
                                                                  tipY - sdy * arrowWidth + perpY * arrowWidth * 0.5f),
                            arrowColor, 2.0f);

                        session.DrawLine(
                            Windows::Foundation::Numerics::float2(tipX, tipY),
                            Windows::Foundation::Numerics::float2(tipX - sdx * arrowWidth - perpX * arrowWidth * 0.5f,
                                                                  tipY - sdy * arrowWidth - perpY * arrowWidth * 0.5f),
                            arrowColor, 2.0f);

                        // Текст "FLIP"
                        session.DrawText(
                            L"?",
                            midScreen.X - 8.0f,
                            midScreen.Y - 25.0f,
                            arrowColor);
                    }

                    // Индикатор Location Line
                    void DrawLocationLineIndicator(
                        const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
                        const Camera& camera,
                        const Wall& wall,
                        LocationLineMode mode)
                    {
                        // Показываем линию размещения цветом
                        WorldPoint start = wall.GetStartPoint();
                        WorldPoint end = wall.GetEndPoint();
                        WorldPoint perp = wall.GetPerpendicular();
                        double thickness = wall.GetThickness();

                        double offset = 0.0;
                        Windows::UI::Color lineColor = Windows::UI::ColorHelper::FromArgb(200, 0, 200, 100);
            
                        switch (mode)
                        {
                        case LocationLineMode::FinishFaceExterior:
                        case LocationLineMode::CoreFaceExterior:
                            offset = thickness / 2.0;
                            break;
                        case LocationLineMode::FinishFaceInterior:
                        case LocationLineMode::CoreFaceInterior:
                            offset = -thickness / 2.0;
                            break;
                        default:
                            return; // Centerline не требует индикатора
                        }

                        WorldPoint lineStart(start.X + perp.X * offset, start.Y + perp.Y * offset);
                        WorldPoint lineEnd(end.X + perp.X * offset, end.Y + perp.Y * offset);

                        ScreenPoint sStart = camera.WorldToScreen(lineStart);
                        ScreenPoint sEnd = camera.WorldToScreen(lineEnd);

                        auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
                        strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dot);

                        session.DrawLine(
                            Windows::Foundation::Numerics::float2(sStart.X, sStart.Y),
                            Windows::Foundation::Numerics::float2(sEnd.X, sEnd.Y),
                            lineColor, 2.0f, strokeStyle);
                    }
                };
            }
