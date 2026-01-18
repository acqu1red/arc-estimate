#pragma once

// R5.2 — Рендерер помещений (Room Renderer)
// Визуализация помещений: заливка, границы, метки с названием и площадью

#include "pch.h"
#include "Room.h"
#include "Camera.h"
#include "Element.h"
#include <cmath>

namespace winrt::estimate1
{
    // Типы заливки помещений
    enum class RoomFillStyle
    {
        Solid,          // Сплошная заливка
        Hatched,        // Штриховка
        ColorByType,    // Цвет по типу помещения
        None            // Без заливки
    };

    // Настройки отображения помещений
    struct RoomDisplaySettings
    {
        bool ShowRooms{ true };
        bool ShowLabels{ true };
        bool ShowArea{ true };
        bool ShowPerimeter{ false };
        RoomFillStyle FillStyle{ RoomFillStyle::Solid };
        uint8_t FillOpacity{ 40 };
        float BorderWidth{ 1.5f };
        bool HighlightSelected{ true };
    };

    // Класс для отрисовки помещений на холсте Win2D
    class RoomRenderer
    {
    public:
        RoomRenderer() = default;

        // Настройки отображения
        void SetDisplaySettings(const RoomDisplaySettings& settings) { m_settings = settings; }
        const RoomDisplaySettings& GetDisplaySettings() const { return m_settings; }

        // Отрисовка всех помещений
        void Draw(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const DocumentModel& document,
            uint64_t hoverRoomId = 0)
        {
            if (!m_settings.ShowRooms)
                return;

            const auto& rooms = document.GetRooms();
            
            for (const auto& room : rooms)
            {
                if (!room)
                    continue;

                bool isHovered = (hoverRoomId != 0 && room->GetId() == hoverRoomId);
                DrawRoom(session, camera, *room, isHovered);
            }
        }

        // Отрисовка одного помещения
        void DrawRoom(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Room& room,
            bool isHovered = false)
        {
            const auto& contour = room.GetContour();
            if (contour.size() < 3)
                return;

            // Получаем цвет на основе типа помещения
            Windows::UI::Color roomColor = GetRoomColor(room);

            // Hover эффект
            if (isHovered && !room.IsSelected())
            {
                roomColor.R = static_cast<uint8_t>((std::min)(255, roomColor.R + 30));
                roomColor.G = static_cast<uint8_t>((std::min)(255, roomColor.G + 30));
                roomColor.B = static_cast<uint8_t>((std::min)(255, roomColor.B + 30));
            }

            // Создаём путь для контура
            auto pathBuilder = Microsoft::Graphics::Canvas::Geometry::CanvasPathBuilder(session.Device());

            ScreenPoint first = camera.WorldToScreen(contour[0]);
            pathBuilder.BeginFigure(Windows::Foundation::Numerics::float2(first.X, first.Y));

            for (size_t i = 1; i < contour.size(); ++i)
            {
                ScreenPoint screen = camera.WorldToScreen(contour[i]);
                pathBuilder.AddLine(Windows::Foundation::Numerics::float2(screen.X, screen.Y));
            }

            pathBuilder.EndFigure(Microsoft::Graphics::Canvas::Geometry::CanvasFigureLoop::Closed);
            auto geometry = Microsoft::Graphics::Canvas::Geometry::CanvasGeometry::CreatePath(pathBuilder);

            // Заливка
            if (m_settings.FillStyle != RoomFillStyle::None)
            {
                Windows::UI::Color fillColor = roomColor;
                fillColor.A = m_settings.FillOpacity;
                
                if (room.IsSelected())
                    fillColor.A = static_cast<uint8_t>((std::min)(255, fillColor.A + 30));

                session.FillGeometry(geometry, fillColor);
            }

            // Граница
            float borderWidth = m_settings.BorderWidth;
            if (room.IsSelected())
                borderWidth += 1.0f;

            Windows::UI::Color borderColor = roomColor;
            borderColor.A = 180;
            
            session.DrawGeometry(geometry, borderColor, borderWidth);

            // Метка помещения
            if (m_settings.ShowLabels)
            {
                DrawRoomLabel(session, camera, room);
            }
        }

