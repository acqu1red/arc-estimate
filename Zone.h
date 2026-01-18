#pragma once

// R5.5 — Зоны (Zone)
// Зона — группа помещений одной категории или пользовательская группировка

#include "pch.h"
#include "Models.h"
#include "Room.h"
#include <vector>
#include <string>
#include <set>
#include <memory>
#include <algorithm>
#include <numeric>

namespace winrt::estimate1
{
    // Тип зоны
    enum class ZoneType
    {
        Living,         // Жилая зона (гостиная, спальня, детская)
        Wet,            // Мокрые помещения (ванная, кухня, санузел)
        Service,        // Технические (кладовая, гардероб, котельная)
        Circulation,    // Коридоры (прихожая, холл, коридор)
        Outdoor,        // Открытые (балкон, лоджия, терраса)
        Custom          // Пользовательская зона
    };

    // Информация о зоне для отчётов
    struct ZoneSummary
    {
        ZoneType Type{ ZoneType::Custom };
        std::wstring Name;
        size_t RoomCount{ 0 };
        double TotalAreaSqM{ 0.0 };
        double TotalPerimeterM{ 0.0 };
        double TotalWallAreaSqM{ 0.0 };
        double TotalVolumeCuM{ 0.0 };
    };

    // Класс зоны
    class Zone
    {
    public:
        Zone() : m_id(IdGenerator::Next()) {}
        explicit Zone(ZoneType type) : m_id(IdGenerator::Next()), m_type(type)
        {
            m_name = GetDefaultName(type);
            m_color = GetDefaultColor(type);
        }

        uint64_t GetId() const { return m_id; }

        // =====================================================================
        // Идентификация
        // =====================================================================

        void SetName(const std::wstring& name) { m_name = name; }
        const std::wstring& GetName() const { return m_name; }

        void SetType(ZoneType type) { m_type = type; }
        ZoneType GetType() const { return m_type; }

        void SetDescription(const std::wstring& desc) { m_description = desc; }
        const std::wstring& GetDescription() const { return m_description; }

        // =====================================================================
        // Цвет отображения
        // =====================================================================

        void SetColor(Windows::UI::Color color) { m_color = color; }
        Windows::UI::Color GetColor() const { return m_color; }

        // =====================================================================
        // Помещения в зоне
        // =====================================================================

        void AddRoom(const std::shared_ptr<Room>& room)
        {
            if (room)
            {
                m_roomIds.insert(room->GetId());
                m_cachedRooms.push_back(room);
                InvalidateCache();
            }
        }

        void AddRoomById(uint64_t roomId)
        {
            m_roomIds.insert(roomId);
            InvalidateCache();
        }

        void RemoveRoom(uint64_t roomId)
        {
            m_roomIds.erase(roomId);
            m_cachedRooms.erase(
                std::remove_if(m_cachedRooms.begin(), m_cachedRooms.end(),
                    [roomId](const std::shared_ptr<Room>& r) { return r && r->GetId() == roomId; }),
                m_cachedRooms.end()
            );
            InvalidateCache();
        }

        void ClearRooms()
        {
            m_roomIds.clear();
            m_cachedRooms.clear();
            InvalidateCache();
        }

        const std::set<uint64_t>& GetRoomIds() const { return m_roomIds; }

        bool ContainsRoom(uint64_t roomId) const
        {
            return m_roomIds.find(roomId) != m_roomIds.end();
        }

        size_t GetRoomCount() const { return m_roomIds.size(); }

        // =====================================================================
        // Вычисляемые свойства (кэшируются)
        // =====================================================================

        double GetTotalAreaSqM() const
        {
            EnsureCache();
            return m_cachedAreaSqM;
        }

        double GetTotalPerimeterM() const
        {
            EnsureCache();
            return m_cachedPerimeterM;
        }

        double GetTotalWallAreaSqM() const
        {
            EnsureCache();
            return m_cachedWallAreaSqM;
        }

        double GetTotalVolumeCuM() const
        {
            EnsureCache();
            return m_cachedVolumeCuM;
        }

        // =====================================================================
        // Сводка для отчётов
        // =====================================================================

