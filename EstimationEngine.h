#pragma once

#include "pch.h"
#include "Element.h"
#include "WallType.h"
#include "Material.h"
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <cmath>

namespace winrt::estimate1
{
    // ============================================================================
    // Quantity Item — одна строка в смете
    // ============================================================================

    struct QuantityItem
    {
        std::wstring Category;        // Категория работ
        std::wstring Description;     // Описание работы/материала
        std::wstring Unit;            // Единица измерения (м, м?, м?, шт)
        double Quantity{ 0.0 };       // Количество
        double UnitPrice{ 0.0 };      // Цена за единицу
        double TotalPrice{ 0.0 };     // Общая стоимость
        
        // Группировка
        WorkStateNative WorkState{ WorkStateNative::Existing };
        std::wstring WallTypeName;
        std::wstring MaterialName;
        std::wstring RoomName;
        
        // Для сортировки
        int SortOrder{ 0 };
    };

    // ============================================================================
    // Wall Quantity Summary — сводка по стенам
    // ============================================================================

    struct WallQuantitySummary
    {
        // Базовые измерения
        double TotalLength{ 0.0 };     // Общая длина (мм)
        double TotalArea{ 0.0 };       // Общая площадь (мм?)
        double TotalVolume{ 0.0 };     // Общий объём (мм?)
        int WallCount{ 0 };            // Количество стен

        // Группировка по типам
        std::map<std::wstring, double> LengthByType;    // Длина по типу стены
        std::map<std::wstring, double> AreaByType;      // Площадь по типу стены

        // Группировка по материалам
        std::map<std::wstring, double> AreaByMaterial;  // Площадь по материалу
        std::map<std::wstring, double> VolumeByMaterial; // Объём по материалу
    };

    // ============================================================================
    // R5.5: Zone Quantity Summary — сводка по зонам
    // ============================================================================

    struct ZoneQuantitySummary
    {
        std::wstring ZoneName;
        ZoneType Type{ ZoneType::Custom };
        size_t RoomCount{ 0 };
        double TotalAreaSqM{ 0.0 };
        double TotalPerimeterM{ 0.0 };
        double TotalWallAreaSqM{ 0.0 };
        double TotalVolumeCuM{ 0.0 };
        
        // Стоимости отделки (примерные)
        double FloorFinishCost{ 0.0 };
        double CeilingFinishCost{ 0.0 };
        double WallFinishCost{ 0.0 };
        double TotalFinishCost{ 0.0 };
    };

    // ============================================================================
    // Estimation Result — результат расчёта сметы
    // ============================================================================

    struct EstimationResult
    {
        // Сводки по рабочим состояниям
        WallQuantitySummary ExistingWalls;   // Существующие (обмер)
        WallQuantitySummary DemolitionWalls; // Под снос
        WallQuantitySummary NewWalls;        // Новое строительство

        // R5.5: Сводки по зонам
        std::vector<ZoneQuantitySummary> ZoneSummaries;

        // Детализированные строки сметы
        std::vector<QuantityItem> Items;

        // Итоги
        double DemolitionSubtotal{ 0.0 };    // Итого демонтаж
        double ConstructionSubtotal{ 0.0 };  // Итого новое строительство
        double Subtotal{ 0.0 };              // Промежуточный итог
        double Contingency{ 0.0 };           // Непредвиденные расходы (10%)
        double GrandTotal{ 0.0 };            // Итого с НР

        // Метаданные
        std::wstring ProjectName;
        std::wstring CalculationDate;
        std::wstring Author;
    };

    // ============================================================================
    // Estimation Settings — настройки расчёта
    // ============================================================================

    struct EstimationSettings
    {
        // Процент непредвиденных расходов
        double ContingencyPercent{ 10.0 };

        // Группировка
        bool GroupByWorkState{ true };
        bool GroupByWallType{ true };
        bool GroupByMaterial{ false };
        bool GroupByRoom{ false };
        bool GroupByZone{ false };  // R5.5: Группировка по зонам

        // Включать в расчёт
        bool IncludeExisting{ false };  // Обычно не включаем в смету
        bool IncludeDemolition{ true };
        bool IncludeNew{ true };

