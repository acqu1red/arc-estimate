#pragma once

#include "pch.h"
#include "IfcParser.h"
#include <memory>
#include <string>
#include <vector>

namespace winrt::estimate1
{
    // ============================================================================
    // IFC Import Settings (Настройки импорта IFC)
    // ============================================================================

    struct IfcImportSettings
    {
        // Масштаб (множитель для координат)
        double Scale{ 1.0 };

        // Смещение после импорта
        WorldPoint Offset{ 0, 0 };

        // Какие типы элементов импортировать
        bool ImportWalls{ true };
        bool ImportDoors{ true };
        bool ImportWindows{ true };
        bool ImportSpaces{ true };
        bool ImportSlabs{ false };

        // Какой этаж импортировать (-1 = все)
        int TargetStoreyIndex{ -1 };

        // Конвертировать в элементы ARC-Estimate
        bool ConvertToElements{ false };
        
        // Настройки конвертации стен
        double DefaultWallThickness{ 200.0 };  // Если толщина не определена
        double DefaultWallHeight{ 2700.0 };
        
        // Назначение WorkState для импортированных элементов
        WorkStateNative TargetWorkState{ WorkStateNative::Existing };
    };

    // ============================================================================
    // IFC Reference Layer (импортированный слой как подложка)
    // ============================================================================

    class IfcReferenceLayer
    {
    public:
        IfcReferenceLayer(const std::wstring& name = L"IFC Import")
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

        // Цвет для стен
        Windows::UI::Color GetWallColor() const { return m_wallColor; }
        void SetWallColor(Windows::UI::Color color) { m_wallColor = color; }

        // Цвет для дверей
        Windows::UI::Color GetDoorColor() const { return m_doorColor; }
        void SetDoorColor(Windows::UI::Color color) { m_doorColor = color; }

        // Цвет для окон
        Windows::UI::Color GetWindowColor() const { return m_windowColor; }
        void SetWindowColor(Windows::UI::Color color) { m_windowColor = color; }

        // Цвет для помещений
        Windows::UI::Color GetSpaceColor() const { return m_spaceColor; }
        void SetSpaceColor(Windows::UI::Color color) { m_spaceColor = color; }

        // Толщина линий
        float GetLineWidth() const { return m_lineWidth; }
        void SetLineWidth(float width) { m_lineWidth = width; }

        // Показывать названия
        bool GetShowNames() const { return m_showNames; }
        void SetShowNames(bool show) { m_showNames = show; }

        // Показывать помещения
        bool GetShowSpaces() const { return m_showSpaces; }
        void SetShowSpaces(bool show) { m_showSpaces = show; }

        // Документ IFC
        IfcDocument* GetDocument() { return m_document.get(); }
        const IfcDocument* GetDocument() const { return m_document.get(); }

        void TakeDocument(std::unique_ptr<IfcDocument> doc)
        {
            m_document = std::move(doc);
        }

        // Границы
        WorldPoint GetMinBounds() const 
        { 
            return m_document ? m_document->MinBounds : WorldPoint(0, 0); 
        }
        
        WorldPoint GetMaxBounds() const 
        { 
            return m_document ? m_document->MaxBounds : WorldPoint(0, 0); 
        }

        // Статистика
        size_t GetWallCount() const 
        { 
            return m_document ? m_document->Walls.size() : 0; 
        }
        
        size_t GetDoorCount() const 
        { 
            return m_document ? m_document->Doors.size() : 0; 
        }
        
        size_t GetWindowCount() const 
        { 
            return m_document ? m_document->Windows.size() : 0; 
        }
        
        size_t GetSpaceCount() const 
        { 
            return m_document ? m_document->Spaces.size() : 0; 
        }
        
        size_t GetTotalEntityCount() const 
        { 
            return m_document ? m_document->TotalEntityCount : 0; 
        }

        // Доступ к стенам для конвертации
        const std::vector<std::unique_ptr<IfcWall>>& GetWalls() const
        {
            static std::vector<std::unique_ptr<IfcWall>> empty;
            return m_document ? m_document->Walls : empty;
        }

        // Доступ к дверям для конвертации
        const std::vector<std::unique_ptr<IfcDoor>>& GetDoors() const
        {
            static std::vector<std::unique_ptr<IfcDoor>> empty;
            return m_document ? m_document->Doors : empty;
        }

        // Доступ к окнам для конвертации
        const std::vector<std::unique_ptr<IfcWindow>>& GetWindows() const
        {
            static std::vector<std::unique_ptr<IfcWindow>> empty;
            return m_document ? m_document->Windows : empty;
        }

        // Путь к исходному файлу
        const std::wstring& GetSourcePath() const { return m_sourcePath; }
        void SetSourcePath(const std::wstring& path) { m_sourcePath = path; }

