#pragma once

#include "pch.h"
#include "Models.h"
#include "Material.h"
#include "WallType.h"
#include "Dimension.h"
#include "Opening.h"
#include "Room.h"
#include "RoomDetector.h"
#include "Zone.h"
#include "Structure.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>

namespace winrt::estimate1
{
    // ��������� ��������� ���������
    class DocumentModel
    {
    public:
        DocumentModel()
        {
            InitializeDefaults();
        }

        // M3.5: ����������� �� ��������� ���������/���������� �����.
        // ������ ������ ������������ ����������� ������� (���������� ��� ������� �����).
        void NotifyWallChanged(uint64_t wallId)
        {
            (void)wallId;
            RebuildAutoDimensions();
        }

        void InitializeDefaults()
        {
            m_materials.clear();
            m_wallTypes.clear();
            m_doorTypes.clear();
            m_windowTypes.clear();

            // ��������� (���������� ��� M3.1)
            auto matBrick = std::make_shared<Material>(IdGenerator::Next(), L"������");
            matBrick->CostPerSquareMeter = 1200.0;
            matBrick->DisplayColor = Windows::UI::ColorHelper::FromArgb(255, 160, 90, 60);
            m_materials.push_back(matBrick);

            auto matConcrete = std::make_shared<Material>(IdGenerator::Next(), L"�����");
            matConcrete->CostPerSquareMeter = 900.0;
            matConcrete->DisplayColor = Windows::UI::ColorHelper::FromArgb(255, 120, 120, 120);
            m_materials.push_back(matConcrete);

            auto matFinish = std::make_shared<Material>(IdGenerator::Next(), L"����������");
            matFinish->CostPerSquareMeter = 250.0;
            matFinish->DisplayColor = Windows::UI::ColorHelper::FromArgb(255, 210, 210, 210);
            m_materials.push_back(matFinish);

            // ���� ����
            {
                auto type = std::make_shared<WallType>(L"���������� 100 (�����)");
                type->AddLayer(WallLayer(L"�������", 100.0, matConcrete));
                m_wallTypes.push_back(type);
            }
            {
                auto type = std::make_shared<WallType>(L"������� 250 (������)");
                type->AddLayer(WallLayer(L"�������", 250.0, matBrick));
                m_wallTypes.push_back(type);
            }
            {
                auto type = std::make_shared<WallType>(L"� �������� 150+20+20");
                type->AddLayer(WallLayer(L"�������", 150.0, matBrick));
                type->AddLayer(WallLayer(L"�������", 20.0, matFinish));
                type->AddLayer(WallLayer(L"�������", 20.0, matFinish));
                m_wallTypes.push_back(type);
            }

            // R4: ���� ������
            {
                auto type = std::make_shared<DoorType>(L"����� 900?2100", 900.0, 2100.0, DoorSwingType::RightInward);
                type->SetUnitCost(15000.0);
                m_doorTypes.push_back(type);
            }
            {
                auto type = std::make_shared<DoorType>(L"����� 800?2100", 800.0, 2100.0, DoorSwingType::RightInward);
                type->SetUnitCost(12000.0);
                m_doorTypes.push_back(type);
            }
            {
                auto type = std::make_shared<DoorType>(L"����� ������������� 1400?2100", 1400.0, 2100.0, DoorSwingType::DoubleInward);
                type->SetUnitCost(25000.0);
                m_doorTypes.push_back(type);
            }

            // R4: ���� ����
            {
                auto type = std::make_shared<WindowType_>(L"���� 1200?1400", 1200.0, 1400.0, 900.0, WindowType::Double);
                type->SetCostPerSqM(8000.0);
                m_windowTypes.push_back(type);
            }
            {
                auto type = std::make_shared<WindowType_>(L"���� 1500?1400", 1500.0, 1400.0, 900.0, WindowType::Triple);
                type->SetCostPerSqM(8500.0);
                m_windowTypes.push_back(type);
            }
            {
                auto type = std::make_shared<WindowType_>(L"���� ���������� 2400?2100", 2400.0, 2100.0, 300.0, WindowType::Panoramic);
                type->SetCostPerSqM(12000.0);
                m_windowTypes.push_back(type);
            }
        }

