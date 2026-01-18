#pragma once

#include "pch.h"
#include "Models.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <optional>
#include <functional>
#include <filesystem>

namespace winrt::estimate1
{
    // ============================================================================
    // DXF Entity Types
    // ============================================================================

    enum class DxfEntityType
    {
        Unknown,
        Line,
        Polyline,
        LWPolyline,
        Circle,
        Arc,
        Text,
        MText,
        Point
    };

    // ============================================================================
    // DXF Entities
    // ============================================================================

    // Базовая сущность DXF
    struct DxfEntity
    {
        DxfEntityType Type{ DxfEntityType::Unknown };
        std::wstring LayerName{ L"0" };
        int ColorIndex{ 256 };  // 256 = ByLayer
        double Thickness{ 0.0 };

        virtual ~DxfEntity() = default;
    };

    // Линия
    struct DxfLine : public DxfEntity
    {
        WorldPoint Start{ 0, 0 };
        WorldPoint End{ 0, 0 };

        DxfLine()
        {
            Type = DxfEntityType::Line;
        }
    };

    // Вершина полилинии
    struct DxfVertex
    {
        WorldPoint Point{ 0, 0 };
        double Bulge{ 0.0 };  // Кривизна (0 = прямая)
    };

    // Полилиния (LWPOLYLINE или POLYLINE)
    struct DxfPolyline : public DxfEntity
    {
        std::vector<DxfVertex> Vertices;
        bool IsClosed{ false };

        DxfPolyline()
        {
            Type = DxfEntityType::LWPolyline;
        }
    };

    // Окружность
    struct DxfCircle : public DxfEntity
    {
        WorldPoint Center{ 0, 0 };
        double Radius{ 0.0 };

        DxfCircle()
        {
            Type = DxfEntityType::Circle;
        }
    };

    // Дуга
    struct DxfArc : public DxfEntity
    {
        WorldPoint Center{ 0, 0 };
        double Radius{ 0.0 };
        double StartAngle{ 0.0 };  // Градусы
        double EndAngle{ 360.0 };  // Градусы

        DxfArc()
        {
            Type = DxfEntityType::Arc;
        }
    };

    // Текст
    struct DxfText : public DxfEntity
    {
        WorldPoint Position{ 0, 0 };
        double Height{ 2.5 };
        double Rotation{ 0.0 };  // Градусы
        std::wstring Content;

        DxfText()
        {
            Type = DxfEntityType::Text;
        }
    };

    // ============================================================================
    // DXF Layer
    // ============================================================================

    struct DxfLayer
    {
        std::wstring Name{ L"0" };
        int ColorIndex{ 7 };  // White/black
        bool IsVisible{ true };
        bool IsFrozen{ false };
        bool IsLocked{ false };
    };

    // ============================================================================
    // DXF Document (результат парсинга)
    // ============================================================================

    struct DxfDocument
    {
        // Единицы измерения (INSUNITS)
        // 0 = Unitless, 1 = Inches, 2 = Feet, 4 = Millimeters, 5 = Centimeters, 6 = Meters
        int Units{ 0 };

        // Слои
        std::map<std::wstring, DxfLayer> Layers;

        // Сущности
        std::vector<std::unique_ptr<DxfEntity>> Entities;

        // Статистика
        size_t LineCount{ 0 };
        size_t PolylineCount{ 0 };
        size_t CircleCount{ 0 };
        size_t ArcCount{ 0 };
        size_t TextCount{ 0 };
        size_t TotalEntityCount{ 0 };

        // Границы (bounding box)
        WorldPoint MinBounds{ 0, 0 };
        WorldPoint MaxBounds{ 0, 0 };
        bool HasBounds{ false };

        void UpdateBounds(const WorldPoint& pt)
        {
            if (!HasBounds)
            {
                MinBounds = MaxBounds = pt;
                HasBounds = true;
            }
            else
            {
                MinBounds.X = (std::min)(MinBounds.X, pt.X);
                MinBounds.Y = (std::min)(MinBounds.Y, pt.Y);
                MaxBounds.X = (std::max)(MaxBounds.X, pt.X);
                MaxBounds.Y = (std::max)(MaxBounds.Y, pt.Y);
            }
        }

