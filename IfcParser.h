#pragma once

#include "pch.h"
#include "Models.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cmath>
#include <optional>
#include <functional>
#include <filesystem>
#include <regex>

namespace winrt::estimate1
{
    // ============================================================================
    // IFC Entity Types (основные для архитектурных планов)
    // ============================================================================

    enum class IfcEntityType
    {
        Unknown,
        // Основные строительные элементы
        Wall,
        WallStandardCase,
        Door,
        Window,
        Slab,
        Column,
        Beam,
        // Пространства
        Space,
        Room,
        // Геометрия
        PolyLine,
        CompositeCurve,
        // Вспомогательные
        Project,
        Site,
        Building,
        BuildingStorey,
        // Представление
        ShapeRepresentation,
        ExtrudedAreaSolid
    };

    // ============================================================================
    // IFC Parsed Entities
    // ============================================================================

    // Базовая сущность IFC
    struct IfcEntity
    {
        uint64_t Id{ 0 };              // #ID в файле
        IfcEntityType Type{ IfcEntityType::Unknown };
        std::wstring TypeName;         // Оригинальное имя типа
        std::wstring Name;             // Имя элемента (из IfcLabel)
        std::wstring GlobalId;         // GUID элемента
        
        virtual ~IfcEntity() = default;
    };

    // 2D точка для плана
    struct IfcPoint2D
    {
        double X{ 0.0 };
        double Y{ 0.0 };

        IfcPoint2D() = default;
        IfcPoint2D(double x, double y) : X(x), Y(y) {}
    };

    // 3D точка
    struct IfcPoint3D
    {
        double X{ 0.0 };
        double Y{ 0.0 };
        double Z{ 0.0 };

        IfcPoint3D() = default;
        IfcPoint3D(double x, double y, double z) : X(x), Y(y), Z(z) {}

        IfcPoint2D To2D() const { return IfcPoint2D(X, Y); }
    };

    // Полилиния (контур)
    struct IfcPolyline
    {
        std::vector<IfcPoint2D> Points;
        bool IsClosed{ false };
    };

    // Стена IFC
    struct IfcWall : public IfcEntity
    {
        // Геометрия оси стены
        IfcPoint3D StartPoint;
        IfcPoint3D EndPoint;
        
        // Параметры
        double Thickness{ 0.0 };       // Толщина (мм)
        double Height{ 0.0 };          // Высота (мм)
        double Length{ 0.0 };          // Длина (мм)
        
        // Контур (если есть)
        std::vector<IfcPolyline> Contours;
        
        // Свойства
        std::wstring MaterialName;
        std::wstring PredefinedType;   // STANDARD, POLYGONAL, etc.
        bool IsExternal{ false };
        bool IsLoadBearing{ false };
        
        // Связь с этажом
        uint64_t BuildingStoreyId{ 0 };

        IfcWall()
        {
            Type = IfcEntityType::Wall;
            TypeName = L"IFCWALL";
        }
    };

    // Дверь IFC
    struct IfcDoor : public IfcEntity
    {
        IfcPoint3D Position;
        double Width{ 0.0 };           // Ширина проёма (мм)
        double Height{ 0.0 };          // Высота проёма (мм)
        
        // Связь со стеной-хозяином
        uint64_t HostWallId{ 0 };
        double OffsetFromWallStart{ 0.0 };
        
        // Свойства
        std::wstring OperationType;    // SINGLE_SWING_LEFT, etc.

        IfcDoor()
        {
            Type = IfcEntityType::Door;
            TypeName = L"IFCDOOR";
        }
    };

    // Окно IFC
    struct IfcWindow : public IfcEntity
    {
        IfcPoint3D Position;
        double Width{ 0.0 };           // Ширина (мм)
        double Height{ 0.0 };          // Высота (мм)
        double SillHeight{ 0.0 };      // Высота подоконника (мм)
        
        // Связь со стеной-хозяином
        uint64_t HostWallId{ 0 };
        double OffsetFromWallStart{ 0.0 };
        
        // Свойства
        std::wstring PartitioningType; // SINGLE_PANEL, DOUBLE_PANEL, etc.

        IfcWindow()
        {
            Type = IfcEntityType::Window;
            TypeName = L"IFCWINDOW";
        }
    };