        const std::vector<std::shared_ptr<WallType>>& GetWallTypes() const { return m_wallTypes; }
        const std::vector<std::shared_ptr<Material>>& GetMaterials() const { return m_materials; }

        std::shared_ptr<WallType> GetWallTypeByName(const std::wstring& name) const
        {
            auto it = std::find_if(m_wallTypes.begin(), m_wallTypes.end(),
                [&name](const std::shared_ptr<WallType>& t) { return t && t->GetName() == name; });
            return (it != m_wallTypes.end()) ? *it : nullptr;
        }

        std::shared_ptr<WallType> GetDefaultWallType() const
        {
            return m_wallTypes.empty() ? nullptr : m_wallTypes.front();
        }

        // ���������� �����
        Wall* AddWall(const WorldPoint& start, const WorldPoint& end, double thickness = 150.0)
        {
            auto wall = std::make_unique<Wall>(start, end, thickness);
            // ���� ���� ��������� ��� �����, ��������� ���.
            if (auto t = GetDefaultWallType())
            {
                wall->SetType(t);
            }
            Wall* ptr = wall.get();
            m_walls.push_back(std::move(wall));

            // M3.5: ������������� ����������� ����� ���������� �����.
            RebuildAutoDimensions();
            return ptr;
        }

        // �������� �����
        bool RemoveWall(uint64_t id)
        {
            auto it = std::find_if(m_walls.begin(), m_walls.end(),
                [id](const std::unique_ptr<Wall>& w) { return w->GetId() == id; });
            
            if (it != m_walls.end())
            {
                // ���� ������� ��������� ������� � �������� �����, ����� �� �������� ������� ���������
                if (m_selectedElement == it->get())
                    m_selectedElement = nullptr;

                m_walls.erase(it);
                RebuildAutoDimensions();
                return true;
            }
            return false;
        }

        // R2.5: ��������� ����� (Split)
        bool SplitWall(uint64_t wallId, const WorldPoint& splitPt)
        {
            Wall* original = GetWall(wallId);
            if (!original) return false;

            WorldPoint start = original->GetStartPoint();
            WorldPoint end = original->GetEndPoint();

            // ������� ��� ����� �����
            auto w1 = std::make_unique<Wall>(start, splitPt, original->GetThickness());
            w1->SetType(original->GetType());
            w1->SetWorkState(original->GetWorkState());
            w1->SetHeight(original->GetHeight());

            auto w2 = std::make_unique<Wall>(splitPt, end, original->GetThickness());
            w2->SetType(original->GetType());
            w2->SetWorkState(original->GetWorkState());
            w2->SetHeight(original->GetHeight());

            // ������� ������ (��� rebuild ����)
            auto it = std::find_if(m_walls.begin(), m_walls.end(),
                [wallId](const std::unique_ptr<Wall>& w) { return w->GetId() == wallId; });
            if (it != m_walls.end())
            {
                if (m_selectedElement == it->get()) m_selectedElement = nullptr;
                m_walls.erase(it);
            }

            // ��������� �����
            m_walls.push_back(std::move(w1));
            m_walls.push_back(std::move(w2));

            RebuildAutoDimensions();
            return true;
        }

        // R2.5: ��������/�������� (Trim/Extend)
        bool TrimExtendWall(uint64_t wallId, const WorldPoint& newStart, const WorldPoint& newEnd)
        {
            Wall* wall = GetWall(wallId);
            if (!wall) return false;

            wall->SetStartPoint(newStart);
            wall->SetEndPoint(newEnd);
            
            NotifyWallChanged(wallId);
            return true;
        }

        // ��������� ����� �� ID
        Wall* GetWall(uint64_t id)
        {
            auto it = std::find_if(m_walls.begin(), m_walls.end(),
                [id](const std::unique_ptr<Wall>& w) { return w->GetId() == id; });
            
            return (it != m_walls.end()) ? it->get() : nullptr;
        }