        // Применение масштаба (конвертация в мм)
        void ApplyScale(double scale)
        {
            for (auto& entity : Entities)
            {
                if (auto* line = dynamic_cast<DxfLine*>(entity.get()))
                {
                    line->Start.X *= scale;
                    line->Start.Y *= scale;
                    line->End.X *= scale;
                    line->End.Y *= scale;
                }
                else if (auto* poly = dynamic_cast<DxfPolyline*>(entity.get()))
                {
                    for (auto& v : poly->Vertices)
                    {
                        v.Point.X *= scale;
                        v.Point.Y *= scale;
                    }
                }
                else if (auto* circle = dynamic_cast<DxfCircle*>(entity.get()))
                {
                    circle->Center.X *= scale;
                    circle->Center.Y *= scale;
                    circle->Radius *= scale;
                }
                else if (auto* arc = dynamic_cast<DxfArc*>(entity.get()))
                {
                    arc->Center.X *= scale;
                    arc->Center.Y *= scale;
                    arc->Radius *= scale;
                }
                else if (auto* text = dynamic_cast<DxfText*>(entity.get()))
                {
                    text->Position.X *= scale;
                    text->Position.Y *= scale;
                    text->Height *= scale;
                }
            }

            // Обновляем границы
            MinBounds.X *= scale;
            MinBounds.Y *= scale;
            MaxBounds.X *= scale;
            MaxBounds.Y *= scale;
        }
    };

    // ============================================================================
    // DXF Parser
    // ============================================================================

    class DxfParser
    {
    public:
        // Результат парсинга
        struct ParseResult
        {
            bool Success{ false };
            std::wstring ErrorMessage;
            std::unique_ptr<DxfDocument> Document;
        };