        // Единицы измерения для вывода
        bool UseMeters{ true };         // true = метры, false = миллиметры
        bool UseSquareMeters{ true };   // true = м?, false = мм?

        // Цены по умолчанию (за м?)
        double DefaultDemolitionPrice{ 500.0 };   // Демонтаж стен
        double DefaultBrickworkPrice{ 3500.0 };   // Кирпичная кладка
        double DefaultDrywallPrice{ 1200.0 };     // Гипсокартон
        double DefaultConcretePrice{ 4500.0 };    // Бетон
        double DefaultPlasterPrice{ 800.0 };      // Штукатурка
    };

    // ============================================================================
    // Estimation Engine — движок расчёта смет
    // ============================================================================

    class EstimationEngine
    {
    public:
        EstimationEngine() = default;

        // Основной метод расчёта
        EstimationResult Calculate(
            const DocumentModel& document,
            const EstimationSettings& settings = {})
        {
            EstimationResult result;
            result.CalculationDate = GetCurrentDateString();

            // Расчёт количеств по стенам
            CalculateWallQuantities(document, result, settings);

            // R5.5: Расчёт количеств по зонам
            if (settings.GroupByZone)
            {
                CalculateZoneQuantities(document, result, settings);
            }

            // Генерация строк сметы
            GenerateEstimateItems(document, result, settings);

            // Расчёт итогов
            CalculateTotals(result, settings);

            return result;
        }

        // R5.5: Получить сводку по зонам
        std::vector<ZoneQuantitySummary> GetZoneSummaries(const DocumentModel& document)
        {
            std::vector<ZoneQuantitySummary> summaries;
            
            for (const auto& zone : document.GetAutoZones())
            {
                if (!zone) continue;
                
                ZoneQuantitySummary summary;
                summary.ZoneName = zone->GetName();
                summary.Type = zone->GetType();
                summary.RoomCount = zone->GetRoomCount();
                summary.TotalAreaSqM = zone->GetTotalAreaSqM();
                summary.TotalPerimeterM = zone->GetTotalPerimeterM();
                summary.TotalWallAreaSqM = zone->GetTotalWallAreaSqM();
                summary.TotalVolumeCuM = zone->GetTotalVolumeCuM();
                
                // Примерные расчёты стоимости отделки по типу зоны
                summary.FloorFinishCost = summary.TotalAreaSqM * GetFloorFinishPrice(summary.Type);
                summary.CeilingFinishCost = summary.TotalAreaSqM * GetCeilingFinishPrice(summary.Type);
                summary.WallFinishCost = summary.TotalWallAreaSqM * GetWallFinishPrice(summary.Type);
                summary.TotalFinishCost = summary.FloorFinishCost + summary.CeilingFinishCost + summary.WallFinishCost;
                
                summaries.push_back(summary);
            }
            
            return summaries;
        }

        // Получить сводку по стенам
        WallQuantitySummary GetWallSummary(
            const DocumentModel& document,
            WorkStateNative workState)
        {
            WallQuantitySummary summary;

            for (const auto& wall : document.GetWalls())
            {
                if (!wall || wall->GetWorkState() != workState) continue;

                double length = wall->GetLength();
                double height = wall->GetHeight();
                double thickness = wall->GetThickness();

                summary.TotalLength += length;
                summary.TotalArea += length * height;
                summary.TotalVolume += length * height * thickness;
                summary.WallCount++;

                // По типу стены
                std::wstring typeName = wall->GetType() 
                    ? wall->GetType()->GetName() 
                    : L"Без типа";
                summary.LengthByType[typeName] += length;
                summary.AreaByType[typeName] += length * height;

                // По материалам (из слоёв типа стены)
                if (wall->GetType())
                {
                    for (const auto& layer : wall->GetType()->GetLayers())
                    {
                        std::wstring matName = layer.MaterialRef 
                            ? layer.MaterialRef->Name 
                            : L"Неизвестный";
                        double layerArea = length * height;
                        double layerVolume = length * height * layer.Thickness;
                        summary.AreaByMaterial[matName] += layerArea;
                        summary.VolumeByMaterial[matName] += layerVolume;
                    }
                }
            }

            return summary;
        }

