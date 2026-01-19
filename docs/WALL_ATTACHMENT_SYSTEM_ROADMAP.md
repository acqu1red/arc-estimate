# ROADMAP: Система привязки и соединения стен (Wall Attachment System)

## ✅ АКТУАЛЬНЫЙ ROADMAP (v3) — 2026‑01‑20

Ниже — актуальный пошаговый план с критериями завершения. Статусы отмечены по текущим изменениям.

### ЭТАП 0 — Базовая архитектура и типы данных
- [x] Добавить `WallAttachmentMode` (Core / FinishExterior / FinishInterior) в модель данных.
- [x] Обеспечить публичную конвертацию `WallAttachmentMode → LocationLineMode` для совместимости.
**Критерии завершения:** тип доступен в коде, компилируется, используется в новых API.

### ЭТАП 1 — Интеграция новой системы рендеринга стыков
- [x] Подключить `WallAttachmentSystem` в `WallRenderer`.
- [x] Удалить использование `WallJoinSystem` в рендеринге.
- [x] Рендерить контур стен на основе `AttachmentJoinInfo`.
**Критерии завершения:** стены отрисовываются через новую систему, старый join‑рендеринг не используется.

### ЭТАП 2 — Инструмент стены и логика привязки
- [x] Перевести `WallTool` на режим `WallAttachmentMode`.
- [x] Устанавливать `LocationLineMode` новых стен через конвертацию из `WallAttachmentMode`.
**Критерии завершения:** новые стены создаются в выбранной привязке.

### ЭТАП 3 — UI панель привязки (ТОЛЬКО при активном инструменте стены)
- [x] Добавить верхнюю панель привязки над холстом.
- [x] Автоматически показывать панель при выборе инструмента стены.
- [x] Радиокнопки: «Осевая линия», «Чистовая наружная», «Чистовая внутренняя».
- [x] Индикатор‑линия с цветом режима.
**Критерии завершения:** панель появляется сразу при выборе инструмента стены, до создания стены.

### ЭТАП 4 — Превью стыков и визуализация
- [x] Превью соединения вычисляется через `WallAttachmentSystem`.
- [x] Превью отображается без старого `WallJoinRenderer`.
**Критерии завершения:** при рисовании показывается индикатор стыка в новой системе.

### ЭТАП 5 — UI редактирования выбранной стены
- [x] Упростить выпадающий список привязки до 3 режимов.
- [x] Корректно маппить старые режимы на 3 базовых.
**Критерии завершения:** свойства выбранной стены не содержат устаревшие режимы.

### ЭТАП 6 — Очистка наследия
- [x] Полностью удалить использование `WallJoinSystem` и `WallJoinRenderer` из проекта.
- [x] Удалить или архивировать старые классы и UI‑элементы, не используемые в новой логике.
**Критерии завершения:** старый join‑код не используется и не влияет на сборку.

### ЭТАП 7 — Расширенные стыки и проверка геометрии
- [x] T‑стыки, X‑стыки и коллинеарные соединения с учётом привязки (реализовано, требуется визуальная проверка).
- [x] Ограничения длины скосов и fallback‑алгоритмы (реализовано, требуется визуальная проверка).
**Критерии завершения:** корректные стыки на всех типах пересечений.

### ЭТАП 8 — Тестирование и визуальные сценарии
- [x] Набор unit‑тестов для смещений, пересечений, углов.
- [ ] Визуальные сценарии: 90°, острые, тупые, цепочки стен.
**Критерии завершения:** прохождение тестов и подтверждённая визуальная корректность.

---

## Документ: Детальный план реализации фундаментальной системы привязки стен

**Дата создания:** 2026-01-19
**Проект:** ARC-Estimate
**Версия:** 1.0

---

## 📋 СОДЕРЖАНИЕ

