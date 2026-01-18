#pragma once

#include "pch.h"
#include "Models.h"
#include "Element.h"
#include "WallType.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <optional>

namespace winrt::estimate1
{
    // ============================================================================
    // Wall Snap Planes (Плоскости привязки стен)
    // ============================================================================

    enum class WallSnapPlane
    {
        None,               // Нет привязки
        Endpoint,           // Конечная точка
        Midpoint,           // Середина стены
        Centerline,         // Ось стены (центральная линия)
        CoreCenterline,     // Ось несущего слоя
        FinishFaceExterior, // Наружная чистовая поверхность
        FinishFaceInterior, // Внутренняя чистовая поверхность
        CoreFaceExterior,   // Наружная поверхность несущего слоя
        CoreFaceInterior,   // Внутренняя поверхность несущего слоя
        AlongFace           // Произвольная точка вдоль грани
    };

    // Режим привязки (пользовательский выбор)
    enum class SnapReferenceMode
    {
        Auto,               // Автоматический выбор ближайшего
        Centerline,         // Только ось стены
        FinishExterior,     // Только наружная чистовая
        FinishInterior,     // Только внутренняя чистовая
        CoreExterior,       // Только наружная несущая
        CoreInterior        // Только внутренняя несущая
    };

    // ============================================================================
    // Snap Candidate (Кандидат привязки)
    // ============================================================================

    struct WallSnapCandidate
    {
        uint64_t WallId{ 0 };           // ID стены
        WallSnapPlane Plane{ WallSnapPlane::None };
        WorldPoint ProjectedPoint{ 0, 0 }; // Точка привязки
        double Distance{ 0.0 };          // Расстояние от курсора (мм)
        bool IsEndpoint{ false };        // Это конечная точка?
        bool IsValid{ false };           // Валидный кандидат?

        // Для сравнения
        bool operator<(const WallSnapCandidate& other) const
        {
            // Приоритет: endpoints > faces > centerline
            if (IsEndpoint != other.IsEndpoint)
                return IsEndpoint; // Endpoints имеют приоритет
            
            return Distance < other.Distance;
        }
    };

    // ============================================================================
    // Wall Reference Line (Линия привязки стены)
    // ============================================================================

    struct WallReferenceLine
    {
        WorldPoint Start{ 0, 0 };
        WorldPoint End{ 0, 0 };
        WallSnapPlane Plane{ WallSnapPlane::Centerline };
        uint64_t WallId{ 0 };

        // Длина линии
        double Length() const
        {
            double dx = End.X - Start.X;
            double dy = End.Y - Start.Y;
            return std::sqrt(dx * dx + dy * dy);
        }

        // Проекция точки на линию
        WorldPoint ProjectPoint(const WorldPoint& point) const
        {
            double dx = End.X - Start.X;
            double dy = End.Y - Start.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return Start;

            // Параметр t для проекции (0..1 = на отрезке)
            double t = ((point.X - Start.X) * dx + (point.Y - Start.Y) * dy) / lengthSq;
            
            // Ограничиваем t в пределах [0, 1] для привязки к отрезку
            t = std::clamp(t, 0.0, 1.0);

            return WorldPoint(
                Start.X + t * dx,
                Start.Y + t * dy
            );
        }

        // Расстояние от точки до линии
        double DistanceToPoint(const WorldPoint& point) const
        {
            WorldPoint proj = ProjectPoint(point);
            return point.Distance(proj);
        }

        // Параметр t для точки на линии (0 = Start, 1 = End)
        double GetParameterT(const WorldPoint& point) const
        {
            double dx = End.X - Start.X;
            double dy = End.Y - Start.Y;
            double lengthSq = dx * dx + dy * dy;

            if (lengthSq < 0.0001)
                return 0.0;

            return ((point.X - Start.X) * dx + (point.Y - Start.Y) * dy) / lengthSq;
        }
    };

    // ============================================================================
    // Wall Snap System (Система привязки стен)
    // ============================================================================

    class WallSnapSystem
    {
    public:
        WallSnapSystem() = default;

        // ============================================================
        // Настройки
        // ============================================================

        // Порог привязки (пиксели экрана)
        void SetSnapThresholdPixels(double pixels) { m_snapThresholdPx = pixels; }
        double GetSnapThresholdPixels() const { return m_snapThresholdPx; }

        // Порог привязки к конечным точкам (пиксели)
        void SetEndpointThresholdPixels(double pixels) { m_endpointThresholdPx = pixels; }
        double GetEndpointThresholdPixels() const { return m_endpointThresholdPx; }

