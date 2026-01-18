#pragma once

#include "pch.h"
#include "Models.h"

namespace winrt::estimate1
{
    // ============================================================================
    // КОЛОННЫ (R6.1)
    // ============================================================================

    enum class ColumnShape
    {
        Rectangular,
        Circular
    };

    class Column : public Element
    {
    public:
        Column() : Element() 
        {
            m_name = L"Колонна";
        }

        // Тип элемента
        std::wstring GetTypeName() const override { return L"Column"; }

        // Геометрия
        void SetPosition(const WorldPoint& pos) { m_position = pos; }
        WorldPoint GetPosition() const { return m_position; }

        void SetExample(double width, double depth)
        {
            m_shape = ColumnShape::Rectangular;
            m_width = width;
            m_depth = depth;
        }

        void SetCircular(double diameter)
        {
            m_shape = ColumnShape::Circular;
            m_width = diameter; // Using width as diameter
            m_depth = diameter;
        }

        ColumnShape GetShape() const { return m_shape; }
        double GetWidth() const { return m_width; }
        double GetDepth() const { return m_depth; } // Or Diameter for circular

        void SetRotation(double degrees) { m_rotation = degrees; }
        double GetRotation() const { return m_rotation; }

        // Высота
        void SetHeight(double h) { m_height = h; }
        double GetHeight() const { return m_height; }
        
        void SetBaseOffset(double offset) { m_baseOffset = offset; }
        double GetBaseOffset() const { return m_baseOffset; }

        // HitTest
        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Transform point to local coords
            WorldPoint local = TransformToLocal(point);

            if (m_shape == ColumnShape::Circular)
            {
                double radius = m_width / 2.0;
                return (local.X * local.X + local.Y * local.Y) <= (radius + tolerance) * (radius + tolerance);
            }
            else
            {
                double halfW = m_width / 2.0 + tolerance;
                double halfD = m_depth / 2.0 + tolerance;
                return std::abs(local.X) <= halfW && std::abs(local.Y) <= halfD;
            }
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            // Simplified bounds (AABB usually larger than rotated box)
            double maxDim = (std::max)(m_width, m_depth) * 1.5; // Rough padding for rotation
            minPoint = WorldPoint{ m_position.X - maxDim, m_position.Y - maxDim };
            maxPoint = WorldPoint{ m_position.X + maxDim, m_position.Y + maxDim };
        }

        // Helpers
        std::vector<WorldPoint> GetContour() const
        {
            std::vector<WorldPoint> points;
            
            if (m_shape == ColumnShape::Rectangular)
            {
                double hw = m_width / 2.0;
                double hd = m_depth / 2.0;
                
                points.push_back(TransformToWorld(WorldPoint{ -hw, -hd }));
                points.push_back(TransformToWorld(WorldPoint{ hw, -hd }));
                points.push_back(TransformToWorld(WorldPoint{ hw, hd }));
                points.push_back(TransformToWorld(WorldPoint{ -hw, hd }));
            }
            else
            {
                // Discretize circle
                int segments = 24;
                double radius = m_width / 2.0;
                for (int i = 0; i < segments; ++i)
                {
                    double angle = (2.0 * 3.14159265359 * i) / segments;
                    points.push_back(TransformToWorld(WorldPoint{
                        radius * std::cos(angle),
                        radius * std::sin(angle)
                    }));
                }
            }
            return points;
        }

    private:
        WorldPoint TransformToLocal(const WorldPoint& p) const
        {
            // Translate to origin
            double dx = p.X - m_position.X;
            double dy = p.Y - m_position.Y;

            // Rotate back (-angle)
            double rad = -m_rotation * 3.14159265359 / 180.0;
            double c = std::cos(rad);
            double s = std::sin(rad);

            return WorldPoint{
                dx * c - dy * s,
                dx * s + dy * c
            };
        }

        WorldPoint TransformToWorld(const WorldPoint& local) const
        {
            double rad = m_rotation * 3.14159265359 / 180.0;
            double c = std::cos(rad);
            double s = std::sin(rad);

            double wx = local.X * c - local.Y * s;
            double wy = local.X * s + local.Y * c;

            return WorldPoint{ wx + m_position.X, wy + m_position.Y };
        }

