#pragma once

// R5.2 — Модель помещения (Room Model)
// Помещение — замкнутый контур, образованный стенами или разделителями

#include "pch.h"
#include "Models.h"
#include <vector>
#include <string>
#include <numeric>
#include <set>

namespace winrt::estimate1
{
    // Тип границы помещения
    enum class RoomBoundaryType
    {
        Wall,           // Стена
        Separator,      // Разделитель помещений (виртуальная линия)
        Opening         // Проём (дверь/окно) — учитывается в стене
    };

    // Граница помещения (сегмент контура)
    struct RoomBoundary
    {
        uint64_t ElementId{ 0 };         // ID стены или разделителя
        RoomBoundaryType Type{ RoomBoundaryType::Wall };
        WorldPoint StartPoint;
        WorldPoint EndPoint;
        double Length{ 0.0 };

        RoomBoundary() = default;
        RoomBoundary(uint64_t id, RoomBoundaryType type, const WorldPoint& start, const WorldPoint& end)
            : ElementId(id), Type(type), StartPoint(start), EndPoint(end)
        {
            Length = start.Distance(end);
        }
    };

    // Тип помещения (категория)
    enum class RoomCategory
    {
        Undefined,      // Не определено
        Living,         // Жилое (гостиная, спальня)
        Wet,            // Мокрое (ванная, кухня)
        Service,        // Техническое (кладовая, гардеробная)
        Circulation,    // Коридор, холл, прихожая
        Balcony,        // Балкон, лоджия, терраса
        Office          // Кабинет, офис
    };

    class Room : public Element
    {
    public:
        Room() = default;

        std::wstring GetTypeName() const override { return L"Room"; }

        // =====================================================================
        // Идентификация
        // =====================================================================

        void SetNumber(const std::wstring& number) { m_number = number; }
        const std::wstring& GetNumber() const { return m_number; }

        void SetName(const std::wstring& name) 
        { 
            m_name = name;
            // Автоопределение категории по названию
            m_category = DetectCategoryFromName(name);
        }
        const std::wstring& GetName() const { return m_name; }

        void SetCategory(RoomCategory category) { m_category = category; }
        RoomCategory GetCategory() const { return m_category; }

        // =====================================================================
        // Размеры и высота
        // =====================================================================

        void SetCeilingHeight(double height) { m_ceilingHeight = height; }
        double GetCeilingHeight() const { return m_ceilingHeight; }

        void SetFloorLevel(double level) { m_floorLevel = level; }
        double GetFloorLevel() const { return m_floorLevel; }

        // =====================================================================
        // Отделка
        // =====================================================================

        void SetFinishType(const std::wstring& finish) { m_finishType = finish; }
        const std::wstring& GetFinishType() const { return m_finishType; }

        void SetFloorFinish(const std::wstring& finish) { m_floorFinish = finish; }
        const std::wstring& GetFloorFinish() const { return m_floorFinish; }

        void SetCeilingFinish(const std::wstring& finish) { m_ceilingFinish = finish; }
        const std::wstring& GetCeilingFinish() const { return m_ceilingFinish; }

        void SetWallFinish(const std::wstring& finish) { m_wallFinish = finish; }
        const std::wstring& GetWallFinish() const { return m_wallFinish; }

        // =====================================================================
        // Геометрия
        // =====================================================================

        const std::vector<WorldPoint>& GetContour() const { return m_contour; }
        void SetContour(std::vector<WorldPoint> contour)
        {
            m_contour = std::move(contour);
            UpdateMetrics();
        }

        double GetArea() const { return m_area; }
        double GetAreaSqM() const { return std::abs(m_area) / 1000000.0; } // м?
        double GetPerimeter() const { return m_perimeter; }
        double GetPerimeterM() const { return m_perimeter / 1000.0; } // м

        // Объём помещения (площадь * высота)
        double GetVolume() const { return std::abs(m_area) * m_ceilingHeight; }
        double GetVolumeCuM() const { return GetVolume() / 1000000000.0; } // м?

        // Площадь стен (периметр * высота)
        double GetWallArea() const { return m_perimeter * m_ceilingHeight; }
        double GetWallAreaSqM() const { return GetWallArea() / 1000000.0; } // м?

        // =====================================================================
        // Границы помещения
        // =====================================================================