    private:
        // Расчёт количеств стен
        void CalculateWallQuantities(
            const DocumentModel& document,
            EstimationResult& result,
            const EstimationSettings& settings)
        {
            result.ExistingWalls = GetWallSummary(document, WorkStateNative::Existing);
            result.DemolitionWalls = GetWallSummary(document, WorkStateNative::Demolish);
            result.NewWalls = GetWallSummary(document, WorkStateNative::New);
        }

        // Генерация строк сметы
        void GenerateEstimateItems(
            const DocumentModel& document,
            EstimationResult& result,
            const EstimationSettings& settings)
        {
            int sortOrder = 0;

            // ===== РАЗДЕЛ 1: ДЕМОНТАЖНЫЕ РАБОТЫ =====
            if (settings.IncludeDemolition && result.DemolitionWalls.WallCount > 0)
            {
                // Заголовок раздела
                QuantityItem header;
                header.Category = L"ДЕМОНТАЖНЫЕ РАБОТЫ";
                header.Description = L"";
                header.WorkState = WorkStateNative::Demolish;
                header.SortOrder = sortOrder++;
                result.Items.push_back(header);

                // Группировка по типам стен
                if (settings.GroupByWallType)
                {
                    for (const auto& [typeName, area] : result.DemolitionWalls.AreaByType)
                    {
                        QuantityItem item;
                        item.Category = L"Демонтаж";
                        item.Description = L"Демонтаж стен: " + typeName;
                        item.Unit = L"м?";
                        item.Quantity = ConvertArea(area, settings);
                        item.UnitPrice = settings.DefaultDemolitionPrice;
                        item.TotalPrice = item.Quantity * item.UnitPrice;
                        item.WorkState = WorkStateNative::Demolish;
                        item.WallTypeName = typeName;
                        item.SortOrder = sortOrder++;
                        result.Items.push_back(item);

                        result.DemolitionSubtotal += item.TotalPrice;
                    }
                }
                else
                {
                    // Общий демонтаж
                    QuantityItem item;
                    item.Category = L"Демонтаж";
                    item.Description = L"Демонтаж стен";
                    item.Unit = L"м?";
                    item.Quantity = ConvertArea(result.DemolitionWalls.TotalArea, settings);
                    item.UnitPrice = settings.DefaultDemolitionPrice;
                    item.TotalPrice = item.Quantity * item.UnitPrice;
                    item.WorkState = WorkStateNative::Demolish;
                    item.SortOrder = sortOrder++;
                    result.Items.push_back(item);

                    result.DemolitionSubtotal += item.TotalPrice;
                }

                // Вывоз мусора (примерно 0.1 м? на м? демонтажа)
                double debrisVolume = ConvertArea(result.DemolitionWalls.TotalArea, settings) * 0.1;
                if (debrisVolume > 0)
                {
                    QuantityItem debris;
                    debris.Category = L"Демонтаж";
                    debris.Description = L"Вывоз строительного мусора";
                    debris.Unit = L"м?";
                    debris.Quantity = debrisVolume;
                    debris.UnitPrice = 1500.0;  // Цена за м?
                    debris.TotalPrice = debris.Quantity * debris.UnitPrice;
                    debris.WorkState = WorkStateNative::Demolish;
                    debris.SortOrder = sortOrder++;
                    result.Items.push_back(debris);

                    result.DemolitionSubtotal += debris.TotalPrice;
                }
            }

            // ===== РАЗДЕЛ 2: СТРОИТЕЛЬНЫЕ РАБОТЫ =====
            if (settings.IncludeNew && result.NewWalls.WallCount > 0)
            {
                // Заголовок раздела
                QuantityItem header;
                header.Category = L"СТРОИТЕЛЬНЫЕ РАБОТЫ";
                header.Description = L"";
                header.WorkState = WorkStateNative::New;
                header.SortOrder = sortOrder++;
                result.Items.push_back(header);

                // Группировка по материалам
                if (settings.GroupByMaterial)
                {
                    for (const auto& [matName, area] : result.NewWalls.AreaByMaterial)
                    {
                        QuantityItem item;
                        item.Category = L"Возведение стен";
                        item.Description = L"Кладка/монтаж: " + matName;
                        item.Unit = L"м?";
                        item.Quantity = ConvertArea(area, settings);
                        item.UnitPrice = GetMaterialPrice(matName, settings);
                        item.TotalPrice = item.Quantity * item.UnitPrice;
                        item.WorkState = WorkStateNative::New;
                        item.MaterialName = matName;
                        item.SortOrder = sortOrder++;
                        result.Items.push_back(item);

                        result.ConstructionSubtotal += item.TotalPrice;
                    }
                }
                else if (settings.GroupByWallType)
                {
                    for (const auto& [typeName, area] : result.NewWalls.AreaByType)
                    {
                        QuantityItem item;
                        item.Category = L"Возведение стен";
                        item.Description = L"Возведение стен: " + typeName;
                        item.Unit = L"м?";
                        item.Quantity = ConvertArea(area, settings);
                        item.UnitPrice = settings.DefaultBrickworkPrice;
                        item.TotalPrice = item.Quantity * item.UnitPrice;
                        item.WorkState = WorkStateNative::New;
                        item.WallTypeName = typeName;
                        item.SortOrder = sortOrder++;
                        result.Items.push_back(item);

                        result.ConstructionSubtotal += item.TotalPrice;
                    }
                }
                else
                {
                    // Общее строительство
                    QuantityItem item;
                    item.Category = L"Возведение стен";
                    item.Description = L"Возведение стен";
                    item.Unit = L"м?";
                    item.Quantity = ConvertArea(result.NewWalls.TotalArea, settings);
                    item.UnitPrice = settings.DefaultBrickworkPrice;
                    item.TotalPrice = item.Quantity * item.UnitPrice;
                    item.WorkState = WorkStateNative::New;
                    item.SortOrder = sortOrder++;
                    result.Items.push_back(item);

                    result.ConstructionSubtotal += item.TotalPrice;
                }
            }
        }

