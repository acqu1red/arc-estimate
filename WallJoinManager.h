#pragma once

#include "pch.h"
#include "Models.h"
#include "Element.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace winrt::estimate1
{
    // ��� ���������� ����
    enum class WallJoinType
    {
        None,       // ��� ����������
        LJoin,      // L-���������� (����)
        TJoin,      // T-���������� (����������)
        XJoin       // X-���������� (�����������)
    };

    // ��������� ������ ����������
    struct WallJoinResult
    {
        WallJoinType joinType{ WallJoinType::None };
        Wall* otherWall{ nullptr };
        bool atStart{ true };          // ���������� � ������ (true) ��� ����� (false) ������� �����
        bool otherAtStart{ true };     // ���������� � ������ (true) ��� ����� (false) ������ �����
        WorldPoint joinPoint{ 0, 0 };  // ����� ����������
    };

    // �������� ���������� ����
    class WallJoinManager
    {
    public:
        WallJoinManager() = default;

        // ������ ��� ����������� ���������� (� ��)
        void SetJoinTolerance(double tolerance) { m_joinTolerance = tolerance; }
        double GetJoinTolerance() const { return m_joinTolerance; }

        // ����� ���������� ��� �����
        std::vector<WallJoinResult> FindJoins(
            const Wall& wall,
            const std::vector<std::unique_ptr<Wall>>& allWalls)
        {
            std::vector<WallJoinResult> results;
            
            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            for (const auto& other : allWalls)
            {
                if (!other || other->GetId() == wall.GetId())
                    continue;

                // ��������� ���������� � ������ ������� �����
                if (wall.IsJoinAllowedAtStart())
                {
                    auto joinResult = CheckJoinAtPoint(start, true, wall, *other);
                    if (joinResult.joinType != WallJoinType::None)
                    {
                        results.push_back(joinResult);
                    }
                }

                // ��������� ���������� � ����� ������� �����
                if (wall.IsJoinAllowedAtEnd())
                {
                    auto joinResult = CheckJoinAtPoint(end, false, wall, *other);
                    if (joinResult.joinType != WallJoinType::None)
                    {
                        results.push_back(joinResult);
                    }
                }
            }

            return results;
        }

        // ��������� ���������� ���� (�������/���������)
        void ApplyJoin(Wall& wall1, Wall& wall2, const WallJoinResult& join)
        {
            if (join.joinType == WallJoinType::None)
                return;

            switch (join.joinType)
            {
            case WallJoinType::LJoin:
                ApplyLJoin(wall1, wall2, join);
                break;
            case WallJoinType::TJoin:
                ApplyTJoin(wall1, wall2, join);
                break;
            case WallJoinType::XJoin:
                // X-���������� ���� �� ������������
                break;
            default:
                break;
            }
        }

        // ����� � ��������� ���������� ��� ����� �����
        void ProcessNewWall(Wall& newWall, std::vector<std::unique_ptr<Wall>>& allWalls)
        {
            auto joins = FindJoins(newWall, allWalls);
            
            for (auto& join : joins)
            {
                if (join.otherWall)
                {
                    ApplyJoin(newWall, *join.otherWall, join);
                }
            }
        }

        // ��������� ����� ����������� ���� ��������
        static bool LineIntersection(
            const WorldPoint& p1, const WorldPoint& p2,
            const WorldPoint& p3, const WorldPoint& p4,
            WorldPoint& intersection)
        {
            double x1 = p1.X, y1 = p1.Y;
            double x2 = p2.X, y2 = p2.Y;
            double x3 = p3.X, y3 = p3.Y;
            double x4 = p4.X, y4 = p4.Y;

            double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
            
            if (std::abs(denom) < 0.0001)
                return false; // ������������ �����

            double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            
            intersection.X = x1 + t * (x2 - x1);
            intersection.Y = y1 + t * (y2 - y1);
            
            return true;
        }

        // ���������, ��������� �� ����� �� �������
        static bool IsPointOnSegment(const WorldPoint& point, const WorldPoint& segStart, const WorldPoint& segEnd, double tolerance)
        {
            double segLength = segStart.Distance(segEnd);
            if (segLength < 0.001)
                return point.Distance(segStart) <= tolerance;

            double distToStart = point.Distance(segStart);
            double distToEnd = point.Distance(segEnd);
            
            // ����� �� �������, ���� ����� ���������� �� ������ �������� ����� ����� �������
            return std::abs(distToStart + distToEnd - segLength) < tolerance;
        }

    private:
        double m_joinTolerance{ 50.0 }; // ��

        // ��������� ���������� � ���������� �����
        WallJoinResult CheckJoinAtPoint(
            const WorldPoint& point,
            bool atStart,
            const Wall& wall,
            const Wall& other)
        {
            WallJoinResult result;
            result.atStart = atStart;

            WorldPoint otherStart = other.GetStartPoint();
            WorldPoint otherEnd = other.GetEndPoint();

            // ��������� ���������� � ������� ������ ����� (L-join)
            if (other.IsJoinAllowedAtStart() && point.Distance(otherStart) < m_joinTolerance)
            {
                result.joinType = WallJoinType::LJoin;
                result.otherWall = const_cast<Wall*>(&other);
                result.otherAtStart = true;
                result.joinPoint = otherStart;
                return result;
            }

            if (other.IsJoinAllowedAtEnd() && point.Distance(otherEnd) < m_joinTolerance)
            {
                result.joinType = WallJoinType::LJoin;
                result.otherWall = const_cast<Wall*>(&other);
                result.otherAtStart = false;
                result.joinPoint = otherEnd;
                return result;
            }

            // ��������� ���������� � �������� ������ ����� (T-join)
            double distToSegment = DistancePointToSegment(point, otherStart, otherEnd);
            if (distToSegment < m_joinTolerance)
            {
                // ���������, ��� ����� �� ������� ������ � ������
                double distToOtherStart = point.Distance(otherStart);
                double distToOtherEnd = point.Distance(otherEnd);
                double otherLength = other.GetLength();
                
                if (distToOtherStart > other.GetThickness() / 2 && 
                    distToOtherEnd > other.GetThickness() / 2 &&
                    distToOtherStart < otherLength &&
                    distToOtherEnd < otherLength)
                {
                    result.joinType = WallJoinType::TJoin;
                    result.otherWall = const_cast<Wall*>(&other);
                    result.joinPoint = ProjectPointOnSegment(point, otherStart, otherEnd);
                    return result;
                }
            }

            return result;
        }

        // ���������� �� ����� �� �������
        static double DistancePointToSegment(const WorldPoint& point, const WorldPoint& segStart, const WorldPoint& segEnd)
        {
            double dx = segEnd.X - segStart.X;
            double dy = segEnd.Y - segStart.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return point.Distance(segStart);

            double t = std::clamp(
                ((point.X - segStart.X) * dx + (point.Y - segStart.Y) * dy) / lengthSq,
                0.0, 1.0);

            WorldPoint closest(segStart.X + t * dx, segStart.Y + t * dy);
            return point.Distance(closest);
        }

        // �������� ����� �� �������
        static WorldPoint ProjectPointOnSegment(const WorldPoint& point, const WorldPoint& segStart, const WorldPoint& segEnd)
        {
            double dx = segEnd.X - segStart.X;
            double dy = segEnd.Y - segStart.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return segStart;

            double t = std::clamp(
                ((point.X - segStart.X) * dx + (point.Y - segStart.Y) * dy) / lengthSq,
                0.0, 1.0);

            return WorldPoint(segStart.X + t * dx, segStart.Y + t * dy);
        }

        // ��������� L-���������� (����)
        void ApplyLJoin(Wall& wall1, Wall& wall2, const WallJoinResult& join)
        {
            // ������� ����� ����������� ������ �����
            WorldPoint intersection;
            bool found = LineIntersection(
                wall1.GetStartPoint(), wall1.GetEndPoint(),
                wall2.GetStartPoint(), wall2.GetEndPoint(),
                intersection);

            if (!found)
            {
                // ����� ����������� - ���������� ����� ����������
                intersection = join.joinPoint;
            }

            // ��������/���������� ����� �� ����� �����������
            if (join.atStart)
            {
                wall1.SetStartPoint(intersection);
            }
            else
            {
                wall1.SetEndPoint(intersection);
            }

            if (join.otherAtStart)
            {
                wall2.SetStartPoint(intersection);
            }
            else
            {
                wall2.SetEndPoint(intersection);
            }
        }

        // ��������� T-���������� (����������)
        void ApplyTJoin(Wall& wall1, Wall& wall2, const WallJoinResult& join)
        {
            // ������� ����� ����������� ������ �����
            WorldPoint intersection;
            bool found = LineIntersection(
                wall1.GetStartPoint(), wall1.GetEndPoint(),
                wall2.GetStartPoint(), wall2.GetEndPoint(),
                intersection);

            if (!found)
            {
                // ����� ����������� - ���������� ��������
                intersection = join.joinPoint;
            }

            // ��������/���������� ����������� ����� (wall1) �� ��� wall2
            if (join.atStart)
            {
                wall1.SetStartPoint(intersection);
            }
            else
            {
                wall1.SetEndPoint(intersection);
            }

            // wall2 �� ���������� ��� T-����������
        }
    };
}
