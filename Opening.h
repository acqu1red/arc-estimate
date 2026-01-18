#pragma once

#include "pch.h"
#include "Models.h"

namespace winrt::estimate1
{
    // =====================================================
    // R4 — ДВЕРИ И ОКНА (ПРОЁМЫ)
    // =====================================================

    // Тип открывания двери
    enum class DoorSwingType
    {
        LeftInward,     // Левая, внутрь
        LeftOutward,    // Левая, наружу
        RightInward,    // Правая, внутрь
        RightOutward,   // Правая, наружу
        DoubleInward,   // Двустворчатая, внутрь
        DoubleOutward,  // Двустворчатая, наружу
        Sliding,        // Раздвижная
        Folding         // Складная
    };

    // Тип окна
    enum class WindowType
    {
        Single,         // Одностворчатое
        Double,         // Двустворчатое
        Triple,         // Трёхстворчатое
        Fixed,          // Глухое
        Panoramic,      // Панорамное
        Skylight        // Мансардное
    };

    // Базовый класс проёма (общий для дверей и окон)
    class Opening : public Element
    {
    public:
        Opening() = default;

        // ID стены-хоста
        uint64_t GetHostWallId() const { return m_hostWallId; }
        void SetHostWallId(uint64_t id) { m_hostWallId = id; }

        // Позиция на стене (0.0 = начало, 1.0 = конец)
        double GetPositionOnWall() const { return m_positionOnWall; }
        void SetPositionOnWall(double pos) { m_positionOnWall = std::clamp(pos, 0.0, 1.0); }

        // Ширина проёма (мм)
        double GetWidth() const { return m_width; }
        void SetWidth(double w) { m_width = (std::max)(100.0, w); }

        // Высота проёма (мм)
        double GetHeight() const { return m_height; }
        void SetHeight(double h) { m_height = (std::max)(100.0, h); }

        // Смещение от пола (мм) — для окон это подоконник
        double GetSillHeight() const { return m_sillHeight; }
        void SetSillHeight(double h) { m_sillHeight = (std::max)(0.0, h); }

        // Флип (отзеркалить)
        bool IsFlipped() const { return m_flipped; }
        void SetFlipped(bool f) { m_flipped = f; }
        void ToggleFlip() { m_flipped = !m_flipped; }

        // Вычислить центральную точку на стене
        WorldPoint GetCenterPoint(const Wall& hostWall) const
        {
            WorldPoint start = hostWall.GetStartPoint();
            WorldPoint end = hostWall.GetEndPoint();
            
            double x = start.X + (end.X - start.X) * m_positionOnWall;
            double y = start.Y + (end.Y - start.Y) * m_positionOnWall;
            
            return WorldPoint(x, y);
        }

        // Вычислить направление стены (нормализованный вектор)
        WorldPoint GetWallDirection(const Wall& hostWall) const
        {
            WorldPoint start = hostWall.GetStartPoint();
            WorldPoint end = hostWall.GetEndPoint();
            
            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double len = std::sqrt(dx * dx + dy * dy);
            
            if (len < 0.001) return WorldPoint(1, 0);
            return WorldPoint(dx / len, dy / len);
        }

        // Вычислить нормаль к стене (перпендикуляр)
        WorldPoint GetWallNormal(const Wall& hostWall) const
        {
            WorldPoint dir = GetWallDirection(hostWall);
            // Поворот на 90 градусов против часовой
            return m_flipped ? WorldPoint(dir.Y, -dir.X) : WorldPoint(-dir.Y, dir.X);
        }

        // Площадь проёма (м?)
        double GetAreaSqM() const
        {
            return (m_width / 1000.0) * (m_height / 1000.0);
        }

    protected:
        uint64_t m_hostWallId{ 0 };
        double m_positionOnWall{ 0.5 };     // Центр по умолчанию
        double m_width{ 900.0 };            // Стандартная ширина двери
        double m_height{ 2100.0 };          // Стандартная высота
        double m_sillHeight{ 0.0 };         // Высота от пола
        bool m_flipped{ false };            // Отзеркалить
    };

    // =====================================================
    // ДВЕРЬ
    // =====================================================

    class Door : public Opening
    {
    public:
        Door()
        {
            m_name = L"Дверь";
            m_width = 900.0;
            m_height = 2100.0;
            m_sillHeight = 0.0;
        }

        Door(uint64_t hostWallId, double positionOnWall = 0.5)
        {
            m_name = L"Дверь";
            m_hostWallId = hostWallId;
            m_positionOnWall = positionOnWall;
            m_width = 900.0;
            m_height = 2100.0;
            m_sillHeight = 0.0;
        }

        std::wstring GetTypeName() const override { return L"Door"; }