        // ��������� ���� ����
        const std::vector<std::unique_ptr<Wall>>& GetWalls() const { return m_walls; }

        // ��������� (R5)
        const std::vector<std::shared_ptr<Room>>& GetRooms() const { return m_rooms; }

                // ���������� ����
                size_t GetWallCount() const { return m_walls.size(); }

                // ������� ������
                void Clear()
                {
                    m_walls.clear();
                    m_dimensions.clear();
                    m_manualDimensions.clear();
                    m_dimensionChains.clear();
                    m_loadedAutoDimensionStates.clear();
                    m_doors.clear();
                    m_windows.clear();
                    m_selectedElement = nullptr;
                }

                // ������� (����)
                const std::vector<std::unique_ptr<Dimension>>& GetDimensions() const { return m_dimensions; }

                // ������ �������
                const std::vector<std::unique_ptr<Dimension>>& GetManualDimensions() const { return m_manualDimensions; }

                // �������� ������ ������
                Dimension* AddManualDimension(const WorldPoint& p1, const WorldPoint& p2, double offset = 200.0)
                {
                    auto dim = std::make_unique<Dimension>(p1, p2);
                    dim->SetOffset(offset);
                    dim->SetLocked(true);
                    Dimension* ptr = dim.get();
                    m_manualDimensions.push_back(std::move(dim));
                    return ptr;
                }

                // ������� ������ ������
                bool RemoveManualDimension(uint64_t id)
                {
                    auto it = std::find_if(m_manualDimensions.begin(), m_manualDimensions.end(),
                        [id](const std::unique_ptr<Dimension>& d) { return d && d->GetId() == id; });
            
                    if (it != m_manualDimensions.end())
                    {
                if (m_selectedElement == it->get())
                    m_selectedElement = nullptr;

                        m_manualDimensions.erase(it);
                        return true;
                    }
                    return false;
                }

                // ������� ��������
                const std::vector<std::unique_ptr<DimensionChain>>& GetDimensionChains() const { return m_dimensionChains; }

                DimensionChain* AddDimensionChain()
                {
                    auto chain = std::make_unique<DimensionChain>();
                    DimensionChain* ptr = chain.get();
                    m_dimensionChains.push_back(std::move(chain));
                    return ptr;
                }

                DimensionChain* GetDimensionChainById(uint64_t chainId)
                {
                    auto it = std::find_if(m_dimensionChains.begin(), m_dimensionChains.end(),
                        [chainId](const std::unique_ptr<DimensionChain>& c) { return c && c->GetId() == chainId; });
                    return (it != m_dimensionChains.end()) ? it->get() : nullptr;
                }

                // ���� �������������� ��������
                bool IsAutoDimensionsEnabled() const { return m_autoDimensionsEnabled; }
                void SetAutoDimensionsEnabled(bool enabled)
                {
                    m_autoDimensionsEnabled = enabled;
                    if (enabled)
                    {
                        RebuildAutoDimensions();
                    }
                    else
                    {
                        // ��������� ������ ������ �������, ���� �������
                        m_dimensions.clear();
                    }
                }

