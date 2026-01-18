#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "Layer.h"
#include <functional>
#include <cmath>

namespace winrt::estimate1
{
    // Результат привязки (snap)
    struct SnapResult
    {
        bool hasSnap{ false };
        WorldPoint point{ 0, 0 };
        std::wstring snapType{ L"" }; // "endpoint", "midpoint", "grid", "perpendicular"
    };

    // Менеджер привязок
    class SnapManager
    {
    public:
        SnapManager() = default;

        // Настройки привязки
        void SetGridSnapEnabled(bool enabled) { m_gridSnapEnabled = enabled; }
        bool IsGridSnapEnabled() const { return m_gridSnapEnabled; }

        void SetEndpointSnapEnabled(bool enabled) { m_endpointSnapEnabled = enabled; }
        bool IsEndpointSnapEnabled() const { return m_endpointSnapEnabled; }

        void SetMidpointSnapEnabled(bool enabled) { m_midpointSnapEnabled = enabled; }
        bool IsMidpointSnapEnabled() const { return m_midpointSnapEnabled; }

        void SetSnapTolerance(double tolerance) { m_snapTolerance = tolerance; }
        double GetSnapTolerance() const { return m_snapTolerance; }

        void SetGridSpacing(double spacing) { m_gridSpacing = spacing; }
        double GetGridSpacing() const { return m_gridSpacing; }

        // Поиск точки привязки
        SnapResult FindSnap(
            const WorldPoint& cursorWorld,
            const DocumentModel& document,
            const LayerManager& layerManager,
            const Camera& camera)
        {
            SnapResult result;
            double bestDistance = m_snapTolerance;

            // Преобразуем допуск из пикселей в мировые единицы
            double worldTolerance = m_snapTolerance / camera.GetZoom();

            // 1. Привязка к конечным точкам стен
            if (m_endpointSnapEnabled)
            {
                for (const auto& wall : document.GetWalls())
                {
                    if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                        continue;

                    // Начальная точка
                    double distStart = cursorWorld.Distance(wall->GetStartPoint());
                    if (distStart < worldTolerance && distStart < bestDistance)
                    {
                        bestDistance = distStart;
                        result.hasSnap = true;
                        result.point = wall->GetStartPoint();
                        result.snapType = L"endpoint";
                    }

                    // Конечная точка
                    double distEnd = cursorWorld.Distance(wall->GetEndPoint());
                    if (distEnd < worldTolerance && distEnd < bestDistance)
                    {
                        bestDistance = distEnd;
                        result.hasSnap = true;
                        result.point = wall->GetEndPoint();
                        result.snapType = L"endpoint";
                    }
                }
            }

            // 2. Привязка к серединам стен
            if (m_midpointSnapEnabled && !result.hasSnap)
            {
                for (const auto& wall : document.GetWalls())
                {
                    if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                        continue;

                    WorldPoint midpoint(
                        (wall->GetStartPoint().X + wall->GetEndPoint().X) / 2,
                        (wall->GetStartPoint().Y + wall->GetEndPoint().Y) / 2
                    );

                    double distMid = cursorWorld.Distance(midpoint);
                    if (distMid < worldTolerance && distMid < bestDistance)
                    {
                        bestDistance = distMid;
                        result.hasSnap = true;
                        result.point = midpoint;
                        result.snapType = L"midpoint";
                    }
                }
            }

            // 3. Привязка к сетке
            if (m_gridSnapEnabled && !result.hasSnap)
            {
                WorldPoint gridPoint = SnapToGrid(cursorWorld);
                double distGrid = cursorWorld.Distance(gridPoint);
                
                if (distGrid < worldTolerance)
                {
                    result.hasSnap = true;
                    result.point = gridPoint;
                    result.snapType = L"grid";
                }
            }

            return result;
        }

        // Привязка точки к сетке
        WorldPoint SnapToGrid(const WorldPoint& point) const
        {
            return WorldPoint(
                std::round(point.X / m_gridSpacing) * m_gridSpacing,
                std::round(point.Y / m_gridSpacing) * m_gridSpacing
            );
        }

    private:
        bool m_gridSnapEnabled{ true };
        bool m_endpointSnapEnabled{ true };
        bool m_midpointSnapEnabled{ true };
        double m_snapTolerance{ 15.0 };    // Допуск в пикселях
        double m_gridSpacing{ 100.0 };      // Шаг сетки в мм
    };