        // Режим привязки
        void SetReferenceMode(SnapReferenceMode mode) { m_referenceMode = mode; }
        SnapReferenceMode GetReferenceMode() const { return m_referenceMode; }

        // Включить/выключить привязку к граням
        void SetFaceSnapEnabled(bool enabled) { m_faceSnapEnabled = enabled; }
        bool IsFaceSnapEnabled() const { return m_faceSnapEnabled; }

        // Цикл режимов (Tab)
        void CycleReferenceMode()
        {
            int mode = static_cast<int>(m_referenceMode);
            mode = (mode + 1) % 6; // 6 режимов
            m_referenceMode = static_cast<SnapReferenceMode>(mode);
        }

        // ============================================================
        // Основной метод поиска привязки
        // ============================================================

        WallSnapCandidate FindBestSnap(
            const WorldPoint& cursor,
            const std::vector<std::unique_ptr<Wall>>& walls,
            double zoomFactor,
            uint64_t excludeWallId = 0)
        {
            std::vector<WallSnapCandidate> candidates;

            // Преобразуем пороги из пикселей в мм
            double snapThresholdMM = m_snapThresholdPx / zoomFactor;
            double endpointThresholdMM = m_endpointThresholdPx / zoomFactor;

            for (const auto& wall : walls)
            {
                if (!wall || wall->GetId() == excludeWallId)
                    continue;

                // Получаем все линии привязки для стены
                auto refLines = GetWallReferenceLines(*wall);

                for (const auto& refLine : refLines)
                {
                    // Фильтруем по режиму привязки
                    if (!IsPlaneAllowedByMode(refLine.Plane))
                        continue;

                    // Проверяем привязку к конечным точкам
                    double distToStart = cursor.Distance(refLine.Start);
                    double distToEnd = cursor.Distance(refLine.End);

                    // Привязка к начальной точке
                    if (distToStart < endpointThresholdMM && wall->IsJoinAllowedAtStart())
                    {
                        WallSnapCandidate candidate;
                        candidate.WallId = wall->GetId();
                        candidate.Plane = WallSnapPlane::Endpoint;
                        candidate.ProjectedPoint = refLine.Start;
                        candidate.Distance = distToStart;
                        candidate.IsEndpoint = true;
                        candidate.IsValid = true;
                        candidates.push_back(candidate);
                    }

                    // Привязка к конечной точке
                    if (distToEnd < endpointThresholdMM && wall->IsJoinAllowedAtEnd())
                    {
                        WallSnapCandidate candidate;
                        candidate.WallId = wall->GetId();
                        candidate.Plane = WallSnapPlane::Endpoint;
                        candidate.ProjectedPoint = refLine.End;
                        candidate.Distance = distToEnd;
                        candidate.IsEndpoint = true;
                        candidate.IsValid = true;
                        candidates.push_back(candidate);
                    }

                    // Привязка к середине (только для centerline)
                    if (refLine.Plane == WallSnapPlane::Centerline)
                    {
                        WorldPoint mid(
                            (refLine.Start.X + refLine.End.X) / 2.0,
                            (refLine.Start.Y + refLine.End.Y) / 2.0);
                        double distToMid = cursor.Distance(mid);
                        
                        if (distToMid < endpointThresholdMM)
                        {
                            WallSnapCandidate candidate;
                            candidate.WallId = wall->GetId();
                            candidate.Plane = WallSnapPlane::Midpoint;
                            candidate.ProjectedPoint = mid;
                            candidate.Distance = distToMid;
                            candidate.IsEndpoint = false;
                            candidate.IsValid = true;
                            candidates.push_back(candidate);
                        }
                    }

                    // Привязка вдоль линии (грани или оси)
                    if (m_faceSnapEnabled || refLine.Plane == WallSnapPlane::Centerline)
                    {
                        WorldPoint proj = refLine.ProjectPoint(cursor);
                        double distToLine = cursor.Distance(proj);

                        if (distToLine < snapThresholdMM)
                        {
                            // Проверяем, что проекция на отрезке (не за его пределами)
                            double t = refLine.GetParameterT(proj);
                            if (t >= -0.01 && t <= 1.01)
                            {
                                // Если совсем на концах, зафиксируем в точку конца для ровного попадания
                                if (t <= 0.0)
                                    proj = refLine.Start;
                                else if (t >= 1.0)
                                    proj = refLine.End;

                                WallSnapCandidate candidate;
                                candidate.WallId = wall->GetId();
                                candidate.Plane = refLine.Plane;
                                candidate.ProjectedPoint = proj;
                                candidate.Distance = distToLine;
                                candidate.IsEndpoint = false;
                                candidate.IsValid = true;
                                candidates.push_back(candidate);
                            }
                        }
                    }
                }
            }

            // Сортируем кандидатов по приоритету и расстоянию
            std::sort(candidates.begin(), candidates.end());

            // Возвращаем лучшего кандидата
            if (!candidates.empty())
            {
                return candidates.front();
            }

            // Нет привязки
            WallSnapCandidate noSnap;
            noSnap.IsValid = false;
            return noSnap;
        }