        void SetBoundaries(std::vector<RoomBoundary> boundaries)
        {
            m_boundaries = std::move(boundaries);
            RebuildBoundingWallIds();
        }

        const std::vector<RoomBoundary>& GetBoundaries() const { return m_boundaries; }

        void AddBoundary(const RoomBoundary& boundary)
        {
            m_boundaries.push_back(boundary);
            if (boundary.Type == RoomBoundaryType::Wall)
                m_boundingWallIds.insert(boundary.ElementId);
        }

        void ClearBoundaries()
        {
            m_boundaries.clear();
            m_boundingWallIds.clear();
        }

        // ID стен, ограничивающих помещение
        const std::set<uint64_t>& GetBoundingWallIds() const { return m_boundingWallIds; }

        bool IsBoundedByWall(uint64_t wallId) const 
        { 
            return m_boundingWallIds.find(wallId) != m_boundingWallIds.end(); 
        }

        // =====================================================================
        // Метка помещения
        // =====================================================================

        WorldPoint GetLabelPoint() const 
        { 
            // Если пользователь переместил метку — используем пользовательскую позицию
            if (m_labelPositionOverride.X != 0.0 || m_labelPositionOverride.Y != 0.0)
                return m_labelPositionOverride;
            return m_centroid; 
        }

        void SetLabelPosition(const WorldPoint& pos) { m_labelPositionOverride = pos; }
        void ResetLabelPosition() { m_labelPositionOverride = WorldPoint{ 0, 0 }; }

        // Вспомогательная функция для внешних алгоритмов (детектор помещений)
        static double ComputeArea(const std::vector<WorldPoint>& poly)
        {
            if (poly.size() < 3) return 0.0;
            double sum = 0.0;
            for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
            {
                sum += (poly[j].X * poly[i].Y) - (poly[i].X * poly[j].Y);
            }
            return 0.5 * sum;
        }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            if (m_contour.size() < 3)
                return false;

            // Bounding box quick check
            if (!m_boundsInitialized)
            {
                const_cast<Room*>(this)->RecalcBounds();
            }

            if (point.X < m_minBounds.X - tolerance || point.X > m_maxBounds.X + tolerance ||
                point.Y < m_minBounds.Y - tolerance || point.Y > m_maxBounds.Y + tolerance)
            {
                return false;
            }

            // Ray casting point-in-polygon
            bool inside = false;
            size_t count = m_contour.size();
            for (size_t i = 0, j = count - 1; i < count; j = i++)
            {
                const WorldPoint& pi = m_contour[i];
                const WorldPoint& pj = m_contour[j];

                bool intersect = ((pi.Y > point.Y) != (pj.Y > point.Y)) &&
                    (point.X < (pj.X - pi.X) * (point.Y - pi.Y) / ((pj.Y - pi.Y) + 1e-12) + pi.X);
                if (intersect)
                    inside = !inside;
            }

            return inside;
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            if (!m_boundsInitialized)
            {
                const_cast<Room*>(this)->RecalcBounds();
            }
            minPoint = m_minBounds;
            maxPoint = m_maxBounds;
        }

    private:
        void UpdateMetrics()
        {
            RecalcBounds();
            m_area = ComputeArea(m_contour);
            m_perimeter = ComputePerimeter(m_contour);
            m_centroid = ComputeCentroid(m_contour, m_area);
        }

        static double ComputePerimeter(const std::vector<WorldPoint>& poly)
        {
            if (poly.size() < 2) return 0.0;
            double length = 0.0;
            for (size_t i = 0; i < poly.size(); ++i)
            {
                const WorldPoint& a = poly[i];
                const WorldPoint& b = poly[(i + 1) % poly.size()];
                length += a.Distance(b);
            }
            return length;
        }

        static WorldPoint ComputeCentroid(const std::vector<WorldPoint>& poly, double signedArea)
        {
            if (poly.size() < 3 || std::abs(signedArea) < 1e-9)
                return WorldPoint{ 0, 0 };

            double cx = 0.0;
            double cy = 0.0;
            for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
            {
                double cross = (poly[j].X * poly[i].Y) - (poly[i].X * poly[j].Y);
                cx += (poly[j].X + poly[i].X) * cross;
                cy += (poly[j].Y + poly[i].Y) * cross;
            }

            double area6 = 6.0 * signedArea;
            return WorldPoint{ cx / area6, cy / area6 };
        }

