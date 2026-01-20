#pragma once

#include "pch.h"
#include "Opening.h"
#include "Models.h"

namespace winrt::estimate1
{
    // =====================================================
    // ИНСТРУМЕНТЫ ДЛЯ ДВЕРЕЙ И ОКОН
    // =====================================================

    // Результат поиска стены под курсором
    struct WallHitInfo
    {
        Wall* wall{ nullptr };
        double positionOnWall{ 0.5 };   // 0.0-1.0
        WorldPoint hitPoint;            // Точка попадания
        bool isValid{ false };          // Можно ли разместить
        double distanceFromWall{ 0 };   // Расстояние до оси стены
    };

    // =====================================================
    // ИНСТРУМЕНТ РАЗМЕЩЕНИЯ ДВЕРИ
    // =====================================================

    class DoorPlacementTool
    {
    public:
        using OnDoorCreatedCallback = std::function<void(std::shared_ptr<Door>)>;

        DoorPlacementTool() = default;

        void SetOnDoorCreated(OnDoorCreatedCallback callback) { m_onDoorCreated = callback; }

        // Установить текущий тип двери
        void SetDoorType(std::shared_ptr<DoorType> type) { m_doorType = type; }
        std::shared_ptr<DoorType> GetDoorType() const { return m_doorType; }

        // Установить размеры
        void SetWidth(double w) { m_width = w; }
        double GetWidth() const { return m_width; }

        void SetHeight(double h) { m_height = h; }
        double GetHeight() const { return m_height; }

        void SetSwingType(DoorSwingType type) { m_swingType = type; }
        DoorSwingType GetSwingType() const { return m_swingType; }

        // Флип текущего превью
        void ToggleFlip() { m_flipped = !m_flipped; }
        bool IsFlipped() const { return m_flipped; }

        // Обновить позицию мыши и найти стену
        void UpdatePreview(const WorldPoint& mousePos, const std::vector<std::shared_ptr<Wall>>& walls)
        {
            m_previewHit = FindWallAtPoint(mousePos, walls);
            m_previewPosition = mousePos;
        }

        // Получить информацию о превью
        bool HasValidPreview() const { return m_previewHit.isValid; }
        WallHitInfo GetPreviewHit() const { return m_previewHit; }
        WorldPoint GetPreviewPosition() const { return m_previewPosition; }

        // Попытаться разместить дверь
        bool TryPlace()
        {
            if (!m_previewHit.isValid || !m_previewHit.wall)
                return false;

            // Создать дверь
            auto door = std::make_shared<Door>(
                m_previewHit.wall->GetId(),
                m_previewHit.positionOnWall
            );

            door->SetWidth(m_width);
            door->SetHeight(m_height);
            door->SetSwingType(m_swingType);
            door->SetFlipped(m_flipped);
            door->SetWorkState(m_previewHit.wall->GetWorkState());

            if (m_onDoorCreated)
            {
                m_onDoorCreated(door);
            }

            return true;
        }

        // Отменить
        void Cancel()
        {
            m_previewHit = WallHitInfo();
        }

    private:
        // Найти стену под точкой
        WallHitInfo FindWallAtPoint(const WorldPoint& point, const std::vector<std::shared_ptr<Wall>>& walls)
        {
            WallHitInfo best;
            double bestDist = (std::numeric_limits<double>::max)();

            for (const auto& wall : walls)
            {
                // Вычислить расстояние до оси стены
                WorldPoint start = wall->GetStartPoint();
                WorldPoint end = wall->GetEndPoint();

                double dx = end.X - start.X;
                double dy = end.Y - start.Y;
                double lengthSq = dx * dx + dy * dy;

                if (lengthSq < 0.001) continue;

                // Параметр проекции
                double t = ((point.X - start.X) * dx + (point.Y - start.Y) * dy) / lengthSq;

                // Проверить, что точка в пределах стены (с учётом отступа для двери)
                double halfDoorWidth = m_width / 2;
                double wallLength = std::sqrt(lengthSq);
                double minT = halfDoorWidth / wallLength;
                double maxT = 1.0 - halfDoorWidth / wallLength;

                if (t < minT || t > maxT) continue;

                // Ближайшая точка на оси стены
                WorldPoint closest(
                    start.X + t * dx,
                    start.Y + t * dy
                );

                double dist = point.Distance(closest);

                // Проверить, что точка в пределах толщины стены
                double halfThickness = wall->GetThickness() / 2;
                if (dist <= halfThickness + 50)  // +50 мм допуск
                {
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best.wall = wall.get();
                        best.positionOnWall = t;
                        best.hitPoint = closest;
                        best.distanceFromWall = dist;
                        best.isValid = true;
                    }
                }
            }

