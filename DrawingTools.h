#pragma once

#include "pch.h"
#include "Element.h"  // Includes Models.h which defines WallAttachmentMode
#include "Camera.h"
#include "Layer.h"
#include "WallAttachmentSystem.h"  // R-WALL: Полное определение для ToLocationLineMode()
#include <functional>
#include <cmath>

namespace winrt::estimate1
{
    // ��������� �������� (snap)
    struct SnapResult
    {
        bool hasSnap{ false };
        WorldPoint point{ 0, 0 };
        std::wstring snapType{ L"" }; // "endpoint", "midpoint", "grid", "perpendicular"
    };

    // �������� ��������
    class SnapManager
    {
    public:
        SnapManager() = default;

        // ��������� ��������
        void SetGridSnapEnabled(bool enabled) { m_gridSnapEnabled = enabled; }
        bool IsGridSnapEnabled() const { return m_gridSnapEnabled; }

        void SetEndpointSnapEnabled(bool enabled) { m_endpointSnapEnabled = enabled; }
        bool IsEndpointSnapEnabled() const { return m_endpointSnapEnabled; }

        void SetMidpointSnapEnabled(bool enabled) { m_midpointSnapEnabled = enabled; }
        bool IsMidpointSnapEnabled() const { return m_midpointSnapEnabled; }

        void SetSnapTolerance(double tolerance) { m_snapTolerance = tolerance; }
        double GetSnapTolerance() const { return m_snapTolerance; }

        void SetGridSpacing(double spacing) { m_gridSpacing = spacing; }
        double GetGridSpacing() const { return m_gridSpacing; }

        // ����� ����� ��������
        SnapResult FindSnap(
            const WorldPoint& cursorWorld,
            const DocumentModel& document,
            const LayerManager& layerManager,
            const Camera& camera)
        {
            SnapResult result;
            double bestDistance = m_snapTolerance;

            // ����������� ������ �� �������� � ������� �������
            double worldTolerance = m_snapTolerance / camera.GetZoom();

            // 1. �������� � �������� ������ ����
            if (m_endpointSnapEnabled)
            {
                for (const auto& wall : document.GetWalls())
                {
                    if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                        continue;

                    // ��������� �����
                    double distStart = cursorWorld.Distance(wall->GetStartPoint());
                    if (distStart < worldTolerance && distStart < bestDistance)
                    {
                        bestDistance = distStart;
                        result.hasSnap = true;
                        result.point = wall->GetStartPoint();
                        result.snapType = L"endpoint";
                    }

                    // �������� �����
                    double distEnd = cursorWorld.Distance(wall->GetEndPoint());
                    if (distEnd < worldTolerance && distEnd < bestDistance)
                    {
                        bestDistance = distEnd;
                        result.hasSnap = true;
                        result.point = wall->GetEndPoint();
                        result.snapType = L"endpoint";
                    }
                }
            }

            // 2. �������� � ��������� ����
            if (m_midpointSnapEnabled && !result.hasSnap)
            {
                for (const auto& wall : document.GetWalls())
                {
                    if (!layerManager.IsWorkStateVisible(wall->GetWorkState()))
                        continue;

                    WorldPoint midpoint(
                        (wall->GetStartPoint().X + wall->GetEndPoint().X) / 2,
                        (wall->GetStartPoint().Y + wall->GetEndPoint().Y) / 2
                    );

                    double distMid = cursorWorld.Distance(midpoint);
                    if (distMid < worldTolerance && distMid < bestDistance)
                    {
                        bestDistance = distMid;
                        result.hasSnap = true;
                        result.point = midpoint;
                        result.snapType = L"midpoint";
                    }
                }
            }

            // 3. �������� � �����
            if (m_gridSnapEnabled && !result.hasSnap)
            {
                WorldPoint gridPoint = SnapToGrid(cursorWorld);
                double distGrid = cursorWorld.Distance(gridPoint);
                
                if (distGrid < worldTolerance)
                {
                    result.hasSnap = true;
                    result.point = gridPoint;
                    result.snapType = L"grid";
                }
            }

            return result;
        }

        // �������� ����� � �����
        WorldPoint SnapToGrid(const WorldPoint& point) const
        {
            return WorldPoint(
                std::round(point.X / m_gridSpacing) * m_gridSpacing,
                std::round(point.Y / m_gridSpacing) * m_gridSpacing
            );
        }

    private:
        bool m_gridSnapEnabled{ false };
        bool m_endpointSnapEnabled{ true };
        bool m_midpointSnapEnabled{ true };
        double m_snapTolerance{ 15.0 };    // ������ � ��������
        double m_gridSpacing{ 100.0 };      // ��� ����� � ��
    };

    // ��������� ����������� ��������� ����
    enum class WallToolState
    {
        Idle,           // �������� ������� �����
        Drawing,        // ��������� (������� ��������� �����)
        ChainDrawing    // ����������� ������� ����
    };