        // ============================================================
        // Вычисление линий привязки для стены
        // ============================================================

        std::vector<WallReferenceLine> GetWallReferenceLines(const Wall& wall) const
        {
            std::vector<WallReferenceLine> lines;

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();
            double thickness = wall.GetThickness();

            // Вектор направления стены
            double dx = end.X - start.X;
            double dy = end.Y - start.Y;
            double len = std::sqrt(dx * dx + dy * dy);

            if (len < 0.001)
                return lines;

            // Нормализованное направление
            double dirX = dx / len;
            double dirY = dy / len;

            // Перпендикуляр (влево от направления)
            double perpX = -dirY;
            double perpY = dirX;

            // Половина толщины
            double halfThickness = thickness / 2.0;

            // 1. Ось стены (Centerline)
            {
                WallReferenceLine line;
                line.Start = start;
                line.End = end;
                line.Plane = WallSnapPlane::Centerline;
                line.WallId = wall.GetId();
                lines.push_back(line);
            }

            // 2. Наружная чистовая поверхность (Finish Face Exterior)
            {
                WallReferenceLine line;
                line.Start = WorldPoint(start.X + perpX * halfThickness, start.Y + perpY * halfThickness);
                line.End = WorldPoint(end.X + perpX * halfThickness, end.Y + perpY * halfThickness);
                line.Plane = WallSnapPlane::FinishFaceExterior;
                line.WallId = wall.GetId();
                lines.push_back(line);
            }

            // 3. Внутренняя чистовая поверхность (Finish Face Interior)
            {
                WallReferenceLine line;
                line.Start = WorldPoint(start.X - perpX * halfThickness, start.Y - perpY * halfThickness);
                line.End = WorldPoint(end.X - perpX * halfThickness, end.Y - perpY * halfThickness);
                line.Plane = WallSnapPlane::FinishFaceInterior;
                line.WallId = wall.GetId();
                lines.push_back(line);
            }

            // 4. Поверхности ядра (если есть тип стены с несколькими слоями)
            auto wallType = wall.GetType();
            if (wallType && wallType->GetLayerCount() > 1)
            {
                double coreThickness = wallType->GetCoreThickness();
                double finishThickness = (thickness - coreThickness) / 2.0;
                double halfCoreThickness = coreThickness / 2.0;

                // Ось несущего слоя (Core Centerline)
                {
                    WallReferenceLine line;
                    line.Start = start;
                    line.End = end;
                    line.Plane = WallSnapPlane::CoreCenterline;
                    line.WallId = wall.GetId();
                    lines.push_back(line);
                }

                // Наружная поверхность ядра (Core Face Exterior)
                {
                    WallReferenceLine line;
                    line.Start = WorldPoint(
                        start.X + perpX * halfCoreThickness,
                        start.Y + perpY * halfCoreThickness);
                    line.End = WorldPoint(
                        end.X + perpX * halfCoreThickness,
                        end.Y + perpY * halfCoreThickness);
                    line.Plane = WallSnapPlane::CoreFaceExterior;
                    line.WallId = wall.GetId();
                    lines.push_back(line);
                }

                // Внутренняя поверхность ядра (Core Face Interior)
                {
                    WallReferenceLine line;
                    line.Start = WorldPoint(
                        start.X - perpX * halfCoreThickness,
                        start.Y - perpY * halfCoreThickness);
                    line.End = WorldPoint(
                        end.X - perpX * halfCoreThickness,
                        end.Y - perpY * halfCoreThickness);
                    line.Plane = WallSnapPlane::CoreFaceInterior;
                    line.WallId = wall.GetId();
                    lines.push_back(line);
                }
            }

            return lines;
        }

        // ============================================================
        // Утилиты
        // ============================================================

        // Название плоскости привязки (для UI)
        static std::wstring GetSnapPlaneName(WallSnapPlane plane)
        {
            switch (plane)
            {
            case WallSnapPlane::None: return L"Нет";
            case WallSnapPlane::Endpoint: return L"Конечная точка";
            case WallSnapPlane::Midpoint: return L"Середина";
            case WallSnapPlane::Centerline: return L"Ось стены";
            case WallSnapPlane::CoreCenterline: return L"Ось ядра";
            case WallSnapPlane::FinishFaceExterior: return L"Наружная грань";
            case WallSnapPlane::FinishFaceInterior: return L"Внутренняя грань";
            case WallSnapPlane::CoreFaceExterior: return L"Ядро (наружн.)";
            case WallSnapPlane::CoreFaceInterior: return L"Ядро (внутр.)";
            case WallSnapPlane::AlongFace: return L"Вдоль грани";
            default: return L"";
            }
        }