            return best;
        }

        OnDoorCreatedCallback m_onDoorCreated;
        std::shared_ptr<DoorType> m_doorType;
        double m_width{ 900.0 };
        double m_height{ 2100.0 };
        DoorSwingType m_swingType{ DoorSwingType::RightInward };
        bool m_flipped{ false };

        WallHitInfo m_previewHit;
        WorldPoint m_previewPosition;
    };

    // =====================================================
    // ИНСТРУМЕНТ РАЗМЕЩЕНИЯ ОКНА
    // =====================================================

    class WindowPlacementTool
    {
    public:
        using OnWindowCreatedCallback = std::function<void(std::shared_ptr<Window>)>;

        WindowPlacementTool() = default;

        void SetOnWindowCreated(OnWindowCreatedCallback callback) { m_onWindowCreated = callback; }

        // Установить текущий тип окна
        void SetWindowType(std::shared_ptr<WindowType_> type) { m_windowType = type; }
        std::shared_ptr<WindowType_> GetWindowType() const { return m_windowType; }

        // Установить размеры
        void SetWidth(double w) { m_width = w; }
        double GetWidth() const { return m_width; }

        void SetHeight(double h) { m_height = h; }
        double GetHeight() const { return m_height; }

        void SetSillHeight(double h) { m_sillHeight = h; }
        double GetSillHeight() const { return m_sillHeight; }

        void SetPaneType(WindowType type) { m_paneType = type; }
        WindowType GetPaneType() const { return m_paneType; }

        // Флип
        void ToggleFlip() { m_flipped = !m_flipped; }
        bool IsFlipped() const { return m_flipped; }

        // Обновить позицию мыши
        void UpdatePreview(const WorldPoint& mousePos, const std::vector<std::shared_ptr<Wall>>& walls)
        {
            m_previewHit = FindWallAtPoint(mousePos, walls);
            m_previewPosition = mousePos;
        }

        bool HasValidPreview() const { return m_previewHit.isValid; }
        WallHitInfo GetPreviewHit() const { return m_previewHit; }
        WorldPoint GetPreviewPosition() const { return m_previewHit.hitPoint; }

        // Разместить окно
        bool TryPlace()
        {
            if (!m_previewHit.isValid || !m_previewHit.wall)
                return false;

            auto window = std::make_shared<Window>(
                m_previewHit.wall->GetId(),
                m_previewHit.positionOnWall
            );

            window->SetWidth(m_width);
            window->SetHeight(m_height);
            window->SetSillHeight(m_sillHeight);
            window->SetWindowType(m_paneType);
            window->SetFlipped(m_flipped);
            window->SetWorkState(m_previewHit.wall->GetWorkState());

            if (m_onWindowCreated)
            {
                m_onWindowCreated(window);
            }

            return true;
        }

        void Cancel()
        {
            m_previewHit = WallHitInfo();
        }

    private:
        WallHitInfo FindWallAtPoint(const WorldPoint& point, const std::vector<std::shared_ptr<Wall>>& walls)
        {
            WallHitInfo best;
            double bestDist = (std::numeric_limits<double>::max)();

            for (const auto& wall : walls)
            {
                WorldPoint start = wall->GetStartPoint();
                WorldPoint end = wall->GetEndPoint();

                double dx = end.X - start.X;
                double dy = end.Y - start.Y;
                double lengthSq = dx * dx + dy * dy;

                if (lengthSq < 0.001) continue;

                double t = ((point.X - start.X) * dx + (point.Y - start.Y) * dy) / lengthSq;

                double halfWidth = m_width / 2;
                double wallLength = std::sqrt(lengthSq);
                double minT = halfWidth / wallLength;
                double maxT = 1.0 - halfWidth / wallLength;

                if (t < minT || t > maxT) continue;

                WorldPoint closest(
                    start.X + t * dx,
                    start.Y + t * dy
                );

                double dist = point.Distance(closest);
                double halfThickness = wall->GetThickness() / 2;

                if (dist <= halfThickness + 50)
                {
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best.wall = wall.get();
                        best.positionOnWall = t;
                        best.hitPoint = closest;
                        best.distanceFromWall = dist;
                        best.isValid = true;
                    }
                }
            }

            return best;
        }

        OnWindowCreatedCallback m_onWindowCreated;
        std::shared_ptr<WindowType_> m_windowType;
        double m_width{ 1200.0 };
        double m_height{ 1400.0 };
        double m_sillHeight{ 900.0 };
        WindowType m_paneType{ WindowType::Double };
        bool m_flipped{ false };

        WallHitInfo m_previewHit;
        WorldPoint m_previewPosition;
    };
}