    // Помещение/Пространство IFC
    struct IfcSpace : public IfcEntity
    {
        std::vector<IfcPolyline> BoundaryContours;
        double Area{ 0.0 };            // Площадь (м?)
        double Height{ 0.0 };          // Высота помещения (мм)
        
        std::wstring LongName;         // Полное название
        std::wstring SpaceType;        // Тип помещения

        IfcSpace()
        {
            Type = IfcEntityType::Space;
            TypeName = L"IFCSPACE";
        }
    };

    // Плита перекрытия IFC
    struct IfcSlab : public IfcEntity
    {
        std::vector<IfcPolyline> Contours;
        double Thickness{ 0.0 };
        std::wstring PredefinedType;   // FLOOR, ROOF, etc.

        IfcSlab()
        {
            Type = IfcEntityType::Slab;
            TypeName = L"IFCSLAB";
        }
    };

    // Этаж здания
    struct IfcBuildingStorey : public IfcEntity
    {
        double Elevation{ 0.0 };       // Отметка этажа (мм)
        std::wstring LongName;

        IfcBuildingStorey()
        {
            Type = IfcEntityType::BuildingStorey;
            TypeName = L"IFCBUILDINGSTOREY";
        }
    };

    // ============================================================================
    // IFC Document (результат парсинга)
    // ============================================================================

    struct IfcDocument
    {
        // Метаданные
        std::wstring Schema;           // IFC2X3, IFC4, etc.
        std::wstring ProjectName;
        std::wstring FileName;
        std::wstring Author;
        std::wstring Organization;
        
        // Единицы измерения
        double LengthUnitScale{ 1.0 }; // Масштаб к мм (по умолчанию мм)
        std::wstring LengthUnitName;   // METRE, MILLI, etc.
        
        // Коллекции сущностей
        std::vector<std::unique_ptr<IfcWall>> Walls;
        std::vector<std::unique_ptr<IfcDoor>> Doors;
        std::vector<std::unique_ptr<IfcWindow>> Windows;
        std::vector<std::unique_ptr<IfcSpace>> Spaces;
        std::vector<std::unique_ptr<IfcSlab>> Slabs;
        std::vector<std::unique_ptr<IfcBuildingStorey>> Storeys;
        
        // Статистика
        size_t TotalEntityCount{ 0 };
        size_t WallCount{ 0 };
        size_t DoorCount{ 0 };
        size_t WindowCount{ 0 };
        size_t SpaceCount{ 0 };
        
        // Границы (bounding box)
        WorldPoint MinBounds{ 0, 0 };
        WorldPoint MaxBounds{ 0, 0 };
        bool HasBounds{ false };

        void UpdateBounds(double x, double y)
        {
            if (!HasBounds)
            {
                MinBounds = MaxBounds = WorldPoint(x, y);
                HasBounds = true;
            }
            else
            {
                MinBounds.X = (std::min)(MinBounds.X, x);
                MinBounds.Y = (std::min)(MinBounds.Y, y);
                MaxBounds.X = (std::max)(MaxBounds.X, x);
                MaxBounds.Y = (std::max)(MaxBounds.Y, y);
            }
        }

        // Применение масштаба (конвертация в мм)
        void ApplyScale(double scale)
        {
            if (std::abs(scale - 1.0) < 0.0001)
                return;

            for (auto& wall : Walls)
            {
                wall->StartPoint.X *= scale;
                wall->StartPoint.Y *= scale;
                wall->StartPoint.Z *= scale;
                wall->EndPoint.X *= scale;
                wall->EndPoint.Y *= scale;
                wall->EndPoint.Z *= scale;
                wall->Thickness *= scale;
                wall->Height *= scale;
                wall->Length *= scale;
                
                for (auto& contour : wall->Contours)
                {
                    for (auto& pt : contour.Points)
                    {
                        pt.X *= scale;
                        pt.Y *= scale;
                    }
                }
            }

            for (auto& door : Doors)
            {
                door->Position.X *= scale;
                door->Position.Y *= scale;
                door->Position.Z *= scale;
                door->Width *= scale;
                door->Height *= scale;
                door->OffsetFromWallStart *= scale;
            }

            for (auto& window : Windows)
            {
                window->Position.X *= scale;
                window->Position.Y *= scale;
                window->Position.Z *= scale;
                window->Width *= scale;
                window->Height *= scale;
                window->SillHeight *= scale;
                window->OffsetFromWallStart *= scale;
            }

            for (auto& space : Spaces)
            {
                space->Height *= scale;
                space->Area *= (scale * scale) / 1000000.0; // м? при масштабировании
                
                for (auto& contour : space->BoundaryContours)
                {
                    for (auto& pt : contour.Points)
                    {
                        pt.X *= scale;
                        pt.Y *= scale;
                    }
                }
            }

            for (auto& slab : Slabs)
            {
                slab->Thickness *= scale;
                
                for (auto& contour : slab->Contours)
                {
                    for (auto& pt : contour.Points)
                    {
                        pt.X *= scale;
                        pt.Y *= scale;
                    }
                }
            }

            for (auto& storey : Storeys)
            {
                storey->Elevation *= scale;
            }

            // Обновляем границы
            MinBounds.X *= scale;
            MinBounds.Y *= scale;
            MaxBounds.X *= scale;
            MaxBounds.Y *= scale;
        }
    };