                void RebuildAutoDimensions()
                {
            // R5.1: ����� ���������� �������� ��������� ���������
            RebuildRooms();

                    if (!m_autoDimensionsEnabled)
                        return;

                    // ��������� ��������� ��������������� �������� (offset)
                    struct LockedState
                    {
                        bool isLocked{ false };
                        double offset{ 0.0 };
                    };

                    std::unordered_map<uint64_t, LockedState> lockedByWall;
                    
                    for (const auto& d : m_dimensions)
                    {
                        // ��������� offset ���� ���� �� ���� ������ ����� ������������
                        // ��� ������� ��� ��� ������ ���� �����������
                        if (d && d->IsLocked())
                        {
                            lockedByWall[d->GetOwnerWallId()] = LockedState{ true, d->GetOffset() };
                        }
                    }

                    // ����������� �� ����� ��������� (���� ���� ��������� ������ offset'�)
                    for (const auto& kv : m_loadedAutoDimensionStates)
                    {
                        lockedByWall[kv.first] = LockedState{ true, kv.second };
                    }

                    m_dimensions.clear();
                    m_dimensionChains.clear();

                    for (const auto& w : m_walls)
                    {
                        if (!w)
                            continue;
                        
                        double len = w->GetLength();
                        if (len < 1.0) continue;

                        WorldPoint start = w->GetStartPoint();
                        WorldPoint end = w->GetEndPoint();
                        WorldPoint dir = w->GetDirection();

                        // 1. �������� �����
                        struct OpSegment { double start; double end; };
                        std::vector<OpSegment> ops;

                        // �����
                        for (const auto& d : m_doors) {
                            if (d && d->GetHostWallId() == w->GetId()) {
                                double center = d->GetPositionOnWall() * len;
                                double half = d->GetWidth() / 2.0;
                                ops.push_back({ center - half, center + half });
                            }
                        }
                        // ����
                        for (const auto& win : m_windows) {
                            if (win && win->GetHostWallId() == w->GetId()) {
                                double center = win->GetPositionOnWall() * len;
                                double half = win->GetWidth() / 2.0;
                                ops.push_back({ center - half, center + half });
                            }
                        }

                        std::sort(ops.begin(), ops.end(), [](const OpSegment& a, const OpSegment& b){ 
                            return a.start < b.start; 
                        });

                        // ���������� ��������
                        double offset = w->GetThickness() / 2.0 + 300.0;
                        bool isLocked = false;
                        
                        auto it = lockedByWall.find(w->GetId());
                        if (it != lockedByWall.end() && it->second.isLocked)
                        {
                            offset = it->second.offset;
                            isLocked = true;
                        }

                        if (ops.empty())
                        {
                            // ���� ����� ������
                            auto dim = std::make_unique<Dimension>(
                                w->GetId(),
                                start,
                                end,
                                DimensionType::WallLength);

                            dim->SetLocked(isLocked);
                            dim->SetOffset(offset);
                            m_dimensions.push_back(std::move(dim));
                        }
                        else
                        {
                            // ������� ��������
                            auto chain = std::make_unique<DimensionChain>();
                            chain->SetOffset(offset);
                            // TODO: Add logic to chain to sync locked state? 
                            // Currently chain is just a container of IDs.

                            double cur = 0.0;
                            auto addDim = [&](double s, double e, DimensionType type) {
                                if (e - s < 1.0) return;
                                WorldPoint p1(start.X + dir.X * s, start.Y + dir.Y * s);
                                WorldPoint p2(start.X + dir.X * e, start.Y + dir.Y * e);
                                auto d = std::make_unique<Dimension>(w->GetId(), p1, p2, type);
                                d->SetOffset(offset);
                                d->SetLocked(isLocked);
                                
                                // ������������� ����� � ��������
                                d->SetChainId(chain->GetId());
                                chain->AddDimensionId(d->GetId());
                                
                                m_dimensions.push_back(std::move(d));
                            };

                            for (const auto& op : ops) {
                                double s = std::clamp(op.start, 0.0, len);
                                double e = std::clamp(op.end, 0.0, len);
                                
                                // ������� �� �����
                                addDim(cur, s, DimensionType::WallSegment);
                                
                                // ����
                                addDim(s, e, DimensionType::OpeningWidth);
                                
                                cur = e;
                            }
                            
                            // ������� ����� ���������� �����
                            addDim(cur, len, DimensionType::WallSegment);
                            
                            m_dimensionChains.push_back(std::move(chain));
                        }
                    }

                    // ����� ������������ ������� ����������� ���������
                    m_loadedAutoDimensionStates.clear();
                }

