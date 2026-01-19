#pragma once

#define _USE_MATH_DEFINES
#include "pch.h"
#include "Models.h"
#include "Camera.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <optional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace winrt::estimate1
{
    // =====================================================
    // R2 — СИСТЕМА СОЕДИНЕНИЙ СТЕН С ДОРИСОВКОЙ УГЛОВ
    // =====================================================

    // Тип соединения стен
    enum class JoinType
    {
        None,           // Нет соединения
        LJoin,          // L-соединение (угол, две стены встречаются)
        TJoin,          // T-соединение (одна стена упирается в другую)
        XJoin,          // X-соединение (пересечение)
        Collinear       // Колинеарное (стены на одной оси)
    };

    // Тип стыка угла
    enum class JoinStyle
    {
        Miter,          // Митра — скос под углом (для одинаковых толщин)
        Butt,           // Встык — одна стена перекрывает другую
        Bevel           // Скос — произвольный угол
    };

    // =====================================================
    // СТРУКТУРЫ ДЛЯ ГЕОМЕТРИИ СОЕДИНЕНИЙ
    // =====================================================

    // Грань стены (для расчёта пересечений)
    struct WallEdge
    {
        WorldPoint Start;
        WorldPoint End;
        bool IsExterior{ true };    // Внешняя (true) или внутренняя (false) грань
    };

    // Контур стены с учётом соединений
    struct WallContour
    {
        std::vector<WorldPoint> Points;     // Вершины контура (по часовой стрелке)
        bool IsClosed{ true };
    };

    // Информация о соединении
    struct JoinInfo
    {
        JoinType Type{ JoinType::None };
        JoinStyle Style{ JoinStyle::Miter };
        uint64_t WallId1{ 0 };
        uint64_t WallId2{ 0 };
        bool Wall1AtStart{ true };          // Соединение в начале (true) или конце (false) Wall1
        bool Wall2AtStart{ true };          // Соединение в начале (true) или конце (false) Wall2
        WorldPoint JoinPoint{ 0, 0 };       // Точка пересечения осей
        double Angle{ 0.0 };                // Угол между стенами (радианы)

        // Вычисленные точки угла
        std::vector<WorldPoint> CornerPoints;   // Точки для отрисовки угла

        bool IsValid() const { return Type != JoinType::None && WallId1 != 0 && WallId2 != 0; }
    };

    // Настройки соединений
    struct JoinSettings
    {
        bool AutoJoinEnabled{ true };           // Автоматическое соединение
        double JoinTolerance{ 50.0 };           // Радиус поиска соединений (мм)
        JoinStyle DefaultStyle{ JoinStyle::Miter };
        bool ShowJoinPreview{ true };           // Показывать превью соединения
        bool ExtendToMeet{ true };              // Удлинять стены до пересечения
    };

    // =====================================================
    // УТИЛИТЫ ГЕОМЕТРИИ
    // =====================================================

    class JoinGeometry
    {
    public:
        // Пересечение двух линий (бесконечных)
        static bool LineIntersection(
            const WorldPoint& p1, const WorldPoint& p2,
            const WorldPoint& p3, const WorldPoint& p4,
            WorldPoint& intersection,
            double& t1,     // Параметр на первой линии (0-1 = на отрезке)
            double& t2)     // Параметр на второй линии
        {
            double x1 = p1.X, y1 = p1.Y;
            double x2 = p2.X, y2 = p2.Y;
            double x3 = p3.X, y3 = p3.Y;
            double x4 = p4.X, y4 = p4.Y;

            double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

            if (std::abs(denom) < 1e-10)
                return false;   // Параллельные линии

            t1 = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
            t2 = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

            intersection.X = x1 + t1 * (x2 - x1);
            intersection.Y = y1 + t1 * (y2 - y1);

            return true;
        }

        // Упрощённая версия
        static bool LineIntersection(
            const WorldPoint& p1, const WorldPoint& p2,
            const WorldPoint& p3, const WorldPoint& p4,
            WorldPoint& intersection)
        {
            double t1, t2;
            return LineIntersection(p1, p2, p3, p4, intersection, t1, t2);
        }

        // Угол между двумя векторами (в радианах, 0 до PI)
        static double AngleBetween(const WorldPoint& dir1, const WorldPoint& dir2)
        {
            double dot = dir1.X * dir2.X + dir1.Y * dir2.Y;
            double mag1 = std::sqrt(dir1.X * dir1.X + dir1.Y * dir1.Y);
            double mag2 = std::sqrt(dir2.X * dir2.X + dir2.Y * dir2.Y);

            if (mag1 < 1e-10 || mag2 < 1e-10)
                return 0.0;

            double cosAngle = std::clamp(dot / (mag1 * mag2), -1.0, 1.0);
            return std::acos(cosAngle);
        }

        // Подписанный угол от dir1 к dir2 (против часовой стрелки положительный)
        static double SignedAngle(const WorldPoint& dir1, const WorldPoint& dir2)
        {
            double angle = std::atan2(dir2.Y, dir2.X) - std::atan2(dir1.Y, dir1.X);
            if (angle > M_PI) angle -= 2 * M_PI;
            if (angle < -M_PI) angle += 2 * M_PI;
            return angle;
        }

        // Проверка, параллельны ли два вектора
        static bool AreParallel(const WorldPoint& dir1, const WorldPoint& dir2, double tolerance = 0.01)
        {
            double cross = dir1.X * dir2.Y - dir1.Y * dir2.X;
            return std::abs(cross) < tolerance;
        }

        // Проверка, колинеарны ли два вектора (параллельны и сонаправлены или противоположны)
        static bool AreCollinear(const WorldPoint& dir1, const WorldPoint& dir2, double tolerance = 0.01)
        {
            if (!AreParallel(dir1, dir2, tolerance))
                return false;
            double dot = dir1.X * dir2.X + dir1.Y * dir2.Y;
            return std::abs(std::abs(dot) - 1.0) < tolerance;
        }

        // Расстояние от точки до бесконечной линии
        static double DistanceToLine(const WorldPoint& point, const WorldPoint& lineStart, const WorldPoint& lineEnd)
        {
            double dx = lineEnd.X - lineStart.X;
            double dy = lineEnd.Y - lineStart.Y;
            double len = std::sqrt(dx * dx + dy * dy);

            if (len < 1e-10)
                return point.Distance(lineStart);

            return std::abs((lineEnd.Y - lineStart.Y) * point.X -
                           (lineEnd.X - lineStart.X) * point.Y +
                           lineEnd.X * lineStart.Y -
                           lineEnd.Y * lineStart.X) / len;
        }

        // Смещённая линия (параллельный перенос)
        static void OffsetLine(
            const WorldPoint& start, const WorldPoint& end,
            double offset,
            WorldPoint& newStart, WorldPoint& newEnd)
        {
            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double len = std::sqrt(dx * dx + dy * dy);

            if (len < 1e-10)
            {
                newStart = start;
                newEnd = end;
                return;
            }

            // Перпендикуляр (влево)
            double perpX = -dy / len;
            double perpY = dx / len;

            newStart.X = start.X + perpX * offset;
            newStart.Y = start.Y + perpY * offset;
            newEnd.X = end.X + perpX * offset;
            newEnd.Y = end.Y + perpY * offset;
        }
    };

    // =====================================================
    // ГЛАВНЫЙ КЛАСС СИСТЕМЫ СОЕДИНЕНИЙ
    // =====================================================

    class WallJoinSystem
    {
    public:
        WallJoinSystem() = default;

        // Настройки
        void SetSettings(const JoinSettings& settings) { m_settings = settings; }
        JoinSettings& GetSettings() { return m_settings; }
        const JoinSettings& GetSettings() const { return m_settings; }

        // =====================================================
        // R2.1 — ОБНАРУЖЕНИЕ ТИПОВ СОЕДИНЕНИЙ
        // =====================================================

        // Найти все соединения для стены
        std::vector<JoinInfo> FindJoins(
            const Wall& wall,
            const std::vector<std::unique_ptr<Wall>>& allWalls) const
        {
            std::vector<JoinInfo> results;

            for (const auto& other : allWalls)
            {
                if (!other || other->GetId() == wall.GetId())
                    continue;

                // Проверяем соединение в начале wall
                if (wall.IsJoinAllowedAtStart())
                {
                    auto join = DetectJoinType(wall, true, *other);
                    if (join.IsValid())
                        results.push_back(join);
                }

                // Проверяем соединение в конце wall
                if (wall.IsJoinAllowedAtEnd())
                {
                    auto join = DetectJoinType(wall, false, *other);
                    if (join.IsValid())
                        results.push_back(join);
                }
            }

            return results;
        }

        // Определить тип соединения между двумя стенами
        JoinInfo DetectJoinType(const Wall& wall1, bool wall1AtStart, const Wall& wall2) const
        {
            JoinInfo result;
            result.WallId1 = wall1.GetId();
            result.WallId2 = wall2.GetId();
            result.Wall1AtStart = wall1AtStart;

            WorldPoint p1 = wall1AtStart ? wall1.GetStartPoint() : wall1.GetEndPoint();
            WorldPoint dir1 = wall1.GetDirection();
            if (!wall1AtStart) dir1 = WorldPoint(-dir1.X, -dir1.Y);   // Направление от конца наружу

            WorldPoint start2 = wall2.GetStartPoint();
            WorldPoint end2 = wall2.GetEndPoint();
            WorldPoint dir2 = wall2.GetDirection();

            double tolerance = m_settings.JoinTolerance;

            // Проверка L-join (концы близки)
            if (wall2.IsJoinAllowedAtStart() && p1.Distance(start2) < tolerance)
            {
                result.Type = JoinType::LJoin;
                result.Wall2AtStart = true;
                result.JoinPoint = WorldPoint((p1.X + start2.X) / 2, (p1.Y + start2.Y) / 2);
                result.Angle = JoinGeometry::AngleBetween(dir1, WorldPoint(-dir2.X, -dir2.Y));
                CalculateMiterCorner(wall1, wall1AtStart, wall2, true, result);
                return result;
            }

            if (wall2.IsJoinAllowedAtEnd() && p1.Distance(end2) < tolerance)
            {
                result.Type = JoinType::LJoin;
                result.Wall2AtStart = false;
                result.JoinPoint = WorldPoint((p1.X + end2.X) / 2, (p1.Y + end2.Y) / 2);
                result.Angle = JoinGeometry::AngleBetween(dir1, dir2);
                CalculateMiterCorner(wall1, wall1AtStart, wall2, false, result);
                return result;
            }

            // Проверка T-join (конец wall1 на теле wall2)
            double distToWall2 = DistancePointToSegment(p1, start2, end2);
            if (distToWall2 < tolerance)
            {
                // Убедимся что точка не у концов wall2
                double distToStart2 = p1.Distance(start2);
                double distToEnd2 = p1.Distance(end2);
                double minEndDist = wall2.GetThickness();

                if (distToStart2 > minEndDist && distToEnd2 > minEndDist)
                {
                    result.Type = JoinType::TJoin;
                    result.Wall2AtStart = false;    // T-join не привязан к концам
                    result.JoinPoint = ProjectPointOnLine(p1, start2, end2);
                    result.Angle = JoinGeometry::AngleBetween(dir1, wall2.GetPerpendicular());
                    CalculateTJoinGeometry(wall1, wall1AtStart, wall2, result);
                    return result;
                }
            }

            // Проверка колинеарности
            if (JoinGeometry::AreCollinear(dir1, dir2))
            {
                double distToLine = JoinGeometry::DistanceToLine(p1, start2, end2);
                if (distToLine < tolerance)
                {
                    result.Type = JoinType::Collinear;
                    result.Angle = 0.0;
                    // Найти ближайший конец wall2
                    if (p1.Distance(start2) < p1.Distance(end2))
                    {
                        result.Wall2AtStart = true;
                        result.JoinPoint = start2;
                    }
                    else
                    {
                        result.Wall2AtStart = false;
                        result.JoinPoint = end2;
                    }
                    return result;
                }
            }

            // Проверка X-join (пересечение в середине обеих стен)
            WorldPoint intersection;
            double t1, t2;
            if (JoinGeometry::LineIntersection(
                    wall1.GetStartPoint(), wall1.GetEndPoint(),
                    start2, end2,
                    intersection, t1, t2))
            {
                // Пересечение внутри обоих отрезков
                if (t1 > 0.1 && t1 < 0.9 && t2 > 0.1 && t2 < 0.9)
                {
                    result.Type = JoinType::XJoin;
                    result.JoinPoint = intersection;
                    result.Angle = JoinGeometry::AngleBetween(dir1, dir2);
                    return result;
                }
            }

            return result;  // None
        }

        // =====================================================
        // R2.3 — РАСЧЁТ ГЕОМЕТРИИ УГЛОВ
        // =====================================================

        // Рассчитать контур стены с учётом всех соединений
        WallContour CalculateWallContour(
            const Wall& wall,
            const std::vector<JoinInfo>& joins) const
        {
            WallContour contour;

            // Базовые угловые точки (без соединений)
            WorldPoint p1, p2, p3, p4;
            wall.GetCornerPoints(p1, p2, p3, p4);

            // Найти соединения в начале и конце
            const JoinInfo* startJoin = nullptr;
            const JoinInfo* endJoin = nullptr;

            for (const auto& join : joins)
            {
                if (join.WallId1 == wall.GetId())
                {
                    if (join.Wall1AtStart)
                        startJoin = &join;
                    else
                        endJoin = &join;
                }
                else if (join.WallId2 == wall.GetId())
                {
                    if (join.Wall2AtStart)
                        startJoin = &join;
                    else
                        endJoin = &join;
                }
            }

            // Начальные точки
            if (startJoin && startJoin->Type == JoinType::LJoin && !startJoin->CornerPoints.empty())
            {
                // Использовать вычисленные точки угла
                for (const auto& pt : startJoin->CornerPoints)
                {
                    contour.Points.push_back(pt);
                }
            }
            else
            {
                contour.Points.push_back(p1);
                contour.Points.push_back(p2);
            }

            // Конечные точки
            if (endJoin && endJoin->Type == JoinType::LJoin && !endJoin->CornerPoints.empty())
            {
                for (auto it = endJoin->CornerPoints.rbegin(); it != endJoin->CornerPoints.rend(); ++it)
                {
                    contour.Points.push_back(*it);
                }
            }
            else
            {
                contour.Points.push_back(p3);
                contour.Points.push_back(p4);
            }

            return contour;
        }

        // =====================================================
        // R2.4 — ПРЕВЬЮ СОЕДИНЕНИЯ ПРИ РИСОВАНИИ
        // =====================================================

        // Найти потенциальное соединение для превью (при рисовании стены)
        std::optional<JoinInfo> FindPreviewJoin(
            const WorldPoint& wallStart,
            const WorldPoint& wallEnd,
            double thickness,
            const std::vector<std::unique_ptr<Wall>>& allWalls) const
        {
            // Создаём временную стену для проверки
            Wall tempWall(wallStart, wallEnd, thickness);

            // Ищем соединения
            auto joins = FindJoins(tempWall, allWalls);

            // Возвращаем первое найденное соединение в конце (там где рисуем)
            for (const auto& join : joins)
            {
                if (!join.Wall1AtStart)     // Соединение в конце рисуемой стены
                    return join;
            }

            // Или соединение в начале
            for (const auto& join : joins)
            {
                if (join.Wall1AtStart)
                    return join;
            }

            return std::nullopt;
        }

        // =====================================================
        // R2.5 — ПРИМЕНЕНИЕ СОЕДИНЕНИЙ
        // =====================================================

        // Применить соединение (обрезать/удлинить стены)
        void ApplyJoin(Wall& wall1, Wall& wall2, const JoinInfo& join)
        {
            if (!join.IsValid())
                return;

            switch (join.Type)
            {
            case JoinType::LJoin:
                ApplyLJoin(wall1, wall2, join);
                break;
            case JoinType::TJoin:
                ApplyTJoin(wall1, wall2, join);
                break;
            case JoinType::Collinear:
                // Колинеарные стены можно объединить (опционально)
                break;
            case JoinType::XJoin:
                // X-соединения обычно не модифицируют геометрию
                break;
            default:
                break;
            }
        }

        // Обработать новую стену (найти и применить все соединения)
        void ProcessNewWall(Wall& newWall, std::vector<std::unique_ptr<Wall>>& allWalls)
        {
            if (!m_settings.AutoJoinEnabled)
                return;

            auto joins = FindJoins(newWall, allWalls);

            for (auto& join : joins)
            {
                // Найти вторую стену
                Wall* otherWall = nullptr;
                for (auto& w : allWalls)
                {
                    if (w && w->GetId() == join.WallId2)
                    {
                        otherWall = w.get();
                        break;
                    }
                }

                if (otherWall)
                {
                    ApplyJoin(newWall, *otherWall, join);
                }
            }
        }

        // =====================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =====================================================

    private:
        JoinSettings m_settings;

        // Расстояние от точки до отрезка
        static double DistancePointToSegment(const WorldPoint& point, const WorldPoint& segStart, const WorldPoint& segEnd)
        {
            double dx = segEnd.X - segStart.X;
            double dy = segEnd.Y - segStart.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 1e-10)
                return point.Distance(segStart);

            double t = std::clamp(
                ((point.X - segStart.X) * dx + (point.Y - segStart.Y) * dy) / lengthSq,
                0.0, 1.0);

            WorldPoint closest(segStart.X + t * dx, segStart.Y + t * dy);
            return point.Distance(closest);
        }

        // Проекция точки на линию
        static WorldPoint ProjectPointOnLine(const WorldPoint& point, const WorldPoint& lineStart, const WorldPoint& lineEnd)
        {
            double dx = lineEnd.X - lineStart.X;
            double dy = lineEnd.Y - lineStart.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 1e-10)
                return lineStart;

            double t = ((point.X - lineStart.X) * dx + (point.Y - lineStart.Y) * dy) / lengthSq;

            return WorldPoint(lineStart.X + t * dx, lineStart.Y + t * dy);
        }

        // =====================================================
        // R2.3 — РАСЧЁТ МИТРА-УГЛА (L-JOIN)
        // =====================================================

        void CalculateMiterCorner(
            const Wall& wall1, bool wall1AtStart,
            const Wall& wall2, bool wall2AtStart,
            JoinInfo& join) const
        {
            join.CornerPoints.clear();
            join.Style = m_settings.DefaultStyle;

            // Получаем направления стен (от точки соединения наружу)
            WorldPoint dir1 = wall1.GetDirection();
            if (wall1AtStart) dir1 = WorldPoint(-dir1.X, -dir1.Y);

            WorldPoint dir2 = wall2.GetDirection();
            if (wall2AtStart) dir2 = WorldPoint(-dir2.X, -dir2.Y);

            // Перпендикуляры
            WorldPoint perp1(-dir1.Y, dir1.X);
            WorldPoint perp2(-dir2.Y, dir2.X);

            double half1 = wall1.GetThickness() / 2.0;
            double half2 = wall2.GetThickness() / 2.0;

            // Получаем грани каждой стены
            // Wall1: внешняя и внутренняя грани
            WorldPoint w1_ext_start, w1_ext_end;
            WorldPoint w1_int_start, w1_int_end;
            GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

            // Wall2: внешняя и внутренняя грани
            WorldPoint w2_ext_start, w2_ext_end;
            WorldPoint w2_int_start, w2_int_end;
            GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

            // Пересечения граней для митра-угла
            WorldPoint corner1, corner2;    // Две точки угла

            // Внешняя грань wall1 с внешней гранью wall2
            if (JoinGeometry::LineIntersection(w1_ext_start, w1_ext_end, w2_ext_start, w2_ext_end, corner1))
            {
                join.CornerPoints.push_back(corner1);
            }

            // Внутренняя грань wall1 с внутренней гранью wall2
            if (JoinGeometry::LineIntersection(w1_int_start, w1_int_end, w2_int_start, w2_int_end, corner2))
            {
                join.CornerPoints.push_back(corner2);
            }
        }

        // Получить внешнюю и внутреннюю грани стены
        void GetWallEdges(
            const Wall& wall,
            WorldPoint& extStart, WorldPoint& extEnd,
            WorldPoint& intStart, WorldPoint& intEnd) const
        {
            WorldPoint perp = wall.GetPerpendicular();
            double half = wall.GetThickness() / 2.0;

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            // Внешняя грань (по положительному перпендикуляру)
            extStart = WorldPoint(start.X + perp.X * half, start.Y + perp.Y * half);
            extEnd = WorldPoint(end.X + perp.X * half, end.Y + perp.Y * half);

            // Внутренняя грань (по отрицательному перпендикуляру)
            intStart = WorldPoint(start.X - perp.X * half, start.Y - perp.Y * half);
            intEnd = WorldPoint(end.X - perp.X * half, end.Y - perp.Y * half);
        }

        // Расчёт геометрии T-соединения
        void CalculateTJoinGeometry(
            const Wall& wall1, bool wall1AtStart,
            const Wall& wall2,
            JoinInfo& join) const
        {
            join.CornerPoints.clear();

            // T-join: wall1 упирается в wall2
            // Обрезаем wall1 по грани wall2

            WorldPoint dir1 = wall1.GetDirection();
            if (!wall1AtStart) dir1 = WorldPoint(-dir1.X, -dir1.Y);

            WorldPoint perp2 = wall2.GetPerpendicular();
            double half2 = wall2.GetThickness() / 2.0;

            // Определяем с какой стороны wall2 подходит wall1
            WorldPoint p1 = wall1AtStart ? wall1.GetStartPoint() : wall1.GetEndPoint();
            WorldPoint mid2((wall2.GetStartPoint().X + wall2.GetEndPoint().X) / 2,
                           (wall2.GetStartPoint().Y + wall2.GetEndPoint().Y) / 2);

            WorldPoint toP1(p1.X - mid2.X, p1.Y - mid2.Y);
            double dot = toP1.X * perp2.X + toP1.Y * perp2.Y;

            // Грань wall2, к которой примыкает wall1
            WorldPoint edgeStart, edgeEnd;
            if (dot > 0)
            {
                // Примыкание к внешней грани
                edgeStart = WorldPoint(wall2.GetStartPoint().X + perp2.X * half2,
                                       wall2.GetStartPoint().Y + perp2.Y * half2);
                edgeEnd = WorldPoint(wall2.GetEndPoint().X + perp2.X * half2,
                                     wall2.GetEndPoint().Y + perp2.Y * half2);
            }
            else
            {
                // Примыкание к внутренней грани
                edgeStart = WorldPoint(wall2.GetStartPoint().X - perp2.X * half2,
                                       wall2.GetStartPoint().Y - perp2.Y * half2);
                edgeEnd = WorldPoint(wall2.GetEndPoint().X - perp2.X * half2,
                                     wall2.GetEndPoint().Y - perp2.Y * half2);
            }

            // Пересечение осей wall1 с гранью wall2
            WorldPoint intersection;
            if (JoinGeometry::LineIntersection(
                    wall1.GetStartPoint(), wall1.GetEndPoint(),
                    edgeStart, edgeEnd,
                    intersection))
            {
                join.JoinPoint = intersection;
            }
        }

        // Применить L-соединение
        void ApplyLJoin(Wall& wall1, Wall& wall2, const JoinInfo& join)
        {
            // Находим точку пересечения осей
            WorldPoint intersection;
            if (!JoinGeometry::LineIntersection(
                    wall1.GetStartPoint(), wall1.GetEndPoint(),
                    wall2.GetStartPoint(), wall2.GetEndPoint(),
                    intersection))
            {
                intersection = join.JoinPoint;
            }

            // Перемещаем концы стен к точке пересечения
            if (join.Wall1AtStart)
                wall1.SetStartPoint(intersection);
            else
                wall1.SetEndPoint(intersection);

            if (join.Wall2AtStart)
                wall2.SetStartPoint(intersection);
            else
                wall2.SetEndPoint(intersection);
        }

        // Применить T-соединение
        void ApplyTJoin(Wall& wall1, Wall& wall2, const JoinInfo& join)
        {
            // Находим точку пересечения оси wall1 с осью wall2
            WorldPoint intersection;
            if (!JoinGeometry::LineIntersection(
                    wall1.GetStartPoint(), wall1.GetEndPoint(),
                    wall2.GetStartPoint(), wall2.GetEndPoint(),
                    intersection))
            {
                intersection = join.JoinPoint;
            }

            // Обрезаем только wall1
            if (join.Wall1AtStart)
                wall1.SetStartPoint(intersection);
            else
                wall1.SetEndPoint(intersection);

            // wall2 не изменяется при T-соединении
        }
    };

    // =====================================================
    // РЕНДЕРЕР СОЕДИНЕНИЙ
    // =====================================================

    class WallJoinRenderer
    {
    public:
        WallJoinRenderer() = default;

        // Отрисовка превью соединения
        void DrawJoinPreview(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const JoinInfo& join,
            bool isValid = true)
        {
            if (!join.IsValid())
                return;

            Windows::UI::Color color = isValid
                ? Windows::UI::ColorHelper::FromArgb(180, 0, 200, 100)
                : Windows::UI::ColorHelper::FromArgb(180, 200, 100, 0);

            // Точка соединения
            ScreenPoint joinScreen = camera.WorldToScreen(join.JoinPoint);
            float radius = 8.0f;

            session.DrawCircle(
                Windows::Foundation::Numerics::float2(joinScreen.X, joinScreen.Y),
                radius,
                color, 2.0f);

            // Крест в центре
            float crossSize = 4.0f;
            session.DrawLine(
                Windows::Foundation::Numerics::float2(joinScreen.X - crossSize, joinScreen.Y),
                Windows::Foundation::Numerics::float2(joinScreen.X + crossSize, joinScreen.Y),
                color, 1.5f);
            session.DrawLine(
                Windows::Foundation::Numerics::float2(joinScreen.X, joinScreen.Y - crossSize),
                Windows::Foundation::Numerics::float2(joinScreen.X, joinScreen.Y + crossSize),
                color, 1.5f);

            // Точки митра-угла
            if (!join.CornerPoints.empty())
            {
                for (const auto& pt : join.CornerPoints)
                {
                    ScreenPoint screen = camera.WorldToScreen(pt);
                    session.FillCircle(
                        Windows::Foundation::Numerics::float2(screen.X, screen.Y),
                        4.0f,
                        Windows::UI::ColorHelper::FromArgb(200, 255, 150, 0));
                }
            }
        }

        // Отрисовка индикатора угла
        void DrawAngleIndicator(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const JoinInfo& join)
        {
            if (join.Type == JoinType::None)
                return;

            ScreenPoint center = camera.WorldToScreen(join.JoinPoint);

            // Угол в градусах
            double angleDeg = join.Angle * 180.0 / M_PI;

            // Форматируем текст
            wchar_t angleText[32];
            swprintf_s(angleText, L"%.1f°", angleDeg);

            // Позиция текста
            float textX = center.X + 15.0f;
            float textY = center.Y - 20.0f;

            // Фон для текста
            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(11);

            auto textLayout = Microsoft::Graphics::Canvas::Text::CanvasTextLayout(
                session.Device(),
                angleText,
                textFormat,
                100, 30);

            float textWidth = textLayout.LayoutBounds().Width;
            float textHeight = textLayout.LayoutBounds().Height;

            session.FillRoundedRectangle(
                Windows::Foundation::Rect(textX - 3, textY - 2, textWidth + 6, textHeight + 4),
                3.0f, 3.0f,
                Windows::UI::ColorHelper::FromArgb(200, 255, 255, 200));

            Windows::UI::Color textColor = (std::abs(angleDeg - 90.0) < 1.0)
                ? Windows::UI::ColorHelper::FromArgb(255, 0, 150, 0)     // Зелёный для 90°
                : Windows::UI::ColorHelper::FromArgb(255, 200, 100, 0); // Оранжевый для других

            session.DrawText(
                angleText,
                textX,
                textY,
                textColor);
        }

        // Отрисовка типа соединения
        void DrawJoinTypeBadge(
            const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
            const Camera& camera,
            const JoinInfo& join)
        {
            if (!join.IsValid())
                return;

            ScreenPoint pos = camera.WorldToScreen(join.JoinPoint);

            const wchar_t* typeName = L"";
            switch (join.Type)
            {
            case JoinType::LJoin: typeName = L"L"; break;
            case JoinType::TJoin: typeName = L"T"; break;
            case JoinType::XJoin: typeName = L"X"; break;
            case JoinType::Collinear: typeName = L"—"; break;
            default: return;
            }

            // Бэйдж
            float badgeSize = 16.0f;
            session.FillCircle(
                Windows::Foundation::Numerics::float2(pos.X - 20.0f, pos.Y - 20.0f),
                badgeSize / 2,
                Windows::UI::ColorHelper::FromArgb(200, 60, 60, 60));

            auto textFormat = Microsoft::Graphics::Canvas::Text::CanvasTextFormat();
            textFormat.FontSize(10);
            textFormat.HorizontalAlignment(Microsoft::Graphics::Canvas::Text::CanvasHorizontalAlignment::Center);

            session.DrawText(
                typeName,
                pos.X - 24.0f,
                pos.Y - 26.0f,
                Windows::UI::ColorHelper::FromArgb(255, 255, 255, 255),
                textFormat);
        }
    };
}