        WorldPoint m_position{ 0, 0 };
        ColumnShape m_shape{ ColumnShape::Rectangular };
        double m_width{ 400.0 };
        double m_depth{ 400.0 };
        double m_rotation{ 0.0 };
        double m_height{ 3000.0 };
        double m_baseOffset{ 0.0 };
    };

    // ============================================================================
    // БАЛКИ (R6.5)
    // ============================================================================

    class Beam : public Element
    {
    public:
        Beam() : Element()
        {
            m_name = L"Балка";
        }

        std::wstring GetTypeName() const override { return L"Beam"; }

        // Геометрия
        void SetStartPoint(const WorldPoint& p) { m_start = p; }
        const WorldPoint& GetStartPoint() const { return m_start; }

        void SetEndPoint(const WorldPoint& p) { m_end = p; }
        const WorldPoint& GetEndPoint() const { return m_end; }

        void SetWidth(double w) { m_width = w; }
        double GetWidth() const { return m_width; }

        void SetHeight(double h) { m_height = h; } // Высота сечения
        double GetHeight() const { return m_height; }
        
        void SetLevelOffset(double offset) { m_levelOffset = offset; }
        double GetLevelOffset() const { return m_levelOffset; }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Simple distance to line segment check
            double dist = DistancePointToSegment(point, m_start, m_end);
            return dist <= (m_width / 2.0 + tolerance);
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            double padding = m_width / 2.0;
            minPoint.X = (std::min)(m_start.X, m_end.X) - padding;
            minPoint.Y = (std::min)(m_start.Y, m_end.Y) - padding;
            maxPoint.X = (std::max)(m_start.X, m_end.X) + padding;
            maxPoint.Y = (std::max)(m_start.Y, m_end.Y) + padding;
        }

    private:
        static double DistancePointToSegment(const WorldPoint& p, const WorldPoint& a, const WorldPoint& b)
        {
            double l2 = (b.X - a.X) * (b.X - a.X) + (b.Y - a.Y) * (b.Y - a.Y);
            if (l2 == 0.0) return std::sqrt((p.X - a.X) * (p.X - a.X) + (p.Y - a.Y) * (p.Y - a.Y));
            double t = ((p.X - a.X) * (b.X - a.X) + (p.Y - a.Y) * (b.Y - a.Y)) / l2;
            t = (std::max)(0.0, (std::min)(1.0, t));
            WorldPoint projection = { a.X + t * (b.X - a.X), a.Y + t * (b.Y - a.Y) };
            return std::sqrt((p.X - projection.X) * (p.X - projection.X) + (p.Y - projection.Y) * (p.Y - projection.Y));
        }

        WorldPoint m_start{ 0, 0 };
        WorldPoint m_end{ 0, 0 };
        double m_width{ 200.0 };
        double m_height{ 400.0 };
        double m_levelOffset{ 3000.0 }; // Обычно под потолком
    };

    // ============================================================================
    // ПЕРЕКРЫТИЯ (R6.2)
    // ============================================================================

    class Slab : public Element
    {
    public:
        Slab() : Element()
        {
            m_name = L"Перекрытие";
        }

        std::wstring GetTypeName() const override { return L"Slab"; }

        void SetContour(const std::vector<WorldPoint>& points)
        {
             m_contour = points;
             UpdateBounds();
        }
        const std::vector<WorldPoint>& GetContour() const { return m_contour; }

        void SetThickness(double t) { m_thickness = t; }
        double GetThickness() const { return m_thickness; }

        void SetLevelOffset(double offset) { m_levelOffset = offset; }
        double GetLevelOffset() const { return m_levelOffset; }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            // Point in polygon test
            // Ray casting algorithm
            // Tolerance is hard for general polygon, but we can check if inside or close to edge.
            // Simplified: classic "is inside"
            
            // First check bbox with tolerance
            if (point.X < m_min.X - tolerance || point.X > m_max.X + tolerance ||
                point.Y < m_min.Y - tolerance || point.Y > m_max.Y + tolerance)
                return false;

            bool inside = false;
            size_t n = m_contour.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++)
            {
                if (((m_contour[i].Y > point.Y) != (m_contour[j].Y > point.Y)) &&
                    (point.X < (m_contour[j].X - m_contour[i].X) * (point.Y - m_contour[i].Y) /
                    (m_contour[j].Y - m_contour[i].Y) + m_contour[i].X))
                {
                    inside = !inside;
                }
            }
            if (inside) return true;

            // Check distance to edges
            // ... (todo if needed)
            
            return false;
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            minPoint = m_min;
            maxPoint = m_max;
        }
        
        double GetArea() const
        {
            double area = 0.0;
            size_t n = m_contour.size();
            for (size_t i = 0; i < n; ++i)
            {
                size_t j = (i + 1) % n;
                area += (m_contour[i].X * m_contour[j].Y) - (m_contour[j].X * m_contour[i].Y);
            }
            return std::abs(area) * 0.5;
        }

    private:
        void UpdateBounds()
        {
            if (m_contour.empty())
            {
                m_min = { 0, 0 };
                m_max = { 0, 0 };
                return;
            }

            double minX = m_contour[0].X;
            double minY = m_contour[0].Y;
            double maxX = minX;
            double maxY = minY;

            for (const auto& p : m_contour)
            {
                if (p.X < minX) minX = p.X;
                if (p.Y < minY) minY = p.Y;
                if (p.X > maxX) maxX = p.X;
                if (p.Y > maxY) maxY = p.Y;
            }

            m_min = { minX, minY };
            m_max = { maxX, maxY };
        }

        std::vector<WorldPoint> m_contour;
        double m_thickness{ 200.0 };
        double m_levelOffset{ 0.0 };
        
        WorldPoint m_min{ 0, 0 };
        WorldPoint m_max{ 0, 0 };
    };
}