    // Состояние инструмента рисования стен
    enum class WallToolState
    {
        Idle,           // Ожидание первого клика
        Drawing,        // Рисование (указана начальная точка)
        ChainDrawing    // Продолжение цепочки стен
    };

    // Инструмент рисования стен
    class WallTool
    {
    public:
        WallTool() = default;

        // Состояние инструмента
        WallToolState GetState() const { return m_state; }

        // Текущая начальная точка
        WorldPoint GetStartPoint() const { return m_startPoint; }

        // Текущая толщина стены
        double GetThickness() const { return m_thickness; }
        void SetThickness(double thickness) { m_thickness = std::clamp(thickness, 50.0, 1000.0); }

        // Текущий WorkState для новых стен
        WorkStateNative GetWorkState() const { return m_workState; }
        void SetWorkState(WorkStateNative state) { m_workState = state; }

        // Текущий LocationLineMode
        LocationLineMode GetLocationLineMode() const { return m_locationLineMode; }
        void SetLocationLineMode(LocationLineMode mode) { m_locationLineMode = mode; }

        // Флаг "перевернуть стену" (Spacebar flip)
        bool IsFlipped() const { return m_isFlipped; }
        void SetFlipped(bool flipped) { m_isFlipped = flipped; }
        void ToggleFlip() { m_isFlipped = !m_isFlipped; }

        // Обработка клика мыши
        // Возвращает true, если стена была создана
        bool OnClick(
            const WorldPoint& worldPoint,
            DocumentModel& document,
            SnapManager& snapManager,
            const LayerManager& layerManager,
            const Camera& camera)
        {
            // Применяем привязку
            SnapResult snap = snapManager.FindSnap(worldPoint, document, layerManager, camera);
            WorldPoint finalPoint = snap.hasSnap ? snap.point : worldPoint;

            switch (m_state)
            {
            case WallToolState::Idle:
                // Начинаем рисование — запоминаем начальную точку
                m_startPoint = finalPoint;
                m_isFlipped = false; // Сбрасываем флип при начале новой стены
                m_state = WallToolState::Drawing;
                return false;

            case WallToolState::Drawing:
            case WallToolState::ChainDrawing:
                // Завершаем стену
                if (m_startPoint.Distance(finalPoint) > 10.0) // Минимальная длина 10мм
                {
                    // Применяем flip если активен
                    WorldPoint effectiveStart = m_startPoint;
                    WorldPoint effectiveEnd = finalPoint;
                    
                    if (m_isFlipped)
                    {
                        // При flip меняем местами start и end
                        std::swap(effectiveStart, effectiveEnd);
                    }

                    Wall* newWall = document.AddWall(effectiveStart, effectiveEnd, m_thickness);
                    newWall->SetWorkState(m_workState);
                    newWall->SetLocationLineMode(m_locationLineMode);

                    // Продолжаем цепочку — конец текущей стены = начало следующей
                    m_startPoint = finalPoint;
                    m_isFlipped = false; // Сбрасываем flip для следующей стены
                    m_state = WallToolState::ChainDrawing;

                    if (m_onWallCreated)
                        m_onWallCreated(newWall);

                    return true;
                }
                return false;
            }

            return false;
        }

        // Обработка движения мыши (для превью)
        void OnMouseMove(const WorldPoint& worldPoint)
        {
            m_currentPoint = worldPoint;
        }

        // Отмена текущего рисования
        void Cancel()
        {
            m_state = WallToolState::Idle;
            m_startPoint = WorldPoint(0, 0);
            m_currentPoint = WorldPoint(0, 0);
            m_isFlipped = false;
        }

        // Завершение цепочки (двойной клик или ESC)
        void EndChain()
        {
            m_state = WallToolState::Idle;
            m_isFlipped = false;
        }

        // Проверка, нужно ли рисовать превью
        bool ShouldDrawPreview() const
        {
            return m_state == WallToolState::Drawing || m_state == WallToolState::ChainDrawing;
        }

        // Получение текущей точки курсора
        WorldPoint GetCurrentPoint() const { return m_currentPoint; }

        // Получить эффективные точки с учётом flip
        void GetEffectivePoints(WorldPoint& outStart, WorldPoint& outEnd) const
        {
            if (m_isFlipped)
            {
                outStart = m_currentPoint;
                outEnd = m_startPoint;
            }
            else
            {
                outStart = m_startPoint;
                outEnd = m_currentPoint;
            }
        }