1. [Анализ текущей системы](#1-анализ-текущей-системы)
2. [Проблемы текущей реализации](#2-проблемы-текущей-реализации)
3. [Архитектура новой системы](#3-архитектура-новой-системы)
4. [Алгоритмы вычисления смещений](#4-алгоритмы-вычисления-смещений)
5. [Алгоритмы дорисовки углов/стыков](#5-алгоритмы-дорисовки-угловстыков)
6. [Интеграция с UI](#6-интеграция-с-ui)
7. [План реализации](#7-план-реализации)
8. [Тестирование](#8-тестирование)

---

## 1. АНАЛИЗ ТЕКУЩЕЙ СИСТЕМЫ

### 1.1 Существующие компоненты

**Файлы для изменения:**
- `WallJoinSystem.h` - текущая система соединений (743 строки)
- `Models.h` - класс Wall с LocationLineMode
- `WallRenderer.h` - рендеринг стен
- `DrawingTools.h` - WallTool для рисования
- `MainWindow.xaml.cpp` - обработка событий

### 1.2 Текущий LocationLineMode

```cpp
enum class LocationLineMode
{
    WallCenterline,         // По центру толщины стены (ЕСТЬ)
    CoreCenterline,         // По центру несущей части
    FinishFaceExterior,     // По внешней грани отделки (ЕСТЬ)
    FinishFaceInterior,     // По внутренней грани отделки (ЕСТЬ)
    CoreFaceExterior,       // По внешней грани несущей части
    CoreFaceInterior        // По внутренней грани несущей части
};
```

**Проблема:** Enum существует, но:
- Используется только в `Wall::GetCornerPoints()` для расчёта базовых углов
- **НЕ влияет на алгоритмы соединения в `WallJoinSystem`**
- **НЕ учитывается в `CalculateMiterCorner()` и `CalculateTJoinGeometry()`**
- **НЕ влияет на логику привязки при рисовании в `WallTool`**

### 1.3 Текущий WallJoinSystem

**Типы соединений:**
- `LJoin` - L-угол (два конца стен рядом)
- `TJoin` - T-пересечение (конец стены попадает на середину другой)
- `XJoin` - крест (две стены пересекаются в середине)
- `Collinear` - коллинеарные стены

**Ключевые методы:**
- `FindJoins()` - поиск всех соединений для стены
- `DetectJoinType()` - определение типа соединения
- `CalculateMiterCorner()` - расчёт скосов для L-угла
- `CalculateWallContour()` - построение контура с учётом соединений

**Как работает:**
1. При создании стены вызывается `ProcessNewWall()`
2. `FindJoins()` ищет соседние стены (расстояние < 50мм)
3. Для каждой пары вызывается `DetectJoinType()`
4. `CalculateMiterCorner()` вычисляет точки пересечения **внешних** и **внутренних** рёбер
5. При рендеринге `CalculateWallContour()` использует эти точки для построения контура

---

## 2. ПРОБЛЕМЫ ТЕКУЩЕЙ РЕАЛИЗАЦИИ

### 2.1 Отсутствие связи привязки с соединениями

**Проблема:**
`LocationLineMode` не влияет на логику соединения. Алгоритм `CalculateMiterCorner()` всегда вычисляет пересечения рёбер относительно центральной линии стены:

```cpp
// Текущий код - строка 590-629
WorldPoint perp1(-dir1.Y, dir1.X);
WorldPoint perp2(-dir2.Y, dir2.X);
double half1 = wall1.GetThickness() / 2.0;  // ❌ Всегда от центра!
double half2 = wall2.GetThickness() / 2.0;  // ❌ Всегда от центра!

// Внешняя грань = центр + half
extStart = WorldPoint(start.X + perp.X * half, start.Y + perp.Y * half);
// Внутренняя грань = центр - half
intStart = WorldPoint(start.X - perp.X * half, start.Y - perp.Y * half);
```

**Результат:**
- Независимо от `LocationLineMode`, углы всегда рассчитываются от центра
- Визуально неправильные стыки при использовании `FinishFaceExterior` или `FinishFaceInterior`

### 2.2 Неправильная дорисовка для разных привязок

**Проблема:**
Алгоритм пересечения рёбер работает только для `WallCenterline`:

```cpp
// Пересечение внешних граней (строка 609-612)
if (JoinGeometry::LineIntersection(w1_ext_start, w1_ext_end, w2_ext_start, w2_ext_end, corner1))
{
    join.CornerPoints.push_back(corner1);
}
```

**Что должно быть:**
- **Для FinishFaceExterior:** угол должен быть на пересечении **внешних** граней
- **Для FinishFaceInterior:** угол должен быть на пересечении **внутренних** граней
- **Для WallCenterline:** угол на пересечении центральных осей

### 2.3 Отсутствие UI для выбора привязки

**Проблема:**
Нет визуального интерфейса для переключения режима привязки во время рисования.

**Что нужно:**
- Панель с индикатором текущего режима
- Текстовые метки: "Осевая линия", "Чистовая наружная", "Чистовая внутренняя"
- Горячие клавиши для быстрого переключения

### 2.4 Некорректная логика привязки в WallTool

**Проблема:**
При рисовании следующей стены `m_locationLineMode` не влияет на начальную точку:

```cpp
// Текущий код DrawingTools.h - строка ~250
WorldPoint newStart = previousWallEndPoint;  // ❌ Просто конец предыдущей стены
Wall* newWall = new Wall(newStart, newEnd, m_thickness);
newWall->SetLocationLineMode(m_locationLineMode);  // ⚠️ Применяется ПОСЛЕ создания
```

**Что должно быть:**
- Начальная точка новой стены должна рассчитываться **с учётом привязки** предыдущей стены
- Если предыдущая стена имела `FinishFaceExterior`, новая должна начинаться от **внешней грани**

---

## 3. АРХИТЕКТУРА НОВОЙ СИСТЕМЫ

### 3.1 Упрощённая система привязки (3 режима)

Отказываемся от 6 режимов Revit, оставляем только 3 базовых:

```cpp
// Новый enum - заменяет LocationLineMode
enum class WallAttachmentMode
{
    Core,               // Осевая линия (центр ядра стены)
    FinishExterior,     // Чистовая поверхность: наружная
    FinishInterior      // Чистовая поверхность: внутренняя
};
```

**Обоснование:**
- Простота использования
- Покрытие 95% сценариев
- Соответствие требованию задачи (Фото 1-6)

### 3.2 Новый класс WallAttachmentSystem

**Файл:** `WallAttachmentSystem.h` (новый)

```cpp
#pragma once
#include "pch.h"
#include "Models.h"
#include "Camera.h"
#include <vector>
#include <optional>

namespace winrt::estimate1
{
    // =====================================================
    // Режим привязки стены
    // =====================================================
    enum class WallAttachmentMode
    {
        Core,               // Осевая линия
        FinishExterior,     // Чистовая наружная
        FinishInterior      // Чистовая внутренняя
    };

    // =====================================================
    // Линия привязки стены
    // =====================================================
    struct AttachmentLine
    {
        WorldPoint Start;
        WorldPoint End;
        WallAttachmentMode Mode;
    };

    // =====================================================
    // Информация о соединении с учётом привязки
    // =====================================================
    struct AttachmentJoinInfo
    {
        uint64_t WallId1{ 0 };
        uint64_t WallId2{ 0 };
        bool Wall1AtStart{ true };
        bool Wall2AtStart{ true };

        WallAttachmentMode Mode1;
        WallAttachmentMode Mode2;

        WorldPoint JoinPoint{ 0, 0 };           // Точка пересечения линий привязки
        double Angle{ 0.0 };                    // Угол между стенами

        // Геометрия угла для рендеринга
        std::vector<WorldPoint> CornerPolygon;  // Контур угла (4-6 точек)

        bool IsValid() const { return WallId1 != 0 && WallId2 != 0; }
    };

    // =====================================================
    // Система привязки и соединения стен
    // =====================================================
    class WallAttachmentSystem
    {
    public:
        WallAttachmentSystem() = default;

        // =====================================================
        // 1. РАСЧЁТ СМЕЩЕНИЙ ДЛЯ ПРИВЯЗОК
        // =====================================================

        // Получить смещение от центральной оси стены для заданной привязки
        static double GetAttachmentOffset(WallAttachmentMode mode, double wallThickness)
        {
            switch (mode)
            {
                case WallAttachmentMode::Core:
                    return 0.0;                     // Центр стены
                case WallAttachmentMode::FinishExterior:
                    return wallThickness / 2.0;     // Внешняя грань
                case WallAttachmentMode::FinishInterior:
                    return -wallThickness / 2.0;    // Внутренняя грань
            }
            return 0.0;
        }

        // Получить линию привязки для стены
        static AttachmentLine GetAttachmentLine(const Wall& wall)
        {
            AttachmentLine line;
            line.Mode = ConvertLocationLineMode(wall.GetLocationLineMode());

            double offset = GetAttachmentOffset(line.Mode, wall.GetThickness());
            WorldPoint perp = wall.GetPerpendicular();

            WorldPoint start = wall.GetStartPoint();
            WorldPoint end = wall.GetEndPoint();

            line.Start.X = start.X + perp.X * offset;
            line.Start.Y = start.Y + perp.Y * offset;
            line.End.X = end.X + perp.X * offset;
            line.End.Y = end.Y + perp.Y * offset;

            return line;
        }

        // =====================================================
        // 2. ПОИСК СОЕДИНЕНИЙ
        // =====================================================

        // Найти все соединения для стены с учётом привязки
        std::vector<AttachmentJoinInfo> FindJoins(
            const Wall& wall,
            const std::vector<std::unique_ptr<Wall>>& allWalls,
            double tolerance = 50.0) const
        {
            std::vector<AttachmentJoinInfo> results;

            AttachmentLine line1 = GetAttachmentLine(wall);

            for (const auto& other : allWalls)
            {
                if (!other || other->GetId() == wall.GetId())
                    continue;

                AttachmentLine line2 = GetAttachmentLine(*other);

                // Проверка соединения в начале wall
                if (wall.IsJoinAllowedAtStart())
                {
                    auto join = DetectJoin(wall, true, line1, *other, line2, tolerance);
                    if (join.IsValid())
                        results.push_back(join);
                }

                // Проверка соединения в конце wall
                if (wall.IsJoinAllowedAtEnd())
                {
                    auto join = DetectJoin(wall, false, line1, *other, line2, tolerance);
                    if (join.IsValid())
                        results.push_back(join);
                }
            }

            return results;
        }

        // =====================================================
        // 3. ПОСТРОЕНИЕ КОНТУРА С УЧЁТОМ СОЕДИНЕНИЙ
        // =====================================================

        // Построить контур стены с учётом соединений
        std::vector<WorldPoint> BuildWallContour(
            const Wall& wall,
            const std::vector<AttachmentJoinInfo>& joins) const
        {
            std::vector<WorldPoint> contour;

            // Базовые 4 угла стены
            WorldPoint p1, p2, p3, p4;
            wall.GetCornerPoints(p1, p2, p3, p4);

            // Найти соединения в начале и конце
            const AttachmentJoinInfo* startJoin = nullptr;
            const AttachmentJoinInfo* endJoin = nullptr;

            for (const auto& join : joins)
            {
                if (join.WallId1 == wall.GetId())
                {
                    if (join.Wall1AtStart)
                        startJoin = &join;
                    else
                        endJoin = &join;
                }
                else if (join.WallId2 == wall.GetId())
                {
                    if (join.Wall2AtStart)
                        startJoin = &join;
                    else
                        endJoin = &join;
                }
            }

            // Построение контура с учётом соединений
            if (startJoin && !startJoin->CornerPolygon.empty())
            {
                // Использовать полигон угла из соединения
                for (const auto& pt : startJoin->CornerPolygon)
                    contour.push_back(pt);
            }
            else
            {
                contour.push_back(p1);
                contour.push_back(p2);
            }

            if (endJoin && !endJoin->CornerPolygon.empty())
            {
                // Использовать полигон угла (в обратном порядке)
                for (auto it = endJoin->CornerPolygon.rbegin(); it != endJoin->CornerPolygon.rend(); ++it)
                    contour.push_back(*it);
            }
            else
            {
                contour.push_back(p3);
                contour.push_back(p4);
            }

            return contour;
        }

        // =====================================================
        // 4. ПРЕДПРОСМОТР ПРИ РИСОВАНИИ
        // =====================================================

        // Найти предпросмотр соединения для новой стены
        std::optional<AttachmentJoinInfo> FindPreviewJoin(
            const WorldPoint& wallStart,
            const WorldPoint& wallEnd,
            double thickness,
            WallAttachmentMode mode,
            const std::vector<std::unique_ptr<Wall>>& allWalls,
            double tolerance = 50.0) const
        {
            // Создать временную стену
            Wall tempWall(wallStart, wallEnd, thickness);
            tempWall.SetLocationLineMode(ConvertToLocationLineMode(mode));

            // Найти соединения
            auto joins = FindJoins(tempWall, allWalls, tolerance);

            // Вернуть соединение в конце (наиболее актуальное при рисовании)
            for (const auto& join : joins)
            {
                if (join.WallId1 == tempWall.GetId() && !join.Wall1AtStart)
                    return join;
            }

            return std::nullopt;
        }

    private:
        // =====================================================
        // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // =====================================================

        // Конвертация LocationLineMode → WallAttachmentMode
        static WallAttachmentMode ConvertLocationLineMode(LocationLineMode mode)
        {
            switch (mode)
            {
                case LocationLineMode::WallCenterline:
                case LocationLineMode::CoreCenterline:
                    return WallAttachmentMode::Core;

                case LocationLineMode::FinishFaceExterior:
                case LocationLineMode::CoreFaceExterior:
                    return WallAttachmentMode::FinishExterior;

                case LocationLineMode::FinishFaceInterior:
                case LocationLineMode::CoreFaceInterior:
                    return WallAttachmentMode::FinishInterior;
            }
            return WallAttachmentMode::Core;
        }

        // Конвертация WallAttachmentMode → LocationLineMode
        static LocationLineMode ConvertToLocationLineMode(WallAttachmentMode mode)
        {
            switch (mode)
            {
                case WallAttachmentMode::Core:
                    return LocationLineMode::WallCenterline;
                case WallAttachmentMode::FinishExterior:
                    return LocationLineMode::FinishFaceExterior;
                case WallAttachmentMode::FinishInterior:
                    return LocationLineMode::FinishFaceInterior;
            }
            return LocationLineMode::WallCenterline;
        }

        // Определить соединение между двумя стенами
        AttachmentJoinInfo DetectJoin(
            const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
            const Wall& wall2, const AttachmentLine& line2,
            double tolerance) const;

        // Вычислить геометрию угла для соединения
        void CalculateJoinGeometry(
            const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
            const Wall& wall2, bool wall2AtStart, const AttachmentLine& line2,
            AttachmentJoinInfo& join) const;
    };
}
```

### 3.3 Интеграция с существующими классами

**MainWindow:**
```cpp
class MainWindow
{
    WallAttachmentSystem m_attachmentSystem;      // Новая система
    WallAttachmentMode m_currentAttachmentMode;   // Текущий режим

    // UI элементы
    AttachmentModePanel m_attachmentModePanel;    // Панель выбора режима
};
```

**WallTool:**
```cpp
class WallTool
{
    WallAttachmentMode m_attachmentMode;  // Режим привязки для новых стен

    // При создании стены учитывать привязку
    void OnClick(WorldPoint clickPoint, ...);
};
```

**WallRenderer:**
```cpp
class WallRenderer
{
    // Использовать WallAttachmentSystem вместо WallJoinSystem
    WallAttachmentSystem* m_attachmentSystem;

    void DrawWall(const Wall& wall, ...);
};
```

---

## 4. АЛГОРИТМЫ ВЫЧИСЛЕНИЯ СМЕЩЕНИЙ

### 4.1 Базовый принцип

Каждая стена имеет:
- **Центральную ось** (start → end)
- **Перпендикуляр** к оси (вектор вправо от направления)
- **Толщину** (thickness)

**Линия привязки** рассчитывается как смещение от центральной оси:

```
AttachmentLine = CenterLine + Perpendicular × Offset
```

### 4.2 Формулы смещений

```cpp
double GetAttachmentOffset(WallAttachmentMode mode, double thickness)
{
    switch (mode)
    {
        case Core:
            return 0.0;                   // Центр
        case FinishExterior:
            return thickness / 2.0;       // +half (внешняя)
        case FinishInterior:
            return -thickness / 2.0;      // -half (внутренняя)
    }
}
```

### 4.3 Визуализация

```
Стена (вид сверху, толщина 200мм):

                    ┌──────────────────────┐ ← FinishExterior (+100mm)
                    │                      │
    Start ●─────────●──────────────────────●──────────● End  ← Core (0mm)
                    │                      │
                    └──────────────────────┘ ← FinishInterior (-100mm)
```

### 4.4 Пример кода

```cpp
// Стена: Start(0, 0), End(1000, 0), Thickness=200mm
// Направление: (1, 0)
// Перпендикуляр: (0, 1)

// Core (центр)
AttachmentLine core;
core.Start = (0, 0);
core.End = (1000, 0);

// FinishExterior (внешняя, +100mm по Y)
AttachmentLine exterior;
exterior.Start = (0, 100);
exterior.End = (1000, 100);

// FinishInterior (внутренняя, -100mm по Y)
AttachmentLine interior;
interior.Start = (0, -100);
interior.End = (1000, -100);
```

---

## 5. АЛГОРИТМЫ ДОРИСОВКИ УГЛОВ/СТЫКОВ

### 5.1 Типы соединений

**1. L-Join (L-угол):**
- Два конца стен на расстоянии < tolerance
- Образуют угол (обычно 90°, но может быть любой)

**2. T-Join (T-пересечение):**
- Конец одной стены попадает на середину другой
- Образуют Т-образное соединение

### 5.2 Алгоритм для L-Join

**Шаг 1:** Найти линии привязки обеих стен

```cpp
AttachmentLine line1 = GetAttachmentLine(wall1);
AttachmentLine line2 = GetAttachmentLine(wall2);
```

**Шаг 2:** Найти точку пересечения линий привязки

```cpp
WorldPoint joinPoint;
bool intersects = LineIntersection(
    line1.Start, line1.End,
    line2.Start, line2.End,
    joinPoint);
```

**Шаг 3:** Построить полигон угла в зависимости от режима

#### 5.2.1 Для режима Core (осевая линия)

```
Угол 90°:

Wall1          Wall2
  │              ──
  │             │
  │             │
  ●─────────────┘
 (joinPoint)

Полигон угла:
  p1 ─────── p2
  │           │
  │           │
  p4 ─────── p3
```

Алгоритм:
```cpp
// Wall1: вертикальная, Wall2: горизонтальная
// Обе имеют режим Core

// 1. Получить внешние и внутренние грани обеих стен
WorldPoint w1_ext_start, w1_ext_end, w1_int_start, w1_int_end;
GetWallEdges(wall1, w1_ext_start, w1_ext_end, w1_int_start, w1_int_end);

WorldPoint w2_ext_start, w2_ext_end, w2_int_start, w2_int_end;
GetWallEdges(wall2, w2_ext_start, w2_ext_end, w2_int_start, w2_int_end);

// 2. Найти пересечения внешних и внутренних граней
WorldPoint cornerOuter, cornerInner;
LineIntersection(w1_ext, w2_ext, cornerOuter);
LineIntersection(w1_int, w2_int, cornerInner);

// 3. Построить полигон
std::vector<WorldPoint> polygon;
polygon.push_back(cornerOuter);
polygon.push_back(/* продолжение внешней грани wall1 */);
polygon.push_back(cornerInner);
polygon.push_back(/* продолжение внутренней грани wall2 */);
```

#### 5.2.2 Для режима FinishExterior (чистовая наружная)

```
Угол 90°:

Wall1 (FinishExterior)    Wall2 (FinishExterior)
  │                         ──────
  │                        │
  ●────────────────────────┘
 (joinPoint = пересечение внешних граней)

Полигон:
- Угол формируется на пересечении ВНЕШНИХ граней
- Внутренние грани образуют "ступеньку"
```

Алгоритм:
```cpp
// Wall1: внешняя грань = линия привязки
// Wall2: внешняя грань = линия привязки

// 1. Точка пересечения линий привязки = внешний угол
WorldPoint outerCorner = joinPoint;

// 2. Найти внутренние грани (offset = -thickness)
AttachmentLine w1_inner = GetOffsetLine(line1, -wall1.Thickness);
AttachmentLine w2_inner = GetOffsetLine(line2, -wall2.Thickness);

// 3. Пересечение внутренних граней
WorldPoint innerCorner;
LineIntersection(w1_inner, w2_inner, innerCorner);

// 4. Построить полигон (6 точек для разнотолщинных стен)
polygon.push_back(outerCorner);
polygon.push_back(/* точка на внешней грани wall1 */);
polygon.push_back(/* точка на стыке граней */);
polygon.push_back(innerCorner);
polygon.push_back(/* точка на внутренней грани wall2 */);
polygon.push_back(/* точка на стыке граней */);
```

#### 5.2.3 Для режима FinishInterior (чистовая внутренняя)

```
Угол 90°:

Wall1 (FinishInterior)    Wall2 (FinishInterior)
  │                         ──────
  ●────────────────────────│
  │                        └─
  └──────────────────────────
(joinPoint = пересечение внутренних граней)

Полигон:
- Угол формируется на пересечении ВНУТРЕННИХ граней
- Внешние грани образуют "выступ"
```

Алгоритм аналогичен `FinishExterior`, но:
- `joinPoint` = пересечение внутренних граней
- Внешние грани вычисляются с offset = +thickness

### 5.3 Алгоритм для T-Join

**T-пересечение:**
- Wall1 (перпендикулярная) упирается в Wall2 (горизонтальная)

#### 5.3.1 Для режима Core

```
Wall1 (вертикальная)
  │
  │
  ●──────────────  Wall2 (горизонтальная)

Полигон:
- Конец Wall1 срезается по линии Wall2
```

Алгоритм:
```cpp
// 1. Найти точку пересечения осей
WorldPoint joinPoint = IntersectionPoint(line1, line2);

// 2. Определить, с какой стороны Wall2 находится Wall1
WorldPoint perp2 = wall2.GetPerpendicular();
double side = Dot(wall1Direction, perp2);

// 3. Найти грань Wall2, на которую опирается Wall1
AttachmentLine wall2Edge = (side > 0)
    ? GetOffsetLine(line2, wall2.Thickness / 2)
    : GetOffsetLine(line2, -wall2.Thickness / 2);

// 4. Построить полигон конца Wall1
polygon.push_back(/* внешний угол Wall1 */);
polygon.push_back(/* пересечение с wall2Edge */);
polygon.push_back(/* внутренний угол Wall1 */);
```

#### 5.3.2 Для режима FinishExterior/Interior

Аналогично, но:
- Используется соответствующая линия привязки
- Полигон строится с учётом смещения

### 5.4 Обработка углов отличных от 90°

**Острые углы (< 90°):**
- Скос может быть очень длинным
- Ограничение длины скоса: `max_miter_length = thickness * 2`

**Тупые углы (> 90°):**
- Скос короче
- Всегда корректен

```cpp
// Ограничение длины скоса
if (cornerOuter.Distance(cornerInner) > wall.Thickness * 2.0)
{
    // Использовать Bevel (срез) вместо Miter (скос)
    polygon = CreateBevelJoin(wall1, wall2);
}
```

### 5.5 Псевдокод полного алгоритма

```cpp
void CalculateJoinGeometry(
    const Wall& wall1, bool wall1AtStart, const AttachmentLine& line1,
    const Wall& wall2, bool wall2AtStart, const AttachmentLine& line2,
    AttachmentJoinInfo& join) const
{
    // 1. Найти точку пересечения линий привязки
    WorldPoint joinPoint;
    if (!LineIntersection(line1.Start, line1.End, line2.Start, line2.End, joinPoint))
        return;  // Параллельные стены

    join.JoinPoint = joinPoint;

    // 2. Определить направления стен
    WorldPoint dir1 = wall1AtStart ? -wall1.GetDirection() : wall1.GetDirection();
    WorldPoint dir2 = wall2AtStart ? -wall2.GetDirection() : wall2.GetDirection();

    join.Angle = AngleBetween(dir1, dir2);

    // 3. В зависимости от режимов привязки построить полигон
    if (line1.Mode == WallAttachmentMode::Core && line2.Mode == WallAttachmentMode::Core)
    {
        // Режим Core: скос по пересечению внешних/внутренних граней
        join.CornerPolygon = BuildCoreMiterJoin(wall1, wall2, joinPoint);
    }
    else if (line1.Mode == WallAttachmentMode::FinishExterior && line2.Mode == WallAttachmentMode::FinishExterior)
    {
        // Режим FinishExterior: угол на внешних гранях
        join.CornerPolygon = BuildExteriorJoin(wall1, wall2, joinPoint);
    }
    else if (line1.Mode == WallAttachmentMode::FinishInterior && line2.Mode == WallAttachmentMode::FinishInterior)
    {
        // Режим FinishInterior: угол на внутренних гранях
        join.CornerPolygon = BuildInteriorJoin(wall1, wall2, joinPoint);
    }
    else
    {
        // Смешанные режимы: комбинированный полигон
        join.CornerPolygon = BuildMixedJoin(wall1, line1.Mode, wall2, line2.Mode, joinPoint);
    }

    // 4. Проверка длины скоса
    if (IsMiterTooLong(join.CornerPolygon, wall1.GetThickness(), wall2.GetThickness()))
    {
        // Использовать Bevel
        join.CornerPolygon = BuildBevelJoin(wall1, wall2, joinPoint);
    }
}
```

---

## 6. ИНТЕГРАЦИЯ С UI

### 6.1 Панель выбора режима привязки

**Расположение:**
Верхняя часть окна, рядом с панелью инструментов

**Компоненты:**
```
┌────────────────────────────────────────────────────┐
│  Привязка: ◉ Осевая линия  ○ Чистовая наружная  ○ Чистовая внутренняя │
└────────────────────────────────────────────────────┘
```

**XAML структура:**
```xml
<StackPanel Orientation="Horizontal" Spacing="12" Padding="8">
    <TextBlock Text="Привязка:" VerticalAlignment="Center"/>

    <RadioButton x:Name="RadioCore"
                 Content="Осевая линия"
                 GroupName="AttachmentMode"
                 IsChecked="True"
                 Click="OnAttachmentModeChanged"/>

    <RadioButton x:Name="RadioExterior"
                 Content="Чистовая наружная"
                 GroupName="AttachmentMode"
                 Click="OnAttachmentModeChanged"/>

    <RadioButton x:Name="RadioInterior"
                 Content="Чистовая внутренняя"
                 GroupName="AttachmentMode"
                 Click="OnAttachmentModeChanged"/>
</StackPanel>
```

### 6.2 Визуальный индикатор при рисовании

**Во время рисования стены:**
- Показывать пунктирную линию привязки
- Цвет линии зависит от режима:
  - Core: синий
  - FinishExterior: зелёный
  - FinishInterior: красный

**Код:**
```cpp
void DrawLocationLineIndicator(
    const CanvasDrawingSession& session,
    const Camera& camera,
    const AttachmentLine& line)
{
    ScreenPoint start = camera.WorldToScreen(line.Start);
    ScreenPoint end = camera.WorldToScreen(line.End);

    Windows::UI::Color color;
    switch (line.Mode)
    {
        case WallAttachmentMode::Core:
            color = ColorHelper::FromArgb(180, 0, 120, 215);  // Синий
            break;
        case WallAttachmentMode::FinishExterior:
            color = ColorHelper::FromArgb(180, 0, 180, 0);    // Зелёный
            break;
        case WallAttachmentMode::FinishInterior:
            color = ColorHelper::FromArgb(180, 200, 0, 0);    // Красный
            break;
    }

    auto style = CanvasStrokeStyle();
    style.DashStyle(CanvasDashStyle::Dash);

    session.DrawLine(
        float2(start.X, start.Y),
        float2(end.X, end.Y),
        color, 1.5f, style);
}
```

### 6.3 Горячие клавиши

**Предлагаемые комбинации:**
- `1` - Core (осевая линия)
- `2` - FinishExterior (чистовая наружная)
- `3` - FinishInterior (чистовая внутренняя)

**Обработка:**
```cpp
void MainWindow::OnKeyDown(KeyRoutedEventArgs const& args)
{
    if (m_wallTool.IsActive())
    {
        switch (args.Key())
        {
            case VirtualKey::Number1:
                SetAttachmentMode(WallAttachmentMode::Core);
                break;
            case VirtualKey::Number2:
                SetAttachmentMode(WallAttachmentMode::FinishExterior);
                break;
            case VirtualKey::Number3:
                SetAttachmentMode(WallAttachmentMode::FinishInterior);
                break;
        }
    }
}
```

---

## 7. ПЛАН РЕАЛИЗАЦИИ

### ЭТАП 1: Подготовка и проектирование

#### 1.1 Изучение текущего кода WallJoinSystem.h
**Критерии завершения:**
- ✅ Прочитан и понят весь код WallJoinSystem.h
- ✅ Определены методы, которые нужно заменить
- ✅ Определены методы, которые можно переиспользовать

**Статус:** ✅ ЗАВЕРШЁН

#### 1.2 Создание детального ROADMAP
**Критерии завершения:**
- ✅ Создан файл `docs/WALL_ATTACHMENT_SYSTEM_ROADMAP.md`
- ✅ Описаны все алгоритмы
- ✅ Определены этапы реализации

**Статус:** ✅ ЗАВЕРШЁН

---

### ЭТАП 2: Базовая инфраструктура

#### 2.1 Создание WallAttachmentSystem.h
**Задачи:**
- Создать новый файл `WallAttachmentSystem.h`
- Определить enum `WallAttachmentMode`
- Определить структуры `AttachmentLine`, `AttachmentJoinInfo`
- Создать базовый класс `WallAttachmentSystem` с пустыми методами

**Критерии завершения:**
- Код компилируется без ошибок
- Определены все публичные методы класса
- Добавлены комментарии для каждого метода

**Время:** ~30 минут

#### 2.2 Реализация расчёта смещений
**Задачи:**
- Реализовать `GetAttachmentOffset()`
- Реализовать `GetAttachmentLine()`
- Реализовать методы конвертации `ConvertLocationLineMode()` и `ConvertToLocationLineMode()`

**Критерии завершения:**
- Методы возвращают корректные значения для всех режимов
- Проверка на простых примерах:
  - Стена (0,0)→(1000,0), толщина 200мм
  - Core: линия (0,0)→(1000,0)
  - FinishExterior: линия (0,100)→(1000,100)
  - FinishInterior: линия (0,-100)→(1000,-100)

**Время:** ~1 час

---

### ЭТАП 3: Алгоритмы соединения

#### 3.1 Реализация DetectJoin()
**Задачи:**
- Реализовать метод `DetectJoin()` для определения соединения между двумя стенами
- Проверка расстояния между концами стен
- Расчёт точки пересечения линий привязки
- Определение угла между стенами

**Критерии завершения:**
- Метод корректно определяет L-join для двух стен под 90°
- Метод корректно определяет T-join
- Метод возвращает `IsValid() == false` для стен без соединения

**Тестовые случаи:**
1. Две стены под 90° с режимом Core
2. Две стены под 90° с режимом FinishExterior
3. Две стены параллельные (не должны соединяться)

**Время:** ~2 часа

#### 3.2 Реализация CalculateJoinGeometry() - режим Core
**Задачи:**
- Реализовать построение полигона угла для режима Core
- Вычисление внешних и внутренних граней обеих стен
- Нахождение пересечений граней
- Построение 4-точечного полигона

**Критерии завершения:**
- Визуально корректный угол для двух стен под 90° (режим Core)
- Визуально корректный угол для двух стен под 45° (режим Core)
- Полигон содержит 4 точки в правильном порядке

**Время:** ~2 часа

#### 3.3 Реализация CalculateJoinGeometry() - режим FinishExterior
**Задачи:**
- Реализовать построение полигона для режима FinishExterior
- Угол формируется на пересечении внешних граней
- Учёт разной толщины стен

**Критерии завершения:**
- Визуально корректный угол (аналогичный Фото 5)
- Полигон содержит 4-6 точек

**Время:** ~2 часа

#### 3.4 Реализация CalculateJoinGeometry() - режим FinishInterior
**Задачи:**
- Реализовать построение полигона для режима FinishInterior
- Угол формируется на пересечении внутренних граней

**Критерии завершения:**
- Визуально корректный угол (аналогичный Фото 6)

**Время:** ~2 часа

#### 3.5 Реализация CalculateJoinGeometry() - смешанные режимы
**Задачи:**
- Реализовать построение полигона когда wall1 и wall2 имеют разные режимы
- Обработка всех 9 комбинаций (3×3)

**Критерии завершения:**
- Визуально корректные углы для всех комбинаций

**Время:** ~3 часа

---

### ЭТАП 4: Интеграция в рендеринг

#### 4.1 Обновление WallRenderer
**Задачи:**
- Добавить поле `WallAttachmentSystem* m_attachmentSystem`
- Изменить метод `DrawWallWithJoins()` для использования новой системы
- Изменить `DrawContour()` для рисования полигонов из `AttachmentJoinInfo`

**Критерии завершения:**
- Стены рисуются с новыми соединениями
- Контур правильно замыкается
- Нет визуальных артефактов

**Время:** ~2 часа

#### 4.2 Обновление DrawPreview()
**Задачи:**
- Добавить рисование индикатора линии привязки
- Цвет индикатора зависит от режима

**Критерии завершения:**
- При рисовании стены видна пунктирная линия привязки
- Цвет линии: синий (Core), зелёный (Exterior), красный (Interior)

**Время:** ~1 час

---

### ЭТАП 5: Интеграция в WallTool

#### 5.1 Добавление поля m_attachmentMode
**Задачи:**
- Добавить поле `WallAttachmentMode m_attachmentMode` в WallTool
- Добавить методы `GetAttachmentMode()` и `SetAttachmentMode()`

**Критерии завершения:**
- Поле корректно инициализируется в конструкторе
- Методы работают

**Время:** ~15 минут

#### 5.2 Обновление OnClick() с учётом привязки
**Задачи:**
- При создании новой стены устанавливать режим привязки
- Рассчитывать начальную точку новой стены с учётом привязки предыдущей

**Критерии завершения:**
- При рисовании цепочки стен новая стена начинается от правильной линии привязки
- Режим привязки сохраняется для новой стены

**Тестовые случаи:**
1. Нарисовать 3 стены подряд с режимом Core
2. Нарисовать 3 стены подряд с режимом FinishExterior
3. Сменить режим в процессе рисования

**Время:** ~2 часа

#### 5.3 Обновление FindPreviewJoin()
**Задачи:**
- Использовать `WallAttachmentSystem::FindPreviewJoin()` вместо старого метода

**Критерии завершения:**
- Предпросмотр соединения работает корректно

**Время:** ~1 час

---

### ЭТАП 6: UI для выбора режима

#### 6.1 Создание панели в MainWindow.xaml
**Задачи:**
- Добавить `StackPanel` с тремя `RadioButton`
- Добавить обработчик `OnAttachmentModeChanged`

**Критерии завершения:**
- Панель отображается в верхней части окна
- Клик по кнопке изменяет режим
- Текущий режим визуально выделен

**Время:** ~1 час

#### 6.2 Интеграция с MainWindow.cpp
**Задачи:**
- Реализовать метод `OnAttachmentModeChanged()`
- При смене режима обновлять `m_wallTool.SetAttachmentMode()`
- Перерисовывать canvas

**Критерии завершения:**
- Смена режима через UI работает
- WallTool использует новый режим

**Время:** ~30 минут

#### 6.3 Добавление горячих клавиш
**Задачи:**
- Обработка клавиш 1, 2, 3 в `OnKeyDown()`
- Синхронизация состояния RadioButton

**Критерии завершения:**
- Клавиши 1/2/3 переключают режим
- RadioButton обновляется при нажатии клавиш

**Время:** ~30 минут

---

### ЭТАП 7: Сериализация

#### 7.1 Добавление сохранения режима привязки
**Задачи:**
- В `ProjectSerializer.h` добавить сохранение `AttachmentMode` для каждой стены
- Конвертация enum в строку для JSON

**Критерии завершения:**
- При сохранении проекта режим привязки сохраняется
- При загрузке проекта режим привязки восстанавливается

**Время:** ~1 час

---

### ЭТАП 8: Тестирование

#### 8.1 Unit-тесты в Tests.h
**Задачи:**
- Добавить тесты для `GetAttachmentOffset()`
- Добавить тесты для `GetAttachmentLine()`
- Добавить тесты для `DetectJoin()`
- Добавить тесты для `CalculateJoinGeometry()`

**Критерии завершения:**
- Минимум 10 unit-тестов
- Все тесты проходят

**Время:** ~2 часа

#### 8.2 Визуальное тестирование
**Тестовые сценарии:**

**Сценарий 1: L-угол 90° (Core)**
1. Установить режим Core
2. Нарисовать стену (0,0)→(1000,0)
3. Нарисовать стену (1000,0)→(1000,1000)
4. Проверить: угол правильный, скос по центру

**Сценарий 2: L-угол 90° (FinishExterior)**
1. Установить режим FinishExterior
2. Нарисовать стену (0,0)→(1000,0)
3. Нарисовать стену (1000,0)→(1000,1000)
4. Проверить: угол на внешних гранях (как Фото 5)

**Сценарий 3: L-угол 90° (FinishInterior)**
1. Установить режим FinishInterior
2. Нарисовать стену (0,0)→(1000,0)
3. Нарисовать стену (1000,0)→(1000,1000)
4. Проверить: угол на внутренних гранях (как Фото 6)

**Сценарий 4: L-угол 45°**
1. Режим Core
2. Нарисовать стену под 45°
3. Проверить: скос корректен, не слишком длинный

**Сценарий 5: T-пересечение**
1. Режим Core
2. Нарисовать горизонтальную стену
3. Нарисовать вертикальную стену, упирающуюся в середину первой
4. Проверить: T-соединение корректно

**Сценарий 6: Цепочка стен**
1. Режим Core
2. Нарисовать прямоугольник из 4 стен
3. Проверить: все углы корректны

**Сценарий 7: Смена режима в процессе**
1. Режим Core, нарисовать стену
2. Сменить на FinishExterior, нарисовать вторую стену
3. Проверить: угол корректен для смешанных режимов

**Сценарий 8: Разная толщина стен**
1. Нарисовать стену толщиной 200мм
2. Нарисовать стену толщиной 300мм
3. Проверить: угол корректен

**Сценарий 9: Сохранение и загрузка**
1. Нарисовать несколько стен с разными режимами
2. Сохранить проект
3. Загрузить проект
4. Проверить: режимы привязки сохранились, углы корректны

**Критерии завершения:**
- Все 9 сценариев проходят успешно
- Нет визуальных артефактов
- Нет крашей или исключений

**Время:** ~3 часа

---

### ЭТАП 9: Удаление старого кода

#### 9.1 Удаление WallJoinSystem
**Задачи:**
- Удалить файл `WallJoinSystem.h`
- Удалить все ссылки на `WallJoinSystem` в проекте
- Заменить на `WallAttachmentSystem`

**Критерии завершения:**
- Проект компилируется без ошибок
- Нет неиспользуемого кода

**Время:** ~1 час

---

### ЭТАП 10: Финализация

#### 10.1 Документация
**Задачи:**
- Обновить `CLAUDE.md` с описанием новой системы
- Добавить примеры кода в документацию

**Критерии завершения:**
- CLAUDE.md содержит актуальную информацию
- Примеры кода корректны

**Время:** ~1 час

#### 10.2 Финальная проверка
**Задачи:**
- Пройти все тестовые сценарии ещё раз
- Проверить производительность (100+ стен)
- Проверить сохранение/загрузку

**Критерии завершения:**
- Всё работает стабильно
- Нет известных багов
- Производительность приемлемая

**Время:** ~2 часа

---

## 8. ТЕСТИРОВАНИЕ

### 8.1 Unit-тесты

**Файл:** `Tests.h`

```cpp
// Тест 1: GetAttachmentOffset
void Test_GetAttachmentOffset()
{
    double thickness = 200.0;

    double offsetCore = WallAttachmentSystem::GetAttachmentOffset(
        WallAttachmentMode::Core, thickness);
    assert(offsetCore == 0.0);

    double offsetExterior = WallAttachmentSystem::GetAttachmentOffset(
        WallAttachmentMode::FinishExterior, thickness);
    assert(offsetExterior == 100.0);

    double offsetInterior = WallAttachmentSystem::GetAttachmentOffset(
        WallAttachmentMode::FinishInterior, thickness);
    assert(offsetInterior == -100.0);
}

// Тест 2: GetAttachmentLine
void Test_GetAttachmentLine()
{
    Wall wall(WorldPoint(0, 0), WorldPoint(1000, 0), 200.0);
    wall.SetLocationLineMode(LocationLineMode::FinishFaceExterior);

    auto line = WallAttachmentSystem::GetAttachmentLine(wall);

    assert(line.Mode == WallAttachmentMode::FinishExterior);
    assert(std::abs(line.Start.X - 0.0) < 0.01);
    assert(std::abs(line.Start.Y - 100.0) < 0.01);
    assert(std::abs(line.End.X - 1000.0) < 0.01);
    assert(std::abs(line.End.Y - 100.0) < 0.01);
}

// Тест 3: DetectJoin для L-угла 90°
void Test_DetectJoin_LJoin90()
{
    // Создать две стены под 90°
    Wall wall1(WorldPoint(0, 0), WorldPoint(1000, 0), 200.0);
    Wall wall2(WorldPoint(1000, 0), WorldPoint(1000, 1000), 200.0);

    WallAttachmentSystem system;
    std::vector<std::unique_ptr<Wall>> walls;
    walls.push_back(std::make_unique<Wall>(wall2));

    auto joins = system.FindJoins(wall1, walls, 50.0);

    assert(joins.size() == 1);
    assert(joins[0].IsValid());
    assert(joins[0].WallId2 == wall2.GetId());
}

// ... Ещё 7-10 тестов
```

### 8.2 Визуальное тестирование

**Чек-лист для каждого сценария:**
- [ ] Углы визуально правильные
- [ ] Нет разрывов между стенами
- [ ] Нет наложения граней
- [ ] Скосы не слишком длинные
- [ ] Индикаторы линий привязки корректны
- [ ] UI панель работает
- [ ] Горячие клавиши работают
- [ ] Сохранение/загрузка работает

---

## 9. ОЦЕНКА ВРЕМЕНИ

| Этап | Задача | Время |
|------|--------|-------|
| 1.1 | Изучение WallJoinSystem.h | ✅ Завершено |
| 1.2 | Создание ROADMAP | ✅ Завершено |
| 2.1 | Создание WallAttachmentSystem.h | 30 мин |
| 2.2 | Реализация расчёта смещений | 1 час |
| 3.1 | Реализация DetectJoin() | 2 часа |
| 3.2 | CalculateJoinGeometry() - Core | 2 часа |
| 3.3 | CalculateJoinGeometry() - FinishExterior | 2 часа |
| 3.4 | CalculateJoinGeometry() - FinishInterior | 2 часа |
| 3.5 | CalculateJoinGeometry() - смешанные | 3 часа |
| 4.1 | Обновление WallRenderer | 2 часа |
| 4.2 | Обновление DrawPreview() | 1 час |
| 5.1 | Добавление m_attachmentMode | 15 мин |
| 5.2 | Обновление OnClick() | 2 часа |
| 5.3 | Обновление FindPreviewJoin() | 1 час |
| 6.1 | Создание UI панели | 1 час |
| 6.2 | Интеграция с MainWindow | 30 мин |
| 6.3 | Горячие клавиши | 30 мин |
| 7.1 | Сериализация | 1 час |
| 8.1 | Unit-тесты | 2 часа |
| 8.2 | Визуальное тестирование | 3 часа |
| 9.1 | Удаление старого кода | 1 час |
| 10.1 | Документация | 1 час |
| 10.2 | Финальная проверка | 2 часа |
| **ИТОГО** | | **~30 часов** |

---

## 10. ПОТЕНЦИАЛЬНЫЕ СЛОЖНОСТИ

### 10.1 Производительность

**Проблема:**
Расчёт соединений для каждой стены при каждом рендере может быть медленным для большого количества стен.

**Решение:**
- Кэшировать результаты `FindJoins()`
- Обновлять кэш только при изменении стен
- Использовать spatial indexing (R-tree) для быстрого поиска соседних стен

### 10.2 Смешанные режимы привязки

**Проблема:**
Две стены с разными режимами привязки (например, Core и FinishExterior) создают сложную геометрию угла.

**Решение:**
- Определить правила для каждой комбинации режимов
- Использовать более сложные полигоны (6-8 точек вместо 4)
- Визуально тестировать все 9 комбинаций (3×3)

### 10.3 Острые углы

**Проблема:**
При углах < 30° скос может быть очень длинным и выходить за пределы стен.

**Решение:**
- Ограничение длины скоса: `max_miter = thickness * 2`
- При превышении использовать Bevel (прямой срез) вместо Miter (скос)

### 10.4 Обратная совместимость

**Проблема:**
Старые проекты используют `LocationLineMode` с 6 значениями.

**Решение:**
- Конвертация при загрузке: `CoreCenterline → Core`, `CoreFaceExterior → FinishExterior`, и т.д.
- Сохранение версии формата в JSON

---

## 11. ИТОГОВЫЙ ЧЕК-ЛИСТ

### Функциональность
- [ ] Три режима привязки работают корректно
- [ ] L-углы 90° рисуются правильно для всех режимов
- [ ] L-углы других градусов рисуются правильно
- [ ] T-пересечения работают
- [ ] Смешанные режимы работают
- [ ] Разная толщина стен обрабатывается
- [ ] Острые углы ограничиваются
- [ ] Цепочка стен работает
- [ ] Предпросмотр соединения работает

### UI
- [ ] Панель выбора режима отображается
- [ ] RadioButton переключают режим
- [ ] Горячие клавиши работают
- [ ] Индикатор линии привязки отображается при рисовании
- [ ] Цвета индикаторов корректны

### Сохранение/Загрузка
- [ ] Режим привязки сохраняется в JSON
- [ ] Режим привязки загружается из JSON
- [ ] Старые проекты загружаются с конвертацией

### Тестирование
- [ ] Все unit-тесты проходят
- [ ] Все 9 визуальных сценариев проходят
- [ ] Нет крашей или исключений
- [ ] Производительность приемлемая (100+ стен)

### Документация
- [ ] CLAUDE.md обновлён
- [ ] Примеры кода добавлены
- [ ] ROADMAP завершён

---

**КОНЕЦ ROADMAP**

Этот документ является основой для пошаговой реализации новой системы привязки и соединения стен в проекте ARC-Estimate.