        ZoneSummary GetSummary() const
        {
            EnsureCache();
            ZoneSummary summary;
            summary.Type = m_type;
            summary.Name = m_name;
            summary.RoomCount = m_roomIds.size();
            summary.TotalAreaSqM = m_cachedAreaSqM;
            summary.TotalPerimeterM = m_cachedPerimeterM;
            summary.TotalWallAreaSqM = m_cachedWallAreaSqM;
            summary.TotalVolumeCuM = m_cachedVolumeCuM;
            return summary;
        }

        // Обновить кэш помещений из внешнего списка
        void UpdateRoomCache(const std::vector<std::shared_ptr<Room>>& allRooms)
        {
            m_cachedRooms.clear();
            for (const auto& room : allRooms)
            {
                if (room && m_roomIds.find(room->GetId()) != m_roomIds.end())
                {
                    m_cachedRooms.push_back(room);
                }
            }
            InvalidateCache();
        }

        // =====================================================================
        // Статические хелперы
        // =====================================================================

        static std::wstring GetDefaultName(ZoneType type)
        {
            switch (type)
            {
            case ZoneType::Living:      return L"Жилая зона";
            case ZoneType::Wet:         return L"Мокрые помещения";
            case ZoneType::Service:     return L"Технические помещения";
            case ZoneType::Circulation: return L"Коридоры и холлы";
            case ZoneType::Outdoor:     return L"Открытые пространства";
            case ZoneType::Custom:      return L"Пользовательская зона";
            default:                    return L"Зона";
            }
        }

