#pragma once

#include "pch.h"
#include "DxfParser.h"
#include <memory>
#include <string>
#include <vector>

namespace winrt::estimate1
{
    // ============================================================================
    // DXF Reference Layer (импортированный слой как подложка)
    // ============================================================================

    // Настройки импорта DXF
    struct DxfImportSettings
    {
        // Масштаб (множитель для координат)
        double Scale{ 1.0 };

        // Смещение после импорта
        WorldPoint Offset{ 0, 0 };

        // Единицы исходного файла (если не определены автоматически)
        int OverrideUnits{ -1 };  // -1 = использовать из файла

        // Какие слои импортировать (пустой = все)
        std::vector<std::wstring> LayersToImport;

        // Назначение WorkState для импортированной геометрии
        WorkStateNative TargetWorkState{ WorkStateNative::Existing };

        // Конвертировать линии в стены
        bool ConvertLinesToWalls{ false };
        double DefaultWallThickness{ 150.0 };
        double DefaultWallHeight{ 2700.0 };
    };

    // Один слой DXF-подложки
    class DxfReferenceLayer
    {
    public:
        DxfReferenceLayer(const std::wstring& name = L"DXF Import")
            : m_name(name)
        {
        }

        // Имя
        const std::wstring& GetName() const { return m_name; }
        void SetName(const std::wstring& name) { m_name = name; }

        // Видимость
        bool IsVisible() const { return m_isVisible; }
        void SetVisible(bool visible) { m_isVisible = visible; }

        // Прозрачность (0-255)
        uint8_t GetOpacity() const { return m_opacity; }
        void SetOpacity(uint8_t opacity) { m_opacity = opacity; }

        // Цвет (для монохромного отображения)
        Windows::UI::Color GetColor() const { return m_color; }
        void SetColor(Windows::UI::Color color) { m_color = color; }

        // Использовать оригинальные цвета DXF
        bool UseOriginalColors() const { return m_useOriginalColors; }
        void SetUseOriginalColors(bool use) { m_useOriginalColors = use; }

        // Толщина линий
        float GetLineWidth() const { return m_lineWidth; }
        void SetLineWidth(float width) { m_lineWidth = width; }

        // Сущности
        const std::vector<std::unique_ptr<DxfEntity>>& GetEntities() const { return m_entities; }

        // Перемещение сущностей из DxfDocument
        void TakeEntities(std::vector<std::unique_ptr<DxfEntity>>& entities)
        {
            m_entities = std::move(entities);
        }

        // Границы
        const WorldPoint& GetMinBounds() const { return m_minBounds; }
        const WorldPoint& GetMaxBounds() const { return m_maxBounds; }

        void SetBounds(const WorldPoint& minPt, const WorldPoint& maxPt)
        {
            m_minBounds = minPt;
            m_maxBounds = maxPt;
        }

        // Количество сущностей
        size_t GetEntityCount() const { return m_entities.size(); }

        // Путь к исходному файлу
        const std::wstring& GetSourcePath() const { return m_sourcePath; }
        void SetSourcePath(const std::wstring& path) { m_sourcePath = path; }

        // M6: Для сериализации
        const std::wstring& GetFilePath() const { return m_sourcePath; }
        double GetScale() const { return m_scale; }
        void SetScale(double scale) { m_scale = scale; }
        const WorldPoint& GetOffset() const { return m_offset; }
        void SetOffset(const WorldPoint& offset) { m_offset = offset; }

    private:
        std::wstring m_name;
        std::wstring m_sourcePath;

        bool m_isVisible{ true };
        uint8_t m_opacity{ 128 };
        Windows::UI::Color m_color{ Windows::UI::ColorHelper::FromArgb(255, 100, 100, 100) };
        bool m_useOriginalColors{ false };
        float m_lineWidth{ 0.5f };
        double m_scale{ 1.0 };
        WorldPoint m_offset{ 0, 0 };

        std::vector<std::unique_ptr<DxfEntity>> m_entities;
        WorldPoint m_minBounds{ 0, 0 };
        WorldPoint m_maxBounds{ 0, 0 };
    };