    // ============================================================================
    // IFC Parser
    // ============================================================================

    class IfcParser
    {
    public:
        // Результат парсинга
        struct ParseResult
        {
            bool Success{ false };
            std::wstring ErrorMessage;
            std::unique_ptr<IfcDocument> Document;
        };

        // Парсит IFC файл
        static ParseResult ParseFile(const std::wstring& filePath)
        {
            ParseResult result;

            try
            {
                std::ifstream file(std::filesystem::path(filePath), std::ios::binary);
                if (!file.is_open())
                {
                    result.ErrorMessage = L"Не удалось открыть файл: " + filePath;
                    return result;
                }

                // Читаем содержимое
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();

                return ParseContent(buffer.str());
            }
            catch (const std::exception& ex)
            {
                result.ErrorMessage = L"Ошибка чтения файла: " + 
                    std::wstring(ex.what(), ex.what() + strlen(ex.what()));
                return result;
            }
        }

        // Парсит содержимое IFC
        static ParseResult ParseContent(const std::string& content)
        {
            ParseResult result;
            result.Document = std::make_unique<IfcDocument>();

            try
            {
                IfcParserState state;
                state.Document = result.Document.get();

                // Проверяем сигнатуру IFC/STEP
                if (content.find("ISO-10303-21") == std::string::npos)
                {
                    result.ErrorMessage = L"Файл не является корректным IFC (STEP) файлом";
                    return result;
                }

                // Извлекаем схему
                ExtractSchema(content, *result.Document);

                // Парсим секцию HEADER
                ParseHeader(content, state);

                // Парсим секцию DATA
                ParseData(content, state);

                // Постобработка: связываем сущности и вычисляем геометрию
                PostProcess(state);

                // Обновляем статистику
                result.Document->WallCount = result.Document->Walls.size();
                result.Document->DoorCount = result.Document->Doors.size();
                result.Document->WindowCount = result.Document->Windows.size();
                result.Document->SpaceCount = result.Document->Spaces.size();
                result.Document->TotalEntityCount = state.AllEntities.size();

                if (result.Document->TotalEntityCount == 0)
                {
                    result.ErrorMessage = L"Файл не содержит распознанных сущностей IFC";
                    return result;
                }

                result.Success = true;
            }
            catch (const std::exception& ex)
            {
                result.ErrorMessage = L"Ошибка парсинга IFC: " + 
                    std::wstring(ex.what(), ex.what() + strlen(ex.what()));
            }

            return result;
        }

        // Определяет масштаб для конвертации в мм
        static double GetScaleToMM(const std::wstring& unitName, double prefix = 1.0)
        {
            // Базовые единицы длины
            if (unitName == L"METRE" || unitName == L"METER")
                return 1000.0 * prefix;
            if (unitName == L"FOOT")
                return 304.8 * prefix;
            if (unitName == L"INCH")
                return 25.4 * prefix;
            
            // По умолчанию предполагаем мм
            return prefix;
        }

    private:
        // Вспомогательные функции
        static std::wstring Widen(const std::string& s)
        {
            std::wstring out;
            out.reserve(s.size());
            for (unsigned char ch : s)
            {
                out.push_back(static_cast<wchar_t>(ch));
            }
            return out;
        }

        static std::string Narrow(const std::wstring& s)
        {
            std::string out;
            out.reserve(s.size());
            for (wchar_t ch : s)
            {
                out.push_back(static_cast<char>(ch & 0xFF));
            }
            return out;
        }

