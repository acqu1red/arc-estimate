#pragma once

#include "pch.h"
#include "Structure.h"
#include "StructureRenderer.h"
#include "DrawingTools.h"

namespace winrt::estimate1
{
    // ============================================================================
    // Инструмент размещения колонн
    // ============================================================================
    class ColumnTool
    {
    public:
        ColumnTool() = default;

        void OnMouseMove(const WorldPoint& worldPoint)
        {
            m_currentPos = worldPoint;
        }

        bool TryPlace()
        {
            if (m_previewColumn)
            {
                // Create actual column copy
                auto newCol = std::make_shared<Column>(*m_previewColumn);
                // Assign new ID
                // Wait, copy constructor copies ID? No, Element copy constructor usually not defined or copies fields.
                // Element has unique ID. Best to recreate or fix ID.
                // Since Element is base, let's create new and copy properties.
                
                auto finalCol = std::make_shared<Column>();
                if (m_previewColumn->GetShape() == ColumnShape::Rectangular)
                {
                    finalCol->SetExample(m_previewColumn->GetWidth(), m_previewColumn->GetDepth());
                }
                else
                {
                    finalCol->SetCircular(m_previewColumn->GetWidth());
                }
                finalCol->SetPosition(m_previewColumn->GetPosition());
                finalCol->SetRotation(m_previewColumn->GetRotation());
                finalCol->SetHeight(m_previewColumn->GetHeight());
                finalCol->SetBaseOffset(m_previewColumn->GetBaseOffset());
                finalCol->SetWorkState(m_workState); // Assume existing for now or parameter
                
                m_lastCreated = finalCol;
                return true;
            }
            return false;
        }

        void UpdatePreview(const WorldPoint& worldPoint)
        {
            if (!m_previewColumn)
            {
                m_previewColumn = std::make_unique<Column>();
                m_previewColumn->SetExample(400.0, 400.0); // Default
            }
            m_previewColumn->SetPosition(worldPoint);
            m_currentPos = worldPoint;
        }
        
        void Rotate()
        {
            if (m_previewColumn)
            {
                double r = m_previewColumn->GetRotation();
                m_previewColumn->SetRotation(r + 90.0);
            }
        }

        std::shared_ptr<Column> GetLastCreated() const { return m_lastCreated; }
        
        // Configuration
        void SetShape(ColumnShape shape, double w, double d)
        {
            if (!m_previewColumn) m_previewColumn = std::make_unique<Column>();
            
            if (shape == ColumnShape::Rectangular)
                m_previewColumn->SetExample(w, d);
            else
                m_previewColumn->SetCircular(w);
        }

        std::unique_ptr<Column> m_previewColumn;
        WorldPoint m_currentPos{ 0, 0 };
        std::shared_ptr<Column> m_lastCreated;
        WorkStateNative m_workState{ WorkStateNative::New };
    };

    // ============================================================================
    // Инструмент создания перекрытий (полигон)
    // ============================================================================
    class SlabTool
    {
    public:
        SlabTool() = default;

        bool OnClick(const WorldPoint& point)
        {
            // Check if closing loop (near start point)
            if (m_points.size() >= 3)
            {
                // Simple distance check
                double dx = point.X - m_points[0].X;
                double dy = point.Y - m_points[0].Y;
                double dist = std::sqrt(dx * dx + dy * dy);
                
                if (dist < 200.0) // 200mm snap to close
                {
                    return TryFinish();
                }
            }
            
            m_points.push_back(point);
            return false;
        }

        bool TryFinish()
        {
            if (m_points.size() < 3) return false;

            auto slab = std::make_shared<Slab>();
            slab->SetContour(m_points);
            slab->SetThickness(m_thickness);
            slab->SetLevelOffset(m_offset);
            
            m_lastCreated = slab;
            Reset();
            return true;
        }

        void Reset()
        {
            m_points.clear();
            m_lastCreated = nullptr;
        }

        void OnMouseMove(const WorldPoint& point)
        {
            m_currentPos = point;
        }

        // Preview rendering helper
        std::vector<WorldPoint> GetPreviewPoints() const
        {
            auto pts = m_points;
            if (!pts.empty())
            {
                pts.push_back(m_currentPos);
            }
            return pts;
        }
        
        bool IsActive() const { return !m_points.empty(); }

        std::shared_ptr<Slab> GetLastCreated() const { return m_lastCreated; }
        
        void SetThickness(double t) { m_thickness = t; }
        void SetOffset(double o) { m_offset = o; }

    private:
        std::vector<WorldPoint> m_points;
        WorldPoint m_currentPos{ 0, 0 };
        std::shared_ptr<Slab> m_lastCreated;
        double m_thickness{ 200.0 };
        double m_offset{ 0.0 };
    };

    // ============================================================================
    // Инструмент создания балок (линия)
    // ============================================================================
    class BeamTool
    {
    public:
        BeamTool() = default;

        bool OnClick(const WorldPoint& point)
        {
            if (!m_isDrawing)
            {
                m_startPoint = point;
                m_isDrawing = true;
                return false;
            }
            else
            {
                // Finish
                auto beam = std::make_shared<Beam>();
                beam->SetStartPoint(m_startPoint);
                beam->SetEndPoint(point);
                beam->SetWidth(m_width);
                beam->SetHeight(m_height);
                beam->SetLevelOffset(m_offset);
                
                m_lastCreated = beam;
                m_isDrawing = false;
                return true;
            }
        }

        void OnMouseMove(const WorldPoint& point)
        {
            m_currentPos = point;
        }

        bool IsDrawing() const { return m_isDrawing; }
        WorldPoint GetStartPoint() const { return m_startPoint; }
        WorldPoint GetCurrentPoint() const { return m_currentPos; }

        std::shared_ptr<Beam> GetLastCreated() const { return m_lastCreated; }
        
        void SetWidth(double w) { m_width = w; }
        void SetHeight(double h) { m_height = h; }
        
        double GetWidth() const { return m_width; }

    private:
        bool m_isDrawing{ false };
        WorldPoint m_startPoint{ 0, 0 };
        WorldPoint m_currentPos{ 0, 0 };
        std::shared_ptr<Beam> m_lastCreated;
        double m_width{ 200.0 };
        double m_height{ 400.0 };
        double m_offset{ 3000.0 };
    };
}
