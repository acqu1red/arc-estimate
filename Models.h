#pragma once

#include "pch.h"
#include "Camera.h"
#include "Layer.h"
#include "WallType.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

namespace winrt::estimate1
{
    class IdGenerator
    {
    public:
        static uint64_t Next()
        {
            static uint64_t counter = 0;
            return ++counter;
        }
    };

    class Element
    {
    public:
        Element()
            : m_id(IdGenerator::Next())
        {
        }

        virtual ~Element() = default;

        uint64_t GetId() const { return m_id; }

        std::wstring GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        WorkStateNative GetWorkState() const { return m_workState; }
        void SetWorkState(WorkStateNative state) { m_workState = state; }

        bool IsSelected() const { return m_isSelected; }
        void SetSelected(bool selected) { m_isSelected = selected; }

        virtual std::wstring GetTypeName() const { return L"Element"; }
        virtual bool HitTest(const WorldPoint& point, double tolerance) const = 0;
        virtual void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const = 0;

    protected:
        uint64_t m_id;
        std::wstring m_name{ L"" };
        WorkStateNative m_workState{ WorkStateNative::Existing };
        bool m_isSelected{ false };
    };

    enum class LocationLineMode
    {
        WallCenterline,
        CoreCenterline,
        FinishFaceExterior,
        FinishFaceInterior,
        CoreFaceExterior,
        CoreFaceInterior
    };

    // R-WALL: Новый упрощенный режим привязки стен (3 режима вместо 6)
    enum class WallAttachmentMode
    {
        Core,               // Осевая линия (центр ядра стены)
        FinishExterior,     // Чистовая поверхность: наружная
        FinishInterior      // Чистовая поверхность: внутренняя
    };

    class Wall : public Element
    {
    public:
        Wall() = default;

        Wall(const WorldPoint& start, const WorldPoint& end, double thickness = 200.0)
            : m_startPoint(start)
            , m_endPoint(end)
            , m_thickness(thickness)
        {
            m_name = L"�����";
        }

        WorldPoint GetStartPoint() const { return m_startPoint; }
        void SetStartPoint(const WorldPoint& point) { m_startPoint = point; }

        WorldPoint GetEndPoint() const { return m_endPoint; }
        void SetEndPoint(const WorldPoint& point) { m_endPoint = point; }

        double GetThickness() const { return m_thickness; }
        void SetThickness(double thickness)
        {
            m_thickness = std::clamp(thickness, 50.0, 1000.0);
            m_type = nullptr;
        }

        std::shared_ptr<WallType> GetType() const { return m_type; }
        void SetType(std::shared_ptr<WallType> type)
        {
            m_type = std::move(type);
            SyncThicknessFromType();
        }

        // �������� ����� �� ���� (��� ��������� �������)
        void ClearType()
        {
            m_type = nullptr;
        }

        void SyncThicknessFromType()
        {
            if (m_type)
            {
                double t = m_type->GetTotalThickness();
                if (t > 0.0)
                    m_thickness = std::clamp(t, 50.0, 2000.0);
            }
        }

        double GetHeight() const { return m_height; }
        void SetHeight(double height) { m_height = height; }

        LocationLineMode GetLocationLineMode() const { return m_locationLineMode; }
        void SetLocationLineMode(LocationLineMode mode) { m_locationLineMode = mode; }

        bool IsJoinAllowedAtStart() const { return m_allowJoinStart; }
        void SetJoinAllowedAtStart(bool allow) { m_allowJoinStart = allow; }

        bool IsJoinAllowedAtEnd() const { return m_allowJoinEnd; }
        void SetJoinAllowedAtEnd(bool allow) { m_allowJoinEnd = allow; }

        double GetLength() const { return m_startPoint.Distance(m_endPoint); }
        double GetArea() const { return GetLength() * m_height; }
        double GetVolume() const { return GetLength() * m_thickness * m_height; }

        double GetAngle() const
        {
            double dx = m_endPoint.X - m_startPoint.X;
            double dy = m_endPoint.Y - m_startPoint.Y;
            return std::atan2(dy, dx);
        }