        static std::string Trim(const std::string& s)
        {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos)
                return "";
            return s.substr(start, end - start + 1);
        }

        static std::wstring TrimW(const std::wstring& s)
        {
            size_t start = s.find_first_not_of(L" \t\r\n");
            size_t end = s.find_last_not_of(L" \t\r\n");
            if (start == std::wstring::npos)
                return L"";
            return s.substr(start, end - start + 1);
        }

        // Извлекает строку из кавычек IFC ('...')
        static std::wstring ExtractString(const std::string& value)
        {
            size_t start = value.find('\'');
            size_t end = value.rfind('\'');
            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                return Widen(value.substr(start + 1, end - start - 1));
            }
            return Widen(value);
        }

        // Извлекает числовое значение
        static double ExtractDouble(const std::string& value)
        {
            try
            {
                std::string cleaned = Trim(value);
                // Удаляем возможные скобки и пробелы
                size_t start = cleaned.find_first_of("-0123456789.");
                size_t end = cleaned.find_last_of("0123456789.");
                if (start != std::string::npos && end != std::string::npos)
                {
                    return std::stod(cleaned.substr(start, end - start + 1));
                }
            }
            catch (...) {}
            return 0.0;
        }

        // Извлекает ID ссылки (#123)
        static uint64_t ExtractReference(const std::string& value)
        {
            size_t hashPos = value.find('#');
            if (hashPos != std::string::npos)
            {
                try
                {
                    return std::stoull(value.substr(hashPos + 1));
                }
                catch (...) {}
            }
            return 0;
        }

        // Разбивает аргументы IFC-сущности
        static std::vector<std::string> SplitArguments(const std::string& args)
        {
            std::vector<std::string> result;
            int parenDepth = 0;
            int quoteDepth = 0;
            std::string current;

            for (size_t i = 0; i < args.size(); ++i)
            {
                char c = args[i];

                if (c == '\'' && (i == 0 || args[i-1] != '\\'))
                {
                    quoteDepth = 1 - quoteDepth;
                    current += c;
                }
                else if (quoteDepth > 0)
                {
                    current += c;
                }
                else if (c == '(')
                {
                    parenDepth++;
                    current += c;
                }
                else if (c == ')')
                {
                    parenDepth--;
                    current += c;
                }
                else if (c == ',' && parenDepth == 0)
                {
                    result.push_back(Trim(current));
                    current.clear();
                }
                else
                {
                    current += c;
                }
            }

            if (!current.empty())
            {
                result.push_back(Trim(current));
            }

            return result;
        }

        // ============================================================================
        // Parser State
        // ============================================================================

        struct RawEntity
        {
            uint64_t Id{ 0 };
            std::string TypeName;
            std::string Arguments;
        };

        struct IfcParserState
        {
            IfcDocument* Document{ nullptr };
            
            // Все сырые сущности по ID
            std::unordered_map<uint64_t, RawEntity> AllEntities;
            
            // Связи между сущностями
            std::unordered_map<uint64_t, std::vector<uint64_t>> RelContainedInSpatial; // Storey -> Elements
            std::unordered_map<uint64_t, uint64_t> RelVoidsElement;  // Opening -> Wall
            std::unordered_map<uint64_t, uint64_t> RelFillsElement;  // Door/Window -> Opening
            
            // Геометрия
            std::unordered_map<uint64_t, std::vector<IfcPoint2D>> Polylines;     // ID -> Points
            std::unordered_map<uint64_t, IfcPoint3D> CartesianPoints;            // ID -> Point
            std::unordered_map<uint64_t, std::pair<IfcPoint3D, IfcPoint3D>> Placements; // ID -> (Location, Direction)
        };

        // ============================================================================
        // Parsing Functions
        // ============================================================================

        static void ExtractSchema(const std::string& content, IfcDocument& doc)
        {
            // Ищем FILE_SCHEMA((...));
            size_t schemaPos = content.find("FILE_SCHEMA");
            if (schemaPos != std::string::npos)
            {
                size_t start = content.find('(', schemaPos);
                size_t end = content.find(')', start);
                if (start != std::string::npos && end != std::string::npos)
                {
                    std::string schemas = content.substr(start + 1, end - start - 1);
                    // Извлекаем первую схему
                    size_t q1 = schemas.find('\'');
                    size_t q2 = schemas.find('\'', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos)
                    {
                        doc.Schema = Widen(schemas.substr(q1 + 1, q2 - q1 - 1));
                    }
                }
            }
        }

        static void ParseHeader(const std::string& content, IfcParserState& state)
        {
            // Ищем FILE_NAME
            size_t fnPos = content.find("FILE_NAME");
            if (fnPos != std::string::npos)
            {
                size_t start = content.find('(', fnPos);
                size_t end = content.find(';', start);
                if (start != std::string::npos && end != std::string::npos)
                {
                    std::string args = content.substr(start + 1, end - start - 2);
                    auto parts = SplitArguments(args);
                    if (parts.size() > 0)
                        state.Document->FileName = ExtractString(parts[0]);
                    if (parts.size() > 2)
                        state.Document->Author = ExtractString(parts[2]);
                    if (parts.size() > 3)
                        state.Document->Organization = ExtractString(parts[3]);
                }
            }
        }

        static void ParseData(const std::string& content, IfcParserState& state)
        {
            // Находим секцию DATA
            size_t dataStart = content.find("DATA;");
            size_t dataEnd = content.find("ENDSEC;", dataStart);
            
            if (dataStart == std::string::npos || dataEnd == std::string::npos)
                return;

            std::string dataSection = content.substr(dataStart + 5, dataEnd - dataStart - 5);

            // Парсим строки вида: #123=IFCWALL(...);
            std::regex entityRegex(R"(#(\d+)\s*=\s*([A-Z0-9_]+)\s*\(([^;]*)\)\s*;)", std::regex::optimize);
            
            auto begin = std::sregex_iterator(dataSection.begin(), dataSection.end(), entityRegex);
            auto end = std::sregex_iterator();

            for (auto it = begin; it != end; ++it)
            {
                std::smatch match = *it;
                
                RawEntity entity;
                entity.Id = std::stoull(match[1].str());
                entity.TypeName = match[2].str();
                entity.Arguments = match[3].str();

                state.AllEntities[entity.Id] = entity;

                // Обрабатываем сущность в зависимости от типа
                ProcessEntity(entity, state);
            }
        }

        static void ProcessEntity(const RawEntity& entity, IfcParserState& state)
        {
            const std::string& type = entity.TypeName;
            auto args = SplitArguments(entity.Arguments);

            // ============================================================
            // Элементы здания
            // ============================================================

            if (type == "IFCWALL" || type == "IFCWALLSTANDARDCASE")
            {
                auto wall = std::make_unique<IfcWall>();
                wall->Id = entity.Id;
                wall->TypeName = Widen(type);
                
                // IFCWALL(GlobalId, OwnerHistory, Name, Description, ObjectType, ObjectPlacement, Representation, Tag)
                if (args.size() > 0) wall->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) wall->Name = ExtractString(args[2]);
                
                state.Document->Walls.push_back(std::move(wall));
            }
            else if (type == "IFCDOOR")
            {
                auto door = std::make_unique<IfcDoor>();
                door->Id = entity.Id;
                door->TypeName = Widen(type);
                
                // IFCDOOR(GlobalId, OwnerHistory, Name, Description, ObjectType, ObjectPlacement, Representation, Tag, OverallHeight, OverallWidth)
                if (args.size() > 0) door->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) door->Name = ExtractString(args[2]);
                if (args.size() > 8) door->Height = ExtractDouble(args[8]);
                if (args.size() > 9) door->Width = ExtractDouble(args[9]);
                
                state.Document->Doors.push_back(std::move(door));
            }
            else if (type == "IFCWINDOW")
            {
                auto window = std::make_unique<IfcWindow>();
                window->Id = entity.Id;
                window->TypeName = Widen(type);
                
                // IFCWINDOW(GlobalId, OwnerHistory, Name, Description, ObjectType, ObjectPlacement, Representation, Tag, OverallHeight, OverallWidth)
                if (args.size() > 0) window->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) window->Name = ExtractString(args[2]);
                if (args.size() > 8) window->Height = ExtractDouble(args[8]);
                if (args.size() > 9) window->Width = ExtractDouble(args[9]);
                
                state.Document->Windows.push_back(std::move(window));
            }
            else if (type == "IFCSPACE")
            {
                auto space = std::make_unique<IfcSpace>();
                space->Id = entity.Id;
                space->TypeName = Widen(type);
                
                if (args.size() > 0) space->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) space->Name = ExtractString(args[2]);
                if (args.size() > 7) space->LongName = ExtractString(args[7]);
                
                state.Document->Spaces.push_back(std::move(space));
            }
            else if (type == "IFCSLAB")
            {
                auto slab = std::make_unique<IfcSlab>();
                slab->Id = entity.Id;
                slab->TypeName = Widen(type);
                
                if (args.size() > 0) slab->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) slab->Name = ExtractString(args[2]);
                
                state.Document->Slabs.push_back(std::move(slab));
            }
            else if (type == "IFCBUILDINGSTOREY")
            {
                auto storey = std::make_unique<IfcBuildingStorey>();
                storey->Id = entity.Id;
                storey->TypeName = Widen(type);
                
                if (args.size() > 0) storey->GlobalId = ExtractString(args[0]);
                if (args.size() > 2) storey->Name = ExtractString(args[2]);
                if (args.size() > 9) storey->Elevation = ExtractDouble(args[9]);
                
                state.Document->Storeys.push_back(std::move(storey));
            }
            else if (type == "IFCPROJECT")
            {
                if (args.size() > 2)
                    state.Document->ProjectName = ExtractString(args[2]);
            }

            // ============================================================
            // Геометрические примитивы
            // ============================================================

            else if (type == "IFCCARTESIANPOINT")
            {
                // IFCCARTESIANPOINT((X,Y) или (X,Y,Z))
                IfcPoint3D pt;
                
                // Извлекаем координаты из списка
                size_t listStart = entity.Arguments.find('(');
                size_t listEnd = entity.Arguments.rfind(')');
                if (listStart != std::string::npos && listEnd != std::string::npos)
                {
                    std::string coords = entity.Arguments.substr(listStart + 1, listEnd - listStart - 1);
                    auto coordList = SplitArguments(coords);
                    if (coordList.size() > 0) pt.X = ExtractDouble(coordList[0]);
                    if (coordList.size() > 1) pt.Y = ExtractDouble(coordList[1]);
                    if (coordList.size() > 2) pt.Z = ExtractDouble(coordList[2]);
                }
                
                state.CartesianPoints[entity.Id] = pt;
            }
            else if (type == "IFCPOLYLINE")
            {
                // IFCPOLYLINE((#ref1, #ref2, ...))
                std::vector<IfcPoint2D> points;
                
                size_t listStart = entity.Arguments.find('(');
                size_t listEnd = entity.Arguments.rfind(')');
                if (listStart != std::string::npos && listEnd != std::string::npos)
                {
                    std::string refs = entity.Arguments.substr(listStart + 1, listEnd - listStart - 1);
                    auto refList = SplitArguments(refs);
                    
                    for (const auto& ref : refList)
                    {
                        uint64_t ptId = ExtractReference(ref);
                        auto it = state.CartesianPoints.find(ptId);
                        if (it != state.CartesianPoints.end())
                        {
                            points.push_back(it->second.To2D());
                        }
                    }
                }
                
                state.Polylines[entity.Id] = points;
            }

            // ============================================================
            // Единицы измерения
            // ============================================================

            else if (type == "IFCSIUNIT")
            {
                // IFCSIUNIT(*, .LENGTHUNIT., .MILLI., .METRE.)
                if (entity.Arguments.find("LENGTHUNIT") != std::string::npos)
                {
                    if (entity.Arguments.find("MILLI") != std::string::npos)
                    {
                        state.Document->LengthUnitScale = 1.0; // мм
                        state.Document->LengthUnitName = L"MILLIMETRE";
                    }
                    else if (entity.Arguments.find("CENTI") != std::string::npos)
                    {
                        state.Document->LengthUnitScale = 10.0; // см -> мм
                        state.Document->LengthUnitName = L"CENTIMETRE";
                    }
                    else if (entity.Arguments.find("METRE") != std::string::npos || 
                             entity.Arguments.find("METER") != std::string::npos)
                    {
                        state.Document->LengthUnitScale = 1000.0; // м -> мм
                        state.Document->LengthUnitName = L"METRE";
                    }
                }
            }
            else if (type == "IFCCONVERSIONBASEDUNIT")
            {
                // Обработка футов, дюймов и т.д.
                if (entity.Arguments.find("LENGTHUNIT") != std::string::npos)
                {
                    if (entity.Arguments.find("FOOT") != std::string::npos ||
                        entity.Arguments.find("foot") != std::string::npos)
                    {
                        state.Document->LengthUnitScale = 304.8; // ft -> мм
                        state.Document->LengthUnitName = L"FOOT";
                    }
                    else if (entity.Arguments.find("INCH") != std::string::npos ||
                             entity.Arguments.find("inch") != std::string::npos)
                    {
                        state.Document->LengthUnitScale = 25.4; // in -> мм
                        state.Document->LengthUnitName = L"INCH";
                    }
                }
            }

            // ============================================================
            // Связи (Relationships)
            // ============================================================

            else if (type == "IFCRELCONTAINEDINSPATIALSTRUCTURE")
            {
                // Связывает элементы с этажом
                // (..., (elements), structure)
                size_t listStart = entity.Arguments.find('(', entity.Arguments.find('(') + 1);
                size_t listEnd = entity.Arguments.find(')', listStart);
                
                if (listStart != std::string::npos && listEnd != std::string::npos)
                {
                    std::string elements = entity.Arguments.substr(listStart + 1, listEnd - listStart - 1);
                    auto elementRefs = SplitArguments(elements);
                    
                    // Последний аргумент - структура (этаж)
                    std::string remainder = entity.Arguments.substr(listEnd + 1);
                    uint64_t structureId = ExtractReference(remainder);
                    
                    for (const auto& ref : elementRefs)
                    {
                        uint64_t elemId = ExtractReference(ref);
                        if (elemId > 0)
                        {
                            state.RelContainedInSpatial[structureId].push_back(elemId);
                        }
                    }
                }
            }
            else if (type == "IFCRELVOIDSELEMENT")
            {
                // Связывает проём со стеной
                // (..., element, opening)
                auto refArgs = SplitArguments(entity.Arguments);
                if (refArgs.size() >= 6)
                {
                    uint64_t elementId = ExtractReference(refArgs[4]);
                    uint64_t openingId = ExtractReference(refArgs[5]);
                    if (elementId > 0 && openingId > 0)
                    {
                        state.RelVoidsElement[openingId] = elementId;
                    }
                }
            }
            else if (type == "IFCRELFILLSELEMENT")
            {
                // Связывает дверь/окно с проёмом
                auto refArgs = SplitArguments(entity.Arguments);
                if (refArgs.size() >= 6)
                {
                    uint64_t openingId = ExtractReference(refArgs[4]);
                    uint64_t fillingId = ExtractReference(refArgs[5]);
                    if (openingId > 0 && fillingId > 0)
                    {
                        state.RelFillsElement[fillingId] = openingId;
                    }
                }
            }
        }

        static void PostProcess(IfcParserState& state)
        {
            // Связываем двери/окна со стенами через цепочку отношений
            for (auto& door : state.Document->Doors)
            {
                auto fillIt = state.RelFillsElement.find(door->Id);
                if (fillIt != state.RelFillsElement.end())
                {
                    uint64_t openingId = fillIt->second;
                    auto voidIt = state.RelVoidsElement.find(openingId);
                    if (voidIt != state.RelVoidsElement.end())
                    {
                        door->HostWallId = voidIt->second;
                    }
                }
            }

            for (auto& window : state.Document->Windows)
            {
                auto fillIt = state.RelFillsElement.find(window->Id);
                if (fillIt != state.RelFillsElement.end())
                {
                    uint64_t openingId = fillIt->second;
                    auto voidIt = state.RelVoidsElement.find(openingId);
                    if (voidIt != state.RelVoidsElement.end())
                    {
                        window->HostWallId = voidIt->second;
                    }
                }
            }

            // Связываем стены с этажами и вычисляем границы
            for (auto& wall : state.Document->Walls)
            {
                // Обновляем границы
                state.Document->UpdateBounds(wall->StartPoint.X, wall->StartPoint.Y);
                state.Document->UpdateBounds(wall->EndPoint.X, wall->EndPoint.Y);
                
                for (const auto& contour : wall->Contours)
                {
                    for (const auto& pt : contour.Points)
                    {
                        state.Document->UpdateBounds(pt.X, pt.Y);
                    }
                }
            }

            for (auto& space : state.Document->Spaces)
            {
                for (const auto& contour : space->BoundaryContours)
                {
                    for (const auto& pt : contour.Points)
                    {
                        state.Document->UpdateBounds(pt.X, pt.Y);
                    }
                }
            }
        }
    };
}