                // ��������� ����������� ��������� ������������ (offset �� �����)
                void LoadAutoDimensionState(uint64_t wallId, double offset)
                {
                    m_loadedAutoDimensionStates[wallId] = offset;
                }

                // ��������� �������
                Element* GetSelectedElement() const { return m_selectedElement; }
        
                void SetSelectedElement(Element* element)
                {
                    // ������� ��������� � �����������
                    if (m_selectedElement)
                        m_selectedElement->SetSelected(false);

                    m_selectedElement = element;

                    // ������������� ��������� �� �����
                    if (m_selectedElement)
                        m_selectedElement->SetSelected(true);
                }

                // ����� �������� � ����� (��� ��������� ������)
                Element* HitTest(const WorldPoint& point, double tolerance, const LayerManager& layerManager)
                {
                    // ������� ������ ������� (����� ������� ����)
                    for (auto it = m_manualDimensions.rbegin(); it != m_manualDimensions.rend(); ++it)
                    {
                        Dimension* dim = it->get();
                        if (dim && dim->HitTest(point, tolerance))
                            return dim;
                    }

                    // ����� �����������
                    for (auto it = m_dimensions.rbegin(); it != m_dimensions.rend(); ++it)
                    {
                        Dimension* dim = it->get();
                        if (dim && dim->HitTest(point, tolerance))
                            return dim;
                    }

                    // R4: ��������� �����
                    for (auto it = m_doors.rbegin(); it != m_doors.rend(); ++it)
                    {
                        Door* door = it->get();
                        if (door && door->HitTest(point, tolerance))
                            return door;
                    }

                    // R4: ��������� ����
                    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
                    {
                        Window* window = it->get();
                        if (window && window->HitTest(point, tolerance))
                            return window;
                    }

                    // R6.1: ��������� ������� (������ ����)
                    for (auto it = m_columns.rbegin(); it != m_columns.rend(); ++it)
                    {
                        if (it->get()->HitTest(point, tolerance)) return it->get();
                    }
                    
                    // R6.5: ��������� ����� (������ ����)
                    for (auto it = m_beams.rbegin(); it != m_beams.rend(); ++it)
                    {
                        if (it->get()->HitTest(point, tolerance)) return it->get();
                    }

                    // ��������� ����� � �������� ������� (������ ����)
                    for (auto it = m_walls.rbegin(); it != m_walls.rend(); ++it)
                    {
                        Wall* wall = it->get();
                
                        // ��������� ��������� ����
                        if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                            continue;

                        if (wall->HitTest(point, tolerance))
                            return wall;
                    }

                    // R6.2: ��������� ���������� (��� �������, �� ��� �����������)
                    for (auto it = m_slabs.rbegin(); it != m_slabs.rend(); ++it)
                    {
                        if (it->get()->HitTest(point, tolerance)) return it->get();
                    }

                    // R5.2: ��������� ��������� (��������� � ��� ��� �������)
                    for (auto it = m_rooms.rbegin(); it != m_rooms.rend(); ++it)
                    {
                        Room* room = it->get();
                        if (room && room->HitTest(point, tolerance))
                            return room;
                    }

                    return nullptr;
                }

                // ����� ��������� �� ���� ���������
                void ClearSelection()
                {
                    for (auto& wall : m_walls)
                        wall->SetSelected(false);

                    for (auto& d : m_dimensions)
                        if (d) d->SetSelected(false);

                    for (auto& d : m_manualDimensions)
                        if (d) d->SetSelected(false);
            
                    for (auto& c : m_columns) c->SetSelected(false);
                    for (auto& s : m_slabs) s->SetSelected(false);
                    for (auto& b : m_beams) b->SetSelected(false);

                    m_selectedElement = nullptr;
                }