        // Тип открывания
        DoorSwingType GetSwingType() const { return m_swingType; }
        void SetSwingType(DoorSwingType type) { m_swingType = type; }

        // Угол открывания (для визуализации)
        double GetSwingAngle() const { return m_swingAngle; }
        void SetSwingAngle(double angle) { m_swingAngle = std::clamp(angle, 0.0, 180.0); }

        // Толщина полотна (мм)
        double GetLeafThickness() const { return m_leafThickness; }
        void SetLeafThickness(double t) { m_leafThickness = t; }

        // Проверка: двустворчатая?
        bool IsDouble() const
        {
            return m_swingType == DoorSwingType::DoubleInward ||
                   m_swingType == DoorSwingType::DoubleOutward;
        }

        // Проверка: открывается наружу?
        bool IsOutward() const
        {
            return m_swingType == DoorSwingType::LeftOutward ||
                   m_swingType == DoorSwingType::RightOutward ||
                   m_swingType == DoorSwingType::DoubleOutward;
        }

        // Проверка: левая петля?
        bool IsLeftHanded() const
        {
            return m_swingType == DoorSwingType::LeftInward ||
                   m_swingType == DoorSwingType::LeftOutward;
        }

        // Hit test для двери
        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Упрощённый hit test — требует стену для полного расчёта
            // Здесь проверяем только если m_cachedCenter установлен
            return point.Distance(m_cachedCenter) <= (m_width / 2 + tolerance);
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            double halfW = m_width / 2;
            minPoint.X = m_cachedCenter.X - halfW;
            minPoint.Y = m_cachedCenter.Y - halfW;
            maxPoint.X = m_cachedCenter.X + halfW;
            maxPoint.Y = m_cachedCenter.Y + halfW;
        }

        // Кэширование позиции для hit test
        void UpdateCachedPosition(const Wall& hostWall)
        {
            m_cachedCenter = GetCenterPoint(hostWall);
        }

        WorldPoint GetCachedCenter() const { return m_cachedCenter; }

