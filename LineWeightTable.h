#pragma once

#include "pch.h"
#include "ViewSettings.h"
#include <array>
#include <cmath>

namespace winrt::estimate1
{
    // Revit-like lineweight system
    // 16 line weights (indices 1-16) mapped to printed thickness in mm
    // The thickness is then converted to screen DIPs based on view scale and DPI

    // Category types for line styles
    enum class LineCategory
    {
        WallCut,            // Wall outline at cut plane
        WallProjection,     // Wall projection below cut plane
        WallLayerBoundary,  // Internal layer lines (Fine detail)
        DimensionLine,      // Dimension lines and ticks
        AnnotationLine,     // Leaders, text outlines
        GridLine,           // Reference grids
        RoomBoundary,       // Room separation lines
        SelectionHighlight  // Selected element emphasis
    };

    // Graphic style for an element/category
    struct GraphicStyle
    {
        int LineWeightIndex{ 3 };       // 1-16, index into weight table
        bool IsDashed{ false };         // Dashed vs solid
        Windows::UI::Color Color{ Windows::UI::Colors::Black() };
    };

    class LineWeightTable
    {
    public:
        LineWeightTable()
        {
            InitializeDefaultWeights();
            InitializeDefaultStyles();
        }

        // Get printed thickness in mm for a weight index (1-16)
        double GetPrintedThicknessMm(int weightIndex) const
        {
            int idx = std::clamp(weightIndex, 1, 16) - 1;
            return m_printedThicknesses[idx];
        }

        // Set printed thickness for a weight index
        void SetPrintedThicknessMm(int weightIndex, double thicknessMm)
        {
            int idx = std::clamp(weightIndex, 1, 16) - 1;
            m_printedThicknesses[idx] = (std::max)(0.05, thicknessMm);
        }

        // Convert printed mm thickness to screen DIPs
        // Formula: screenDips = (mm / 25.4) * dpi
        // This is INDEPENDENT of camera zoom - stroke stays constant on screen
        static float PrintedMmToScreenDips(double printedMm, float dpi)
        {
            return static_cast<float>((printedMm / 25.4) * dpi);
        }

        // Get screen stroke width for a weight index, considering view scale
        // viewScaleDenominator: e.g., 50 for 1:50
        // At 1:50, lines appear thicker on paper than at 1:100
        float GetScreenStrokeWidth(
            int weightIndex,
            const ViewSettings& viewSettings) const
        {
            // If thin lines enabled, return hairline width
            if (viewSettings.IsThinLinesEnabled())
            {
                return m_thinLineWidth;
            }

            // Get base printed thickness for this weight
            double printedMm = GetPrintedThicknessMm(weightIndex);

            // View scale adjustment:
            // At larger scales (1:50), lines should appear thicker
            // At smaller scales (1:100), lines should appear thinner
            // Reference scale is 1:100, so factor = 100 / viewScaleDenominator
            double scaleFactor = 100.0 / static_cast<double>(viewSettings.GetViewScaleDenominator());
            double adjustedMm = printedMm * scaleFactor;

            // Convert to screen DIPs
            float screenDips = PrintedMmToScreenDips(adjustedMm, viewSettings.GetDpi());

            // Clamp to reasonable range
            return (std::clamp)(screenDips, m_minStrokeWidth, m_maxStrokeWidth);
        }

        // Get graphic style for a category
        const GraphicStyle& GetStyle(LineCategory category) const
        {
            return m_categoryStyles[static_cast<size_t>(category)];
        }

        GraphicStyle& GetStyleMutable(LineCategory category)
        {
            return m_categoryStyles[static_cast<size_t>(category)];
        }

        // Convenience: get stroke width for a category
        float GetCategoryStrokeWidth(LineCategory category, const ViewSettings& viewSettings) const
        {
            const auto& style = GetStyle(category);
            return GetScreenStrokeWidth(style.LineWeightIndex, viewSettings);
        }

        // Thin line width (used when ThinLines toggle is ON)
        float GetThinLineWidth() const { return m_thinLineWidth; }
        void SetThinLineWidth(float width) { m_thinLineWidth = (std::max)(0.5f, width); }

        // Selection highlight stroke addition
        float GetSelectionStrokeAddition() const { return m_selectionStrokeAddition; }

    private:
        void InitializeDefaultWeights()
        {
            // Revit-like default printed thicknesses (mm)
            // Weight 1 is thinnest, Weight 16 is thickest
            m_printedThicknesses = {
                0.05,   // 1 - Hairline
                0.10,   // 2
                0.13,   // 3 - Default thin
                0.18,   // 4
                0.25,   // 5 - Default medium
                0.35,   // 6
                0.50,   // 7
                0.70,   // 8 - Default cut lines
                1.00,   // 9
                1.40,   // 10
                2.00,   // 11
                2.50,   // 12
                3.00,   // 13
                3.50,   // 14
                4.00,   // 15
                5.00    // 16 - Heaviest
            };
        }

        void InitializeDefaultStyles()
        {
            // Wall cut lines - heavier
            m_categoryStyles[static_cast<size_t>(LineCategory::WallCut)] = {
                8,      // Weight 8 (0.70mm printed)
                false,
                Windows::UI::Colors::Black()
            };

            // Wall projection - lighter
            m_categoryStyles[static_cast<size_t>(LineCategory::WallProjection)] = {
                4,      // Weight 4 (0.18mm printed)
                false,
                Windows::UI::ColorHelper::FromArgb(255, 80, 80, 80)
            };

            // Wall layer boundary - thin
            m_categoryStyles[static_cast<size_t>(LineCategory::WallLayerBoundary)] = {
                2,      // Weight 2 (0.10mm printed)
                false,
                Windows::UI::ColorHelper::FromArgb(255, 120, 120, 120)
            };

            // Dimension lines
            m_categoryStyles[static_cast<size_t>(LineCategory::DimensionLine)] = {
                3,      // Weight 3
                false,
                Windows::UI::Colors::Black()
            };

            // Annotation lines
            m_categoryStyles[static_cast<size_t>(LineCategory::AnnotationLine)] = {
                3,
                false,
                Windows::UI::Colors::Black()
            };

            // Grid lines
            m_categoryStyles[static_cast<size_t>(LineCategory::GridLine)] = {
                2,
                false,
                Windows::UI::ColorHelper::FromArgb(255, 100, 100, 100)
            };

            // Room boundary
            m_categoryStyles[static_cast<size_t>(LineCategory::RoomBoundary)] = {
                3,
                false,
                Windows::UI::ColorHelper::FromArgb(255, 0, 128, 0)
            };

            // Selection highlight
            m_categoryStyles[static_cast<size_t>(LineCategory::SelectionHighlight)] = {
                10,
                false,
                Windows::UI::ColorHelper::FromArgb(255, 90, 180, 255)
            };
        }

        // Printed thicknesses in mm for weights 1-16
        std::array<double, 16> m_printedThicknesses;

        // Styles for categories
        std::array<GraphicStyle, 8> m_categoryStyles;

        // Thin line width in DIPs
        float m_thinLineWidth{ 1.0f };

        // Additional stroke width for selection
        float m_selectionStrokeAddition{ 2.0f };

        // Min/max stroke width constraints
        static constexpr float m_minStrokeWidth = 0.5f;
        static constexpr float m_maxStrokeWidth = 15.0f;
    };
}