        // ��������, ��� ������� �� ��� �������� � ��������� (������ �� ������� ����������)
        bool IsElementAlive(const Element* element) const
        {
            if (!element)
                return false;

            for (const auto& w : m_walls)
                if (w.get() == element) return true;

            for (const auto& d : m_dimensions)
                if (d.get() == element) return true;

            for (const auto& d : m_manualDimensions)
                if (d.get() == element) return true;

            for (const auto& d : m_doors)
                if (d.get() == element) return true;

            for (const auto& w : m_windows)
                if (w.get() == element) return true;

            // R5.2: ��������� ���������
            for (const auto& r : m_rooms)
                if (r.get() == element) return true;

            // R6: �����������
            for (const auto& c : m_columns)
                if (c.get() == element) return true;
            for (const auto& s : m_slabs)
                if (s.get() == element) return true;
            for (const auto& b : m_beams)
                if (b.get() == element) return true;
            
            return false;
        }

        // =====================================================================
        // ���������� ������ ���� (M6 - Wall Type Editor)
        // =====================================================================

        // �������� ����� ��� �����
        void AddWallType(std::shared_ptr<WallType> wallType)
        {
            if (!wallType) return;
            // �������� �� �������� �� �����
            for (const auto& wt : m_wallTypes)
            {
                if (wt && wt->GetName() == wallType->GetName())
                    return; // ��� � ����� ������ ��� ����������
            }
            m_wallTypes.push_back(wallType);
        }

        // ������� ��� ����� �� �����
        bool RemoveWallType(const std::wstring& name)
        {
            auto it = std::find_if(m_wallTypes.begin(), m_wallTypes.end(),
                [&name](const std::shared_ptr<WallType>& t) { return t && t->GetName() == name; });
            
            if (it != m_wallTypes.end())
            {
                // ���������� ����� �� ���������� ����
                for (auto& wall : m_walls)
                {
                    if (wall && wall->GetType() && wall->GetType()->GetName() == name)
                    {
                        wall->ClearType();
                    }
                }
                m_wallTypes.erase(it);
                return true;
            }
            return false;
        }

        // =====================================================================
        // ���������� ����������� (M6 - Wall Type Editor)
        // =====================================================================

        // �������� ����� ��������
        void AddMaterial(std::shared_ptr<Material> material)
        {
            if (!material) return;
            // �������� �� �������� �� �����
            for (const auto& mat : m_materials)
            {
                if (mat && mat->Name == material->Name)
                    return; // �������� � ����� ������ ��� ����������
            }
            m_materials.push_back(material);
        }

        // ������� �������� �� �����
        bool RemoveMaterial(const std::wstring& name)
        {
            auto it = std::find_if(m_materials.begin(), m_materials.end(),
                [&name](const std::shared_ptr<Material>& m) { return m && m->Name == name; });
            
            if (it != m_materials.end())
            {
                // ������� ������ �� �������� �� ���� ����� ����
                for (auto& wt : m_wallTypes)
                {
                    if (!wt) continue;
                    for (auto& layer : wt->GetLayersForEdit())
                    {
                        if (layer.MaterialRef && layer.MaterialRef->Name == name)
                        {
                            layer.MaterialRef = nullptr;
                        }
                    }
                }
                m_materials.erase(it);
                return true;
            }
            return false;
        }

        // �������� �������� �� �����
        std::shared_ptr<Material> GetMaterialByName(const std::wstring& name) const
        {
            auto it = std::find_if(m_materials.begin(), m_materials.end(),
                [&name](const std::shared_ptr<Material>& m) { return m && m->Name == name; });
            return (it != m_materials.end()) ? *it : nullptr;
        }

        // =====================================================================
        // R4: ���������� �������
        // =====================================================================

        // �������� �����
        Door* AddDoor(std::shared_ptr<Door> door)
        {
            if (!door) return nullptr;
            Door* ptr = door.get();
            m_doors.push_back(door);
            
            // �������� ��� �������
            if (Wall* hostWall = GetWall(door->GetHostWallId()))
            {
                door->UpdateCachedPosition(*hostWall);
            }
            
            return ptr;
        }

