#pragma once

// ARC-Estimate Wall Type Editor Dialog (M6 - deferred feature)
// Редактор типов стен

#include "pch.h"
#include "Material.h"
#include "WallType.h"
#include "Element.h"
#include <vector>
#include <memory>

namespace winrt::estimate1
{
    // Результат редактирования типа стен
    struct WallTypeEditorResult
    {
        bool Accepted{ false };
        bool Modified{ false };
    };

    // Класс для управления редактированием типов стен
    // Инкапсулирует логику редактирования типов стен для отображения в ContentDialog
    class WallTypeEditorController
    {
    public:
        WallTypeEditorController(DocumentModel& document)
            : m_document(document)
        {
            RefreshTypes();
            RefreshMaterials();
        }

        // Обновить список типов
        void RefreshTypes()
        {
            m_types.clear();
            for (const auto& wt : m_document.GetWallTypes())
            {
                if (wt) m_types.push_back(wt);
            }
        }

        // Обновить список материалов
        void RefreshMaterials()
        {
            m_materials.clear();
            for (const auto& mat : m_document.GetMaterials())
            {
                if (mat) m_materials.push_back(mat);
            }
        }

        // Получить все типы стен
        const std::vector<std::shared_ptr<WallType>>& GetTypes() const { return m_types; }

        // Получить все материалы
        const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return m_materials; }

        // Получить выбранный тип стены
        std::shared_ptr<WallType> GetSelectedType() const { return m_selectedType; }

        // Выбрать тип стены по индексу
        void SelectType(int index)
        {
            if (index >= 0 && index < static_cast<int>(m_types.size()))
            {
                m_selectedType = m_types[index];
            }
            else
            {
                m_selectedType = nullptr;
            }
        }

        // Создать новый тип стены
        std::shared_ptr<WallType> CreateNewType(const std::wstring& name)
        {
            auto newType = std::make_shared<WallType>(name);
            m_document.AddWallType(newType);
            RefreshTypes();
            m_selectedType = newType;
            return newType;
        }

        // Удалить выбранный тип стены
        bool DeleteSelectedType()
        {
            if (!m_selectedType) return false;
            
            bool removed = m_document.RemoveWallType(m_selectedType->GetName());
            if (removed)
            {
                m_selectedType = nullptr;
                RefreshTypes();
            }
            return removed;
        }

        // Переименовать выбранный тип
        bool RenameSelectedType(const std::wstring& newName)
        {
            if (!m_selectedType || newName.empty()) return false;
            m_selectedType->SetName(newName);
            return true;
        }

        // Добавить слой к выбранному типу
        bool AddLayerToSelected(const std::wstring& name, double thickness, std::shared_ptr<Material> material = nullptr)
        {
            if (!m_selectedType) return false;
            
            WallLayer layer(name, thickness, material);
            m_selectedType->AddLayer(layer);
            return true;
        }

        // Удалить слой из выбранного типа
        bool RemoveLayerFromSelected(int layerIndex)
        {
            if (!m_selectedType) return false;
            return m_selectedType->RemoveLayer(layerIndex);
        }

        // Изменить слой в выбранном типе
        bool ModifyLayerInSelected(int layerIndex, const std::wstring& name, double thickness, std::shared_ptr<Material> material)
        {
            if (!m_selectedType) return false;
            
            auto& layers = m_selectedType->GetLayersForEdit();
            if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
                return false;

            layers[layerIndex].Name = name;
            layers[layerIndex].Thickness = thickness;
            layers[layerIndex].MaterialRef = material;
            return true;
        }

        // Создать новый материал
        std::shared_ptr<Material> CreateNewMaterial(const std::wstring& name)
        {
            auto newMat = std::make_shared<Material>(IdGenerator::Next(), name);
            m_document.AddMaterial(newMat);
            RefreshMaterials();
            return newMat;
        }

        // Найти материал по имени
        std::shared_ptr<Material> FindMaterialByName(const std::wstring& name) const
        {
            for (const auto& mat : m_materials)
            {
                if (mat && mat->Name == name)
                    return mat;
            }
            return nullptr;
        }

    private:
        DocumentModel& m_document;
        std::vector<std::shared_ptr<WallType>> m_types;
        std::vector<std::shared_ptr<Material>> m_materials;
        std::shared_ptr<WallType> m_selectedType;
    };

    // Хелпер для формирования строки слоёв
    inline std::wstring FormatWallTypeLayers(const WallType& wt)
    {
        std::wstring result;
        const auto& layers = wt.GetLayers();
        for (size_t i = 0; i < layers.size(); ++i)
        {
            if (i > 0) result += L"\n";
            result += L"• " + layers[i].Name + L": ";
            result += std::to_wstring(static_cast<int>(layers[i].Thickness)) + L" мм";
            if (layers[i].MaterialRef)
            {
                result += L" (" + layers[i].MaterialRef->Name + L")";
            }
        }
        return result;
    }

    // Хелпер для формирования краткого описания типа
    inline std::wstring FormatWallTypeSummary(const WallType& wt)
    {
        return wt.GetName() + L" — " + 
               std::to_wstring(static_cast<int>(wt.GetTotalThickness())) + L" мм";
    }
}