        void RecalcBounds()
        {
            if (m_contour.empty())
            {
                m_minBounds = WorldPoint{ 0,0 };
                m_maxBounds = WorldPoint{ 0,0 };
                m_boundsInitialized = true;
                return;
            }

            double minX = m_contour.front().X;
            double minY = m_contour.front().Y;
            double maxX = m_contour.front().X;
            double maxY = m_contour.front().Y;
            for (const auto& p : m_contour)
            {
                minX = (std::min)(minX, p.X);
                minY = (std::min)(minY, p.Y);
                maxX = (std::max)(maxX, p.X);
                maxY = (std::max)(maxY, p.Y);
            }
            m_minBounds = WorldPoint{ minX, minY };
            m_maxBounds = WorldPoint{ maxX, maxY };
            m_boundsInitialized = true;
        }

        void RebuildBoundingWallIds()
        {
            m_boundingWallIds.clear();
            for (const auto& b : m_boundaries)
            {
                if (b.Type == RoomBoundaryType::Wall)
                    m_boundingWallIds.insert(b.ElementId);
            }
        }

        static RoomCategory DetectCategoryFromName(const std::wstring& name)
        {
            // Мокрые помещения
            if (ContainsAnyStatic(name, { L"кухня", L"Кухня", L"ванная", L"Ванная", 
                L"санузел", L"Санузел", L"туалет", L"Туалет", L"душ", L"Душ" }))
                return RoomCategory::Wet;

            // Жилые помещения
            if (ContainsAnyStatic(name, { L"спальня", L"Спальня", L"гостиная", L"Гостиная", 
                L"зал", L"Зал", L"детская", L"Детская" }))
                return RoomCategory::Living;

            // Коридоры
            if (ContainsAnyStatic(name, { L"прихожая", L"Прихожая", L"холл", L"Холл", 
                L"коридор", L"Коридор", L"тамбур", L"Тамбур" }))
                return RoomCategory::Circulation;

            // Балконы
            if (ContainsAnyStatic(name, { L"балкон", L"Балкон", L"лоджия", L"Лоджия", 
                L"терраса", L"Терраса", L"веранда", L"Веранда" }))
                return RoomCategory::Balcony;

            // Технические
            if (ContainsAnyStatic(name, { L"кладов", L"Кладов", L"гардероб", L"Гардероб", 
                L"постир", L"Постир", L"котельн", L"Котельн" }))
                return RoomCategory::Service;

            // Офисные
            if (ContainsAnyStatic(name, { L"кабинет", L"Кабинет", L"офис", L"Офис" }))
                return RoomCategory::Office;

            return RoomCategory::Undefined;
        }

        static bool ContainsAnyStatic(const std::wstring& str, std::initializer_list<const wchar_t*> substrings)
        {
            for (const auto& sub : substrings)
            {
                if (str.find(sub) != std::wstring::npos)
                    return true;
            }
            return false;
        }

        // Геометрия
        std::vector<WorldPoint> m_contour;
        double m_area{ 0.0 };          // signed area (мм?)
        double m_perimeter{ 0.0 };     // мм
        WorldPoint m_centroid{ 0, 0 };
        bool m_boundsInitialized{ false };
        WorldPoint m_minBounds{ 0, 0 };
        WorldPoint m_maxBounds{ 0, 0 };

        // Идентификация
        std::wstring m_number;
        std::wstring m_name;
        RoomCategory m_category{ RoomCategory::Undefined };

        // Размеры
        double m_ceilingHeight{ 2700.0 };  // мм
        double m_floorLevel{ 0.0 };        // мм (уровень пола относительно 0)

        // Отделка
        std::wstring m_finishType;        // Общий тип отделки
        std::wstring m_floorFinish;       // Отделка пола
        std::wstring m_ceilingFinish;     // Отделка потолка
        std::wstring m_wallFinish;        // Отделка стен

        // Границы
        std::vector<RoomBoundary> m_boundaries;
        std::set<uint64_t> m_boundingWallIds;

        // Метка
        WorldPoint m_labelPositionOverride{ 0, 0 };
    };
}