        // ������� �����
        bool RemoveDoor(uint64_t id)
        {
            auto it = std::find_if(m_doors.begin(), m_doors.end(),
                [id](const std::shared_ptr<Door>& d) { return d && d->GetId() == id; });
            
            if (it != m_doors.end())
            {
                if (m_selectedElement == it->get())
                    m_selectedElement = nullptr;
                m_doors.erase(it);
                return true;
            }
            return false;
        }

        // �������� ����� �� ID
        Door* GetDoor(uint64_t id)
        {
            auto it = std::find_if(m_doors.begin(), m_doors.end(),
                [id](const std::shared_ptr<Door>& d) { return d && d->GetId() == id; });
            return (it != m_doors.end()) ? it->get() : nullptr;
        }

        // �������� ��� �����
        const std::vector<std::shared_ptr<Door>>& GetDoors() const { return m_doors; }

        // �������� ����� ��� ���������� �����
        std::vector<Door*> GetDoorsForWall(uint64_t wallId)
        {
            std::vector<Door*> result;
            for (const auto& door : m_doors)
            {
                if (door && door->GetHostWallId() == wallId)
                    result.push_back(door.get());
            }
            return result;
        }

        // =====================================================================
        // R4: ���������� ������
        // =====================================================================

        // �������� ����
        Window* AddWindow(std::shared_ptr<Window> window)
        {
            if (!window) return nullptr;
            Window* ptr = window.get();
            m_windows.push_back(window);
            
            // �������� ��� �������
            if (Wall* hostWall = GetWall(window->GetHostWallId()))
            {
                window->UpdateCachedPosition(*hostWall);
            }
            
            return ptr;
        }

        // ������� ����
        bool RemoveWindow(uint64_t id)
        {
            auto it = std::find_if(m_windows.begin(), m_windows.end(),
                [id](const std::shared_ptr<Window>& w) { return w && w->GetId() == id; });
            
            if (it != m_windows.end())
            {
                if (m_selectedElement == it->get())
                    m_selectedElement = nullptr;
                m_windows.erase(it);
                return true;
            }
            return false;
        }

        // �������� ���� �� ID
        Window* GetWindow(uint64_t id)
        {
            auto it = std::find_if(m_windows.begin(), m_windows.end(),
                [id](const std::shared_ptr<Window>& w) { return w && w->GetId() == id; });
            return (it != m_windows.end()) ? it->get() : nullptr;
        }

        // �������� ��� ����
        const std::vector<std::shared_ptr<Window>>& GetWindows() const { return m_windows; }

        // �������� ���� ��� ���������� �����
        std::vector<Window*> GetWindowsForWall(uint64_t wallId)
        {
            std::vector<Window*> result;
            for (const auto& window : m_windows)
            {
                if (window && window->GetHostWallId() == wallId)
                    result.push_back(window.get());
            }
            return result;
        }


        // =====================================================================
        // R4: ���� ������ � ����
        // =====================================================================

        const std::vector<std::shared_ptr<DoorType>>& GetDoorTypes() const { return m_doorTypes; }
        const std::vector<std::shared_ptr<WindowType_>>& GetWindowTypes() const { return m_windowTypes; }

        void AddDoorType(std::shared_ptr<DoorType> type)
        {
            if (type) m_doorTypes.push_back(type);
        }

        void AddWindowType(std::shared_ptr<WindowType_> type)
        {
            if (type) m_windowTypes.push_back(type);
        }

        std::shared_ptr<DoorType> GetDefaultDoorType() const
        {
            return m_doorTypes.empty() ? nullptr : m_doorTypes.front();
        }

        std::shared_ptr<WindowType_> GetDefaultWindowType() const
        {
            return m_windowTypes.empty() ? nullptr : m_windowTypes.front();
        }

        // =====================================================================
        // ���������� ����� ��� ��������� ����
        // =====================================================================

        void UpdateOpeningPositions()
        {
            for (auto& door : m_doors)
            {
                if (Wall* hostWall = GetWall(door->GetHostWallId()))
                {
                    door->UpdateCachedPosition(*hostWall);
                }
            }
            for (auto& window : m_windows)
            {
                if (Wall* hostWall = GetWall(window->GetHostWallId()))
                {
                    window->UpdateCachedPosition(*hostWall);
                }
            }
        }

