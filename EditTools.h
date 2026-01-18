#pragma once

#include "pch.h"
#include "Element.h"
#include "Camera.h"
#include "WallJoinManager.h"

namespace winrt::estimate1
{
    // =====================================================
    // ИНСТРУМЕНТ TRIM/EXTEND (ОБРЕЗАТЬ/УДЛИНИТЬ)
    // =====================================================
    enum class TrimExtendState
    {
        PickBoundary,
        PickSubject
    };

    class TrimExtendTool
    {
    public:
        TrimExtendTool() = default;

        TrimExtendState GetState() const { return m_state; }

        void Reset()
        {
            m_state = TrimExtendState::PickBoundary;
            m_boundaryWallId = 0;
            m_subjectWallId = 0;
        }

        bool HandleClick(
            const WorldPoint& worldPos,
            DocumentModel& document,
            const LayerManager& layerManager)
        {
            double tolerance = 200.0; // Tolerance for picking

            if (m_state == TrimExtendState::PickBoundary)
            {
                // Ищем стену-границу
                // Используем HitTest документа, но нам нужен ID и тип
                Element* hit = document.HitTest(worldPos, tolerance, layerManager);
                Wall* wall = dynamic_cast<Wall*>(hit);
                
                if (wall)
                {
                    m_boundaryWallId = wall->GetId();
                    m_state = TrimExtendState::PickSubject;
                    // TODO: Highlight boundary (requires visual state in Document or Renderer)
                }
                return false;
            }
            else if (m_state == TrimExtendState::PickSubject)
            {
                Element* hit = document.HitTest(worldPos, tolerance, layerManager);
                Wall* subjectWall = dynamic_cast<Wall*>(hit);
                
                if (subjectWall && subjectWall->GetId() != m_boundaryWallId)
                {
                    ApplyTrimExtend(document, subjectWall, worldPos);
                    // Reset to pick another subject (Revit behavior)
                    // Or Reset completely? Revit keeps boundary active.
                    return true;
                }
            }
            return false;
        }

        void OnMouseMove(const WorldPoint& worldPos)
        {
            m_cursorPos = worldPos;
        }

        uint64_t GetBoundaryID() const { return m_boundaryWallId; }

    private:
        TrimExtendState m_state{ TrimExtendState::PickBoundary };
        uint64_t m_boundaryWallId{ 0 };
        uint64_t m_subjectWallId{ 0 };
        WorldPoint m_cursorPos{ 0, 0 };

        void ApplyTrimExtend(DocumentModel& document, Wall* subject, const WorldPoint& pickPt)
        {
            Wall* boundary = document.GetWall(m_boundaryWallId);
            if (!boundary || !subject) return;

            // 1. Найти пересечение прямых
            WorldPoint b1 = boundary->GetStartPoint();
            WorldPoint b2 = boundary->GetEndPoint();
            WorldPoint s1 = subject->GetStartPoint();
            WorldPoint s2 = subject->GetEndPoint();

            double det = (b2.X - b1.X) * (s2.Y - s1.Y) - (s2.X - s1.X) * (b2.Y - b1.Y);
            if (std::abs(det) < 0.001) return; // Параллельны

            // Используем формулу пересечения
            // (Px, Py) = P1 + t(P2 - P1)
            
            // Line1: B1 + t(B2-B1)
            // Line2: S1 + u(S2-S1)
            
            double t = ((s1.X - b1.X) * (s1.Y - s2.Y) - (s1.Y - b1.Y) * (s1.X - s2.X)) / det; // Parameter for Boundary
            double u = ((b1.X - s1.X) * (b1.Y - b2.Y) - (b1.Y - s1.Y) * (b1.X - b2.X)) / det; // Parameter for Subject (Wait, sign flip?)
            
            // Standard intersection formula
            // x = x1 + t(x2-x1)
            // x = x3 + u(x4-x3)
            
            // intersection point
            double intersectX = b1.X + (b2.X - b1.X) * t; // wait, correct parameter?
            
            // Check formula logic.
            // Using logic from WallJoinSystem or generic math
            double dxB = b2.X - b1.X;
            double dyB = b2.Y - b1.Y;
            double dxS = s2.X - s1.X;
            double dyS = s2.Y - s1.Y;
            
            double d = dxB * dyS - dyB * dxS;
            if (std::abs(d) < 0.001) return; 
            
            double t1 = ((s1.X - b1.X) * dyS - (s1.Y - b1.Y) * dxS) / d;
            
            WorldPoint intersection(b1.X + dxB * t1, b1.Y + dyB * t1);

            // 2. Определить какую часть оставить
            // Проецируем точку клика на subject
            double distStart = pickPt.Distance(s1);
            double distEnd = pickPt.Distance(s2);
            
            // Логика Revit: кликаем на ту часть, которую ОСТАВЛЯЕМ.
            // Значит дальнюю точку двигаем к пересечению.
            
            if (distStart < distEnd) 
            {
                // Клик ближе к Start => Keep Start => Move End to Intersection
                document.TrimExtendWall(subject->GetId(), s1, intersection);
            }
            else
            {
                // Клик ближе к End => Keep End => Move Start to Intersection
                document.TrimExtendWall(subject->GetId(), intersection, s2);
            }
        }
    };

    // =====================================================
    // ИНСТРУМЕНТ SPLIT (РАЗДЕЛИТЬ)
    // =====================================================
    class SplitTool
    {
    public:
        SplitTool() = default;

        bool HandleClick(
            const WorldPoint& worldPos,
            DocumentModel& document,
            const LayerManager& layerManager)
        {
            double tolerance = 200.0;
            Element* hit = document.HitTest(worldPos, tolerance, layerManager);
            Wall* wall = dynamic_cast<Wall*>(hit);

            if (wall)
            {
                // Проецируем точку на стену
                WorldPoint s = wall->GetStartPoint();
                WorldPoint e = wall->GetEndPoint();
                
                // Проекция worldPos на отрезок s-e
                double dx = e.X - s.X;
                double dy = e.Y - s.Y;
                double len2 = dx * dx + dy * dy;
                
                if (len2 > 0.001)
                {
                    double t = ((worldPos.X - s.X) * dx + (worldPos.Y - s.Y) * dy) / len2;
                    if (t > 0.05 && t < 0.95) // Don't split too close to ends
                    {
                        WorldPoint splitPt(s.X + t * dx, s.Y + t * dy);
                        return document.SplitWall(wall->GetId(), splitPt);
                    }
                }
            }
            return false;
        }

        void OnMouseMove(const WorldPoint& worldPos)
        {
            m_cursorPos = worldPos;
        }

    private:
        WorldPoint m_cursorPos{ 0, 0 };
    };
}