        // Расчёт итогов
        void CalculateTotals(
            EstimationResult& result,
            const EstimationSettings& settings)
        {
            result.Subtotal = result.DemolitionSubtotal + result.ConstructionSubtotal;
            result.Contingency = result.Subtotal * (settings.ContingencyPercent / 100.0);
            result.GrandTotal = result.Subtotal + result.Contingency;
        }

        // Конвертация площади
        double ConvertArea(double areaMmSq, const EstimationSettings& settings)
        {
            if (settings.UseSquareMeters)
            {
                return areaMmSq / 1000000.0;  // мм? ? м?
            }
            return areaMmSq;
        }

        // Конвертация длины
        double ConvertLength(double lengthMm, const EstimationSettings& settings)
        {
            if (settings.UseMeters)
            {
                return lengthMm / 1000.0;  // мм ? м
            }
            return lengthMm;
        }

        // Получить цену материала
        double GetMaterialPrice(const std::wstring& materialName, const EstimationSettings& settings)
        {
            // Определяем цену по названию материала
            std::wstring lower = materialName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

            if (lower.find(L"кирпич") != std::wstring::npos)
                return settings.DefaultBrickworkPrice;
            if (lower.find(L"гипс") != std::wstring::npos || lower.find(L"гкл") != std::wstring::npos)
                return settings.DefaultDrywallPrice;
            if (lower.find(L"бетон") != std::wstring::npos)
                return settings.DefaultConcretePrice;
            if (lower.find(L"штукатур") != std::wstring::npos)
                return settings.DefaultPlasterPrice;

            // По умолчанию
            return settings.DefaultBrickworkPrice;
        }

        // R5.5: Расчёт количеств по зонам
        void CalculateZoneQuantities(
            const DocumentModel& document,
            EstimationResult& result,
            [[maybe_unused]] const EstimationSettings& settings)
        {
            result.ZoneSummaries = GetZoneSummaries(document);
        }

        // R5.5: Цена отделки пола по типу зоны
        static double GetFloorFinishPrice(ZoneType type)
        {
            switch (type)
            {
            case ZoneType::Living:      return 2500.0;  // Ламинат/паркет
            case ZoneType::Wet:         return 3500.0;  // Плитка
            case ZoneType::Service:     return 1500.0;  // Линолеум
            case ZoneType::Circulation: return 2000.0;  // Ламинат
            case ZoneType::Outdoor:     return 1000.0;  // Минимальная отделка
            default:                    return 2000.0;
            }
        }

