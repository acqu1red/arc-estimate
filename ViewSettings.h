#pragma once

#include "pch.h"

namespace winrt::estimate1
{
    // Detail level for wall/element representation (Revit-like)
    enum class DetailLevel
    {
        Coarse,     // Simple boundary lines, no internal structure
        Medium,     // Show core boundaries
        Fine        // Show all compound layers
    };

    // View Range settings stub (for future cut plane implementation)
    struct ViewRange
    {
        double TopClipPlane{ 3000.0 };      // mm above floor
        double CutPlane{ 1200.0 };          // mm - typical door/window cut height
        double BottomClipPlane{ 0.0 };      // mm - floor level
        double ViewDepth{ -300.0 };         // mm below floor (for foundations visibility)
    };

    // View settings controlling how elements are displayed
    // Separates VIEW SCALE (affects printed lineweights) from CAMERA ZOOM (navigation only)
    class ViewSettings
    {
    public:
        ViewSettings() = default;

        // View Scale denominator (e.g., 50 for 1:50, 100 for 1:100)
        // This affects printed line thickness, NOT camera zoom
        int GetViewScaleDenominator() const { return m_viewScaleDenominator; }
        void SetViewScaleDenominator(int denom) 
        { 
            m_viewScaleDenominator = (denom > 0) ? denom : 50; 
        }

        // Get the scale as a fraction (e.g., 0.02 for 1:50)
        double GetViewScaleFraction() const 
        { 
            return 1.0 / static_cast<double>(m_viewScaleDenominator); 
        }

        // Detail level (Coarse/Medium/Fine)
        DetailLevel GetDetailLevel() const { return m_detailLevel; }
        void SetDetailLevel(DetailLevel level) { m_detailLevel = level; }

        // Thin Lines toggle (Revit-like: when ON, all lines are hairline width)
        bool IsThinLinesEnabled() const { return m_thinLinesEnabled; }
        void SetThinLinesEnabled(bool enabled) { m_thinLinesEnabled = enabled; }

        // DPI for screen rendering (used for lineweight computation)
        // Default 96 DPI (standard Windows)
        float GetDpi() const { return m_dpi; }
        void SetDpi(float dpi) { m_dpi = (dpi > 0) ? dpi : 96.0f; }

        // View Range (stub for future implementation)
        const ViewRange& GetViewRange() const { return m_viewRange; }
        ViewRange& GetViewRange() { return m_viewRange; }

        // Preset view scales
        static constexpr int Scale_1_20  = 20;
        static constexpr int Scale_1_50  = 50;
        static constexpr int Scale_1_100 = 100;
        static constexpr int Scale_1_200 = 200;
        static constexpr int Scale_1_500 = 500;

        // Common detail level presets
        void SetForPrinting()
        {
            m_thinLinesEnabled = false;
        }

        void SetForDrafting()
        {
            m_thinLinesEnabled = true;
        }

    private:
        int m_viewScaleDenominator{ 50 };           // Default 1:50
        DetailLevel m_detailLevel{ DetailLevel::Medium };
        bool m_thinLinesEnabled{ false };
        float m_dpi{ 96.0f };
        ViewRange m_viewRange{};
    };
}