        // M6: Для сериализации
        const std::wstring& GetFilePath() const { return m_sourcePath; }
        double GetScale() const { return m_scale; }
        void SetScale(double scale) { m_scale = scale; }
        const WorldPoint& GetOffset() const { return m_offset; }
        void SetOffset(const WorldPoint& offset) { m_offset = offset; }

        // Схема IFC
        std::wstring GetSchema() const 
        { 
            return m_document ? m_document->Schema : L""; 
        }

        // Название проекта
        std::wstring GetProjectName() const 
        { 
            return m_document ? m_document->ProjectName : L""; 
        }

        // Этажи
        const std::vector<std::unique_ptr<IfcBuildingStorey>>& GetStoreys() const
        {
            static std::vector<std::unique_ptr<IfcBuildingStorey>> empty;
            return m_document ? m_document->Storeys : empty;
        }

    private:
        std::wstring m_name;
        std::wstring m_sourcePath;

        bool m_isVisible{ true };
        uint8_t m_opacity{ 180 };
        
        Windows::UI::Color m_wallColor{ Windows::UI::ColorHelper::FromArgb(255, 80, 80, 80) };
        Windows::UI::Color m_doorColor{ Windows::UI::ColorHelper::FromArgb(255, 139, 90, 43) };
        Windows::UI::Color m_windowColor{ Windows::UI::ColorHelper::FromArgb(255, 100, 149, 237) };
        Windows::UI::Color m_spaceColor{ Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200) };
        
        float m_lineWidth{ 1.0f };
        bool m_showNames{ true };
        bool m_showSpaces{ true };
        double m_scale{ 1.0 };
        WorldPoint m_offset{ 0, 0 };

        std::unique_ptr<IfcDocument> m_document;
    };

    // ============================================================================
    // IFC Reference Manager (управление импортированными подложками)
    // ============================================================================

    class IfcReferenceManager
    {
    public:
        // Результат импорта
        struct ImportResult
        {
            bool Success{ false };
            std::wstring ErrorMessage;
            
            // Статистика
            size_t WallCount{ 0 };
            size_t DoorCount{ 0 };
            size_t WindowCount{ 0 };
            size_t SpaceCount{ 0 };
            size_t TotalEntityCount{ 0 };
            
            // Индекс созданного слоя
            size_t LayerIndex{ 0 };
            
            // Информация о файле
            std::wstring Schema;
            std::wstring ProjectName;
            std::wstring LengthUnit;
        };

        ImportResult ImportFile(const std::wstring& filePath, const IfcImportSettings& settings = {})
        {
            ImportResult result;

            // Парсим файл
            auto parseResult = IfcParser::ParseFile(filePath);
            if (!parseResult.Success || !parseResult.Document)
            {
                result.ErrorMessage = parseResult.ErrorMessage;
                return result;
            }

            // Применяем масштаб единиц измерения
            double scale = settings.Scale * parseResult.Document->LengthUnitScale;
            if (std::abs(scale - 1.0) > 0.0001)
            {
                parseResult.Document->ApplyScale(scale);
            }

            // Создаём слой подложки
            auto layer = std::make_unique<IfcReferenceLayer>();
            
            // Извлекаем имя файла для названия слоя
            size_t lastSlash = filePath.find_last_of(L"\\/");
            size_t lastDot = filePath.find_last_of(L'.');
            std::wstring fileName = (lastSlash != std::wstring::npos) 
                ? filePath.substr(lastSlash + 1, (lastDot != std::wstring::npos ? lastDot - lastSlash - 1 : std::wstring::npos))
                : filePath;
            
            // Используем имя проекта или имя файла
            if (!parseResult.Document->ProjectName.empty())
            {
                layer->SetName(parseResult.Document->ProjectName);
            }
            else
            {
                layer->SetName(fileName);
            }
            
            layer->SetSourcePath(filePath);

            // Заполняем результат
            result.WallCount = parseResult.Document->Walls.size();
            result.DoorCount = parseResult.Document->Doors.size();
            result.WindowCount = parseResult.Document->Windows.size();
            result.SpaceCount = parseResult.Document->Spaces.size();
            result.TotalEntityCount = parseResult.Document->TotalEntityCount;
            result.Schema = parseResult.Document->Schema;
            result.ProjectName = parseResult.Document->ProjectName;
            result.LengthUnit = parseResult.Document->LengthUnitName;
            
            // Передаём документ в слой
            layer->TakeDocument(std::move(parseResult.Document));

            result.LayerIndex = m_layers.size();
            m_layers.push_back(std::move(layer));

            result.Success = true;
            return result;
        }

        // Получение слоёв
        const std::vector<std::unique_ptr<IfcReferenceLayer>>& GetLayers() const { return m_layers; }

        // Получение слоя по индексу
        IfcReferenceLayer* GetLayer(size_t index)
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

        // Есть ли импортированные IFC
        bool HasLayers() const { return !m_layers.empty(); }

    private:
        std::vector<std::unique_ptr<IfcReferenceLayer>> m_layers;
    };
}
