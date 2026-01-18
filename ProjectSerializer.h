#pragma once

// ARC-Estimate Project Serialization (M6)
// Сериализация и десериализация проекта в формате JSON (.arcp)

#include "pch.h"
#include "json.hpp"
#include "Camera.h"
#include "Layer.h"
#include "Material.h"
#include "WallType.h"
#include "Models.h"
#include "Dimension.h"
#include "Element.h"
#include "DxfReference.h"
#include "IfcReference.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace winrt::estimate1
{
    // Версия формата файла проекта
    constexpr const char* PROJECT_FORMAT_VERSION = "1.0";
    constexpr const wchar_t* PROJECT_FILE_EXTENSION = L".arcp";

    // Результат операции сериализации
    struct SerializationResult
    {
        bool Success{ false };
        std::wstring ErrorMessage;
        std::wstring FilePath;
    };

    // Структура для хранения метаданных проекта
    struct ProjectMetadata
    {
        std::wstring Name{ L"Новый проект" };
        std::wstring Author;
        std::wstring Description;
        std::wstring CreatedDate;
        std::wstring ModifiedDate;
    };

    // Класс для сериализации/десериализации проекта
    class ProjectSerializer
    {
    public:
        // =====================================================================
        // СЕРИАЛИЗАЦИЯ (Save)
        // =====================================================================

        static SerializationResult SaveProject(
            const std::wstring& filePath,
            const ProjectMetadata& metadata,
            const Camera& camera,
            const LayerManager& layerManager,
            const DocumentModel& document,
            const DxfReferenceManager* dxfManager = nullptr,
            const IfcReferenceManager* ifcManager = nullptr)
        {
            SerializationResult result;
            result.FilePath = filePath;

            try
            {
                nlohmann::json root = nlohmann::json::object();

                // Версия формата
                root["version"] = PROJECT_FORMAT_VERSION;

                // Метаданные проекта
                root["metadata"] = SerializeMetadata(metadata);

                // Состояние камеры
                root["camera"] = SerializeCamera(camera);

                // Состояние слоёв
                root["layers"] = SerializeLayers(layerManager);

                // Каталог материалов
                root["materials"] = SerializeMaterials(document.GetMaterials());

                // Каталог типов стен
                root["wallTypes"] = SerializeWallTypes(document.GetWallTypes());

                // Элементы (стены)
                root["elements"] = SerializeElements(document);

                // Размеры
                root["dimensions"] = SerializeDimensions(document);

                // R5.2: Помещения (сохраняем пользовательские данные)
                root["rooms"] = SerializeRooms(document);

                // R5.5: Пользовательские зоны
                root["customZones"] = SerializeCustomZones(document);

                // R6.7: Конструкции
                root["columns"] = SerializeColumns(document);
                root["slabs"] = SerializeSlabs(document);
                root["beams"] = SerializeBeams(document);

                // DXF подложки (если есть)
                if (dxfManager && dxfManager->HasLayers())
                {
                    root["dxfReferences"] = SerializeDxfReferences(*dxfManager);
                }

                // IFC подложки (если есть)
                if (ifcManager && ifcManager->HasLayers())
                {
                    root["ifcReferences"] = SerializeIfcReferences(*ifcManager);
                }

                // Настройки документа
                root["settings"] = SerializeSettings(document);

                // Записываем в файл с форматированием
                std::string jsonStr = root.dump(2);

                std::ofstream file(filePath, std::ios::out | std::ios::binary);
                if (!file.is_open())
                {
                    result.ErrorMessage = L"Не удалось открыть файл для записи: " + filePath;
                    return result;
                }

                // Записываем UTF-8 BOM для совместимости
                const char bom[] = { '\xEF', '\xBB', '\xBF' };
                file.write(bom, 3);
                file.write(jsonStr.c_str(), jsonStr.size());
                file.close();

                result.Success = true;
            }
            catch (const std::exception& ex)
            {
                result.ErrorMessage = L"Ошибка сохранения: " + Utf8ToWstring(ex.what());
            }

            return result;
        }

        // =====================================================================
        // ДЕСЕРИАЛИЗАЦИЯ (Load)
        // =====================================================================

        static SerializationResult LoadProject(
            const std::wstring& filePath,
            ProjectMetadata& outMetadata,
            Camera& outCamera,
            LayerManager& outLayerManager,
            DocumentModel& outDocument,
            DxfReferenceManager* dxfManager = nullptr,
            IfcReferenceManager* ifcManager = nullptr)
        {
            SerializationResult result;
            result.FilePath = filePath;

            try
            {
                // Читаем файл
                std::ifstream file(filePath, std::ios::in | std::ios::binary);
                if (!file.is_open())
                {
                    result.ErrorMessage = L"Не удалось открыть файл: " + filePath;
                    return result;
                }

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                file.close();

                // Пропускаем UTF-8 BOM если есть
                if (content.size() >= 3 &&
                    static_cast<unsigned char>(content[0]) == 0xEF &&
                    static_cast<unsigned char>(content[1]) == 0xBB &&
                    static_cast<unsigned char>(content[2]) == 0xBF)
                {
                    content = content.substr(3);
                }

                // Парсим JSON
                nlohmann::json root = nlohmann::json::parse(content);

                // Проверяем версию
                if (root.contains("version"))
                {
                    std::string version = root["version"].get_string();
                    // В будущем здесь можно добавить миграцию версий
                }

                // Очищаем модель документа
                outDocument.Clear();

                // Загружаем метаданные
                if (root.contains("metadata"))
                {
                    DeserializeMetadata(root["metadata"], outMetadata);
                }

                // Загружаем камеру
                if (root.contains("camera"))
                {
                    DeserializeCamera(root["camera"], outCamera);
                }

                // Загружаем слои
                if (root.contains("layers"))
                {
                    DeserializeLayers(root["layers"], outLayerManager);
                }

                // Загружаем материалы (нужны для wallTypes)
                std::map<uint64_t, std::shared_ptr<Material>> materialMap;
                if (root.contains("materials"))
                {
                    DeserializeMaterials(root["materials"], outDocument, materialMap);
                }

                // Загружаем типы стен
                std::map<std::wstring, std::shared_ptr<WallType>> wallTypeMap;
                if (root.contains("wallTypes"))
                {
                    DeserializeWallTypes(root["wallTypes"], outDocument, materialMap, wallTypeMap);
                }

                // Загружаем элементы (стены)
                if (root.contains("elements"))
                {
                    DeserializeElements(root["elements"], outDocument, wallTypeMap);
                }

                // Загружаем размеры
                if (root.contains("dimensions"))
                {
                    DeserializeDimensions(root["dimensions"], outDocument);
                }

                // Загружаем DXF подложки
                if (root.contains("dxfReferences") && dxfManager)
                {
                    DeserializeDxfReferences(root["dxfReferences"], *dxfManager);
                }

                // Загружаем IFC подложки
                if (root.contains("ifcReferences") && ifcManager)
                {
                    DeserializeIfcReferences(root["ifcReferences"], *ifcManager);
                }

                // Загружаем настройки
                if (root.contains("settings"))
                {
                    DeserializeSettings(root["settings"], outDocument);
                }

                // Пересобираем авторазмеры (включая помещения и зоны)
                outDocument.RebuildAutoDimensions();

                // R5.2: Загружаем пользовательские данные помещений после пересборки
                if (root.contains("rooms"))
                {
                    DeserializeRooms(root["rooms"], outDocument);
                }

                // R5.5: Загружаем пользовательские зоны
                if (root.contains("customZones"))
                {
                    DeserializeCustomZones(root["customZones"], outDocument);
                }

                // R6.7: Конструкции
                if (root.contains("columns"))
                {
                    DeserializeColumns(root["columns"], outDocument);
                }
                if (root.contains("slabs"))
                {
                    DeserializeSlabs(root["slabs"], outDocument);
                }
                if (root.contains("beams"))
                {
                    DeserializeBeams(root["beams"], outDocument);
                }

                result.Success = true;
            }
            catch (const std::exception& ex)
            {
                result.ErrorMessage = L"Ошибка загрузки: " + Utf8ToWstring(ex.what());
            }

            return result;
        }

    private:
        // =====================================================================
        // СЕРИАЛИЗАЦИЯ КОМПОНЕНТОВ
        // =====================================================================

        static nlohmann::json SerializeMetadata(const ProjectMetadata& metadata)
        {
            nlohmann::json j = nlohmann::json::object();
            j["name"] = metadata.Name;
            j["author"] = metadata.Author;
            j["description"] = metadata.Description;
            j["createdDate"] = metadata.CreatedDate;
            j["modifiedDate"] = GetCurrentDateTime();
            return j;
        }

        static nlohmann::json SerializeCamera(const Camera& camera)
        {
            nlohmann::json j = nlohmann::json::object();
            j["offsetX"] = camera.GetOffset().X;
            j["offsetY"] = camera.GetOffset().Y;
            j["zoom"] = camera.GetZoom();
            return j;
        }

        static nlohmann::json SerializeLayers(const LayerManager& layerManager)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& layer : layerManager.GetLayers())
            {
                nlohmann::json j = nlohmann::json::object();
                j["name"] = layer.GetName();
                j["isVisible"] = layer.IsVisible();
                j["isLocked"] = layer.IsLocked();
                j["isActive"] = layer.IsActive();
                j["color"] = SerializeColor(layer.GetColor());
                j["linkedWorkState"] = static_cast<int>(layer.GetLinkedWorkState());
                arr.push_back(j);
            }
            return arr;
        }

        static nlohmann::json SerializeColor(Windows::UI::Color color)
        {
            nlohmann::json j = nlohmann::json::object();
            j["a"] = color.A;
            j["r"] = color.R;
            j["g"] = color.G;
            j["b"] = color.B;
            return j;
        }

        static nlohmann::json SerializeMaterials(const std::vector<std::shared_ptr<Material>>& materials)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& mat : materials)
            {
                if (!mat) continue;
                nlohmann::json j = nlohmann::json::object();
                j["id"] = mat->Id;
                j["name"] = mat->Name;
                j["code"] = mat->Code;
                j["costPerSquareMeter"] = mat->CostPerSquareMeter;
                j["color"] = SerializeColor(mat->DisplayColor);
                arr.push_back(j);
            }
            return arr;
        }

        static nlohmann::json SerializeWallTypes(const std::vector<std::shared_ptr<WallType>>& wallTypes)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& wt : wallTypes)
            {
                if (!wt) continue;
                nlohmann::json j = nlohmann::json::object();
                j["name"] = wt->GetName();
                
                nlohmann::json layersArr = nlohmann::json::array();
                for (const auto& layer : wt->GetLayers())
                {
                    nlohmann::json lj = nlohmann::json::object();
                    lj["name"] = layer.Name;
                    lj["thickness"] = layer.Thickness;
                    if (layer.MaterialRef)
                    {
                        lj["materialId"] = layer.MaterialRef->Id;
                    }
                    layersArr.push_back(lj);
                }
                j["layers"] = layersArr;
                arr.push_back(j);
            }
            return arr;
        }

        static nlohmann::json SerializeElements(const DocumentModel& document)
        {
            nlohmann::json elements = nlohmann::json::object();
            
            // Сериализуем стены
            nlohmann::json wallsArr = nlohmann::json::array();
            for (const auto& wall : document.GetWalls())
            {
                if (!wall) continue;
                nlohmann::json j = nlohmann::json::object();
                j["id"] = wall->GetId();
                j["name"] = wall->GetName();
                j["startX"] = wall->GetStartPoint().X;
                j["startY"] = wall->GetStartPoint().Y;
                j["endX"] = wall->GetEndPoint().X;
                j["endY"] = wall->GetEndPoint().Y;
                j["thickness"] = wall->GetThickness();
                j["height"] = wall->GetHeight();
                j["workState"] = static_cast<int>(wall->GetWorkState());
                j["locationLineMode"] = static_cast<int>(wall->GetLocationLineMode());
                j["allowJoinStart"] = wall->IsJoinAllowedAtStart();
                j["allowJoinEnd"] = wall->IsJoinAllowedAtEnd();
                
                if (wall->GetType())
                {
                    j["wallTypeName"] = wall->GetType()->GetName();
                }
                
                wallsArr.push_back(j);
            }
            elements["walls"] = wallsArr;
            
            return elements;
        }

        static nlohmann::json SerializeDimensions(const DocumentModel& document)
        {
            nlohmann::json dims = nlohmann::json::object();
            
            // Ручные размеры
            nlohmann::json manualArr = nlohmann::json::array();
            for (const auto& dim : document.GetManualDimensions())
            {
                if (!dim) continue;
                manualArr.push_back(SerializeDimension(*dim));
            }
            dims["manual"] = manualArr;
            
            // Авторазмеры - сохраняем только состояния locked и offsets
            nlohmann::json autoStatesArr = nlohmann::json::array();
            for (const auto& dim : document.GetDimensions())
            {
                if (!dim || !dim->IsLocked()) continue;
                nlohmann::json j = nlohmann::json::object();
                j["ownerWallId"] = dim->GetOwnerWallId();
                j["isLocked"] = dim->IsLocked();
                j["offset"] = dim->GetOffset();
                autoStatesArr.push_back(j);
            }
            dims["autoStates"] = autoStatesArr;
            
            return dims;
        }

        static nlohmann::json SerializeDimension(const Dimension& dim)
        {
            nlohmann::json j = nlohmann::json::object();
            j["id"] = dim.GetId();
            j["p1x"] = dim.GetP1().X;
            j["p1y"] = dim.GetP1().Y;
            j["p2x"] = dim.GetP2().X;
            j["p2y"] = dim.GetP2().Y;
            j["offset"] = dim.GetOffset();
            j["isLocked"] = dim.IsLocked();
            j["type"] = static_cast<int>(dim.GetDimensionType());
            j["ownerWallId"] = dim.GetOwnerWallId();
            j["chainId"] = dim.GetChainId();
            j["tickType"] = static_cast<int>(dim.GetTickType());
            return j;
        }

        static nlohmann::json SerializeDxfReferences(const DxfReferenceManager& manager)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& layer : manager.GetLayers())
            {
                if (!layer) continue;
                nlohmann::json j = nlohmann::json::object();
                j["filePath"] = layer->GetFilePath();
                j["name"] = layer->GetName();
                j["isVisible"] = layer->IsVisible();
                j["opacity"] = layer->GetOpacity();
                j["scale"] = layer->GetScale();
                j["offsetX"] = layer->GetOffset().X;
                j["offsetY"] = layer->GetOffset().Y;
                arr.push_back(j);
            }
            return arr;
        }

        static nlohmann::json SerializeIfcReferences(const IfcReferenceManager& manager)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& layer : manager.GetLayers())
            {
                if (!layer) continue;
                nlohmann::json j = nlohmann::json::object();
                j["filePath"] = layer->GetFilePath();
                j["name"] = layer->GetName();
                j["isVisible"] = layer->IsVisible();
                j["opacity"] = layer->GetOpacity();
                j["scale"] = layer->GetScale();
                j["offsetX"] = layer->GetOffset().X;
                j["offsetY"] = layer->GetOffset().Y;
                arr.push_back(j);
            }
            return arr;
        }

        static nlohmann::json SerializeSettings(const DocumentModel& document)
        {
            nlohmann::json j = nlohmann::json::object();
            j["autoDimensionsEnabled"] = document.IsAutoDimensionsEnabled();

            // R2.6: Join settings
            const auto& join = document.GetJoinSettings();
            nlohmann::json jJoin = nlohmann::json::object();
            jJoin["autoJoinEnabled"] = join.AutoJoinEnabled;
            jJoin["joinTolerance"] = join.JoinTolerance;
            jJoin["defaultStyle"] = static_cast<int>(join.DefaultStyle);
            jJoin["showJoinPreview"] = join.ShowJoinPreview;
            jJoin["extendToMeet"] = join.ExtendToMeet;
            j["joinSettings"] = jJoin;


            return j;
        }

        // R5.2: Сериализация помещений (пользовательские данные)
        static nlohmann::json SerializeRooms(const DocumentModel& document)
        {
            nlohmann::json arr = nlohmann::json::array();
            
            for (const auto& room : document.GetRooms())
            {
                if (!room) continue;
                
                nlohmann::json j = nlohmann::json::object();
                
                // Идентификация (сохраняем центроид для сопоставления при загрузке)
                WorldPoint centroid = room->GetLabelPoint();
                j["centroidX"] = centroid.X;
                j["centroidY"] = centroid.Y;
                
                // Пользовательские данные
                j["number"] = room->GetNumber();
                j["name"] = room->GetName();
                j["category"] = static_cast<int>(room->GetCategory());
                j["ceilingHeight"] = room->GetCeilingHeight();
                j["floorLevel"] = room->GetFloorLevel();
                
                // Отделка
                j["finishType"] = room->GetFinishType();
                j["floorFinish"] = room->GetFloorFinish();
                j["ceilingFinish"] = room->GetCeilingFinish();
                j["wallFinish"] = room->GetWallFinish();
                
                // Позиция метки (если переопределена)
                WorldPoint labelPos = room->GetLabelPoint();
                WorldPoint roomCentroid = room->GetLabelPoint();
                // Сохраняем только если метка перемещена пользователем
                if (labelPos.X != roomCentroid.X || labelPos.Y != roomCentroid.Y)
                {
                    j["labelX"] = labelPos.X;
                    j["labelY"] = labelPos.Y;
                }
                
                arr.push_back(j);
            }
            
            return arr;
        }

        // R5.5: Сериализация пользовательских зон
        static nlohmann::json SerializeCustomZones(const DocumentModel& document)
        {
            nlohmann::json arr = nlohmann::json::array();
            
            for (const auto& zone : document.GetZoneManager().GetCustomZones())
            {
                if (!zone) continue;
                
                nlohmann::json j = nlohmann::json::object();
                j["id"] = zone->GetId();
                j["name"] = zone->GetName();
                j["description"] = zone->GetDescription();
                j["type"] = static_cast<int>(zone->GetType());
                j["color"] = SerializeColor(zone->GetColor());
                
                // Сохраняем ID помещений в зоне
                nlohmann::json roomIds = nlohmann::json::array();
                for (uint64_t roomId : zone->GetRoomIds())
                {
                    roomIds.push_back(roomId);
                }
                j["roomIds"] = roomIds;
                
                arr.push_back(j);
            }
            
            return arr;
        }

        // R6.7: Сериализация колонн
        static nlohmann::json SerializeColumns(const DocumentModel& document)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& col : document.GetColumns())
            {
                if (!col) continue;
                nlohmann::json j = nlohmann::json::object();
                
                WorldPoint pos = col->GetPosition();
                j["x"] = pos.X;
                j["y"] = pos.Y;
                j["shape"] = static_cast<int>(col->GetShape());
                j["width"] = col->GetWidth();
                j["depth"] = col->GetDepth();
                j["rotation"] = col->GetRotation();
                j["height"] = col->GetHeight();
                j["baseOffset"] = col->GetBaseOffset();
                j["workState"] = static_cast<int>(col->GetWorkState());
                
                arr.push_back(j);
            }
            return arr;
        }

        // R6.7: Сериализация перекрытий
        static nlohmann::json SerializeSlabs(const DocumentModel& document)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& slab : document.GetSlabs())
            {
                if (!slab) continue;
                nlohmann::json j = nlohmann::json::object();
                
                j["thickness"] = slab->GetThickness();
                j["levelOffset"] = slab->GetLevelOffset();
                j["workState"] = static_cast<int>(slab->GetWorkState());

                nlohmann::json points = nlohmann::json::array();
                for (const auto& p : slab->GetContour())
                {
                    nlohmann::json jp = nlohmann::json::object();
                    jp["x"] = p.X;
                    jp["y"] = p.Y;
                    points.push_back(jp);
                }
                j["contour"] = points;
                
                arr.push_back(j);
            }
            return arr;
        }

        // R6.5: Сериализация балок
        static nlohmann::json SerializeBeams(const DocumentModel& document)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& beam : document.GetBeams())
            {
                if (!beam) continue;
                nlohmann::json j = nlohmann::json::object();
                
                WorldPoint start = beam->GetStartPoint();
                WorldPoint end = beam->GetEndPoint();
                
                j["startX"] = start.X;
                j["startY"] = start.Y;
                j["endX"] = end.X;
                j["endY"] = end.Y;
                j["width"] = beam->GetWidth();
                j["height"] = beam->GetHeight();
                j["levelOffset"] = beam->GetLevelOffset();
                j["workState"] = static_cast<int>(beam->GetWorkState());
                
                arr.push_back(j);
            }
            return arr;
        }

        // =====================================================================
        // ДЕСЕРИАЛИЗАЦИЯ КОМПОНЕНТОВ
        // =====================================================================

        static void DeserializeMetadata(const nlohmann::json& j, ProjectMetadata& metadata)
        {
            if (j.contains("name"))
                metadata.Name = j["name"].get_wstring();
            if (j.contains("author"))
                metadata.Author = j["author"].get_wstring();
            if (j.contains("description"))
                metadata.Description = j["description"].get_wstring();
            if (j.contains("createdDate"))
                metadata.CreatedDate = j["createdDate"].get_wstring();
            if (j.contains("modifiedDate"))
                metadata.ModifiedDate = j["modifiedDate"].get_wstring();
        }

        static void DeserializeCamera(const nlohmann::json& j, Camera& camera)
        {
            double offsetX = j.contains("offsetX") ? j["offsetX"].get_double() : 0.0;
            double offsetY = j.contains("offsetY") ? j["offsetY"].get_double() : 0.0;
            double zoom = j.contains("zoom") ? j["zoom"].get_double() : 0.5;
            
            camera.SetOffset(offsetX, offsetY);
            camera.SetZoom(zoom);
        }

        static void DeserializeLayers(const nlohmann::json& arr, LayerManager& layerManager)
        {
            if (!arr.is_array()) return;
            
            auto& layers = layerManager.GetLayers();
            
            for (size_t i = 0; i < arr.size() && i < layers.size(); ++i)
            {
                const auto& j = arr[i];
                
                if (j.contains("isVisible"))
                    layers[i].SetVisible(j["isVisible"].get_bool());
                if (j.contains("isLocked"))
                    layers[i].SetLocked(j["isLocked"].get_bool());
                if (j.contains("color"))
                    layers[i].SetColor(DeserializeColor(j["color"]));
            }
        }

        static Windows::UI::Color DeserializeColor(const nlohmann::json& j)
        {
            uint8_t a = j.contains("a") ? static_cast<uint8_t>(j["a"].get_int()) : 255;
            uint8_t r = j.contains("r") ? static_cast<uint8_t>(j["r"].get_int()) : 0;
            uint8_t g = j.contains("g") ? static_cast<uint8_t>(j["g"].get_int()) : 0;
            uint8_t b = j.contains("b") ? static_cast<uint8_t>(j["b"].get_int()) : 0;
            return Windows::UI::ColorHelper::FromArgb(a, r, g, b);
        }

        static void DeserializeMaterials(
            const nlohmann::json& arr,
            DocumentModel& document,
            std::map<uint64_t, std::shared_ptr<Material>>& materialMap)
        {
            if (!arr.is_array()) return;
            
            // Используем существующие материалы из документа
            const auto& existingMaterials = document.GetMaterials();
            for (const auto& mat : existingMaterials)
            {
                if (mat)
                    materialMap[mat->Id] = mat;
            }
            
            // Обновляем свойства из файла
            for (const auto& j : arr)
            {
                uint64_t id = j.contains("id") ? j["id"].get_uint64() : 0;
                std::wstring name = j.contains("name") ? j["name"].get_wstring() : L"";
                
                // Ищем по имени среди существующих
                std::shared_ptr<Material> mat = nullptr;
                for (const auto& m : existingMaterials)
                {
                    if (m && m->Name == name)
                    {
                        mat = m;
                        break;
                    }
                }
                
                if (mat && id > 0)
                {
                    materialMap[id] = mat;
                    // Обновляем свойства
                    if (j.contains("costPerSquareMeter"))
                        mat->CostPerSquareMeter = j["costPerSquareMeter"].get_double();
                    if (j.contains("color"))
                        mat->DisplayColor = DeserializeColor(j["color"]);
                }
            }
        }

        static void DeserializeWallTypes(
            const nlohmann::json& arr,
            DocumentModel& document,
            const std::map<uint64_t, std::shared_ptr<Material>>& materialMap,
            std::map<std::wstring, std::shared_ptr<WallType>>& wallTypeMap)
        {
            if (!arr.is_array()) return;
            
            // Используем существующие типы стен
            for (const auto& wt : document.GetWallTypes())
            {
                if (wt)
                    wallTypeMap[wt->GetName()] = wt;
            }
        }

        static void DeserializeElements(
            const nlohmann::json& elements,
            DocumentModel& document,
            const std::map<std::wstring, std::shared_ptr<WallType>>& wallTypeMap)
        {
            if (!elements.contains("walls")) return;
            
            const auto& wallsArr = elements["walls"];
            if (!wallsArr.is_array()) return;
            
            for (const auto& j : wallsArr)
            {
                double startX = j.contains("startX") ? j["startX"].get_double() : 0.0;
                double startY = j.contains("startY") ? j["startY"].get_double() : 0.0;
                double endX = j.contains("endX") ? j["endX"].get_double() : 0.0;
                double endY = j.contains("endY") ? j["endY"].get_double() : 0.0;
                double thickness = j.contains("thickness") ? j["thickness"].get_double() : 150.0;
                
                Wall* wall = document.AddWall(WorldPoint(startX, startY), WorldPoint(endX, endY), thickness);
                
                if (wall)
                {
                    if (j.contains("name"))
                        wall->SetName(j["name"].get_wstring());
                    if (j.contains("height"))
                        wall->SetHeight(j["height"].get_double());
                    if (j.contains("workState"))
                        wall->SetWorkState(static_cast<WorkStateNative>(j["workState"].get_int()));
                    if (j.contains("locationLineMode"))
                        wall->SetLocationLineMode(static_cast<LocationLineMode>(j["locationLineMode"].get_int()));
                    if (j.contains("allowJoinStart"))
                        wall->SetJoinAllowedAtStart(j["allowJoinStart"].get_bool());
                    if (j.contains("allowJoinEnd"))
                        wall->SetJoinAllowedAtEnd(j["allowJoinEnd"].get_bool());
                    
                    // Связываем с типом стены по имени
                    if (j.contains("wallTypeName"))
                    {
                        std::wstring typeName = j["wallTypeName"].get_wstring();
                        auto it = wallTypeMap.find(typeName);
                        if (it != wallTypeMap.end())
                        {
                            wall->SetType(it->second);
                        }
                    }
                }
            }
        }

        static void DeserializeDimensions(const nlohmann::json& dims, DocumentModel& document)
        {
            // Загружаем ручные размеры
            if (dims.contains("manual") && dims["manual"].is_array())
            {
                for (const auto& j : dims["manual"])
                {
                    double p1x = j.contains("p1x") ? j["p1x"].get_double() : 0.0;
                    double p1y = j.contains("p1y") ? j["p1y"].get_double() : 0.0;
                    double p2x = j.contains("p2x") ? j["p2x"].get_double() : 0.0;
                    double p2y = j.contains("p2y") ? j["p2y"].get_double() : 0.0;
                    double offset = j.contains("offset") ? j["offset"].get_double() : 200.0;
                    
                    Dimension* dim = document.AddManualDimension(
                        WorldPoint(p1x, p1y),
                        WorldPoint(p2x, p2y),
                        offset);
                    
                    if (dim)
                    {
                        if (j.contains("isLocked"))
                            dim->SetLocked(j["isLocked"].get_bool());
                        if (j.contains("tickType"))
                            dim->SetTickType(static_cast<DimensionTickType>(j["tickType"].get_int()));
                    }
                }
            }
            
            // Авторазмеры: сохраняем только состояния offset/lock на стену
            if (dims.contains("autoStates") && dims["autoStates"].is_array())
            {
                for (const auto& j : dims["autoStates"]) 
                {
                    if (!j.contains("ownerWallId"))
                        continue;
                    uint64_t wallId = j["ownerWallId"].get_uint64();
                    double offset = j.contains("offset") ? j["offset"].get_double() : 0.0;
                    document.LoadAutoDimensionState(wallId, offset);
                }
            }
        }

        static void DeserializeDxfReferences(const nlohmann::json& arr, DxfReferenceManager& manager)
        {
            if (!arr.is_array()) return;
            
            for (const auto& j : arr)
            {
                if (!j.contains("filePath")) continue;
                
                std::wstring filePath = j["filePath"].get_wstring();
                
                // Проверяем существование файла
                if (!std::filesystem::exists(filePath))
                    continue;
                
                // Импортируем DXF файл
                DxfImportSettings settings;
                if (j.contains("scale"))
                    settings.Scale = j["scale"].get_double();
                if (j.contains("offsetX") && j.contains("offsetY"))
                    settings.Offset = WorldPoint(j["offsetX"].get_double(), j["offsetY"].get_double());
                
                auto importResult = manager.ImportFile(filePath, settings);
                if (importResult.Success)
                {
                    auto* layer = manager.GetLayer(importResult.LayerIndex);
                    if (layer)
                    {
                        if (j.contains("name"))
                            layer->SetName(j["name"].get_wstring());
                        if (j.contains("isVisible"))
                            layer->SetVisible(j["isVisible"].get_bool());
                        if (j.contains("opacity"))
                            layer->SetOpacity(static_cast<uint8_t>(j["opacity"].get_double()));
                    }
                }
            }
        }

        static void DeserializeIfcReferences(const nlohmann::json& arr, IfcReferenceManager& manager)
        {
            if (!arr.is_array()) return;
            
            for (const auto& j : arr)
            {
                if (!j.contains("filePath")) continue;
                
                std::wstring filePath = j["filePath"].get_wstring();
                
                // Проверяем существование файла
                if (!std::filesystem::exists(filePath))
                    continue;
                
                // Импортируем IFC файл
                IfcImportSettings settings;
                if (j.contains("scale"))
                    settings.Scale = j["scale"].get_double();
                if (j.contains("offsetX") && j.contains("offsetY"))
                    settings.Offset = WorldPoint(j["offsetX"].get_double(), j["offsetY"].get_double());
                
                auto importResult = manager.ImportFile(filePath, settings);
                if (importResult.Success)
                {
                    auto* layer = manager.GetLayer(importResult.LayerIndex);
                    if (layer)
                    {
                        if (j.contains("name"))
                            layer->SetName(j["name"].get_wstring());
                        if (j.contains("isVisible"))
                            layer->SetVisible(j["isVisible"].get_bool());
                        if (j.contains("opacity"))
                            layer->SetOpacity(static_cast<uint8_t>(j["opacity"].get_double()));
                    }
                }
            }
        }

        static void DeserializeSettings(const nlohmann::json& j, DocumentModel& document)
        {
            if (j.contains("autoDimensionsEnabled"))
            {
                document.SetAutoDimensionsEnabled(j["autoDimensionsEnabled"].get_bool());
            }

            // R2.6: Join settings
            if (j.contains("joinSettings"))
            {
                const auto& js = j["joinSettings"];
                JoinSettings settings = document.GetJoinSettings();
                if (js.contains("autoJoinEnabled")) settings.AutoJoinEnabled = js["autoJoinEnabled"].get_bool();
                if (js.contains("joinTolerance")) settings.JoinTolerance = js["joinTolerance"].get_double();
                if (js.contains("defaultStyle")) settings.DefaultStyle = static_cast<JoinStyle>(js["defaultStyle"].get_int());
                if (js.contains("showJoinPreview")) settings.ShowJoinPreview = js["showJoinPreview"].get_bool();
                if (js.contains("extendToMeet")) settings.ExtendToMeet = js["extendToMeet"].get_bool();
                document.SetJoinSettings(settings);
            }
        }

        // R5.2: Десериализация помещений (применяем пользовательские данные к автодетектированным)
        static void DeserializeRooms(const nlohmann::json& arr, DocumentModel& document)
        {
            if (!arr.is_array()) return;
            
            const auto& rooms = document.GetRooms();
            const double matchTolerance = 500.0; // 500 мм для сопоставления центроидов
            
            for (const auto& j : arr)
            {
                if (!j.contains("centroidX") || !j.contains("centroidY"))
                    continue;
                
                double savedCentroidX = j["centroidX"].get_double();
                double savedCentroidY = j["centroidY"].get_double();
                WorldPoint savedCentroid{ savedCentroidX, savedCentroidY };
                
                // Ищем ближайшее помещение по центроиду
                Room* bestMatch = nullptr;
                double bestDist = matchTolerance;
                
                for (const auto& room : rooms)
                {
                    if (!room) continue;
                    WorldPoint roomCentroid = room->GetLabelPoint();
                    double dist = roomCentroid.Distance(savedCentroid);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestMatch = room.get();
                    }
                }
                
                if (!bestMatch)
                    continue;
                
                // Применяем сохранённые данные
                if (j.contains("number"))
                    bestMatch->SetNumber(j["number"].get_wstring());
                if (j.contains("name"))
                    bestMatch->SetName(j["name"].get_wstring());
                if (j.contains("category"))
                    bestMatch->SetCategory(static_cast<RoomCategory>(j["category"].get_int()));
                if (j.contains("ceilingHeight"))
                    bestMatch->SetCeilingHeight(j["ceilingHeight"].get_double());
                if (j.contains("floorLevel"))
                    bestMatch->SetFloorLevel(j["floorLevel"].get_double());
                
                // Отделка
                if (j.contains("finishType"))
                    bestMatch->SetFinishType(j["finishType"].get_wstring());
                if (j.contains("floorFinish"))
                    bestMatch->SetFloorFinish(j["floorFinish"].get_wstring());
                if (j.contains("ceilingFinish"))
                    bestMatch->SetCeilingFinish(j["ceilingFinish"].get_wstring());
                if (j.contains("wallFinish"))
                    bestMatch->SetWallFinish(j["wallFinish"].get_wstring());
                
                // Позиция метки
                if (j.contains("labelX") && j.contains("labelY"))
                {
                    bestMatch->SetLabelPosition(WorldPoint{
                        j["labelX"].get_double(),
                        j["labelY"].get_double()
                    });
                }
            }
        }

        // R5.5: Десериализация пользовательских зон
        static void DeserializeCustomZones(const nlohmann::json& arr, DocumentModel& document)
        {
            if (!arr.is_array()) return;
            
            ZoneManager& zoneManager = document.GetZoneManager();
            
            for (const auto& j : arr)
            {
                if (!j.contains("name"))
                    continue;
                
                std::wstring name = j["name"].get_wstring();
                Zone* zone = zoneManager.CreateCustomZone(name);
                
                if (!zone) continue;
                
                if (j.contains("description"))
                    zone->SetDescription(j["description"].get_wstring());
                if (j.contains("type"))
                    zone->SetType(static_cast<ZoneType>(j["type"].get_int()));
                if (j.contains("color"))
                    zone->SetColor(DeserializeColor(j["color"]));
                
                // Восстанавливаем связи с помещениями
                if (j.contains("roomIds") && j["roomIds"].is_array())
                {
                    for (const auto& idVal : j["roomIds"])
                    {
                        uint64_t roomId = idVal.get_uint64();
                        zone->AddRoomById(roomId);
                    }
                    // Обновляем кэш помещений
                    zone->UpdateRoomCache(document.GetRooms());
                }
            }
        }

        // R6.7: Десериализация колонн
        static void DeserializeColumns(const nlohmann::json& arr, DocumentModel& document)
        {
            if (!arr.is_array()) return;
            for (const auto& j : arr)
            {
                auto col = std::make_shared<Column>();
                
                if (j.contains("width") && j.contains("shape"))
                {
                    ColumnShape shape = static_cast<ColumnShape>(j["shape"].get_int());
                    double w = j["width"].get_double();
                    double d = j.contains("depth") ? j["depth"].get_double() : w;
                    
                    if (shape == ColumnShape::Rectangular)
                        col->SetExample(w, d);
                    else
                        col->SetCircular(w);
                }
                
                if (j.contains("x") && j.contains("y"))
                    col->SetPosition(WorldPoint{ j["x"].get_double(), j["y"].get_double() });
                    
                if (j.contains("rotation")) col->SetRotation(j["rotation"].get_double());
                if (j.contains("height")) col->SetHeight(j["height"].get_double());
                if (j.contains("baseOffset")) col->SetBaseOffset(j["baseOffset"].get_double());
                if (j.contains("workState")) col->SetWorkState(static_cast<WorkStateNative>(j["workState"].get_int()));
                
                document.AddColumn(col);
            }
        }

        // R6.7: Десериализация перекрытий
        static void DeserializeSlabs(const nlohmann::json& arr, DocumentModel& document)
        {
             if (!arr.is_array()) return;
             for (const auto& j : arr)
             {
                 auto slab = std::make_shared<Slab>();
                 
                 if (j.contains("thickness")) slab->SetThickness(j["thickness"].get_double());
                 if (j.contains("levelOffset")) slab->SetLevelOffset(j["levelOffset"].get_double());
                 if (j.contains("workState")) slab->SetWorkState(static_cast<WorkStateNative>(j["workState"].get_int()));
                 
                 if (j.contains("contour") && j["contour"].is_array())
                 {
                     std::vector<WorldPoint> points;
                     for (const auto& jp : j["contour"])
                     {
                         points.push_back(WorldPoint{ jp["x"].get_double(), jp["y"].get_double() });
                     }
                     slab->SetContour(points);
                 }
                 
                 document.AddSlab(slab);
             }
        }

        // R6.5: Десериализация балок
        static void DeserializeBeams(const nlohmann::json& arr, DocumentModel& document)
        {
             if (!arr.is_array()) return;
             for (const auto& j : arr)
             {
                 auto beam = std::make_shared<Beam>();
                 
                 if (j.contains("startX") && j.contains("startY"))
                     beam->SetStartPoint(WorldPoint{ j["startX"].get_double(), j["startY"].get_double() });
                 
                 if (j.contains("endX") && j.contains("endY"))
                     beam->SetEndPoint(WorldPoint{ j["endX"].get_double(), j["endY"].get_double() });

                 if (j.contains("width")) beam->SetWidth(j["width"].get_double());
                 if (j.contains("height")) beam->SetHeight(j["height"].get_double());
                 if (j.contains("levelOffset")) beam->SetLevelOffset(j["levelOffset"].get_double());
                 if (j.contains("workState")) beam->SetWorkState(static_cast<WorkStateNative>(j["workState"].get_int()));
                 
                 document.AddBeam(beam);
             }
        }

        // =====================================================================
        // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
        // =====================================================================

        static std::wstring Utf8ToWstring(const std::string& utf8)
        {
            if (utf8.empty()) return L"";
            int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
            std::wstring result(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
            return result;
        }

        static std::wstring GetCurrentDateTime()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);
            
            wchar_t buffer[64];
            std::wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &tm);
            return buffer;
        }
    };

} // namespace winrt::estimate1
