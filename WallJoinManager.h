#pragma once

#include "pch.h"
#include "Models.h"
#include "Element.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace winrt::estimate1
{
    // Тип соединения стен
    enum class WallJoinType
    {
        None,       // Нет соединения
        LJoin,      // L-соединение (угол)
        TJoin,      // T-соединение (примыкание)
        XJoin       // X-соединение (пересечение)
    };

    // Результат поиска соединения
    struct WallJoinResult
    {
        WallJoinType joinType{ WallJoinType::None };
        Wall* otherWall{ nullptr };
        bool atStart{ true };          // Соединение в начале (true) или конце (false) текущей стены
        bool otherAtStart{ true };     // Соединение в начале (true) или конце (false) другой стены
        WorldPoint joinPoint{ 0, 0 };  // Точка соединения
    };

    // Менеджер соединений стен
    class WallJoinManager
    {
    public:
        WallJoinManager() = default;

        // Допуск для определения соединения (в мм)
        void SetJoinTolerance(double tolerance) { m_joinTolerance = tolerance; }
        double GetJoinTolerance() const { return m_joinTolerance; }

        // Найти соединения для стены
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

                // Проверяем соединение в начале текущей стены
                if (wall.IsJoinAllowedAtStart())
                {
                    auto joinResult = CheckJoinAtPoint(start, true, wall, *other);
                    if (joinResult.joinType != WallJoinType::None)
                    {
                        results.push_back(joinResult);
                    }
                }

                // Проверяем соединение в конце текущей стены
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

        // Выполнить соединение стен (обрезка/продление)
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
                // X-соединения пока не поддерживаем
                break;
            default:
                break;
            }
        }

        // Найти и применить соединения для новой стены
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

        // Вычислить точку пересечения двух отрезков
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
                return false; // Параллельные линии

            double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            
            intersection.X = x1 + t * (x2 - x1);
            intersection.Y = y1 + t * (y2 - y1);
            
            return true;
        }

        // Проверить, находится ли точка на отрезке
        static bool IsPointOnSegment(const WorldPoint& point, const WorldPoint& segStart, const WorldPoint& segEnd, double tolerance)
        {
            double segLength = segStart.Distance(segEnd);
            if (segLength < 0.001)
                return point.Distance(segStart) <= tolerance;

            double distToStart = point.Distance(segStart);
            double distToEnd = point.Distance(segEnd);
            
            // Точка на отрезке, если сумма расстояний до концов примерно равна длине отрезка
            return std::abs(distToStart + distToEnd - segLength) < tolerance;
        }

    private:
        double m_joinTolerance{ 50.0 }; // мм

        // Проверить соединение в конкретной точке
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

            // Проверяем совпадение с концами другой стены (L-join)
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

            // Проверяем примыкание к середине другой стены (T-join)
            double distToSegment = DistancePointToSegment(point, otherStart, otherEnd);
            if (distToSegment < m_joinTolerance)
            {
                // Проверяем, что точка не слишком близко к концам
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

        // Расстояние от точки до отрезка
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

        // Проекция точки на отрезок
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

        // Применить L-соединение (угол)
        void ApplyLJoin(Wall& wall1, Wall& wall2, const WallJoinResult& join)
        {
            // Находим точку пересечения осевых линий
            WorldPoint intersection;
            bool found = LineIntersection(
                wall1.GetStartPoint(), wall1.GetEndPoint(),
                wall2.GetStartPoint(), wall2.GetEndPoint(),
                intersection);

            if (!found)
            {
                // Линии параллельны - используем точку соединения
                intersection = join.joinPoint;
            }

            // Обрезаем/продлеваем стены до точки пересечения
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

        // Применить T-соединение (примыкание)
        void ApplyTJoin(Wall& wall1, Wall& wall2, const WallJoinResult& join)
        {
            // Находим точку пересечения осевых линий
            WorldPoint intersection;
            bool found = LineIntersection(
                wall1.GetStartPoint(), wall1.GetEndPoint(),
                wall2.GetStartPoint(), wall2.GetEndPoint(),
                intersection);

            if (!found)
            {
                // Линии параллельны - используем проекцию
                intersection = join.joinPoint;
            }

            // Обрезаем/продлеваем примыкающую стену (wall1) до оси wall2
            if (join.atStart)
            {
                wall1.SetStartPoint(intersection);
            }
            else
            {
                wall1.SetEndPoint(intersection);
            }

            // wall2 не изменяется при T-соединении
        }
    };
}