        WorldPoint GetDirection() const
        {
            double length = GetLength();
            if (length < 0.001) return WorldPoint(1, 0);

            return WorldPoint(
                (m_endPoint.X - m_startPoint.X) / length,
                (m_endPoint.Y - m_startPoint.Y) / length);
        }

        WorldPoint GetPerpendicular() const
        {
            WorldPoint dir = GetDirection();
            return WorldPoint(-dir.Y, dir.X);
        }

        void GetCornerPoints(WorldPoint& p1, WorldPoint& p2, WorldPoint& p3, WorldPoint& p4) const
        {
            WorldPoint perp = GetPerpendicular();
            double halfThickness = m_thickness / 2.0;

            double coreThickness = m_type ? m_type->GetCoreThickness() : m_thickness;
            if (coreThickness <= 0.0)
                coreThickness = m_thickness;
            coreThickness = (std::min)(coreThickness, m_thickness);
            double finishThickness = (std::max)(0.0, m_thickness - coreThickness);

            double offset = 0.0;
            switch (m_locationLineMode)
            {
            case LocationLineMode::WallCenterline:
            case LocationLineMode::CoreCenterline:
                offset = 0.0;
                break;
            case LocationLineMode::FinishFaceExterior:
                offset = halfThickness;
                break;
            case LocationLineMode::CoreFaceExterior:
                offset = coreThickness / 2.0 + finishThickness;
                break;
            case LocationLineMode::FinishFaceInterior:
                offset = -halfThickness;
                break;
            case LocationLineMode::CoreFaceInterior:
                offset = -(coreThickness / 2.0 + finishThickness);
                break;
            }

            WorldPoint startOffset(m_startPoint.X + perp.X * offset, m_startPoint.Y + perp.Y * offset);
            WorldPoint endOffset(m_endPoint.X + perp.X * offset, m_endPoint.Y + perp.Y * offset);

            p1 = WorldPoint(startOffset.X + perp.X * halfThickness, startOffset.Y + perp.Y * halfThickness);
            p2 = WorldPoint(startOffset.X - perp.X * halfThickness, startOffset.Y - perp.Y * halfThickness);
            p3 = WorldPoint(endOffset.X - perp.X * halfThickness, endOffset.Y - perp.Y * halfThickness);
            p4 = WorldPoint(endOffset.X + perp.X * halfThickness, endOffset.Y + perp.Y * halfThickness);
        }

        std::wstring GetTypeName() const override { return L"Wall"; }

        bool HitTest(const WorldPoint& point, double tolerance) const override
        {
            double dist = DistanceToSegment(point);
            return dist <= (m_thickness / 2.0 + tolerance);
        }

        void GetBounds(WorldPoint& minPoint, WorldPoint& maxPoint) const override
        {
            WorldPoint p1, p2, p3, p4;
            GetCornerPoints(p1, p2, p3, p4);

            minPoint.X = (std::min)((std::min)(p1.X, p2.X), (std::min)(p3.X, p4.X));
            minPoint.Y = (std::min)((std::min)(p1.Y, p2.Y), (std::min)(p3.Y, p4.Y));
            maxPoint.X = (std::max)((std::max)(p1.X, p2.X), (std::max)(p3.X, p4.X));
            maxPoint.Y = (std::max)((std::max)(p1.Y, p2.Y), (std::max)(p3.Y, p4.Y));
        }

    private:
        double DistanceToSegment(const WorldPoint& point) const
        {
            double dx = m_endPoint.X - m_startPoint.X;
            double dy = m_endPoint.Y - m_startPoint.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return point.Distance(m_startPoint);

            double t = ((point.X - m_startPoint.X) * dx + (point.Y - m_startPoint.Y) * dy) / lengthSq;
            t = std::clamp(t, 0.0, 1.0);

            WorldPoint closest(m_startPoint.X + t * dx, m_startPoint.Y + t * dy);
            return point.Distance(closest);
        }

        WorldPoint m_startPoint{ 0, 0 };
        WorldPoint m_endPoint{ 0, 0 };
        double m_thickness{ 200.0 };
        double m_height{ 2700.0 };
        std::shared_ptr<WallType> m_type{};
        LocationLineMode m_locationLineMode{ LocationLineMode::WallCenterline };
        bool m_allowJoinStart{ true };
        bool m_allowJoinEnd{ true };
    };
}