        // R5.5: Цена отделки потолка по типу зоны
        static double GetCeilingFinishPrice(ZoneType type)
        {
            switch (type)
            {
            case ZoneType::Living:      return 800.0;   // Натяжной
            case ZoneType::Wet:         return 1200.0;  // Влагостойкий
            case ZoneType::Service:     return 500.0;   // Покраска
            case ZoneType::Circulation: return 600.0;   // Натяжной
            case ZoneType::Outdoor:     return 0.0;     // Без отделки
            default:                    return 700.0;
            }
        }

        // R5.5: Цена отделки стен по типу зоны
        static double GetWallFinishPrice(ZoneType type)
        {
            switch (type)
            {
            case ZoneType::Living:      return 600.0;   // Обои
            case ZoneType::Wet:         return 2500.0;  // Плитка
            case ZoneType::Service:     return 400.0;   // Покраска
            case ZoneType::Circulation: return 500.0;   // Обои
            case ZoneType::Outdoor:     return 0.0;     // Без отделки
            default:                    return 500.0;
            }
        }

        // Текущая дата
        std::wstring GetCurrentDateString()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);

            wchar_t buffer[64];
            swprintf_s(buffer, L"%02d.%02d.%04d",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
            return buffer;
        }
    };

    // ============================================================================
    // Estimation Formatter — форматирование для отображения
    // ============================================================================

    class EstimationFormatter
    {
    public:
        // Форматирование числа с разделителями тысяч
        static std::wstring FormatNumber(double value, int decimals = 2)
        {
            wchar_t format[32];
            swprintf_s(format, L"%%.%df", decimals);

            wchar_t buffer[64];
            swprintf_s(buffer, format, value);

            std::wstring result = buffer;

            // Добавляем пробелы как разделители тысяч
            size_t dotPos = result.find(L'.');
            if (dotPos == std::wstring::npos) dotPos = result.length();

            for (int i = static_cast<int>(dotPos) - 3; i > 0; i -= 3)
            {
                result.insert(i, L" ");
            }

            return result;
        }

        // Форматирование валюты
        static std::wstring FormatCurrency(double value)
        {
            return FormatNumber(value, 2) + L" ?";
        }

        // Форматирование количества
        static std::wstring FormatQuantity(double value, const std::wstring& unit)
        {
            return FormatNumber(value, 2) + L" " + unit;
        }

        // Краткая сводка
        static std::wstring FormatSummary(const EstimationResult& result)
        {
            std::wstringstream ss;
            
            ss << L"=== СМЕТА НА РЕМОНТНЫЕ РАБОТЫ ===\n";
            ss << L"Дата расчёта: " << result.CalculationDate << L"\n\n";

            if (result.DemolitionWalls.WallCount > 0)
            {
                ss << L"ДЕМОНТАЖ:\n";
                ss << L"  Стен: " << result.DemolitionWalls.WallCount << L" шт\n";
                ss << L"  Площадь: " << FormatNumber(result.DemolitionWalls.TotalArea / 1000000.0) << L" м?\n";
                ss << L"  Итого: " << FormatCurrency(result.DemolitionSubtotal) << L"\n\n";
            }

            if (result.NewWalls.WallCount > 0)
            {
                ss << L"СТРОИТЕЛЬСТВО:\n";
                ss << L"  Стен: " << result.NewWalls.WallCount << L" шт\n";
                ss << L"  Площадь: " << FormatNumber(result.NewWalls.TotalArea / 1000000.0) << L" м?\n";
                ss << L"  Итого: " << FormatCurrency(result.ConstructionSubtotal) << L"\n\n";
            }

            ss << L"ИТОГО:\n";
            ss << L"  Подитог: " << FormatCurrency(result.Subtotal) << L"\n";
            ss << L"  Непредвиденные (10%): " << FormatCurrency(result.Contingency) << L"\n";
            ss << L"  ВСЕГО: " << FormatCurrency(result.GrandTotal) << L"\n";

            return ss.str();
        }
    };

} // namespace winrt::estimate1
