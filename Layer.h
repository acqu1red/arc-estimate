#pragma once

#include "pch.h"
#include <vector>
#include <functional>

namespace winrt::estimate1
{
    // Перечисление рабочих состояний (дублируем из IDL для C++ использования)
    enum class WorkStateNative
    {
        Existing,   // Существующее
        Demolish,   // Под снос
        New         // Новое
    };

    // Класс слоя для управления видимостью и редактированием элементов
    class Layer
    {
    public:
        Layer() = default;

        Layer(
            const std::wstring& name,
            WorkStateNative linkedWorkState,
            Windows::UI::Color color)
            : m_name(name)
            , m_linkedWorkState(linkedWorkState)
            , m_color(color)
        {
        }

        // Имя слоя
        std::wstring GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        // Видимость слоя
        bool IsVisible() const { return m_isVisible; }
        void SetVisible(bool visible) { m_isVisible = visible; }

        // Заблокирован ли слой для редактирования
        bool IsLocked() const { return m_isLocked; }
        void SetLocked(bool locked) { m_isLocked = locked; }

        // Цвет слоя для отображения
        Windows::UI::Color GetColor() const { return m_color; }
        void SetColor(Windows::UI::Color color) { m_color = color; }

        // Связанное рабочее состояние
        WorkStateNative GetLinkedWorkState() const { return m_linkedWorkState; }
        void SetLinkedWorkState(WorkStateNative state) { m_linkedWorkState = state; }

        // Активен ли слой (для рисования новых элементов)
        bool IsActive() const { return m_isActive; }
        void SetActive(bool active) { m_isActive = active; }

    private:
        std::wstring m_name{ L"Новый слой" };
        bool m_isVisible{ true };
        bool m_isLocked{ false };
        bool m_isActive{ false };
        Windows::UI::Color m_color{ Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0) };
        WorkStateNative m_linkedWorkState{ WorkStateNative::Existing };
    };

    // Перечисление видов плана (для C++ использования)
    enum class PlanViewNative
    {
        Measure,      // Обмерный план
        Demolition,   // План демонтажа
        Construction  // План нового строительства
    };

    // Менеджер слоёв
    class LayerManager
    {
    public:
        LayerManager()
        {
            InitializeDefaultLayers();
        }

        // Инициализация стандартных слоёв
        void InitializeDefaultLayers()
        {
            m_layers.clear();

            // Слой существующих элементов (чёрный)
            m_layers.push_back(Layer(
                L"Существующее",
                WorkStateNative::Existing,
                Windows::UI::ColorHelper::FromArgb(255, 40, 40, 40)
            ));

            // Слой элементов под снос (красный)
            m_layers.push_back(Layer(
                L"Демонтаж",
                WorkStateNative::Demolish,
                Windows::UI::ColorHelper::FromArgb(255, 200, 50, 50)
            ));

            // Слой новых элементов (синий)
            m_layers.push_back(Layer(
                L"Новое",
                WorkStateNative::New,
                Windows::UI::ColorHelper::FromArgb(255, 50, 100, 200)
            ));

            // По умолчанию первый слой активен
            if (!m_layers.empty())
            {
                m_layers[0].SetActive(true);
                m_activeLayerIndex = 0;
            }
        }

        // Получить все слои
        const std::vector<Layer>& GetLayers() const { return m_layers; }
        std::vector<Layer>& GetLayers() { return m_layers; }

        // Получить слой по индексу
        Layer* GetLayer(size_t index)
        {
            if (index < m_layers.size())
                return &m_layers[index];
            return nullptr;
        }

        // Получить слой по рабочему состоянию
        Layer* GetLayerByWorkState(WorkStateNative workState)
        {
            for (auto& layer : m_layers)
            {
                if (layer.GetLinkedWorkState() == workState)
                    return &layer;
            }
            return nullptr;
        }

        // Получить активный слой
        Layer* GetActiveLayer()
        {
            if (m_activeLayerIndex < m_layers.size())
                return &m_layers[m_activeLayerIndex];
            return nullptr;
        }

        // Установить активный слой
        void SetActiveLayer(size_t index)
        {
            if (index < m_layers.size())
            {
                // Сбрасываем предыдущий активный
                if (m_activeLayerIndex < m_layers.size())
                    m_layers[m_activeLayerIndex].SetActive(false);

                m_activeLayerIndex = index;
                m_layers[index].SetActive(true);
            }
        }

        // Применить настройки видимости для выбранного вида плана
        void ApplyViewVisibility(PlanViewNative view)
        {
            m_currentView = view;

            for (auto& layer : m_layers)
            {
                WorkStateNative state = layer.GetLinkedWorkState();
                bool shouldBeVisible = false;

                switch (view)
                {
                case PlanViewNative::Measure:
                    // Обмерный план: только существующее
                    shouldBeVisible = (state == WorkStateNative::Existing);
                    break;

                case PlanViewNative::Demolition:
                    // План демонтажа: существующее + под снос
                    shouldBeVisible = (state == WorkStateNative::Existing || 
                                       state == WorkStateNative::Demolish);
                    break;

                case PlanViewNative::Construction:
                    // План нового строительства: существующее + новое
                    shouldBeVisible = (state == WorkStateNative::Existing || 
                                       state == WorkStateNative::New);
                    break;
                }

                layer.SetVisible(shouldBeVisible);
            }
        }

        // Получить текущий вид
        PlanViewNative GetCurrentView() const { return m_currentView; }

        // Проверить, виден ли элемент с данным WorkState
        bool IsWorkStateVisible(WorkStateNative workState) const
        {
            for (const auto& layer : m_layers)
            {
                if (layer.GetLinkedWorkState() == workState)
                    return layer.IsVisible();
            }
            return false;
        }

        // Получить цвет для WorkState
        Windows::UI::Color GetColorForWorkState(WorkStateNative workState) const
        {
            for (const auto& layer : m_layers)
            {
                if (layer.GetLinkedWorkState() == workState)
                    return layer.GetColor();
            }
            return Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0);
        }

        // Количество слоёв
        size_t GetLayerCount() const { return m_layers.size(); }

        // Callback для уведомления об изменениях
        void SetOnLayerChanged(std::function<void()> callback)
        {
            m_onLayerChanged = callback;
        }

        void NotifyLayerChanged()
        {
            if (m_onLayerChanged)
                m_onLayerChanged();
        }

    private:
        std::vector<Layer> m_layers;
        size_t m_activeLayerIndex{ 0 };
        PlanViewNative m_currentView{ PlanViewNative::Measure };
        std::function<void()> m_onLayerChanged;
    };
}