    // ���������� ��������� ����
    class WallTool
    {
    public:
        WallTool() = default;

        // ��������� �����������
        WallToolState GetState() const { return m_state; }

        // ������� ��������� �����
        WorldPoint GetStartPoint() const { return m_startPoint; }

        // ������� ������� �����
        double GetThickness() const { return m_thickness; }
        void SetThickness(double thickness) { m_thickness = std::clamp(thickness, 50.0, 1000.0); }

        // ������� WorkState ��� ����� ����
        WorkStateNative GetWorkState() const { return m_workState; }
        void SetWorkState(WorkStateNative state) { m_workState = state; }

        // R-WALL: ������� WallAttachmentMode (����� �������)
        WallAttachmentMode GetAttachmentMode() const { return m_attachmentMode; }
        void SetAttachmentMode(WallAttachmentMode mode) { m_attachmentMode = mode; }

        // ������� LocationLineMode (��� �������� ��������������)
        LocationLineMode GetLocationLineMode() const
        {
            return WallAttachmentSystem::ToLocationLineMode(m_attachmentMode);
        }
        void SetLocationLineMode(LocationLineMode mode)
        {
            // ������������� LocationLineMode � WallAttachmentMode
            switch (mode)
            {
            case LocationLineMode::WallCenterline:
            case LocationLineMode::CoreCenterline:
                m_attachmentMode = WallAttachmentMode::Core;
                break;
            case LocationLineMode::FinishFaceExterior:
            case LocationLineMode::CoreFaceExterior:
                m_attachmentMode = WallAttachmentMode::FinishExterior;
                break;
            case LocationLineMode::FinishFaceInterior:
            case LocationLineMode::CoreFaceInterior:
                m_attachmentMode = WallAttachmentMode::FinishInterior;
                break;
            }
        }

        // ���� "����������� �����" (Spacebar flip)
        bool IsFlipped() const { return m_isFlipped; }
        void SetFlipped(bool flipped) { m_isFlipped = flipped; }
        void ToggleFlip() { m_isFlipped = !m_isFlipped; }

        // ��������� ����� ����
        // ���������� true, ���� ����� ���� �������
        bool OnClick(
            const WorldPoint& worldPoint,
            DocumentModel& document,
            SnapManager& snapManager,
            const LayerManager& layerManager,
            const Camera& camera)
        {
            // ��������� ��������
            SnapResult snap = snapManager.FindSnap(worldPoint, document, layerManager, camera);
            WorldPoint finalPoint = snap.hasSnap ? snap.point : worldPoint;

            switch (m_state)
            {
            case WallToolState::Idle:
                // �������� ��������� � ���������� ��������� �����
                m_startPoint = finalPoint;
                m_isFlipped = false; // ���������� ���� ��� ������ ����� �����
                m_state = WallToolState::Drawing;
                return false;

            case WallToolState::Drawing:
            case WallToolState::ChainDrawing:
                // ��������� �����
                if (m_startPoint.Distance(finalPoint) > 10.0) // ����������� ����� 10��
                {
                    // ��������� flip ���� �������
                    WorldPoint effectiveStart = m_startPoint;
                    WorldPoint effectiveEnd = finalPoint;
                    
                    if (m_isFlipped)
                    {
                        // ��� flip ������ ������� start � end
                        std::swap(effectiveStart, effectiveEnd);
                    }

                    Wall* newWall = document.AddWall(effectiveStart, effectiveEnd, m_thickness);
                    newWall->SetWorkState(m_workState);

                    // R-WALL: ������������� ����� �������� ����� ����������� �� WallAttachmentMode
                    newWall->SetLocationLineMode(
                        WallAttachmentSystem::ToLocationLineMode(m_attachmentMode));

                    // ���������� ������� � ����� ������� ����� = ������ ���������
                    m_startPoint = finalPoint;
                    m_isFlipped = false; // ���������� flip ��� ��������� �����
                    m_state = WallToolState::ChainDrawing;

                    if (m_onWallCreated)
                        m_onWallCreated(newWall);

                    return true;
                }
                return false;
            }

            return false;
        }

        // ��������� �������� ���� (��� ������)
        void OnMouseMove(const WorldPoint& worldPoint)
        {
            m_currentPoint = worldPoint;
        }

        // ������ �������� ���������
        void Cancel()
        {
            m_state = WallToolState::Idle;
            m_startPoint = WorldPoint(0, 0);
            m_currentPoint = WorldPoint(0, 0);
            m_isFlipped = false;
        }

        // ���������� ������� (������� ���� ��� ESC)
        void EndChain()
        {
            m_state = WallToolState::Idle;
            m_isFlipped = false;
        }