        // Короткое название для тултипа
        static std::wstring GetSnapPlaneShortName(WallSnapPlane plane)
        {
            switch (plane)
            {
            case WallSnapPlane::None: return L"";
            case WallSnapPlane::Endpoint: return L"Точка";
            case WallSnapPlane::Midpoint: return L"Середина";
            case WallSnapPlane::Centerline: return L"Ось";
            case WallSnapPlane::CoreCenterline: return L"Ось ядра";
            case WallSnapPlane::FinishFaceExterior: return L"Нар. грань";
            case WallSnapPlane::FinishFaceInterior: return L"Вн. грань";
            case WallSnapPlane::CoreFaceExterior: return L"Ядро нар.";
            case WallSnapPlane::CoreFaceInterior: return L"Ядро вн.";
            case WallSnapPlane::AlongFace: return L"Грань";
            default: return L"";
            }
        }

        // Название режима привязки
        static std::wstring GetReferenceModeName(SnapReferenceMode mode)
        {
            switch (mode)
            {
            case SnapReferenceMode::Auto: return L"Авто";
            case SnapReferenceMode::Centerline: return L"Ось стены";
            case SnapReferenceMode::FinishExterior: return L"Наружная грань";
            case SnapReferenceMode::FinishInterior: return L"Внутренняя грань";
            case SnapReferenceMode::CoreExterior: return L"Ядро (наружн.)";
            case SnapReferenceMode::CoreInterior: return L"Ядро (внутр.)";
            default: return L"Авто";
            }
        }

        // Цвет для плоскости привязки
        static Windows::UI::Color GetSnapPlaneColor(WallSnapPlane plane)
        {
            switch (plane)
            {
            case WallSnapPlane::Endpoint:
            case WallSnapPlane::Midpoint:
                return Windows::UI::ColorHelper::FromArgb(255, 255, 0, 255); // Magenta
            case WallSnapPlane::Centerline:
            case WallSnapPlane::CoreCenterline:
                return Windows::UI::ColorHelper::FromArgb(255, 0, 200, 255); // Cyan
            case WallSnapPlane::FinishFaceExterior:
            case WallSnapPlane::CoreFaceExterior:
                return Windows::UI::ColorHelper::FromArgb(255, 0, 255, 100); // Green
            case WallSnapPlane::FinishFaceInterior:
            case WallSnapPlane::CoreFaceInterior:
                return Windows::UI::ColorHelper::FromArgb(255, 255, 150, 0); // Orange
            default:
                return Windows::UI::ColorHelper::FromArgb(255, 255, 255, 0); // Yellow
            }
        }

    private:
        double m_snapThresholdPx{ 15.0 };       // Порог привязки к линиям (пиксели)
        double m_endpointThresholdPx{ 12.0 };   // Порог привязки к точкам (пиксели)
        SnapReferenceMode m_referenceMode{ SnapReferenceMode::Auto };
        bool m_faceSnapEnabled{ true };

        // Проверка, разрешена ли плоскость текущим режимом
        bool IsPlaneAllowedByMode(WallSnapPlane plane) const
        {
            if (m_referenceMode == SnapReferenceMode::Auto)
                return true;

            switch (m_referenceMode)
            {
            case SnapReferenceMode::Centerline:
                return plane == WallSnapPlane::Centerline || 
                       plane == WallSnapPlane::Endpoint ||
                       plane == WallSnapPlane::Midpoint;
            
            case SnapReferenceMode::FinishExterior:
                return plane == WallSnapPlane::FinishFaceExterior ||
                       plane == WallSnapPlane::Endpoint;
            
            case SnapReferenceMode::FinishInterior:
                return plane == WallSnapPlane::FinishFaceInterior ||
                       plane == WallSnapPlane::Endpoint;
            
            case SnapReferenceMode::CoreExterior:
                return plane == WallSnapPlane::CoreFaceExterior ||
                       plane == WallSnapPlane::Endpoint;
            
            case SnapReferenceMode::CoreInterior:
                return plane == WallSnapPlane::CoreFaceInterior ||
                       plane == WallSnapPlane::Endpoint;
            
            default:
                return true;
            }
        }
    };
}