    private:
        DoorSwingType m_swingType{ DoorSwingType::RightInward };
        double m_swingAngle{ 90.0 };        // Угол открывания в градусах
        double m_leafThickness{ 40.0 };     // Толщина полотна
        WorldPoint m_cachedCenter{ 0, 0 };  // Кэш позиции
    };

    // =====================================================
    // ОКНО
    // =====================================================

    class Window : public Opening
    {
    public:
        Window()
        {
            m_name = L"Окно";
            m_width = 1200.0;
            m_height = 1400.0;
            m_sillHeight = 900.0;       // Стандартная высота подоконника
        }

        Window(uint64_t hostWallId, double positionOnWall = 0.5)
        {
            m_name = L"Окно";
            m_hostWallId = hostWallId;
            m_positionOnWall = positionOnWall;
            m_width = 1200.0;
            m_height = 1400.0;
            m_sillHeight = 900.0;
        }

        std::wstring GetTypeName() const override { return L"Window"; }

        // Тип окна
        WindowType GetWindowType() const { return m_windowType; }
        void SetWindowType(WindowType type) { m_windowType = type; }

        // Глубина рамы (мм)
        double GetFrameDepth() const { return m_frameDepth; }
        void SetFrameDepth(double d) { m_frameDepth = d; }

        // Ширина рамы (мм)
        double GetFrameWidth() const { return m_frameWidth; }
        void SetFrameWidth(double w) { m_frameWidth = w; }

        // Количество створок
        int GetPaneCount() const
        {
            switch (m_windowType)
            {
            case WindowType::Single: return 1;
            case WindowType::Double: return 2;
            case WindowType::Triple: return 3;
            default: return 1;
            }
        }

        // Площадь остекления (м?)
        double GetGlazingAreaSqM() const
        {
            // Площадь за вычетом рамы
            double innerW = m_width - 2 * m_frameWidth;
            double innerH = m_height - 2 * m_frameWidth;
            if (innerW <= 0 || innerH <= 0) return 0;
            return (innerW / 1000.0) * (innerH / 1000.0);
        }

        // Hit test для окна
        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            return point.Distance(m_cachedCenter) <= (m_width / 2 + tolerance);
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            double halfW = m_width / 2;
            minPoint.X = m_cachedCenter.X - halfW;
            minPoint.Y = m_cachedCenter.Y - halfW;
            maxPoint.X = m_cachedCenter.X + halfW;
            maxPoint.Y = m_cachedCenter.Y + halfW;
        }

        // Кэширование позиции для hit test
        void UpdateCachedPosition(const Wall& hostWall)
        {
            m_cachedCenter = GetCenterPoint(hostWall);
        }

        WorldPoint GetCachedCenter() const { return m_cachedCenter; }

    private:
        WindowType m_windowType{ WindowType::Double };
        double m_frameDepth{ 70.0 };        // Глубина рамы
        double m_frameWidth{ 50.0 };        // Ширина профиля рамы
        WorldPoint m_cachedCenter{ 0, 0 };  // Кэш позиции
    };

    // =====================================================
    // ТИПЫ ДВЕРЕЙ И ОКОН (семейства)
    // =====================================================

    class DoorType
    {
    public:
        DoorType() : m_id(IdGenerator::Next()) {}

        DoorType(const std::wstring& name, double width, double height, DoorSwingType swing)
            : m_id(IdGenerator::Next()), m_name(name), 
              m_defaultWidth(width), m_defaultHeight(height), m_defaultSwing(swing)
        {}

        uint64_t GetId() const { return m_id; }
        
        std::wstring GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        double GetDefaultWidth() const { return m_defaultWidth; }
        void SetDefaultWidth(double w) { m_defaultWidth = w; }

        double GetDefaultHeight() const { return m_defaultHeight; }
        void SetDefaultHeight(double h) { m_defaultHeight = h; }

        DoorSwingType GetDefaultSwing() const { return m_defaultSwing; }
        void SetDefaultSwing(DoorSwingType s) { m_defaultSwing = s; }

        // Стоимость за единицу (руб)
        double GetUnitCost() const { return m_unitCost; }
        void SetUnitCost(double c) { m_unitCost = c; }

    private:
        uint64_t m_id{ 0 };
        std::wstring m_name{ L"Дверь стандартная" };
        double m_defaultWidth{ 900.0 };
        double m_defaultHeight{ 2100.0 };
        DoorSwingType m_defaultSwing{ DoorSwingType::RightInward };
        double m_unitCost{ 15000.0 };   // Стоимость по умолчанию
    };

    class WindowType_
    {
    public:
        WindowType_() : m_id(IdGenerator::Next()) {}

        WindowType_(const std::wstring& name, double width, double height, double sill, WindowType type)
            : m_id(IdGenerator::Next()), m_name(name),
              m_defaultWidth(width), m_defaultHeight(height), 
              m_defaultSillHeight(sill), m_defaultType(type)
        {}

        uint64_t GetId() const { return m_id; }

        std::wstring GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        double GetDefaultWidth() const { return m_defaultWidth; }
        void SetDefaultWidth(double w) { m_defaultWidth = w; }

        double GetDefaultHeight() const { return m_defaultHeight; }
        void SetDefaultHeight(double h) { m_defaultHeight = h; }

        double GetDefaultSillHeight() const { return m_defaultSillHeight; }
        void SetDefaultSillHeight(double h) { m_defaultSillHeight = h; }

        WindowType GetDefaultType() const { return m_defaultType; }
        void SetDefaultType(WindowType t) { m_defaultType = t; }

        // Стоимость за м? остекления (руб)
        double GetCostPerSqM() const { return m_costPerSqM; }
        void SetCostPerSqM(double c) { m_costPerSqM = c; }

    private:
        uint64_t m_id{ 0 };
        std::wstring m_name{ L"Окно стандартное" };
        double m_defaultWidth{ 1200.0 };
        double m_defaultHeight{ 1400.0 };
        double m_defaultSillHeight{ 900.0 };
        WindowType m_defaultType{ WindowType::Double };
        double m_costPerSqM{ 8000.0 };  // Стоимость за м? остекления
    };

    // =====================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // =====================================================

    inline std::wstring DoorSwingTypeToString(DoorSwingType type)
    {
        switch (type)
        {
        case DoorSwingType::LeftInward:    return L"Левая внутрь";
        case DoorSwingType::LeftOutward:   return L"Левая наружу";
        case DoorSwingType::RightInward:   return L"Правая внутрь";
        case DoorSwingType::RightOutward:  return L"Правая наружу";
        case DoorSwingType::DoubleInward:  return L"Двустворчатая внутрь";
        case DoorSwingType::DoubleOutward: return L"Двустворчатая наружу";
        case DoorSwingType::Sliding:       return L"Раздвижная";
        case DoorSwingType::Folding:       return L"Складная";
        default: return L"Неизвестно";
        }
    }

    inline std::wstring WindowTypeToString(WindowType type)
    {
        switch (type)
        {
        case WindowType::Single:    return L"Одностворчатое";
        case WindowType::Double:    return L"Двустворчатое";
        case WindowType::Triple:    return L"Трёхстворчатое";
        case WindowType::Fixed:     return L"Глухое";
        case WindowType::Panoramic: return L"Панорамное";
        case WindowType::Skylight:  return L"Мансардное";
        default: return L"Неизвестно";
        }
    }
}