        static Windows::UI::Color GetDefaultColor(ZoneType type)
        {
            switch (type)
            {
            case ZoneType::Living:      return Windows::UI::ColorHelper::FromArgb(255, 180, 220, 180);
            case ZoneType::Wet:         return Windows::UI::ColorHelper::FromArgb(255, 150, 200, 255);
            case ZoneType::Service:     return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200);
            case ZoneType::Circulation: return Windows::UI::ColorHelper::FromArgb(255, 220, 220, 180);
            case ZoneType::Outdoor:     return Windows::UI::ColorHelper::FromArgb(255, 180, 230, 200);
            case ZoneType::Custom:      return Windows::UI::ColorHelper::FromArgb(255, 200, 180, 220);
            default:                    return Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200);
            }
        }

        // Преобразование RoomCategory в ZoneType
        static ZoneType FromRoomCategory(RoomCategory category)
        {
            switch (category)
            {
            case RoomCategory::Living:      return ZoneType::Living;
            case RoomCategory::Wet:         return ZoneType::Wet;
            case RoomCategory::Service:     return ZoneType::Service;
            case RoomCategory::Circulation: return ZoneType::Circulation;
            case RoomCategory::Balcony:     return ZoneType::Outdoor;
            case RoomCategory::Office:      return ZoneType::Living; // Офис = жилая зона
            default:                        return ZoneType::Custom;
            }
        }

    private:
        void InvalidateCache() const
        {
            m_cacheValid = false;
        }

        void EnsureCache() const
        {
            if (m_cacheValid)
                return;

            m_cachedAreaSqM = 0.0;
            m_cachedPerimeterM = 0.0;
            m_cachedWallAreaSqM = 0.0;
            m_cachedVolumeCuM = 0.0;

            for (const auto& room : m_cachedRooms)
            {
                if (!room) continue;
                m_cachedAreaSqM += room->GetAreaSqM();
                m_cachedPerimeterM += room->GetPerimeterM();
                m_cachedWallAreaSqM += room->GetWallAreaSqM();
                m_cachedVolumeCuM += room->GetVolumeCuM();
            }

            m_cacheValid = true;
        }

        uint64_t m_id;
        ZoneType m_type{ ZoneType::Custom };
        std::wstring m_name;
        std::wstring m_description;
        Windows::UI::Color m_color{ Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200) };

        std::set<uint64_t> m_roomIds;
        std::vector<std::shared_ptr<Room>> m_cachedRooms;

        // Кэш вычислений
        mutable bool m_cacheValid{ false };
        mutable double m_cachedAreaSqM{ 0.0 };
        mutable double m_cachedPerimeterM{ 0.0 };
        mutable double m_cachedWallAreaSqM{ 0.0 };
        mutable double m_cachedVolumeCuM{ 0.0 };
    };

    // =========================================================================
    // Менеджер зон — автоматическая группировка и управление
    // =========================================================================

    class ZoneManager
    {
    public:
        ZoneManager() = default;

        // Автоматическая группировка помещений по категориям
        void RebuildFromRooms(const std::vector<std::shared_ptr<Room>>& rooms)
        {
            // Очищаем автоматические зоны (сохраняем пользовательские)
            m_autoZones.clear();

            // Группируем помещения по категориям
            std::map<ZoneType, std::vector<std::shared_ptr<Room>>> grouped;

            for (const auto& room : rooms)
            {
                if (!room) continue;
                ZoneType zoneType = Zone::FromRoomCategory(room->GetCategory());
                grouped[zoneType].push_back(room);
            }

            // Создаём зоны для каждой группы
            for (const auto& [zoneType, zoneRooms] : grouped)
            {
                if (zoneRooms.empty()) continue;

                auto zone = std::make_shared<Zone>(zoneType);
                for (const auto& room : zoneRooms)
                {
                    zone->AddRoom(room);
                }
                m_autoZones.push_back(zone);
            }

            // Сортируем зоны по типу
            std::sort(m_autoZones.begin(), m_autoZones.end(),
                [](const std::shared_ptr<Zone>& a, const std::shared_ptr<Zone>& b) {
                    return static_cast<int>(a->GetType()) < static_cast<int>(b->GetType());
                });
        }

        // Получить все автоматические зоны
        const std::vector<std::shared_ptr<Zone>>& GetAutoZones() const { return m_autoZones; }

        // Получить все пользовательские зоны
        const std::vector<std::shared_ptr<Zone>>& GetCustomZones() const { return m_customZones; }

        // Получить все зоны (авто + пользовательские)
        std::vector<std::shared_ptr<Zone>> GetAllZones() const
        {
            std::vector<std::shared_ptr<Zone>> all;
            all.reserve(m_autoZones.size() + m_customZones.size());
            all.insert(all.end(), m_autoZones.begin(), m_autoZones.end());
            all.insert(all.end(), m_customZones.begin(), m_customZones.end());
            return all;
        }

        // Создать пользовательскую зону
        Zone* CreateCustomZone(const std::wstring& name)
        {
            auto zone = std::make_shared<Zone>(ZoneType::Custom);
            zone->SetName(name);
            m_customZones.push_back(zone);
            return zone.get();
        }

        // Удалить пользовательскую зону
        bool RemoveCustomZone(uint64_t zoneId)
        {
            auto it = std::find_if(m_customZones.begin(), m_customZones.end(),
                [zoneId](const std::shared_ptr<Zone>& z) { return z && z->GetId() == zoneId; });
            if (it != m_customZones.end())
            {
                m_customZones.erase(it);
                return true;
            }
            return false;
        }

        // Найти зону по ID
        Zone* GetZone(uint64_t zoneId)
        {
            for (auto& z : m_autoZones)
                if (z && z->GetId() == zoneId) return z.get();
            for (auto& z : m_customZones)
                if (z && z->GetId() == zoneId) return z.get();
            return nullptr;
        }

        // Найти зону, содержащую помещение
        Zone* GetZoneContainingRoom(uint64_t roomId)
        {
            for (auto& z : m_autoZones)
                if (z && z->ContainsRoom(roomId)) return z.get();
            for (auto& z : m_customZones)
                if (z && z->ContainsRoom(roomId)) return z.get();
            return nullptr;
        }

        // Получить сводку по всем зонам
        std::vector<ZoneSummary> GetAllSummaries() const
        {
            std::vector<ZoneSummary> summaries;
            for (const auto& z : m_autoZones)
                if (z) summaries.push_back(z->GetSummary());
            for (const auto& z : m_customZones)
                if (z) summaries.push_back(z->GetSummary());
            return summaries;
        }

        // Общая площадь всех зон
        double GetTotalAreaSqM() const
        {
            double total = 0.0;
            for (const auto& z : m_autoZones)
                if (z) total += z->GetTotalAreaSqM();
            // Не добавляем пользовательские, чтобы избежать дублирования
            return total;
        }

        // Очистить все зоны
        void Clear()
        {
            m_autoZones.clear();
            m_customZones.clear();
        }

    private:
        std::vector<std::shared_ptr<Zone>> m_autoZones;    // Автоматические зоны по категориям
        std::vector<std::shared_ptr<Zone>> m_customZones;  // Пользовательские зоны
    };
}
