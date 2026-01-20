#pragma once

// R5.2 – Рендер помещений (Room Renderer)
// Отображение помещений: границы, заливка, подписи и площади на плане

#include "pch.h"
#include "Room.h"
#include "Camera.h"
#include "Element.h"
#include <cmath>
#include <vector>

namespace winrt::estimate1
{
    // Тип заливки помещения
    enum class RoomFillStyle
    {
        Solid,          // сплошная заливка
        Hatched,        // штриховка (резерв)
        ColorByType,    // цвет по типу помещения
        None            // без заливки
    };

    // Настройки отображения помещений
    struct RoomDisplaySettings
    {
        bool ShowRooms{ true };
        bool ShowLabels{ true };
        bool ShowArea{ true };
        bool ShowPerimeter{ false };
        RoomFillStyle FillStyle{ RoomFillStyle::Solid };
        uint8_t FillOpacity{ 40 }; // 0-100, процент
        float BorderWidth{ 1.5f };
    };

    // Рендерер для отображения помещений на холсте Win2D
    class RoomRenderer
    {
    public:
        RoomRenderer() = default;

        void Settings(const RoomDisplaySettings& settings)
        {
            m_settings = settings;
        }

        const RoomDisplaySettings& Settings() const
        {
            return m_settings;
        }

        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const std::vector<std::shared_ptr<Room>>& rooms)
        {
            if (!m_settings.ShowRooms)
                return;

            for (const auto& room : rooms)
            {
                if (room)
                {
                    DrawRoom(session, camera, *room);
                }
            }
        }

    private:
        void DrawRoom(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Room& room)
        {
            const auto& contour = room.GetContour();
            if (contour.size() < 3)
                return;

            auto builder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());
            ScreenPoint start = camera.WorldToScreen(contour.front());
            builder.BeginFigure(start.X, start.Y);
            for (size_t i = 1; i < contour.size(); ++i)
            {
                ScreenPoint pt = camera.WorldToScreen(contour[i]);
                builder.AddLine(pt.X, pt.Y);
            }
            builder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);

            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(builder);

            // Заливка
            if (m_settings.FillStyle != RoomFillStyle::None)
            {
                Windows::UI::Color fillColor = GetRoomColor(room);
                uint8_t opacity = (m_settings.FillOpacity > 100 ? 100 : m_settings.FillOpacity);
                fillColor.A = static_cast<uint8_t>(opacity * 255 / 100);
                session.FillGeometry(geometry, fillColor);
            }

            // Контур
            session.DrawGeometry(
                geometry,
                Windows::UI::ColorHelper::FromArgb(255, 80, 80, 80),
                m_settings.BorderWidth);

            if (m_settings.ShowLabels)
            {
                DrawRoomLabel(session, camera, room);
            }
        }

        void DrawRoomLabel(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Room& room)
        {
            WorldPoint labelPoint = room.GetLabelPoint();
            ScreenPoint screenPos = camera.WorldToScreen(labelPoint);

            std::wstring labelText;

            if (!room.GetNumber().empty())
            {
                labelText = room.GetNumber();
            }

            if (!room.GetName().empty())
            {
                if (!labelText.empty())
                    labelText += L"\n";
                labelText += room.GetName();
            }

            if (m_settings.ShowArea)
            {
                double areaSqM = std::abs(room.GetArea()) / 1000000.0;
                wchar_t areaStr[64];
                swprintf_s(areaStr, L"%.2f м?", areaSqM);

                if (!labelText.empty())
                    labelText += L"\n";
                labelText += areaStr;
            }

            if (m_settings.ShowPerimeter)
            {
                double perimeterM = room.GetPerimeter() / 1000.0;
                wchar_t perimStr[64];
                swprintf_s(perimStr, L"P: %.2f м", perimeterM);

                if (!labelText.empty())
                    labelText += L"\n";
                labelText += perimStr;
            }

            if (labelText.empty())
                return;

            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontFamily(L"Segoe UI");
            textFormat.FontSize(12.0f);
            textFormat.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);
            textFormat.VerticalAlignment(Microsoft::Graphics::Canvas::Text::CanvasVerticalAlignment::Center);

            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session.Device(),
                labelText,
                textFormat,
                300.0f,
                200.0f);

            float textWidth = textLayout.LayoutBounds().Width;
            float textHeight = textLayout.LayoutBounds().Height;

            float textX = screenPos.X - textWidth / 2.0f;
            float textY = screenPos.Y - textHeight / 2.0f;

            Windows::UI::Color textColor = Windows::UI::ColorHelper::FromArgb(255, 40, 40, 40);
            session.DrawTextLayout(
                textLayout,
                Windows::Foundation::Numerics::float2(textX, textY),
                textColor);
        }

        Windows::UI::Color GetRoomColor(const Room& room)
        {
            const std::wstring& name = room.GetName();
            const std::wstring& finishType = room.GetFinishType();
            (void)finishType;

            if (ContainsAny(name, { L"ванн", L"сануз" }))
                return Windows::UI::ColorHelper::FromArgb(255, 255, 200, 150);

            if (ContainsAny(name, { L"кух", L"стол", L"пита", L"обед" }))
                return Windows::UI::ColorHelper::FromArgb(255, 150, 200, 255);

            if (ContainsAny(name, { L"детск", L"спальн" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 180, 220);

            if (ContainsAny(name, { L"кладов", L"гардер" }))
                return Windows::UI::ColorHelper::FromArgb(255, 180, 220, 180);

            if (ContainsAny(name, { L"коридор", L"холл", L"прихож" }))
                return Windows::UI::ColorHelper::FromArgb(255, 220, 220, 180);

            if (ContainsAny(name, { L"балкон", L"лодж" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200);

            if (ContainsAny(name, { L"офис", L"кабинет" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 230);

            return Windows::UI::ColorHelper::FromArgb(255, 200, 220, 200);
        }

        bool ContainsAny(const std::wstring& str, std::initializer_list<const wchar_t*> substrings)
        {
            for (const auto& sub : substrings)
            {
                if (str.find(sub) != std::wstring::npos)
                    return true;
            }
            return false;
        }

        RoomDisplaySettings m_settings;
    };
}