        // Отрисовка метки помещения
        void DrawRoomLabel(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const Room& room)
        {
            WorldPoint labelPoint = room.GetLabelPoint();
            ScreenPoint screenPos = camera.WorldToScreen(labelPoint);

            // Формируем текст метки
            std::wstring labelText;

            // Номер помещения
            if (!room.GetNumber().empty())
            {
                labelText = room.GetNumber();
            }

            // Название помещения
            if (!room.GetName().empty())
            {
                if (!labelText.empty())
                    labelText += L"\n";
                labelText += room.GetName();
            }

            // Площадь
            if (m_settings.ShowArea)
            {
                double areaSqM = std::abs(room.GetArea()) / 1000000.0; // мм? -> м?
                wchar_t areaStr[64];
                swprintf_s(areaStr, L"%.2f м?", areaSqM);
                
                if (!labelText.empty())
                    labelText += L"\n";
                labelText += areaStr;
            }

            // Периметр
            if (m_settings.ShowPerimeter)
            {
                double perimeterM = room.GetPerimeter() / 1000.0; // мм -> м
                wchar_t perimStr[64];
                swprintf_s(perimStr, L"P: %.2f м", perimeterM);
                
                if (!labelText.empty())
                    labelText += L"\n";
                labelText += perimStr;
            }

            if (labelText.empty())
                return;

            // Создаём формат текста
            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontFamily(L"Segoe UI");
            textFormat.FontSize(12.0f);
            textFormat.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);
            textFormat.VerticalAlignment(Microsoft::Graphics::Canvas::Text::CanvasVerticalAlignment::Center);

            // Измеряем размер текста
            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session.Device(),
                labelText,
                textFormat,
                300.0f,  // max width
                200.0f   // max height
            );

            float textWidth = textLayout.LayoutBounds().Width;
            float textHeight = textLayout.LayoutBounds().Height;

            // Позиция текста (центрирование)
            float textX = screenPos.X - textWidth / 2.0f;
            float textY = screenPos.Y - textHeight / 2.0f;

            // Фон метки (полупрозрачный)
            float padding = 4.0f;
            Windows::Foundation::Rect bgRect{
                textX - padding,
                textY - padding,
                textWidth + padding * 2,
                textHeight + padding * 2
            };

            Windows::UI::Color bgColor = Windows::UI::ColorHelper::FromArgb(200, 255, 255, 255);
            if (room.IsSelected())
                bgColor = Windows::UI::ColorHelper::FromArgb(220, 255, 255, 200);

            session.FillRoundedRectangle(bgRect, 3.0f, 3.0f, bgColor);

            // Рамка метки
            Windows::UI::Color frameBorderColor = Windows::UI::ColorHelper::FromArgb(180, 100, 100, 100);
            session.DrawRoundedRectangle(bgRect, 3.0f, 3.0f, frameBorderColor, 0.5f);

            // Текст метки
            Windows::UI::Color textColor = Windows::UI::ColorHelper::FromArgb(255, 40, 40, 40);
            session.DrawTextLayout(
                textLayout,
                Windows::Foundation::Numerics::float2(textX, textY),
                textColor);
        }

    private:
        RoomDisplaySettings m_settings;

        // Получение цвета помещения на основе типа отделки / имени
        Windows::UI::Color GetRoomColor(const Room& room)
        {
            // Цвета по типу помещения
            const std::wstring& name = room.GetName();
            const std::wstring& finishType = room.GetFinishType();

            // Определяем категорию помещения по названию
            if (ContainsAny(name, { L"кухня", L"Кухня" }))
                return Windows::UI::ColorHelper::FromArgb(255, 255, 200, 150); // Оранжевый

            if (ContainsAny(name, { L"ванная", L"Ванная", L"санузел", L"Санузел", L"туалет", L"Туалет", L"душ", L"Душ" }))
                return Windows::UI::ColorHelper::FromArgb(255, 150, 200, 255); // Голубой

            if (ContainsAny(name, { L"спальня", L"Спальня" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 180, 220); // Лавандовый

            if (ContainsAny(name, { L"гостиная", L"Гостиная", L"зал", L"Зал" }))
                return Windows::UI::ColorHelper::FromArgb(255, 180, 220, 180); // Зелёный

            if (ContainsAny(name, { L"прихожая", L"Прихожая", L"холл", L"Холл", L"коридор", L"Коридор" }))
                return Windows::UI::ColorHelper::FromArgb(255, 220, 220, 180); // Бежевый

            if (ContainsAny(name, { L"кладов", L"Кладов", L"гардероб", L"Гардероб" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200); // Серый

            if (ContainsAny(name, { L"балкон", L"Балкон", L"лоджия", L"Лоджия", L"терраса", L"Терраса" }))
                return Windows::UI::ColorHelper::FromArgb(255, 180, 230, 200); // Мятный

            if (ContainsAny(name, { L"кабинет", L"Кабинет", L"офис", L"Офис" }))
                return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 230); // Сиреневый

            if (ContainsAny(name, { L"детская", L"Детская" }))
                return Windows::UI::ColorHelper::FromArgb(255, 255, 220, 200); // Персиковый

            // Цвет по умолчанию
            return Windows::UI::ColorHelper::FromArgb(255, 200, 220, 200); // Светло-зелёный
        }

        // Проверка, содержит ли строка любую из подстрок
        bool ContainsAny(const std::wstring& str, std::initializer_list<const wchar_t*> substrings)
        {
            for (const auto& sub : substrings)
            {
                if (str.find(sub) != std::wstring::npos)
                    return true;
            }
            return false;
        }
    };
}
