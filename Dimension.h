#pragma once

#include "pch.h"
#include "Models.h"

namespace winrt::estimate1
{
    enum class DimensionHandle
    {
        None,
        Start,
        Middle,
        End
    };

    enum class DimensionType
    {
        WallLength,      // Длина стены (авто)
        WallSegment,     // Сегмент стены
        OpeningWidth,    // Ширина проёма
        OpeningOffset,   // Смещение проёма
        Manual           // Ручной размер
    };

    enum class DimensionTickType
    {
        Tick,       // Засечка (архитектурная)
        Arrow,      // Стрелка
        Dot         // Точка
    };

    // Класс размера (Dimension)
    class Dimension : public Element
    {
    public:
        Dimension() = default;

        Dimension(uint64_t ownerWallId, const WorldPoint& p1, const WorldPoint& p2, DimensionType type)
            : m_ownerWallId(ownerWallId), m_p1(p1), m_p2(p2), m_type(type)
        {
            m_name = L"Размер";
        }

        // Конструктор для ручных размеров (без привязки к стене)
        Dimension(const WorldPoint& p1, const WorldPoint& p2)
            : m_ownerWallId(0), m_p1(p1), m_p2(p2), m_type(DimensionType::Manual)
        {
            m_name = L"Ручной размер";
            m_isLocked = true; // Ручные размеры всегда "заблокированы"
        }

        DimensionHandle HitTestHandleKind(const WorldPoint& point, double worldTolerance) const
        {
            WorldPoint h1, hm, h2;
            GetHandlePoints(h1, hm, h2);

            if (point.Distance(h1) <= worldTolerance) return DimensionHandle::Start;
            if (point.Distance(hm) <= worldTolerance) return DimensionHandle::Middle;
            if (point.Distance(h2) <= worldTolerance) return DimensionHandle::End;
            return DimensionHandle::None;
        }

        std::wstring GetTypeName() const override { return L"Dimension"; }

        DimensionType GetDimensionType() const { return m_type; }
        void SetDimensionType(DimensionType type) { m_type = type; }

        bool IsManual() const { return m_type == DimensionType::Manual; }
        bool IsAuto() const { return m_type != DimensionType::Manual; }

        uint64_t GetOwnerWallId() const { return m_ownerWallId; }
        void SetOwnerWallId(uint64_t id) { m_ownerWallId = id; }

        // ID цепочки размеров (0 = не в цепочке)
        uint64_t GetChainId() const { return m_chainId; }
        void SetChainId(uint64_t id) { m_chainId = id; }

        WorldPoint GetP1() const { return m_p1; }
        WorldPoint GetP2() const { return m_p2; }

        void SetP1(const WorldPoint& p) { m_p1 = p; }
        void SetP2(const WorldPoint& p) { m_p2 = p; }

        bool IsLocked() const { return m_isLocked; }
        void SetLocked(bool locked) { m_isLocked = locked; }

        // Смещение размерной линии от базовой геометрии (в мм, по нормали к измеряемому отрезку)
        double GetOffset() const { return m_offset; }
        void SetOffset(double offsetMm) { m_offset = offsetMm; }

        double GetValueMm() const
        {
            return m_p1.Distance(m_p2);
        }

        // Точки ручек на размерной линии (в мировых координатах)
        void GetHandlePoints(WorldPoint& h1, WorldPoint& hm, WorldPoint& h2) const
        {
            // Ручки располагаются на параллельной (offset) линии
            double dx = m_p2.X - m_p1.X;
            double dy = m_p2.Y - m_p1.Y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001)
            {
                h1 = m_p1;
                hm = m_p1;
                h2 = m_p2;
                return;
            }

            double nx = -dy / len;
            double ny = dx / len;

            WorldPoint a1(m_p1.X + nx * m_offset, m_p1.Y + ny * m_offset);
            WorldPoint a2(m_p2.X + nx * m_offset, m_p2.Y + ny * m_offset);
            WorldPoint mid((a1.X + a2.X) * 0.5, (a1.Y + a2.Y) * 0.5);