        // Парсит DXF файл
        static ParseResult ParseFile(const std::wstring& filePath)
        {
            ParseResult result;

            try
            {
                // DXF обычно хранится как ASCII/ANSI/UTF-8, но не UTF-16.
                // Поэтому читаем как байты/узкие строки.
                // Важно: не конвертировать wchar_t путь через std::string(filePath.begin(), ...)
                // Это ломает не-ASCII пути (кириллица) и может приводить к сбоям.
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

        // Парсит содержимое DXF
        static ParseResult ParseContent(const std::string& content)
        {
            ParseResult result;
            result.Document = std::make_unique<DxfDocument>();

            try
            {
                DxfParserState state;
                state.Document = result.Document.get();

                // Разбиваем на строки
                std::stringstream ss(content);
                std::string line;

                while (std::getline(ss, line))
                {
                    // Убираем \r если есть
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();

                    // Trim
                    size_t start = line.find_first_not_of(" \t");
                    size_t end = line.find_last_not_of(" \t");
                    if (start != std::string::npos)
                        line = line.substr(start, end - start + 1);
                    else
                        line.clear();

                    // Альтернация: код / значение
                    if (state.ExpectingCode)
                    {
                        try
                        {
                            state.CurrentCode = std::stoi(line);
                        }
                        catch (...)
                        {
                            state.CurrentCode = 0;
                        }
                        state.ExpectingCode = false;
                    }
                    else
                    {
                        state.CurrentValue = Widen(line);
                        ProcessPair(state);
                        state.ExpectingCode = true;
                    }
                }

                // Проверяем, что что-то нашли
                if (result.Document->Entities.empty() && result.Document->Layers.empty())
                {
                    result.ErrorMessage = L"Файл не содержит распознанных сущностей DXF";
                    return result;
                }

                result.Success = true;
            }
            catch (const std::exception& ex)
            {
                result.ErrorMessage = L"Ошибка парсинга DXF: " + 
                    std::wstring(ex.what(), ex.what() + strlen(ex.what()));
            }

            return result;
        }

        // Определяет масштаб для конвертации в мм на основе единиц
        static double GetScaleToMM(int insunits)
        {
            switch (insunits)
            {
            case 1: return 25.4;      // Inches -> mm
            case 2: return 304.8;     // Feet -> mm
            case 4: return 1.0;       // Millimeters
            case 5: return 10.0;      // Centimeters -> mm
            case 6: return 1000.0;    // Meters -> mm
            default: return 1.0;      // Assume mm or unitless
            }
        }

    private:
        static std::wstring Widen(const std::string& s)
        {
            // Безопасное расширение байтов в wchar_t.
            // Для ASCII ключевых слов это корректно; для ANSI кириллицы сохранит байты.
            std::wstring out;
            out.reserve(s.size());
            for (unsigned char ch : s)
            {
                out.push_back(static_cast<wchar_t>(ch));
            }
            return out;
        }

        // Состояние парсера
        struct DxfParserState
        {
            DxfDocument* Document{ nullptr };

            bool ExpectingCode{ true };
            int CurrentCode{ 0 };
            std::wstring CurrentValue;

            // Текущая секция
            enum class Section { None, Header, Tables, Blocks, Entities, Objects };
            Section CurrentSection{ Section::None };

            // Текущая сущность
            std::unique_ptr<DxfEntity> CurrentEntity;
            std::wstring CurrentEntityType;

            // Текущий слой (для секции TABLES)
            std::optional<DxfLayer> CurrentLayer;
            bool InLayerTable{ false };

            // Для LWPOLYLINE
            int PolylineVertexCount{ 0 };
            int PolylineVertexIndex{ 0 };

            // Временные значения для полилинии
            std::vector<DxfVertex> TempVertices;
            DxfVertex TempVertex;
        };

        static void ProcessPair(DxfParserState& state)
        {
            int code = state.CurrentCode;
            const std::wstring& value = state.CurrentValue;

            // Определение секции
            if (code == 0)
            {
                // Завершаем текущую сущность
                FinalizeCurrentEntity(state);

                if (value == L"SECTION")
                {
                    // Следующий код 2 определит имя секции
                }
                else if (value == L"ENDSEC")
                {
                    state.CurrentSection = DxfParserState::Section::None;
                    state.InLayerTable = false;
                }
                else if (value == L"EOF")
                {
                    // Конец файла
                }
                else if (value == L"TABLE")
                {
                    // Начало таблицы
                }
                else if (value == L"ENDTAB")
                {
                    state.InLayerTable = false;
                }
                else if (value == L"LAYER" && state.CurrentSection == DxfParserState::Section::Tables)
                {
                    // Запись слоя
                    if (state.CurrentLayer.has_value())
                    {
                        state.Document->Layers[state.CurrentLayer->Name] = state.CurrentLayer.value();
                    }
                    state.CurrentLayer = DxfLayer();
                    state.InLayerTable = true;
                }
                else if (state.CurrentSection == DxfParserState::Section::Entities)
                {
                    // Начало новой сущности
                    StartNewEntity(state, value);
                }
            }
            else if (code == 2)
            {
                // Имя секции или таблицы
                if (value == L"HEADER")
                    state.CurrentSection = DxfParserState::Section::Header;
                else if (value == L"TABLES")
                    state.CurrentSection = DxfParserState::Section::Tables;
                else if (value == L"BLOCKS")
                    state.CurrentSection = DxfParserState::Section::Blocks;
                else if (value == L"ENTITIES")
                    state.CurrentSection = DxfParserState::Section::Entities;
                else if (value == L"OBJECTS")
                    state.CurrentSection = DxfParserState::Section::Objects;
                else if (value == L"LAYER" && state.CurrentSection == DxfParserState::Section::Tables)
                    state.InLayerTable = true;

                // Имя слоя в таблице
                if (state.CurrentLayer.has_value())
                    state.CurrentLayer->Name = value;
            }
            else if (code == 9 && state.CurrentSection == DxfParserState::Section::Header)
            {
                // Переменная заголовка
                if (value == L"$INSUNITS")
                {
                    // Следующий код 70 даст значение
                }
            }
            else if (code == 70)
            {
                // Флаги/значения
                if (state.CurrentSection == DxfParserState::Section::Header)
                {
                    try
                    {
                        state.Document->Units = std::stoi(value);
                    }
                    catch (...) {}
                }
                else if (state.InLayerTable && state.CurrentLayer.has_value())
                {
                    try
                    {
                        int flags = std::stoi(value);
                        state.CurrentLayer->IsFrozen = (flags & 1) != 0;
                        state.CurrentLayer->IsLocked = (flags & 4) != 0;
                    }
                    catch (...) {}
                }
            }
            else if (code == 62 && state.InLayerTable && state.CurrentLayer.has_value())
            {
                // Цвет слоя
                try
                {
                    state.CurrentLayer->ColorIndex = std::stoi(value);
                    // Отрицательный цвет = слой выключен
                    if (state.CurrentLayer->ColorIndex < 0)
                    {
                        state.CurrentLayer->IsVisible = false;
                        state.CurrentLayer->ColorIndex = -state.CurrentLayer->ColorIndex;
                    }
                }
                catch (...) {}
            }
            else
            {
                // Обработка атрибутов текущей сущности
                ProcessEntityAttribute(state, code, value);
            }
        }

        static void StartNewEntity(DxfParserState& state, const std::wstring& entityType)
        {
            state.CurrentEntityType = entityType;
            state.TempVertices.clear();
            state.TempVertex = DxfVertex();
            state.PolylineVertexCount = 0;
            state.PolylineVertexIndex = 0;

            if (entityType == L"LINE")
            {
                state.CurrentEntity = std::make_unique<DxfLine>();
            }
            else if (entityType == L"LWPOLYLINE" || entityType == L"POLYLINE")
            {
                state.CurrentEntity = std::make_unique<DxfPolyline>();
            }
            else if (entityType == L"CIRCLE")
            {
                state.CurrentEntity = std::make_unique<DxfCircle>();
            }
            else if (entityType == L"ARC")
            {
                state.CurrentEntity = std::make_unique<DxfArc>();
            }
            else if (entityType == L"TEXT" || entityType == L"MTEXT")
            {
                state.CurrentEntity = std::make_unique<DxfText>();
            }
        }

        static void ProcessEntityAttribute(DxfParserState& state, int code, const std::wstring& value)
        {
            if (!state.CurrentEntity)
                return;

            auto* entity = state.CurrentEntity.get();

            // Общие атрибуты
            if (code == 8)
            {
                entity->LayerName = value;
                return;
            }
            if (code == 62)
            {
                try { entity->ColorIndex = std::stoi(value); }
                catch (...) {}
                return;
            }

            // Специфичные атрибуты
            if (auto* line = dynamic_cast<DxfLine*>(entity))
            {
                ProcessLineAttribute(line, code, value);
            }
            else if (auto* poly = dynamic_cast<DxfPolyline*>(entity))
            {
                ProcessPolylineAttribute(state, poly, code, value);
            }
            else if (auto* circle = dynamic_cast<DxfCircle*>(entity))
            {
                ProcessCircleAttribute(circle, code, value);
            }
            else if (auto* arc = dynamic_cast<DxfArc*>(entity))
            {
                ProcessArcAttribute(arc, code, value);
            }
            else if (auto* text = dynamic_cast<DxfText*>(entity))
            {
                ProcessTextAttribute(text, code, value);
            }
        }

        static void ProcessLineAttribute(DxfLine* line, int code, const std::wstring& value)
        {
            try
            {
                double d = std::stod(value);
                switch (code)
                {
                case 10: line->Start.X = d; break;
                case 20: line->Start.Y = d; break;
                case 11: line->End.X = d; break;
                case 21: line->End.Y = d; break;
                }
            }
            catch (...) {}
        }

        static void ProcessPolylineAttribute(DxfParserState& state, DxfPolyline* poly, int code, const std::wstring& value)
        {
            try
            {
                if (code == 70)
                {
                    int flags = std::stoi(value);
                    poly->IsClosed = (flags & 1) != 0;
                }
                else if (code == 90)
                {
                    state.PolylineVertexCount = std::stoi(value);
                    state.TempVertices.reserve(state.PolylineVertexCount);
                }
                else if (code == 10)
                {
                    // Новая вершина — сохраняем предыдущую, если была
                    if (state.PolylineVertexIndex > 0)
                    {
                        state.TempVertices.push_back(state.TempVertex);
                    }
                    state.TempVertex = DxfVertex();
                    state.TempVertex.Point.X = std::stod(value);
                    state.PolylineVertexIndex++;
                }
                else if (code == 20)
                {
                    state.TempVertex.Point.Y = std::stod(value);
                }
                else if (code == 42)
                {
                    state.TempVertex.Bulge = std::stod(value);
                }
            }
            catch (...) {}
        }

        static void ProcessCircleAttribute(DxfCircle* circle, int code, const std::wstring& value)
        {
            try
            {
                double d = std::stod(value);
                switch (code)
                {
                case 10: circle->Center.X = d; break;
                case 20: circle->Center.Y = d; break;
                case 40: circle->Radius = d; break;
                }
            }
            catch (...) {}
        }

        static void ProcessArcAttribute(DxfArc* arc, int code, const std::wstring& value)
        {
            try
            {
                double d = std::stod(value);
                switch (code)
                {
                case 10: arc->Center.X = d; break;
                case 20: arc->Center.Y = d; break;
                case 40: arc->Radius = d; break;
                case 50: arc->StartAngle = d; break;
                case 51: arc->EndAngle = d; break;
                }
            }
            catch (...) {}
        }

        static void ProcessTextAttribute(DxfText* text, int code, const std::wstring& value)
        {
            try
            {
                switch (code)
                {
                case 1:
                    text->Content = value;
                    break;
                case 10:
                    text->Position.X = std::stod(value);
                    break;
                case 20:
                    text->Position.Y = std::stod(value);
                    break;
                case 40:
                    text->Height = std::stod(value);
                    break;
                case 50:
                    text->Rotation = std::stod(value);
                    break;
                }
            }
            catch (...) {}
        }

        static void FinalizeCurrentEntity(DxfParserState& state)
        {
            if (!state.CurrentEntity)
                return;

            // Для полилинии добавляем последнюю вершину
            if (auto* poly = dynamic_cast<DxfPolyline*>(state.CurrentEntity.get()))
            {
                if (state.PolylineVertexIndex > 0)
                {
                    state.TempVertices.push_back(state.TempVertex);
                }
                poly->Vertices = std::move(state.TempVertices);
                state.TempVertices.clear();
                state.TempVertex = DxfVertex();

                if (poly->Vertices.size() >= 2)
                {
                    for (const auto& v : poly->Vertices)
                    {
                        state.Document->UpdateBounds(v.Point);
                    }
                    state.Document->Entities.push_back(std::move(state.CurrentEntity));
                    state.Document->PolylineCount++;
                    state.Document->TotalEntityCount++;
                }
            }
            else if (auto* line = dynamic_cast<DxfLine*>(state.CurrentEntity.get()))
            {
                state.Document->UpdateBounds(line->Start);
                state.Document->UpdateBounds(line->End);
                state.Document->Entities.push_back(std::move(state.CurrentEntity));
                state.Document->LineCount++;
                state.Document->TotalEntityCount++;
            }
            else if (auto* circle = dynamic_cast<DxfCircle*>(state.CurrentEntity.get()))
            {
                WorldPoint p1{ circle->Center.X - circle->Radius, circle->Center.Y - circle->Radius };
                WorldPoint p2{ circle->Center.X + circle->Radius, circle->Center.Y + circle->Radius };
                state.Document->UpdateBounds(p1);
                state.Document->UpdateBounds(p2);
                state.Document->Entities.push_back(std::move(state.CurrentEntity));
                state.Document->CircleCount++;
                state.Document->TotalEntityCount++;
            }
            else if (auto* arc = dynamic_cast<DxfArc*>(state.CurrentEntity.get()))
            {
                WorldPoint p1{ arc->Center.X - arc->Radius, arc->Center.Y - arc->Radius };
                WorldPoint p2{ arc->Center.X + arc->Radius, arc->Center.Y + arc->Radius };
                state.Document->UpdateBounds(p1);
                state.Document->UpdateBounds(p2);
                state.Document->Entities.push_back(std::move(state.CurrentEntity));
                state.Document->ArcCount++;
                state.Document->TotalEntityCount++;
            }
            else if (auto* text = dynamic_cast<DxfText*>(state.CurrentEntity.get()))
            {
                if (!text->Content.empty())
                {
                    state.Document->UpdateBounds(text->Position);
                    state.Document->Entities.push_back(std::move(state.CurrentEntity));
                    state.Document->TextCount++;
                    state.Document->TotalEntityCount++;
                }
            }
            else if (state.CurrentEntity)
            {
                // Неизвестная сущность — пропускаем
            }

            state.CurrentEntity.reset();
            state.CurrentEntityType.clear();
        }
    };
}
