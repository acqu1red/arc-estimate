#pragma once

#include "pch.h"
#include <string>
#include <cstdint>
#include <cmath>

namespace winrt::estimate1
{
    // Базовая единица описания материала для сметы.
    // На этапе M3.1 используется в WallLayer для хранения стоимости, имени и кода.
    struct Material
    {
        uint64_t Id{ 0 };
        std::wstring Name{ L"Материал" };
        std::wstring Code{ L"" };

        // Стоимость за 1 м^2 (в валюте проекта)
        double CostPerSquareMeter{ 0.0 };

        // Необязательно: цвет для визуализации/штриховки в будущем.
        Windows::UI::Color DisplayColor{ Windows::UI::ColorHelper::FromArgb(255, 120, 120, 120) };

        Material() = default;

        Material(uint64_t id, std::wstring name)
            : Id(id), Name(std::move(name))
        {
        }
    };
}
