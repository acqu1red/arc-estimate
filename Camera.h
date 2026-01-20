#pragma once

#include <cmath>

namespace winrt::estimate1
{
    // Структура для представления точки в мировых координатах (мм)
    struct WorldPoint
    {
        double X{ 0.0 };
        double Y{ 0.0 };

        WorldPoint() = default;
        WorldPoint(double x, double y) : X(x), Y(y) {}

        WorldPoint operator+(const WorldPoint& other) const {
            return WorldPoint(X + other.X, Y + other.Y);
        }

        WorldPoint operator-(const WorldPoint& other) const {
            return WorldPoint(X - other.X, Y - other.Y);
        }

        WorldPoint operator*(double scale) const {
            return WorldPoint(X * scale, Y * scale);
        }

        double Distance(const WorldPoint& other) const {
            double dx = X - other.X;
            double dy = Y - other.Y;
            return std::sqrt(dx * dx + dy * dy);
        }
    };

    // Структура для представления точки на экране (пиксели)
    struct ScreenPoint
    {
        float X{ 0.0f };
        float Y{ 0.0f };

        ScreenPoint() = default;
        ScreenPoint(float x, float y) : X(x), Y(y) {}
    };

    // Класс камеры для преобразования между мировыми и экранными координатами
    class Camera
    {
    public:
        Camera() = default;

        // Получение/установка смещения (pan) в мировых единицах (мм)
        WorldPoint GetOffset() const { return m_offset; }
        void SetOffset(WorldPoint offset) { m_offset = offset; }
        void SetOffset(double x, double y) { m_offset = WorldPoint(x, y); }

        // Получение/установка масштаба (пикселей на мм)
        double GetZoom() const { return m_zoom; }
        void SetZoom(double zoom) { 
            m_zoom = std::clamp(zoom, m_minZoom, m_maxZoom); 
        }

        // Получение размера холста
        float GetCanvasWidth() const { return m_canvasWidth; }
        float GetCanvasHeight() const { return m_canvasHeight; }
        void SetCanvasSize(float width, float height) {
            m_canvasWidth = width;
            m_canvasHeight = height;
        }

        // Преобразование мировых координат в экранные
        ScreenPoint WorldToScreen(const WorldPoint& world) const
        {
            // Use camera-relative coordinates for better precision at high zoom
            // Screen = (World - CameraCenter) * Zoom + CanvasCenter
            double cameraCenterX = -m_offset.X;
            double cameraCenterY = -m_offset.Y;

            float centerX = m_canvasWidth / 2.0f;
            float centerY = m_canvasHeight / 2.0f;

            double localX = world.X - cameraCenterX;
            double localY = world.Y - cameraCenterY;

            float screenX = static_cast<float>(localX * m_zoom) + centerX;
            float screenY = static_cast<float>(localY * m_zoom) + centerY;

            return ScreenPoint(screenX, screenY);
        }

        // Преобразование экранных координат в мировые
        WorldPoint ScreenToWorld(const ScreenPoint& screen) const
        {
            float centerX = m_canvasWidth / 2.0f;
            float centerY = m_canvasHeight / 2.0f;

            double localX = (screen.X - centerX) / m_zoom;
            double localY = (screen.Y - centerY) / m_zoom;

            double cameraCenterX = -m_offset.X;
            double cameraCenterY = -m_offset.Y;

            return WorldPoint(localX + cameraCenterX, localY + cameraCenterY);
        }

        // Панорамирование на дельту в экранных координатах
        void Pan(float deltaScreenX, float deltaScreenY)
        {
            // Преобразуем экранную дельту в мировую
            m_offset.X += deltaScreenX / m_zoom;
            m_offset.Y += deltaScreenY / m_zoom;
        }

        // Масштабирование с центром в указанной экранной точке
        void ZoomAt(const ScreenPoint& screenPoint, double zoomFactor)
        {
            // Запоминаем мировую точку под курсором
            WorldPoint worldPoint = ScreenToWorld(screenPoint);

            // Применяем масштаб
            double newZoom = std::clamp(m_zoom * zoomFactor, m_minZoom, m_maxZoom);
            
            if (newZoom == m_zoom) return; // Достигнут предел
            
            m_zoom = newZoom;

            // Корректируем смещение, чтобы мировая точка осталась под курсором
            float centerX = m_canvasWidth / 2.0f;
            float centerY = m_canvasHeight / 2.0f;

            m_offset.X = (screenPoint.X - centerX) / m_zoom - worldPoint.X;
            m_offset.Y = (screenPoint.Y - centerY) / m_zoom - worldPoint.Y;
        }

        // Сброс камеры в начальное положение
        void Reset()
        {
            m_offset = WorldPoint(0, 0);
            m_zoom = m_defaultZoom;
        }

        // Получение видимой области в мировых координатах
        void GetVisibleBounds(WorldPoint& topLeft, WorldPoint& bottomRight) const
        {
            topLeft = ScreenToWorld(ScreenPoint(0, 0));
            bottomRight = ScreenToWorld(ScreenPoint(m_canvasWidth, m_canvasHeight));
        }

    private:
        WorldPoint m_offset{ 0, 0 };           // Смещение в мировых единицах (мм)
        double m_zoom{ 0.5 };                   // Масштаб: пикселей на мм (0.5 = 1 пиксель = 2 мм)
        float m_canvasWidth{ 800.0f };
        float m_canvasHeight{ 600.0f };

        // Пределы масштабирования
        static constexpr double m_minZoom = 0.01;   // Минимум: 1 пиксель = 100 мм
        static constexpr double m_maxZoom = 10.0;   // Максимум: 10 пикселей = 1 мм
        static constexpr double m_defaultZoom = 0.5; // По умолчанию: 1 пиксель = 2 мм
    };
}
