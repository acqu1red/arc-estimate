#pragma once

#include "pch.h"
#include "Models.h"
#include "Element.h"
#include "Dimension.h"

namespace winrt::estimate1
{
    class AutoDimensionManager
    {
    public:
        AutoDimensionManager() = default;

        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }

        // Полный пересчёт авторазмеров (M3.5: пока только длины стен)
        void Rebuild(DocumentModel& document)
        {
            if (!m_enabled)
                return;

            document.RebuildAutoDimensions();
        }

    private:
        bool m_enabled{ true };
    };
}