    // ============================================================================
    // DXF Reference Manager (управление импортированными подложками)
    // ============================================================================

    class DxfReferenceManager
    {
    public:
        // Импорт DXF файла
        struct ImportResult
        {
            bool Success{ false };
            std::wstring ErrorMessage;
            size_t EntityCount{ 0 };
            size_t LayerIndex{ 0 };
        };

        ImportResult ImportFile(const std::wstring& filePath, const DxfImportSettings& settings = {})
        {
            ImportResult result;

            // Парсим файл
            auto parseResult = DxfParser::ParseFile(filePath);
            if (!parseResult.Success || !parseResult.Document)
            {
                result.ErrorMessage = parseResult.ErrorMessage;
                return result;
            }

            // Определяем масштаб
            double scale = settings.Scale;
            if (settings.OverrideUnits < 0)
            {
                // Используем единицы из файла
                scale *= DxfParser::GetScaleToMM(parseResult.Document->Units);
            }
            else
            {
                scale *= DxfParser::GetScaleToMM(settings.OverrideUnits);
            }

            // Применяем масштаб
            if (scale != 1.0)
            {
                parseResult.Document->ApplyScale(scale);
            }

            // Создаём слой подложки
            auto layer = std::make_unique<DxfReferenceLayer>();
            
            // Извлекаем имя файла для названия слоя
            size_t lastSlash = filePath.find_last_of(L"\\/");
            size_t lastDot = filePath.find_last_of(L'.');
            std::wstring fileName = (lastSlash != std::wstring::npos) 
                ? filePath.substr(lastSlash + 1, (lastDot != std::wstring::npos ? lastDot - lastSlash - 1 : std::wstring::npos))
                : filePath;
            
            layer->SetName(fileName);
            layer->SetSourcePath(filePath);
            layer->SetBounds(parseResult.Document->MinBounds, parseResult.Document->MaxBounds);
            layer->TakeEntities(parseResult.Document->Entities);

            result.EntityCount = layer->GetEntityCount();
            result.LayerIndex = m_layers.size();
            
            m_layers.push_back(std::move(layer));

            result.Success = true;
            return result;
        }

        // Получение слоёв
        const std::vector<std::unique_ptr<DxfReferenceLayer>>& GetLayers() const { return m_layers; }

        // Получение слоя по индексу
        DxfReferenceLayer* GetLayer(size_t index)
        {
            return (index < m_layers.size()) ? m_layers[index].get() : nullptr;
        }

        // Удаление слоя
        bool RemoveLayer(size_t index)
        {
            if (index >= m_layers.size())
                return false;

            m_layers.erase(m_layers.begin() + index);
            return true;
        }

        // Очистка всех слоёв
        void Clear()
        {
            m_layers.clear();
        }

        // Количество слоёв
        size_t GetLayerCount() const { return m_layers.size(); }

        // M6: Есть ли импортированные DXF
        bool HasLayers() const { return !m_layers.empty(); }

        // Общие границы всех слоёв
        bool GetCombinedBounds(WorldPoint& outMin, WorldPoint& outMax) const
        {
            if (m_layers.empty())
                return false;

            bool first = true;
            for (const auto& layer : m_layers)
            {
                if (!layer->IsVisible())
                    continue;

                if (first)
                {
                    outMin = layer->GetMinBounds();
                    outMax = layer->GetMaxBounds();
                    first = false;
                }
                else
                {
                    outMin.X = (std::min)(outMin.X, layer->GetMinBounds().X);
                    outMin.Y = (std::min)(outMin.Y, layer->GetMinBounds().Y);
                    outMax.X = (std::max)(outMax.X, layer->GetMaxBounds().X);
                    outMax.Y = (std::max)(outMax.Y, layer->GetMaxBounds().Y);
                }
            }

            return !first;
        }

    private:
        std::vector<std::unique_ptr<DxfReferenceLayer>> m_layers;
    };
}
