#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "Layer.h"

namespace winrt::estimate1
{
    // ����� ��� ��������� ���� �� ������ Win2D
    class WallRenderer
    {
    public:
        WallRenderer() = default;

        // ��������� ���� ���� � ������ ����������
        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            const LayerManager& layerManager,
            uint64_t hoverWallId = 0)
        {
            const auto& walls = document.GetWalls();

            for (const auto& wall : walls)
            {
                // ��������� ��������� ����
                if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                    continue;

                bool isHovered = (hoverWallId != 0 && wall->GetId() == hoverWallId);

                DrawWall(session, camera, *wall, layerManager, false, isHovered);
            }
        }

        // ��������� ������ ����� (��� ���������)

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
                    (void)isFlipped;
                    // ������ ��������� ����� ��� ������
                    Wall previewWall(startPoint, endPoint, thickness);
                    previewWall.SetWorkState(workState);
                    previewWall.SetLocationLineMode(locationLine);

                    // ������ � �����������������
                    DrawWall(session, camera, previewWall, layerManager, true);
                }

                // ��������� ����� ��������
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

        // ��������� ����� ����� (�������, ��� ����������)
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
            
            // �����: ������������� ��������� � ������������� �������� ����� �������� ����������
            // ������ ����� ������ � ������� ����������� (�� �����), 
            // �� ������� ����� ����������� � ������������ � ���������� ������� ������
            
            // 1. ����������� ������� � �������� ����� � �������� ����������
            WorldPoint startWorld = wall.GetStartPoint();
            WorldPoint endWorld = wall.GetEndPoint();
            
            ScreenPoint startScreen = camera.WorldToScreen(startWorld);
            ScreenPoint endScreen = camera.WorldToScreen(endWorld);
            
            // 2. ��������� ����������� � ��������������� ������ � �������� �����������
            float dx = endScreen.X - startScreen.X;
            float dy = endScreen.Y - startScreen.Y;
            float length = std::sqrt(dx * dx + dy * dy);
            
            if (length < 0.1f)
                return; // ������� �������� �����
            
            // ��������������� ������ � �������� �����������
            float perpX = -dy / length;
            float perpY = dx / length;
            
            // 3. ��������� ������� ����� � �������� � ������������ � ���������� �������
            // �������� ������� �� ������� ����������� (��) � �������� ��� �� ������� ������
            double realThickness = wall.GetThickness(); // � ����������� (��)
            float screenThickness = static_cast<float>(realThickness * camera.GetZoom());
            float halfThickness = screenThickness / 2.0f;
            
            // 4. ��������� 4 ������� ���� � �������� �����������
            ScreenPoint s1(startScreen.X + perpX * halfThickness, startScreen.Y + perpY * halfThickness);
            ScreenPoint s2(startScreen.X - perpX * halfThickness, startScreen.Y - perpY * halfThickness);
            ScreenPoint s3(endScreen.X - perpX * halfThickness, endScreen.Y - perpY * halfThickness);
            ScreenPoint s4(endScreen.X + perpX * halfThickness, endScreen.Y + perpY * halfThickness);

            // ����� �����
            Windows::UI::Color strokeColor = isPreview
                ? Windows::UI::ColorHelper::FromArgb(179, 255, 255, 255)
                : Windows::UI::Colors::White();

            Windows::UI::Color fillColor = Windows::UI::ColorHelper::FromArgb(100, 60, 60, 60);

            if (wall.IsSelected())
            {
                strokeColor = Windows::UI::ColorHelper::FromArgb(255, 90, 180, 255);
                fillColor = Windows::UI::ColorHelper::FromArgb(120, 90, 180, 255);
            }
            else if (isHovered)
            {
                strokeColor = Windows::UI::ColorHelper::FromArgb(255, 120, 210, 255);
                fillColor = Windows::UI::ColorHelper::FromArgb(100, 120, 210, 255);
            }

            // ������ ������������� �������
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(s1.X, s1.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s2.X, s2.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s3.X, s3.Y));
            pathBuilder.AddLine(Windows::Foundation::Numerics::float2(s4.X, s4.Y));
            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            // ������� �������
            float strokeWidth = wall.IsSelected() ? m_selectedStrokeWidthPx : m_wallStrokeWidthPx;

            auto strokeStyle = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
            strokeStyle.LineJoin(Microsoft::Graphics::Canvas::Geometry::CanvasLineJoin::Miter);
            strokeStyle.StartCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Square);
            strokeStyle.EndCap(Microsoft::Graphics::Canvas::Geometry::CanvasCapStyle::Square);
            
            WorkStateNative state = wall.GetWorkState();
            if (state == WorkStateNative::Demolish)
            {
                strokeStyle.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);
                Windows::UI::Color demolishColor = strokeColor;
                demolishColor.A = 180;
                
                session.FillGeometry(geometry, fillColor);
                session.DrawGeometry(geometry, demolishColor, strokeWidth, strokeStyle);
            }
            else
            {
                session.FillGeometry(geometry, fillColor);
                session.DrawGeometry(geometry, strokeColor, strokeWidth, strokeStyle);
            }

        }
    private:
        // ���������� ������ � ���������� (�������) - ������ ��� �������
        const float m_wallStrokeWidthPx{ 5.0f };      // ������� ������� ����� (������������ � ��������)
        const float m_selectedStrokeWidthPx{ 6.0f };  // ������� ����� ��� ��������� ����
    };
}