        // ��������, ����� �� �������� ������
        bool ShouldDrawPreview() const
        {
            return m_state == WallToolState::Drawing || m_state == WallToolState::ChainDrawing;
        }

        // ��������� ������� ����� �������
        WorldPoint GetCurrentPoint() const { return m_currentPoint; }

        // �������� ����������� ����� � ������ flip
        void GetEffectivePoints(WorldPoint& outStart, WorldPoint& outEnd) const
        {
            if (m_isFlipped)
            {
                outStart = m_currentPoint;
                outEnd = m_startPoint;
            }
            else
            {
                outStart = m_startPoint;
                outEnd = m_currentPoint;
            }
        }

        // Callback ��� �������� �����
        void SetOnWallCreated(std::function<void(Wall*)> callback)
        {
            m_onWallCreated = callback;
        }

    private:
        WallToolState m_state{ WallToolState::Idle };
        WorldPoint m_startPoint{ 0, 0 };
        WorldPoint m_currentPoint{ 0, 0 };
        double m_thickness{ 200.0 };
        WorkStateNative m_workState{ WorkStateNative::Existing };

        // R-WALL: ����� WallAttachmentMode ������ ������� LocationLineMode
        WallAttachmentMode m_attachmentMode{ WallAttachmentMode::Core };

        bool m_isFlipped{ false };
        std::function<void(Wall*)> m_onWallCreated;
    };

    // ���������� ���������
    class SelectTool
        {
        public:
            SelectTool() = default;

            // ��������� �����
            Element* OnClick(
                const WorldPoint& worldPoint,
                DocumentModel& document,
                const LayerManager& layerManager)
            {
                // ������ ��� ��������� (� ��)
                double tolerance = 20.0;

                // ���� ������� ��� ��������
                Element* hitElement = document.HitTest(worldPoint, tolerance, layerManager);

                if (hitElement)
                {
                    document.SetSelectedElement(hitElement);
                }
                else
                {
                    document.ClearSelection();
                }

                return hitElement;
            }
        };

        // ��������� ����������� ��������
        enum class DimensionToolState
        {
            Idle,       // �������� ������� ����� (������ �����)
            Drawing     // �������� ������� ����� (������ �����)
        };

        // ���������� ������� ���������� ��������
        class DimensionTool
        {
        public:
            DimensionTool() = default;

            DimensionToolState GetState() const { return m_state; }

            WorldPoint GetStartPoint() const { return m_startPoint; }
            WorldPoint GetCurrentPoint() const { return m_currentPoint; }

            double GetOffset() const { return m_offset; }
            void SetOffset(double offset) { m_offset = offset; }

            bool ShouldDrawPreview() const { return m_state == DimensionToolState::Drawing; }

            // ��������� �����
            // ���������� true, ���� ������ ��� ������
            bool OnClick(
                const WorldPoint& worldPoint,
                DocumentModel& document,
                SnapManager& snapManager,
                const LayerManager& layerManager,
                const Camera& camera)
            {
                // ��������� ��������
                SnapResult snap = snapManager.FindSnap(worldPoint, document, layerManager, camera);
                WorldPoint finalPoint = snap.hasSnap ? snap.point : worldPoint;

                switch (m_state)
                {
                case DimensionToolState::Idle:
                    // �������� � ���������� ������ �����
                    m_startPoint = finalPoint;
                    m_currentPoint = finalPoint;
                    m_state = DimensionToolState::Drawing;
                    return false;

                case DimensionToolState::Drawing:
                    // ��������� ������
                    if (m_startPoint.Distance(finalPoint) > 10.0) // ����������� ����� 10��
                    {
                        Dimension* newDim = document.AddManualDimension(m_startPoint, finalPoint, m_offset);
                    
                        if (m_onDimensionCreated)
                            m_onDimensionCreated(newDim);

                        // ���������� � Idle
                        m_state = DimensionToolState::Idle;
                        m_startPoint = WorldPoint(0, 0);
                        m_currentPoint = WorldPoint(0, 0);

                        return true;
                    }
                    return false;
                }

                return false;
            }

            // ��������� �������� ����
            void OnMouseMove(const WorldPoint& worldPoint)
            {
                m_currentPoint = worldPoint;
            }

            // ������
            void Cancel()
            {
                m_state = DimensionToolState::Idle;
                m_startPoint = WorldPoint(0, 0);
                m_currentPoint = WorldPoint(0, 0);
            }

            // Callback ��� �������� �������
            void SetOnDimensionCreated(std::function<void(Dimension*)> callback)
            {
                m_onDimensionCreated = callback;
            }

        private:
            DimensionToolState m_state{ DimensionToolState::Idle };
            WorldPoint m_startPoint{ 0, 0 };
            WorldPoint m_currentPoint{ 0, 0 };
            double m_offset{ 200.0 };
            std::function<void(Dimension*)> m_onDimensionCreated;
        };
    }
