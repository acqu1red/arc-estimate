#pragma once

#include "pch.h"
#include "Material.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace winrt::estimate1
{
    // Слой стены (несущий/отделочный и т.п.)
    // Толщина в мм.
    struct WallLayer
    {
        std::wstring Name{ L"Слой" };
        double Thickness{ 0.0 }; // мм

        // Материал (опционально). Для простоты M3.1 храним shared_ptr.
        std::shared_ptr<Material> MaterialRef{};

        WallLayer() = default;

        WallLayer(std::wstring name, double thicknessMm, std::shared_ptr<Material> material = {})
            : Name(std::move(name)), Thickness(thicknessMm), MaterialRef(std::move(material))
        {
        }
    };

    // Тип стены - набор слоёв и вычисляемая суммарная толщина.
    class WallType
    {
    public:
        WallType() = default;

        explicit WallType(std::wstring name)
            : m_name(std::move(name))
        {
        }

        const std::wstring& GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        const std::vector<WallLayer>& GetLayers() const { return m_layers; }
        std::vector<WallLayer>& GetLayers() { return m_layers; }

        void ClearLayers() { m_layers.clear(); }

        void AddLayer(const WallLayer& layer)
        {
            m_layers.push_back(layer);
        }

        double GetTotalThickness() const
        {
            double sum = 0.0;
            for (const auto& layer : m_layers)
                sum += (std::max)(0.0, layer.Thickness);
            return sum;
        }

        // Толщина несущей части (core). На M3.1 считаем core-слоями всё, что не "Отделка".
        // Позже можно заменить на явный флаг в WallLayer.
        double GetCoreThickness() const
        {
            double sum = 0.0;
            for (const auto& layer : m_layers)
            {
                if (layer.Name.find(L"Отдел") != std::wstring::npos)
                    continue;
                sum += (std::max)(0.0, layer.Thickness);
            }
            return sum;
        }

        // Количество слоёв в типе стены (M5.6)
        size_t GetLayerCount() const { return m_layers.size(); }

        // Редактируемый доступ к слоям (для Wall Type Editor)
        std::vector<WallLayer>& GetLayersForEdit() { return m_layers; }

        // Удалить слой по индексу
        bool RemoveLayer(int index)
        {
            if (index < 0 || index >= static_cast<int>(m_layers.size()))
                return false;
            m_layers.erase(m_layers.begin() + index);
            return true;
        }

        // Переместить слой вверх/вниз
        bool MoveLayerUp(int index)
        {
            if (index <= 0 || index >= static_cast<int>(m_layers.size()))
                return false;
            std::swap(m_layers[index], m_layers[index - 1]);
            return true;
        }

        bool MoveLayerDown(int index)
        {
            if (index < 0 || index >= static_cast<int>(m_layers.size()) - 1)
                return false;
            std::swap(m_layers[index], m_layers[index + 1]);
            return true;
        }

    private:
        std::wstring m_name{ L"Тип стены" };
        std::vector<WallLayer> m_layers;
    };
}