            h1 = a1;
            hm = mid;
            h2 = a2;
        }

        // Хит-тест по ручкам (в мм, удобно для drag)
        bool HitTestHandle(const WorldPoint& point, double worldTolerance) const
        {
            return HitTestHandleKind(point, worldTolerance) != DimensionHandle::None;
        }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Хит-тест по расстоянию до размерной линии (с учётом offset)
            WorldPoint h1, hm, h2;
            GetHandlePoints(h1, hm, h2);
            
            double dx = h2.X - h1.X;
            double dy = h2.Y - h1.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return point.Distance(h1) <= tolerance;

            double t = ((point.X - h1.X) * dx + (point.Y - h1.Y) * dy) / lengthSq;
            t = std::clamp(t, 0.0, 1.0);

            WorldPoint closest(h1.X + t * dx, h1.Y + t * dy);
            return point.Distance(closest) <= tolerance;
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            minPoint.X = (std::min)(m_p1.X, m_p2.X);
            minPoint.Y = (std::min)(m_p1.Y, m_p2.Y);
            maxPoint.X = (std::max)(m_p1.X, m_p2.X);
            maxPoint.Y = (std::max)(m_p1.Y, m_p2.Y);
        }

    private:
        uint64_t m_ownerWallId{ 0 };
        uint64_t m_chainId{ 0 };
        WorldPoint m_p1{ 0, 0 };
        WorldPoint m_p2{ 0, 0 };
        DimensionType m_type{ DimensionType::WallLength };
        DimensionTickType m_tickType{ DimensionTickType::Tick }; // Use Tick by default
        bool m_isLocked{ false };
        double m_offset{ 200.0 };

    public:
        DimensionTickType GetTickType() const { return m_tickType; }
        void SetTickType(DimensionTickType type) { m_tickType = type; }
    };

    // Цепочка размеров (DimensionChain)
    // Группирует связанные размеры для совместного управления offset и выравнивания
    class DimensionChain
    {
    public:
        DimensionChain()
            : m_id(IdGenerator::Next())
        {
        }

        uint64_t GetId() const { return m_id; }

        // Имя цепочки (опционально)
        std::wstring GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        // Общий offset для всех размеров в цепочке
        double GetOffset() const { return m_offset; }
        void SetOffset(double offset) { m_offset = offset; }

        // Заблокирована ли цепочка
        bool IsLocked() const { return m_isLocked; }
        void SetLocked(bool locked) { m_isLocked = locked; }

        // Направление цепочки (нормаль)
        WorldPoint GetDirection() const { return m_direction; }
        void SetDirection(const WorldPoint& dir) { m_direction = dir; }

        // ID размеров в цепочке
        const std::vector<uint64_t>& GetDimensionIds() const { return m_dimensionIds; }

        void AddDimensionId(uint64_t dimId)
        {
            if (std::find(m_dimensionIds.begin(), m_dimensionIds.end(), dimId) == m_dimensionIds.end())
            {
                m_dimensionIds.push_back(dimId);
            }
        }

        void RemoveDimensionId(uint64_t dimId)
        {
            m_dimensionIds.erase(
                std::remove(m_dimensionIds.begin(), m_dimensionIds.end(), dimId),
                m_dimensionIds.end());
        }

        void ClearDimensionIds() { m_dimensionIds.clear(); }

        size_t GetDimensionCount() const { return m_dimensionIds.size(); }

        // Вычислить общую длину цепочки
        double GetTotalLength() const { return m_totalLength; }
        void SetTotalLength(double length) { m_totalLength = length; }

    private:
        uint64_t m_id{ 0 };
        std::wstring m_name{ L"Цепочка" };
        double m_offset{ 300.0 };
        bool m_isLocked{ false };
        WorldPoint m_direction{ 0, 1 }; // Нормаль к размерной линии
        std::vector<uint64_t> m_dimensionIds;
        double m_totalLength{ 0.0 };
    };

    // =====================================================
    // R3 — УГЛОВОЙ РАЗМЕР (Angular Dimension)
    // =====================================================

    class AngularDimension : public Element
    {
    public:
        AngularDimension() = default;

        AngularDimension(const WorldPoint& center, const WorldPoint& start, const WorldPoint& end)
            : m_center(center), m_startPoint(start), m_endPoint(end)
        {
            m_name = L"Угловой размер";
            CalculateAngle();
        }

        std::wstring GetTypeName() const override { return L"AngularDimension"; }

        // Центр дуги (точка пересечения линий)
        WorldPoint GetCenter() const { return m_center; }
        void SetCenter(const WorldPoint& p) { m_center = p; CalculateAngle(); }

        // Начальная точка на первой линии
        WorldPoint GetStartPoint() const { return m_startPoint; }
        void SetStartPoint(const WorldPoint& p) { m_startPoint = p; CalculateAngle(); }

        // Конечная точка на второй линии
        WorldPoint GetEndPoint() const { return m_endPoint; }
        void SetEndPoint(const WorldPoint& p) { m_endPoint = p; CalculateAngle(); }

        // Угол в радианах
        double GetAngleRadians() const { return m_angleRadians; }

        // Угол в градусах
        double GetAngleDegrees() const { return m_angleRadians * 180.0 / 3.14159265358979323846; }

        // Радиус дуги для отображения
        double GetArcRadius() const { return m_arcRadius; }
        void SetArcRadius(double radius) { m_arcRadius = radius; }

        // Начальный угол (для отрисовки дуги)
        double GetStartAngle() const { return m_startAngle; }

        // Конечный угол
        double GetEndAngle() const { return m_endAngle; }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Hit test по дуге
            double dist = point.Distance(m_center);
            if (std::abs(dist - m_arcRadius) > tolerance)
                return false;

            // Проверяем что точка в секторе угла
            double angle = std::atan2(point.Y - m_center.Y, point.X - m_center.X);
            return IsAngleInRange(angle);
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            double r = m_arcRadius;
            minPoint.X = m_center.X - r;
            minPoint.Y = m_center.Y - r;
            maxPoint.X = m_center.X + r;
            maxPoint.Y = m_center.Y + r;
        }

    private:
        WorldPoint m_center{ 0, 0 };
        WorldPoint m_startPoint{ 0, 0 };
        WorldPoint m_endPoint{ 0, 0 };
        double m_angleRadians{ 0.0 };
        double m_arcRadius{ 100.0 };    // Радиус отображения дуги
        double m_startAngle{ 0.0 };
        double m_endAngle{ 0.0 };

        void CalculateAngle()
        {
            // Векторы от центра к точкам
            double dx1 = m_startPoint.X - m_center.X;
            double dy1 = m_startPoint.Y - m_center.Y;
            double dx2 = m_endPoint.X - m_center.X;
            double dy2 = m_endPoint.Y - m_center.Y;

            // Углы
            m_startAngle = std::atan2(dy1, dx1);
            m_endAngle = std::atan2(dy2, dx2);

            // Угол между векторами
            double dot = dx1 * dx2 + dy1 * dy2;
            double mag1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            double mag2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

            if (mag1 > 0.001 && mag2 > 0.001)
            {
                double cosAngle = std::clamp(dot / (mag1 * mag2), -1.0, 1.0);
                m_angleRadians = std::acos(cosAngle);
            }
            else
            {
                m_angleRadians = 0.0;
            }
        }

        bool IsAngleInRange(double angle) const
        {
            constexpr double PI2 = 2 * 3.14159265358979323846;
            // Нормализуем углы
            auto normalize = [](double a) {
                while (a < 0) a += 2 * 3.14159265358979323846;
                while (a >= 2 * 3.14159265358979323846) a -= 2 * 3.14159265358979323846;
                return a;
            };

            double start = normalize(m_startAngle);
            double end = normalize(m_endAngle);
            double test = normalize(angle);

            if (start <= end)
                return test >= start && test <= end;
            else
                return test >= start || test <= end;
        }
    };
}