        // Callback при создании стены
        void SetOnWallCreated(std::function<void(Wall*)> callback)
        {
            m_onWallCreated = callback;
        }

    private:
        WallToolState m_state{ WallToolState::Idle };
        WorldPoint m_startPoint{ 0, 0 };
        WorldPoint m_currentPoint{ 0, 0 };
        double m_thickness{ 150.0 };
        WorkStateNative m_workState{ WorkStateNative::Existing };
        LocationLineMode m_locationLineMode{ LocationLineMode::WallCenterline };
        bool m_isFlipped{ false };
        std::function<void(Wall*)> m_onWallCreated;
    };

    // Инструмент выделения
    class SelectTool
        {
        public:
            SelectTool() = default;

            // Обработка клика
            Element* OnClick(
                const WorldPoint& worldPoint,
                DocumentModel& document,
                const LayerManager& layerManager)
            {
                // Допуск для попадания (в мм)
                double tolerance = 20.0;

                // Ищем элемент под курсором
                Element* hitElement = document.HitTest(worldPoint, tolerance, layerManager);

                if (hitElement)
                {
                    document.SetSelectedElement(hitElement);
                }
                else
                {
                    document.ClearSelection();
                }

                return hitElement;
            }
        };

        // Состояние инструмента размеров
        enum class DimensionToolState
        {
            Idle,       // Ожидание первого клика (первая точка)
            Drawing     // Ожидание второго клика (вторая точка)
        };

        // Инструмент ручного размещения размеров
        class DimensionTool
        {
        public:
            DimensionTool() = default;

            DimensionToolState GetState() const { return m_state; }

            WorldPoint GetStartPoint() const { return m_startPoint; }
            WorldPoint GetCurrentPoint() const { return m_currentPoint; }

            double GetOffset() const { return m_offset; }
            void SetOffset(double offset) { m_offset = offset; }

            bool ShouldDrawPreview() const { return m_state == DimensionToolState::Drawing; }

            // Обработка клика
            // Возвращает true, если размер был создан
            bool OnClick(
                const WorldPoint& worldPoint,
                DocumentModel& document,
                SnapManager& snapManager,
                const LayerManager& layerManager,
                const Camera& camera)
            {
                // Применяем привязку
                SnapResult snap = snapManager.FindSnap(worldPoint, document, layerManager, camera);
                WorldPoint finalPoint = snap.hasSnap ? snap.point : worldPoint;

                switch (m_state)
                {
                case DimensionToolState::Idle:
                    // Начинаем — запоминаем первую точку
                    m_startPoint = finalPoint;
                    m_currentPoint = finalPoint;
                    m_state = DimensionToolState::Drawing;
                    return false;

                case DimensionToolState::Drawing:
                    // Завершаем размер
                    if (m_startPoint.Distance(finalPoint) > 10.0) // Минимальная длина 10мм
                    {
                        Dimension* newDim = document.AddManualDimension(m_startPoint, finalPoint, m_offset);
                    
                        if (m_onDimensionCreated)
                            m_onDimensionCreated(newDim);

                        // Сбрасываем в Idle
                        m_state = DimensionToolState::Idle;
                        m_startPoint = WorldPoint(0, 0);
                        m_currentPoint = WorldPoint(0, 0);

                        return true;
                    }
                    return false;
                }

                return false;
            }

            // Обработка движения мыши
            void OnMouseMove(const WorldPoint& worldPoint)
            {
                m_currentPoint = worldPoint;
            }

            // Отмена
            void Cancel()
            {
                m_state = DimensionToolState::Idle;
                m_startPoint = WorldPoint(0, 0);
                m_currentPoint = WorldPoint(0, 0);
            }

            // Callback при создании размера
            void SetOnDimensionCreated(std::function<void(Dimension*)> callback)
            {
                m_onDimensionCreated = callback;
            }

        private:
            DimensionToolState m_state{ DimensionToolState::Idle };
            WorldPoint m_startPoint{ 0, 0 };
            WorldPoint m_currentPoint{ 0, 0 };
            double m_offset{ 200.0 };
            std::function<void(Dimension*)> m_onDimensionCreated;
        };
    }
