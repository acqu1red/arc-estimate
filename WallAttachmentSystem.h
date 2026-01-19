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
    // НОВАЯ СИСТЕМА ПРИВЯЗКИ И СОЕДИНЕНИЯ СТЕН
    // Wall Attachment System - замена WallJoinSystem
    // =====================================================

    // =====================================================
    // 1. РЕЖИМ ПРИВЯЗКИ СТЕНЫ
    // =====================================================

    // ВАЖНО: enum class WallAttachmentMode определён в Models.h
    // чтобы избежать циклических зависимостей

    // Конвертация строки в enum для сериализации
    inline WallAttachmentMode WallAttachmentModeFromString(const std::wstring& str)
    {
        if (str == L"Core") return WallAttachmentMode::Core;
        if (str == L"FinishExterior") return WallAttachmentMode::FinishExterior;
        if (str == L"FinishInterior") return WallAttachmentMode::FinishInterior;
        return WallAttachmentMode::Core;
    }

    // Конвертация enum в строку для сериализации
    inline std::wstring WallAttachmentModeToString(WallAttachmentMode mode)
    {
        switch (mode)
        {
            case WallAttachmentMode::Core: return L"Core";
            case WallAttachmentMode::FinishExterior: return L"FinishExterior";
            case WallAttachmentMode::FinishInterior: return L"FinishInterior";
        }
        return L"Core";
    }

    // =====================================================
    // 2. СТРУКТУРЫ ДАННЫХ
    // =====================================================

    // Линия привязки стены (смещённая от центральной оси)
    struct AttachmentLine
    {
        WorldPoint Start;
        WorldPoint End;
        WallAttachmentMode Mode;

        AttachmentLine() : Start(0, 0), End(0, 0), Mode(WallAttachmentMode::Core) {}

        AttachmentLine(const WorldPoint& start, const WorldPoint& end, WallAttachmentMode mode)
            : Start(start), End(end), Mode(mode) {}
    };

    // Информация о соединении с учётом привязки
    struct AttachmentJoinInfo
    {
        enum class JoinType
        {
            None,
            LJoin,
            TJoin,
            XJoin,
            Collinear
        };

        enum class ContactType
        {
            Start,
            End,
            Middle
        };

        uint64_t WallId1{ 0 };
        uint64_t WallId2{ 0 };
        bool Wall1AtStart{ true };      // Соединение в начале (true) или конце (false) Wall1
        bool Wall2AtStart{ true };      // Соединение в начале (true) или конце (false) Wall2

        ContactType Wall1Contact{ ContactType::Start };
        ContactType Wall2Contact{ ContactType::Start };

        JoinType Type{ JoinType::None };

        WallAttachmentMode Mode1{ WallAttachmentMode::Core };
        WallAttachmentMode Mode2{ WallAttachmentMode::Core };

        WorldPoint JoinPoint{ 0, 0 };   // Точка пересечения линий привязки
        double Angle{ 0.0 };            // Угол между стенами (радианы)

        // Геометрия угла для рендеринга (полигон из 4-8 точек)
        std::vector<WorldPoint> CornerPolygon;

        bool IsValid() const { return WallId1 != 0 && WallId2 != 0; }
    };

    // =====================================================
    // 3. ГЕОМЕТРИЧЕСКИЕ УТИЛИТЫ
    // =====================================================

    class AttachmentGeometry
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

        // Упрощённая версия без параметров
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

        // Нормализовать вектор
        static WorldPoint Normalize(const WorldPoint& v)
        {
            double len = std::sqrt(v.X * v.X + v.Y * v.Y);
            if (len < 1e-10)
                return WorldPoint(0, 0);
            return WorldPoint(v.X / len, v.Y / len);
        }

        // Расстояние между точками
        static double Distance(const WorldPoint& p1, const WorldPoint& p2)
        {
            double dx = p2.X - p1.X;
            double dy = p2.Y - p1.Y;
            return std::sqrt(dx * dx + dy * dy);
        }

        // Ограничить длину сегмента, сохранив центр
        static void ClampSegment(WorldPoint& a, WorldPoint& b, double maxLength)
        {
            double len = Distance(a, b);
            if (len < 1e-10 || len <= maxLength)
                return;

            WorldPoint mid((a.X + b.X) * 0.5, (a.Y + b.Y) * 0.5);
            double half = maxLength * 0.5;
            WorldPoint dir((b.X - a.X) / len, (b.Y - a.Y) / len);

            a = WorldPoint(mid.X - dir.X * half, mid.Y - dir.Y * half);
            b = WorldPoint(mid.X + dir.X * half, mid.Y + dir.Y * half);
        }

        // Скалярное произведение
        static double Dot(const WorldPoint& a, const WorldPoint& b)
        {
            return a.X * b.X + a.Y * b.Y;
        }

        // Векторное произведение (2D, скаляр)
        static double Cross(const WorldPoint& a, const WorldPoint& b)
        {
            return a.X * b.Y - a.Y * b.X;
        }

        // Расстояние от точки до отрезка (с параметром проекции t)
        static double DistancePointToSegment(
            const WorldPoint& p,
            const WorldPoint& a,
            const WorldPoint& b,
            double& t,
            WorldPoint& projection)
        {
            WorldPoint ab(b.X - a.X, b.Y - a.Y);
            double abLen2 = ab.X * ab.X + ab.Y * ab.Y;
            if (abLen2 < 1e-10)
            {
                t = 0.0;
                projection = a;
                return Distance(p, a);
            }

            WorldPoint ap(p.X - a.X, p.Y - a.Y);
            t = Dot(ap, ab) / abLen2;
            t = std::clamp(t, 0.0, 1.0);

            projection = WorldPoint(a.X + ab.X * t, a.Y + ab.Y * t);
            return Distance(p, projection);
        }
    };

    // =====================================================
    // 4. ГЛАВНЫЙ КЛАСС СИСТЕМЫ ПРИВЯЗКИ
    // =====================================================

    class WallAttachmentSystem
    {
    public:
        WallAttachmentSystem() = default;

        // Конвертация WallAttachmentMode → LocationLineMode (для совместимости)
        static LocationLineMode ToLocationLineMode(WallAttachmentMode mode)
        {
            switch (mode)
            {
                case WallAttachmentMode::Core:
                    return LocationLineMode::WallCenterline;
                case WallAttachmentMode::FinishExterior:
                    return LocationLineMode::FinishFaceExterior;
                case WallAttachmentMode::FinishInterior:
                    return LocationLineMode::FinishFaceInterior;
            }
            return LocationLineMode::WallCenterline;
        }

        // =====================================================
        // 4.1 РАСЧЁТ СМЕЩЕНИЙ ДЛЯ ПРИВЯЗОК
        // =====================================================

        // Получить смещение от центральной оси стены для заданной привязки
        static double GetAttachmentOffset(WallAttachmentMode mode, double wallThickness)
        {
            switch (mode)
            {
                case WallAttachmentMode::Core:
                    return 0.0;                         // Центр стены
                case WallAttachmentMode::FinishExterior:
                    return wallThickness / 2.0;         // Внешняя грань (+half)
                case WallAttachmentMode::FinishInterior:
                    return -wallThickness / 2.0;        // Внутренняя грань (-half)
            }
            return 0.0;
        }

        // Получить линию привязки для стены
        static AttachmentLine GetAttachmentLine(const Wall& wall)
        {
            AttachmentLine line;
            line.Mode = ConvertLocationLineMode(wall.GetLocationLineMode());

            double offset = GetAttachmentOffset(line.Mode, wall.GetThickness());
            WorldPoint perp = wall.GetPerpendicular();

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            // Линия привязки = центральная ось + смещение по перпендикуляру
            line.Start.X = start.X + perp.X * offset;
            line.Start.Y = start.Y + perp.Y * offset;
            line.End.X = end.X + perp.X * offset;
            line.End.Y = end.Y + perp.Y * offset;

            return line;
        }

        // Получить смещённую линию от заданной линии
        static AttachmentLine GetOffsetLine(const AttachmentLine& line, double offset)
        {
            // Направление линии
            WorldPoint dir(line.End.X - line.Start.X, line.End.Y - line.Start.Y);
            double len = std::sqrt(dir.X * dir.X + dir.Y * dir.Y);

            if (len < 1e-10)
                return line;

            // Перпендикуляр (поворот на 90° против часовой)
            WorldPoint perp(-dir.Y / len, dir.X / len);

            AttachmentLine result;
            result.Mode = line.Mode;
            result.Start.X = line.Start.X + perp.X * offset;
            result.Start.Y = line.Start.Y + perp.Y * offset;
            result.End.X = line.End.X + perp.X * offset;
            result.End.Y = line.End.Y + perp.Y * offset;

            return result;
        }

        // =====================================================
        // 4.2 ПОИСК СОЕДИНЕНИЙ
        // =====================================================

        // Найти все соединения для стены с учётом привязки
        std::vector<AttachmentJoinInfo> FindJoins(
            const Wall& wall,
            const std::vector<std::unique_ptr<Wall>>& allWalls,
            double tolerance = 50.0) const
        {
            std::vector<AttachmentJoinInfo> results;

            AttachmentLine line1 = GetAttachmentLine(wall);

            for (const auto& other : allWalls)
            {
                if (!other || other->GetId() == wall.GetId())
                    continue;

                AttachmentLine line2 = GetAttachmentLine(*other);

                // Проверка соединения в начале wall
                if (wall.IsJoinAllowedAtStart())
                {
                    auto join = DetectJoin(wall, true, line1, *other, line2, tolerance);
                    if (join.IsValid())
                        results.push_back(join);
                }

                // Проверка соединения в конце wall
                if (wall.IsJoinAllowedAtEnd())
                {
                    auto join = DetectJoin(wall, false, line1, *other, line2, tolerance);
                    if (join.IsValid())
                        results.push_back(join);
                }

                // Дополнительная проверка: X-стыки и коллинеарность (середина-середина)
                auto midJoin = DetectCrossOrCollinear(wall, line1, *other, line2, tolerance);
                if (midJoin.IsValid())
                    results.push_back(midJoin);
            }

            return results;
        }

        // =====================================================
        // 4.3 ПОСТРОЕНИЕ КОНТУРА С УЧЁТОМ СОЕДИНЕНИЙ
        // =====================================================

        // Построить контур стены с учётом соединений
        std::vector<WorldPoint> BuildWallContour(
            const Wall& wall,
            const std::vector<AttachmentJoinInfo>& joins) const
        {
            std::vector<WorldPoint> contour;

            // Базовые 4 угла стены
            WorldPoint p1, p2, p3, p4;
            wall.GetCornerPoints(p1, p2, p3, p4);

            // Найти соединения в начале и конце
            const AttachmentJoinInfo* startJoin = nullptr;
            const AttachmentJoinInfo* endJoin = nullptr;

            for (const auto& join : joins)
            {
                if (join.WallId1 == wall.GetId())
                {
                    if (join.Wall1Contact == AttachmentJoinInfo::ContactType::Start)
                        startJoin = &join;
                    else if (join.Wall1Contact == AttachmentJoinInfo::ContactType::End)
                        endJoin = &join;
                }
                else if (join.WallId2 == wall.GetId())
                {
                    if (join.Wall2Contact == AttachmentJoinInfo::ContactType::Start)
                        startJoin = &join;
                    else if (join.Wall2Contact == AttachmentJoinInfo::ContactType::End)
                        endJoin = &join;
                }
            }

            // Построение контура с учётом соединений
            if (startJoin && !startJoin->CornerPolygon.empty())
            {
                // Использовать полигон угла из соединения
                for (const auto& pt : startJoin->CornerPolygon)
                    contour.push_back(pt);
            }
            else
            {
                contour.push_back(p1);
                contour.push_back(p2);
            }

            if (endJoin && !endJoin->CornerPolygon.empty())
            {
                // Использовать полигон угла (в обратном порядке)
                for (auto it = endJoin->CornerPolygon.rbegin(); it != endJoin->CornerPolygon.rend(); ++it)
                    contour.push_back(*it);
            }
            else
            {
                contour.push_back(p3);
                contour.push_back(p4);
            }

            return contour;
        }

        // =====================================================
        // 4.4 ПРЕДПРОСМОТР ПРИ РИСОВАНИИ
        // =====================================================

        // Найти предпросмотр соединения для новой стены
        std::optional<AttachmentJoinInfo> FindPreviewJoin(
            const WorldPoint& wallStart,
            const WorldPoint& wallEnd,
            double thickness,
            WallAttachmentMode mode,
            const std::vector<std::unique_ptr<Wall>>& allWalls,
            double tolerance = 50.0) const
        {
            // Создать временную стену
            Wall tempWall(wallStart, wallEnd, thickness);
            tempWall.SetLocationLineMode(ConvertToLocationLineMode(mode));

            // Найти соединения
            auto joins = FindJoins(tempWall, allWalls, tolerance);

            // Вернуть соединение в конце (наиболее актуальное при рисовании)
            for (const auto& join : joins)
            {
                if (join.WallId1 == tempWall.GetId() &&
                    join.Wall1Contact == AttachmentJoinInfo::ContactType::End)
                    return join;
            }

            // Или соединение в начале
            for (const auto& join : joins)
            {
                if (join.WallId1 == tempWall.GetId() &&
                    join.Wall1Contact == AttachmentJoinInfo::ContactType::Start)
                    return join;
            }

            return std::nullopt;
        }

    private:
        // =====================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =====================================================

        // Конвертация LocationLineMode → WallAttachmentMode
        static WallAttachmentMode ConvertLocationLineMode(LocationLineMode mode)
        {
            switch (mode)
            {
                case LocationLineMode::WallCenterline:
                case LocationLineMode::CoreCenterline:
                    return WallAttachmentMode::Core;

                case LocationLineMode::FinishFaceExterior:
                case LocationLineMode::CoreFaceExterior:
                    return WallAttachmentMode::FinishExterior;

                case LocationLineMode::FinishFaceInterior:
                case LocationLineMode::CoreFaceInterior:
                    return WallAttachmentMode::FinishInterior;
            }
            return WallAttachmentMode::Core;
        }

        // Конвертация WallAttachmentMode → LocationLineMode
        static LocationLineMode ConvertToLocationLineMode(WallAttachmentMode mode)
        {
            return ToLocationLineMode(mode);
        }

        // Определить соединение между двумя стенами
        AttachmentJoinInfo DetectJoin(
            const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
            const Wall& wall2, const AttachmentLine& line2,
            double tolerance) const;

        // Определить X-стык или коллинеарность (середина-середина)
        AttachmentJoinInfo DetectCrossOrCollinear(
            const Wall& wall1, const AttachmentLine& line1,
            const Wall& wall2, const AttachmentLine& line2,
            double tolerance) const;

        // Вычислить геометрию угла для соединения
        void CalculateJoinGeometry(
            const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
            const Wall& wall2, bool wall2AtStart, const AttachmentLine& line2,
            AttachmentJoinInfo& join) const;

        // Построить полигон угла для режима Core
        std::vector<WorldPoint> BuildCoreMiterJoin(
            const Wall& wall1, bool wall1AtStart,
            const Wall& wall2, bool wall2AtStart,
            const WorldPoint& joinPoint) const;

        // Построить полигон угла для режима FinishExterior
        std::vector<WorldPoint> BuildExteriorJoin(
            const Wall& wall1, bool wall1AtStart,
            const Wall& wall2, bool wall2AtStart,
            const WorldPoint& joinPoint) const;

        // Построить полигон угла для режима FinishInterior
        std::vector<WorldPoint> BuildInteriorJoin(
            const Wall& wall1, bool wall1AtStart,
            const Wall& wall2, bool wall2AtStart,
            const WorldPoint& joinPoint) const;

        // Построить полигон угла для смешанных режимов
        std::vector<WorldPoint> BuildMixedJoin(
            const Wall& wall1, WallAttachmentMode mode1, bool wall1AtStart,
            const Wall& wall2, WallAttachmentMode mode2, bool wall2AtStart,
            const WorldPoint& joinPoint) const;

        // Построить полигон Т-стыка (конец wall1 упирается в середину wall2)
        std::vector<WorldPoint> BuildTJoin(
            const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
            const Wall& wall2, const AttachmentLine& line2,
            const WorldPoint& joinPoint) const;

        // Получить внешние и внутренние грани стены
        void GetWallEdges(
            const Wall& wall,
            WorldPoint& extStart, WorldPoint& extEnd,
            WorldPoint& intStart, WorldPoint& intEnd) const
        {
            WorldPoint perp = wall.GetPerpendicular();
            double half = wall.GetThickness() / 2.0;

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            // Внешняя грань (по перпендикуляру)
            extStart = WorldPoint(start.X + perp.X * half, start.Y + perp.Y * half);
            extEnd = WorldPoint(end.X + perp.X * half, end.Y + perp.Y * half);

            // Внутренняя грань (против перпендикуляра)
            intStart = WorldPoint(start.X - perp.X * half, start.Y - perp.Y * half);
            intEnd = WorldPoint(end.X - perp.X * half, end.Y - perp.Y * half);
        }
    };

    // =====================================================
    // РЕАЛИЗАЦИЯ МЕТОДОВ (ниже будут добавлены на следующих этапах)
    // =====================================================

    // DetectJoin - будет реализован в ЭТАПЕ 3.1
    inline AttachmentJoinInfo WallAttachmentSystem::DetectJoin(
        const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
        const Wall& wall2, const AttachmentLine& line2,
        double tolerance) const
    {
        AttachmentJoinInfo result;
        result.WallId1 = wall1.GetId();
        result.WallId2 = wall2.GetId();
        result.Wall1AtStart = wall1AtStart;
        result.Mode1 = line1.Mode;
        result.Mode2 = line2.Mode;
        result.Wall1Contact = wall1AtStart
            ? AttachmentJoinInfo::ContactType::Start
            : AttachmentJoinInfo::ContactType::End;

        // Точка на wall1 для проверки соединения
        WorldPoint p1 = wall1AtStart ? line1.Start : line1.End;

        // Проверка: находятся ли концы линий привязки рядом?
        double distToStart2 = AttachmentGeometry::Distance(p1, line2.Start);
        double distToEnd2 = AttachmentGeometry::Distance(p1, line2.End);

        if (distToStart2 < tolerance)
        {
            result.Wall2AtStart = true;
            result.Wall2Contact = AttachmentJoinInfo::ContactType::Start;
            result.JoinPoint = WorldPoint((p1.X + line2.Start.X) / 2, (p1.Y + line2.Start.Y) / 2);
            result.Type = AttachmentJoinInfo::JoinType::LJoin;
        }
        else if (distToEnd2 < tolerance)
        {
            result.Wall2AtStart = false;
            result.Wall2Contact = AttachmentJoinInfo::ContactType::End;
            result.JoinPoint = WorldPoint((p1.X + line2.End.X) / 2, (p1.Y + line2.End.Y) / 2);
            result.Type = AttachmentJoinInfo::JoinType::LJoin;
        }
        else
        {
            // Проверка T-соединения: конец wall1 попадает на середину wall2
            double t2 = 0.0;
            WorldPoint projection;
            double distToSegment = AttachmentGeometry::DistancePointToSegment(
                p1, line2.Start, line2.End, t2, projection);

            if (distToSegment <= tolerance)
            {
                result.Wall2AtStart = false;
                result.Wall2Contact = AttachmentJoinInfo::ContactType::Middle;

                // Определить, если линии коллинеарны
                WorldPoint dir1(line1.End.X - line1.Start.X, line1.End.Y - line1.Start.Y);
                WorldPoint dir2(line2.End.X - line2.Start.X, line2.End.Y - line2.Start.Y);
                double len1 = std::sqrt(dir1.X * dir1.X + dir1.Y * dir1.Y);
                double len2 = std::sqrt(dir2.X * dir2.X + dir2.Y * dir2.Y);

                bool isParallel = (len1 > 1e-6 && len2 > 1e-6)
                    ? (std::abs(AttachmentGeometry::Cross(dir1, dir2) / (len1 * len2)) < 1e-4)
                    : false;

                if (isParallel)
                {
                    result.Type = AttachmentJoinInfo::JoinType::Collinear;
                    result.JoinPoint = projection;
                    return result;
                }

                result.Type = AttachmentJoinInfo::JoinType::TJoin;

                // Попытка использовать пересечение бесконечных линий
                WorldPoint intersection;
                if (AttachmentGeometry::LineIntersection(line1.Start, line1.End, line2.Start, line2.End, intersection))
                {
                    result.JoinPoint = intersection;
                }
                else
                {
                    result.JoinPoint = projection;
                }
            }
            else
            {
                // Не L/T-соединение
                result.WallId1 = 0;
                result.WallId2 = 0;
                return result;  // IsValid() == false
            }
        }

        // Проверка коллинеарности для L-соединений (концы на одной линии)
        if (result.Type == AttachmentJoinInfo::JoinType::LJoin)
        {
            WorldPoint dir1(line1.End.X - line1.Start.X, line1.End.Y - line1.Start.Y);
            WorldPoint dir2(line2.End.X - line2.Start.X, line2.End.Y - line2.Start.Y);
            double len1 = std::sqrt(dir1.X * dir1.X + dir1.Y * dir1.Y);
            double len2 = std::sqrt(dir2.X * dir2.X + dir2.Y * dir2.Y);

            if (len1 > 1e-6 && len2 > 1e-6)
            {
                double cross = std::abs(AttachmentGeometry::Cross(dir1, dir2) / (len1 * len2));
                if (cross < 1e-4)
                {
                    double t2 = 0.0;
                    WorldPoint projection;
                    double dist = AttachmentGeometry::DistancePointToSegment(p1, line2.Start, line2.End, t2, projection);
                    if (dist <= tolerance)
                    {
                        result.Type = AttachmentJoinInfo::JoinType::Collinear;
                        result.JoinPoint = projection;
                        return result;
                    }
                }
            }
        }

        // Найти точку пересечения линий привязки
        WorldPoint intersection;
        if (AttachmentGeometry::LineIntersection(line1.Start, line1.End, line2.Start, line2.End, intersection))
        {
            result.JoinPoint = intersection;
        }

        // Вычислить угол
        WorldPoint dir1 = wall1.GetDirection();
        if (wall1AtStart) dir1 = WorldPoint(-dir1.X, -dir1.Y);

        WorldPoint dir2 = wall2.GetDirection();
        if (result.Wall2Contact == AttachmentJoinInfo::ContactType::End)
            dir2 = WorldPoint(-dir2.X, -dir2.Y);

        result.Angle = AttachmentGeometry::AngleBetween(dir1, WorldPoint(-dir2.X, -dir2.Y));

        // Вычислить геометрию угла
        CalculateJoinGeometry(wall1, wall1AtStart, line1, wall2, result.Wall2AtStart, line2, result);

        return result;
    }

    inline AttachmentJoinInfo WallAttachmentSystem::DetectCrossOrCollinear(
        const Wall& wall1, const AttachmentLine& line1,
        const Wall& wall2, const AttachmentLine& line2,
        double tolerance) const
    {
        AttachmentJoinInfo result;
        result.WallId1 = wall1.GetId();
        result.WallId2 = wall2.GetId();
        result.Mode1 = line1.Mode;
        result.Mode2 = line2.Mode;
        result.Wall1Contact = AttachmentJoinInfo::ContactType::Middle;
        result.Wall2Contact = AttachmentJoinInfo::ContactType::Middle;
        result.Wall1AtStart = false;
        result.Wall2AtStart = false;

        // Проверка пересечения отрезков (X-стык)
        WorldPoint intersection;
        double t1 = 0.0;
        double t2 = 0.0;
        if (AttachmentGeometry::LineIntersection(line1.Start, line1.End, line2.Start, line2.End, intersection, t1, t2))
        {
            const double eps = 1e-3;
            if (t1 > eps && t1 < 1.0 - eps && t2 > eps && t2 < 1.0 - eps)
            {
                result.Type = AttachmentJoinInfo::JoinType::XJoin;
                result.JoinPoint = intersection;
                result.Angle = AttachmentGeometry::AngleBetween(wall1.GetDirection(), wall2.GetDirection());
                return result;
            }
        }

        // Проверка коллинеарности и перекрытия
        WorldPoint dir1(line1.End.X - line1.Start.X, line1.End.Y - line1.Start.Y);
        WorldPoint dir2(line2.End.X - line2.Start.X, line2.End.Y - line2.Start.Y);
        double len1 = std::sqrt(dir1.X * dir1.X + dir1.Y * dir1.Y);
        double len2 = std::sqrt(dir2.X * dir2.X + dir2.Y * dir2.Y);
        if (len1 < 1e-6 || len2 < 1e-6)
            return result;

        double cross = std::abs(AttachmentGeometry::Cross(dir1, dir2) / (len1 * len2));
        if (cross > 1e-4)
            return result;

        double tProj = 0.0;
        WorldPoint proj;
        double dist = AttachmentGeometry::DistancePointToSegment(line1.Start, line2.Start, line2.End, tProj, proj);
        if (dist > tolerance)
            return result;

        WorldPoint dir2Norm(dir2.X / len2, dir2.Y / len2);
        double s1 = AttachmentGeometry::Dot(WorldPoint(line1.Start.X - line2.Start.X, line1.Start.Y - line2.Start.Y), dir2Norm);
        double s2 = AttachmentGeometry::Dot(WorldPoint(line1.End.X - line2.Start.X, line1.End.Y - line2.Start.Y), dir2Norm);

        double minS = (std::min)(s1, s2);
        double maxS = (std::max)(s1, s2);
        double overlapStart = (std::max)(0.0, minS);
        double overlapEnd = (std::min)(len2, maxS);

        if (overlapEnd >= overlapStart + tolerance)
        {
            result.Type = AttachmentJoinInfo::JoinType::Collinear;
            double mid = (overlapStart + overlapEnd) * 0.5;
            result.JoinPoint = WorldPoint(line2.Start.X + dir2Norm.X * mid, line2.Start.Y + dir2Norm.Y * mid);
            return result;
        }

        return AttachmentJoinInfo{};
    }

    // CalculateJoinGeometry - будет реализован в ЭТАПАХ 3.2-3.5
    inline void WallAttachmentSystem::CalculateJoinGeometry(
        const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
        const Wall& wall2, bool wall2AtStart, const AttachmentLine& line2,
        AttachmentJoinInfo& join) const
    {
        join.CornerPolygon.clear();

        // Для X и коллинеарных соединений геометрия не требуется
        if (join.Type == AttachmentJoinInfo::JoinType::XJoin ||
            join.Type == AttachmentJoinInfo::JoinType::Collinear)
        {
            return;
        }

        if (join.Type == AttachmentJoinInfo::JoinType::TJoin)
        {
            join.CornerPolygon = BuildTJoin(wall1, wall1AtStart, line1, wall2, line2, join.JoinPoint);
            return;
        }

        // Определить какой алгоритм использовать в зависимости от режимов
        if (line1.Mode == WallAttachmentMode::Core && line2.Mode == WallAttachmentMode::Core)
        {
            join.CornerPolygon = BuildCoreMiterJoin(wall1, wall1AtStart, wall2, wall2AtStart, join.JoinPoint);
        }
        else if (line1.Mode == WallAttachmentMode::FinishExterior && line2.Mode == WallAttachmentMode::FinishExterior)
        {
            join.CornerPolygon = BuildExteriorJoin(wall1, wall1AtStart, wall2, wall2AtStart, join.JoinPoint);
        }
        else if (line1.Mode == WallAttachmentMode::FinishInterior && line2.Mode == WallAttachmentMode::FinishInterior)
        {
            join.CornerPolygon = BuildInteriorJoin(wall1, wall1AtStart, wall2, wall2AtStart, join.JoinPoint);
        }
        else
        {
            // Смешанные режимы
            join.CornerPolygon = BuildMixedJoin(wall1, line1.Mode, wall1AtStart, wall2, line2.Mode, wall2AtStart, join.JoinPoint);
        }
    }

    // BuildCoreMiterJoin - ЭТАП 3.2 - ПОЛНАЯ РЕАЛИЗАЦИЯ
    // Режим Core: угол формируется на пересечении внешних и внутренних граней
    inline std::vector<WorldPoint> WallAttachmentSystem::BuildCoreMiterJoin(
        const Wall& wall1, bool wall1AtStart,
        const Wall& wall2, bool wall2AtStart,
        const WorldPoint& joinPoint) const
    {
        std::vector<WorldPoint> polygon;

        // Получить внешние и внутренние грани обеих стен
        WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
        GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

        WorldPoint w2_ext_start, w2_ext_end, w2_int_start, w2_int_end;
        GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

        // Найти пересечения граней (расширенные линии)
        WorldPoint intersectionOuter, intersectionInner;
        bool hasOuterIntersection = AttachmentGeometry::LineIntersection(
            w1_ext_start, w1_ext_end, w2_ext_start, w2_ext_end, intersectionOuter);
        bool hasInnerIntersection = AttachmentGeometry::LineIntersection(
            w1_int_start, w1_int_end, w2_int_start, w2_int_end, intersectionInner);

        if (!hasOuterIntersection && !hasInnerIntersection)
        {
            // Параллельные стены - вернуть пустой полигон
            return polygon;
        }

        // Если одно из пересечений не найдено, используем joinPoint
        if (!hasOuterIntersection)
            intersectionOuter = joinPoint;
        if (!hasInnerIntersection)
            intersectionInner = joinPoint;

        // Проверка длины скоса (miter length limit) - предотвращение слишком острых углов
        double miterLength = AttachmentGeometry::Distance(intersectionOuter, intersectionInner);
        double maxMiterLength = (std::max)(wall1.GetThickness(), wall2.GetThickness()) * 2.5;

        if (miterLength > maxMiterLength)
        {
            // Скос слишком длинный - использовать bevel (ограничить длину)
            AttachmentGeometry::ClampSegment(intersectionOuter, intersectionInner, maxMiterLength);
        }

        // Добавить точки пересечения в полигон
        // Эти точки будут использованы в BuildWallContour для замены базовых углов
        polygon.push_back(intersectionOuter);
        polygon.push_back(intersectionInner);

        return polygon;
    }

    // BuildExteriorJoin - ЭТАП 3.3 - ПОЛНАЯ РЕАЛИЗАЦИЯ
    // Режим FinishExterior: угол формируется на пересечении ВНЕШНИХ граней
    // Линия привязки = внешняя грань
    inline std::vector<WorldPoint> WallAttachmentSystem::BuildExteriorJoin(
        const Wall& wall1, bool wall1AtStart,
        const Wall& wall2, bool wall2AtStart,
        const WorldPoint& joinPoint) const
    {
        std::vector<WorldPoint> polygon;

        // Получить внешние и внутренние грани обеих стен
        WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
        GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

        WorldPoint w2_ext_start, w2_ext_end, w2_int_start, w2_int_end;
        GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

        // Для FinishExterior joinPoint уже находится на пересечении внешних граней
        WorldPoint outerCorner = joinPoint;

        // Найти пересечение внутренних граней (offset = -thickness от внешних)
        WorldPoint intersectionInner;
        bool hasInnerIntersection = AttachmentGeometry::LineIntersection(
            w1_int_start, w1_int_end, w2_int_start, w2_int_end, intersectionInner);

        if (!hasInnerIntersection)
        {
            // Параллельные стены - вернуть пустой полигон
            return polygon;
        }

        // Проверка длины скоса
        double miterLength = AttachmentGeometry::Distance(outerCorner, intersectionInner);
        double maxMiterLength = (std::max)(wall1.GetThickness(), wall2.GetThickness()) * 2.5;

        if (miterLength > maxMiterLength)
        {
            AttachmentGeometry::ClampSegment(outerCorner, intersectionInner, maxMiterLength);
        }

        // Вернуть 2 точки: внешний угол (joinPoint) и внутренний угол (пересечение внутренних граней)
        polygon.push_back(outerCorner);
        polygon.push_back(intersectionInner);

        return polygon;
    }

    // BuildInteriorJoin - ЭТАП 3.4 - ПОЛНАЯ РЕАЛИЗАЦИЯ
    // Режим FinishInterior: угол формируется на пересечении ВНУТРЕННИХ граней
    // Линия привязки = внутренняя грань
    inline std::vector<WorldPoint> WallAttachmentSystem::BuildInteriorJoin(
        const Wall& wall1, bool wall1AtStart,
        const Wall& wall2, bool wall2AtStart,
        const WorldPoint& joinPoint) const
    {
        std::vector<WorldPoint> polygon;

        // Получить внешние и внутренние грани обеих стен
        WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
        GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

        WorldPoint w2_ext_start, w2_ext_end, w2_int_start, w2_int_end;
        GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

        // Для FinishInterior joinPoint уже находится на пересечении внутренних граней
        WorldPoint innerCorner = joinPoint;

        // Найти пересечение внешних граней (offset = +thickness от внутренних)
        WorldPoint intersectionOuter;
        bool hasOuterIntersection = AttachmentGeometry::LineIntersection(
            w1_ext_start, w1_ext_end, w2_ext_start, w2_ext_end, intersectionOuter);

        if (!hasOuterIntersection)
        {
            // Параллельные стены - вернуть пустой полигон
            return polygon;
        }

        // Проверка длины скоса
        double miterLength = AttachmentGeometry::Distance(innerCorner, intersectionOuter);
        double maxMiterLength = (std::max)(wall1.GetThickness(), wall2.GetThickness()) * 2.5;

        if (miterLength > maxMiterLength)
        {
            AttachmentGeometry::ClampSegment(intersectionOuter, innerCorner, maxMiterLength);
        }

        // Вернуть 2 точки: внешний угол (пересечение внешних граней) и внутренний угол (joinPoint)
        polygon.push_back(intersectionOuter);
        polygon.push_back(innerCorner);

        return polygon;
    }

    // BuildMixedJoin - ЭТАП 3.5 - ПОЛНАЯ РЕАЛИЗАЦИЯ
    // Смешанные режимы: когда две стены имеют разные режимы привязки
    inline std::vector<WorldPoint> WallAttachmentSystem::BuildMixedJoin(
        const Wall& wall1, WallAttachmentMode mode1, bool wall1AtStart,
        const Wall& wall2, WallAttachmentMode mode2, bool wall2AtStart,
        const WorldPoint& joinPoint) const
    {
        std::vector<WorldPoint> polygon;

        // Получить внешние и внутренние грани обеих стен
        WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
        GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

        WorldPoint w2_ext_start, w2_ext_end, w2_int_start, w2_int_end;
        GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

        if (mode1 == WallAttachmentMode::Core)
        {
            // Центральная линия
        }
        else if (mode1 == WallAttachmentMode::FinishExterior)
        {
            // Внешняя грань
        }
        else  // FinishInterior
        {
            // Внутренняя грань
        }

        if (mode2 == WallAttachmentMode::Core)
        {
        }
        else if (mode2 == WallAttachmentMode::FinishExterior)
        {
        }
        else  // FinishInterior
        {
        }

        // joinPoint уже находится на пересечении линий привязки

        // Для смешанных режимов нужно найти противоположные линии и их пересечение
        // Например, если wall1 = Exterior и wall2 = Interior:
        // - joinPoint на пересечении внешней грани wall1 и внутренней грани wall2
        // - Нужно найти пересечение внутренней грани wall1 и внешней грани wall2

        WorldPoint oppositeLine1Start, oppositeLine1End;
        WorldPoint oppositeLine2Start, oppositeLine2End;

        // Определить противоположные линии
        if (mode1 == WallAttachmentMode::Core)
        {
            // Core: противоположная линия - тоже центр (но нет противоположности)
            // Используем внешнюю и внутреннюю грани
            oppositeLine1Start = w1_int_start;
            oppositeLine1End = w1_int_end;
        }
        else if (mode1 == WallAttachmentMode::FinishExterior)
        {
            oppositeLine1Start = w1_int_start;
            oppositeLine1End = w1_int_end;
        }
        else  // FinishInterior
        {
            oppositeLine1Start = w1_ext_start;
            oppositeLine1End = w1_ext_end;
        }

        if (mode2 == WallAttachmentMode::Core)
        {
            oppositeLine2Start = w2_int_start;
            oppositeLine2End = w2_int_end;
        }
        else if (mode2 == WallAttachmentMode::FinishExterior)
        {
            oppositeLine2Start = w2_int_start;
            oppositeLine2End = w2_int_end;
        }
        else  // FinishInterior
        {
            oppositeLine2Start = w2_ext_start;
            oppositeLine2End = w2_ext_end;
        }

        // Найти пересечение противоположных линий
        WorldPoint oppositeIntersection;
        bool hasOppositeIntersection = AttachmentGeometry::LineIntersection(
            oppositeLine1Start, oppositeLine1End,
            oppositeLine2Start, oppositeLine2End,
            oppositeIntersection);

        if (!hasOppositeIntersection)
        {
            // Параллельные стены - вернуть пустой полигон
            return polygon;
        }

        // Проверка длины скоса
        double miterLength = AttachmentGeometry::Distance(joinPoint, oppositeIntersection);
        double maxMiterLength = (std::max)(wall1.GetThickness(), wall2.GetThickness()) * 2.5;

        if (miterLength > maxMiterLength)
        {
            auto clampedJoin = joinPoint;
            AttachmentGeometry::ClampSegment(clampedJoin, oppositeIntersection, maxMiterLength);
            // Обновляем joinPoint для дальнейшего использования
            polygon.push_back(clampedJoin);
            polygon.push_back(oppositeIntersection);
            return polygon;
        }

        // Вернуть 2 точки: joinPoint и противоположное пересечение
        polygon.push_back(joinPoint);
        polygon.push_back(oppositeIntersection);

        return polygon;
    }

    // BuildTJoin - ЭТАП 7 - Т-стык (конец wall1 упирается в середину wall2)
    inline std::vector<WorldPoint> WallAttachmentSystem::BuildTJoin(
        const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
        const Wall& wall2, const AttachmentLine& line2,
        const WorldPoint& joinPoint) const
    {
        std::vector<WorldPoint> polygon;

        (void)wall1AtStart;
        (void)line1;

        // Используем линию привязки wall2 как линию среза
        // Находим пересечения внешней и внутренней граней wall1 с этой линией
        WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
        GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

        WorldPoint cutOuter, cutInner;
        bool hasOuter = AttachmentGeometry::LineIntersection(
            w1_ext_start, w1_ext_end, line2.Start, line2.End, cutOuter);
        bool hasInner = AttachmentGeometry::LineIntersection(
            w1_int_start, w1_int_end, line2.Start, line2.End, cutInner);

        if (!hasOuter && !hasInner)
            return polygon;

        if (!hasOuter)
            cutOuter = joinPoint;
        if (!hasInner)
            cutInner = joinPoint;

        double maxMiterLength = (std::max)(wall1.GetThickness(), wall2.GetThickness()) * 2.5;
        AttachmentGeometry::ClampSegment(cutOuter, cutInner, maxMiterLength);

        polygon.push_back(cutOuter);
        polygon.push_back(cutInner);

        return polygon;
    }
}