        // ������� ��� ����� ��� �������� �����
        void RemoveOpeningsForWall(uint64_t wallId)
        {
            m_doors.erase(
                std::remove_if(m_doors.begin(), m_doors.end(),
                    [wallId](const std::shared_ptr<Door>& d) { return d && d->GetHostWallId() == wallId; }),
                m_doors.end()
            );
            m_windows.erase(
                std::remove_if(m_windows.begin(), m_windows.end(),
                    [wallId](const std::shared_ptr<Window>& w) { return w && w->GetHostWallId() == wallId; }),
                m_windows.end()
            );
        }

        // =====================================================================
        // R5.1: ������������� ���������
        // =====================================================================

        void RebuildRooms()
        {
            m_rooms = RoomDetector::DetectRooms(m_walls);
            // R5.5: ������������ ���� ����� ���������� ���������
            RebuildZones();
        }

        // =====================================================================
        // R5.5: ����
        // =====================================================================

        void RebuildZones()
        {
            m_zoneManager.RebuildFromRooms(m_rooms);
        }

        ZoneManager& GetZoneManager() { return m_zoneManager; }
        const ZoneManager& GetZoneManager() const { return m_zoneManager; }

        const std::vector<std::shared_ptr<Zone>>& GetAutoZones() const 
        { 
            return m_zoneManager.GetAutoZones(); 
        }

        std::vector<ZoneSummary> GetZoneSummaries() const
        {
            return m_zoneManager.GetAllSummaries();
        }

        // =====================================================================
        // R6: ������� � ����������
        // =====================================================================

        void AddColumn(std::shared_ptr<Column> column)
        {
            if (column) m_columns.push_back(column);
        }

        const std::vector<std::shared_ptr<Column>>& GetColumns() const { return m_columns; }
        
        void AddSlab(std::shared_ptr<Slab> slab)
        {
            if (slab) m_slabs.push_back(slab);
        }

        const std::vector<std::shared_ptr<Slab>>& GetSlabs() const { return m_slabs; }

        void AddBeam(std::shared_ptr<Beam> beam)
        {
            if (beam) m_beams.push_back(beam);
        }

        const std::vector<std::shared_ptr<Beam>>& GetBeams() const { return m_beams; }

        void RemoveStructuralElements()
        {
            m_columns.clear();
            m_slabs.clear();
            m_beams.clear();
        }

            private:
                std::vector<std::unique_ptr<Wall>> m_walls;
                std::vector<std::unique_ptr<Dimension>> m_dimensions;         // �����������
                std::vector<std::unique_ptr<Dimension>> m_manualDimensions;   // ������ �������
                std::vector<std::unique_ptr<DimensionChain>> m_dimensionChains;
                std::vector<std::shared_ptr<Room>> m_rooms;                   // R5: ���������
                ZoneManager m_zoneManager;                                    // R5.5: ����
                Element* m_selectedElement{ nullptr };
                bool m_autoDimensionsEnabled{ true };

                // �������� M3.1
                std::vector<std::shared_ptr<Material>> m_materials;
                std::vector<std::shared_ptr<WallType>> m_wallTypes;

                // R4: ����� � ����
                std::vector<std::shared_ptr<Door>> m_doors;
                std::vector<std::shared_ptr<Window>> m_windows;

                // R6: �����������
                std::vector<std::shared_ptr<Column>> m_columns;
                std::vector<std::shared_ptr<Slab>> m_slabs;
                std::vector<std::shared_ptr<Beam>> m_beams;
                std::vector<std::shared_ptr<DoorType>> m_doorTypes;
                std::vector<std::shared_ptr<WindowType_>> m_windowTypes;

                // ����������� ��������� ������������ �� ����� (offset �� �����)
                std::unordered_map<uint64_t, double> m_loadedAutoDimensionStates;

            };
        }

