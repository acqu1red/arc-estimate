# ИНСТРУКЦИЯ: Доработка визуализации рисования стен в стиле AutoCAD

## Создано: 2025-01-20
## Статус: В ПРОЦЕССЕ (70% готово)

---

## ЧТО УЖЕ СДЕЛАНО ?

1. **Создан TemporaryDimensionRenderer.h** - система временных размеров (появляются после клика)
2. **Создан AutoCADStyleRenderer.h** - рендерер в стиле AutoCAD для:
   - Отображения размеров НА стене оранжевым текстом
   - Рисования углов с дугами
   - Пунктирных линий привязки
   - Подсказок режима ("Вертикаль", "Горизонталь")

3. **Обновлен MainWindow.xaml.h** - добавлены include для новых рендереров

---

## ЧТО НУЖНО ДОДЕЛАТЬ ??

### Шаг 1: Добавить поле m_temporaryDimensionManager в MainWindow.xaml.h

**Файл:** `MainWindow.xaml.h`
**Строка:** После `m_currentAttachmentMode` (около стр. 304)

```cpp
// RWALL: Менеджер временных размеров (как в AutoCAD)
TemporaryDimensionManager m_temporaryDimensionManager;
```

---

### Шаг 2: Обновить DrawPreview() в WallRenderer.h

**Файл:** `WallRenderer.h`
**Метод:** `DrawPreview()` (около строки 110-147)

**ЗАМЕНИТЕ:**
```cpp
// Рисуем длину стены
double length = previewWall.GetLength();
if (length > 10.0)
{
    DrawDimensionLabel(session, camera, previewWall, length);
}
```

**НА:**
```cpp
// Рисуем длину стены В СТИЛЕ AUTOCAD
double length = previewWall.GetLength();
if (length > 10.0)
{
    AutoCADStyleRenderer::DrawWallDimension(
        session, camera,
        startPoint, endPoint, length);
}

// Рисуем пунктирную линию привязки
bool isHorizontal = std::abs(endPoint.Y - startPoint.Y) < 50.0;
bool isVertical = std::abs(endPoint.X - startPoint.X) < 50.0;
AutoCADStyleRenderer::DrawAttachmentLine(
    session, camera,
    startPoint, endPoint,
    isHorizontal, isVertical);
```

---

### Шаг 3: Обновить обработку клика в MainWindow.xaml.cpp

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `HandleToolClick()` в case DrawingTool::Wall (около строки 843-886)

**ДОБАВЬТЕ ПОСЛЕ:**
```cpp
bool wallCreated = m_wallTool.OnClick(
    clickPoint, m_document, m_snapManager, m_layerManager, m_camera);

if (wallCreated)
{
    UpdateSelectedElementUI();
    
    // ? НОВЫЙ КОД: Показать временные размеры после клика
    m_temporaryDimensionManager.GenerateFromPoint(
        clickPoint,
        m_document.GetWalls(),
        3000.0  // Максимальное расстояние 3000мм
    );
}
```

---

### Шаг 4: Скрывать временные размеры при движении мыши

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `OnCanvasPointerMoved()` в блоке DrawingTool::Wall (около строки 1525-1569)

**ДОБАВЬТЕ В НАЧАЛО БЛОКА:**
```cpp
if (m_viewModel.CurrentTool() == DrawingTool::Wall)
{
    // ? Скрыть временные размеры при движении мыши
    if (m_temporaryDimensionManager.IsVisible())
    {
        m_temporaryDimensionManager.Hide();
    }
    
    // ... остальной код
```

---

### Шаг 5: Рисовать временные размеры в OnCanvasDraw()

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `OnCanvasDraw()` (около строки 412-700)

**ДОБАВЬТЕ ПЕРЕД рисованием превью стены (строка ~503):**
```cpp
// ? Рисуем временные размеры (появляются после клика)
if (m_temporaryDimensionManager.IsVisible())
{
    TemporaryDimensionRenderer::DrawTemporaryDimensions(
        session, m_camera,
        m_temporaryDimensionManager.GetDimensions());
}

// Рисуем превью стены (если рисуем)
if (m_viewModel.CurrentTool() == DrawingTool::Wall && m_wallTool.ShouldDrawPreview())
```

---

### Шаг 6: Улучшить рисование угла

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `OnCanvasDraw()` в блоке рисования превью стены (около строки 534-538)

**ЗАМЕНИТЕ:**
```cpp
// R3: Рисуем угол от горизонтали при рисовании стены
WorldPoint startPt = m_wallTool.GetStartPoint();
if (startPt.Distance(endPoint) > 50.0)
{
    m_dimensionRenderer.DrawAngleFromHorizontal(session, m_camera, startPt, endPoint);
}
```

**НА:**
```cpp
// ? Рисуем угол В СТИЛЕ AUTOCAD (дуга + текст)
WorldPoint startPt = m_wallTool.GetStartPoint();
if (startPt.Distance(endPoint) > 50.0)
{
    // Вычисляем угол
    double dx = endPoint.X - startPt.X;
    double dy = endPoint.Y - startPt.Y;
    double angleRad = std::atan2(dy, dx);
    double angleDeg = angleRad * 180.0 / 3.14159265;
    if (angleDeg < 0) angleDeg += 360.0;

    // Точки для дуги
    WorldPoint horizontalEnd(startPt.X + 500.0, startPt.Y);
    
    AutoCADStyleRenderer::DrawAngleBetweenWalls(
        session, m_camera,
        startPt,          // Вершина
        horizontalEnd,    // Конец горизонтальной линии
        endPoint,         // Конец текущей стены
        angleDeg);
}
```

---

### Шаг 7: Добавить подсказки режима привязки

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `OnCanvasDraw()` в блоке рисования превью стены

**ДОБАВЬТЕ ПОСЛЕ рисования угла:**
```cpp
// ? Показать подсказку режима ("Вертикаль", "Горизонталь")
std::wstring modeHint;
double dx = std::abs(endPoint.X - startPt.X);
double dy = std::abs(endPoint.Y - startPt.Y);

if (dy < 50.0)
    modeHint = L"Горизонталь";
else if (dx < 50.0)
    modeHint = L"Вертикаль";

if (!modeHint.empty())
{
    AutoCADStyleRenderer::DrawModeHint(
        session, m_camera, endPoint, modeHint);
}
```

---

### Шаг 8: Добавить маркеры привязки на концах стен

**Файл:** `MainWindow.xaml.cpp`
**Метод:** `OnCanvasDraw()` после рисования стен

**ДОБАВЬТЕ ПЕРЕД рисованием превью:**
```cpp
// ? Рисуем маркеры привязки на концах всех стен
for (const auto& wall : m_document.GetWalls())
{
    if (!wall || !m_layerManager.IsWorkStateVisible(wall->GetWorkState()))
        continue;

    AutoCADStyleRenderer::DrawSnapMarker(
        session, m_camera, wall->GetStartPoint(), false);
    AutoCADStyleRenderer::DrawSnapMarker(
        session, m_camera, wall->GetEndPoint(), false);
}

// Активный маркер (если есть привязка)
if (m_currentSnap.hasSnap)
{
    AutoCADStyleRenderer::DrawSnapMarker(
        session, m_camera, m_currentSnap.point, true);
}
```

---

## ИТОГОВЫЙ РЕЗУЛЬТАТ ??

После выполнения всех шагов вы получите:

1. **Размеры на стене** - оранжевый масштабируемый текст (как Скриншот 2)
2. **Пунктирные линии привязки** - через всю стену (как Скриншот 3)
3. **Углы с дугами** - дуга + текст "90.00°" (как Скриншот 1)
4. **Временные размеры** - после клика показывают расстояния до всех стен (как Скриншот 1)
5. **Подсказки режима** - "Вертикаль", "Горизонталь" (как Скриншот 3)
6. **Маркеры привязки** - квадратики на концах стен

---

## ТЕСТИРОВАНИЕ ??

### Тест 1: Размеры на стене
1. Активировать инструмент "Стена"
2. Начать рисовать стену
3. **Ожидается:** Оранжевый текст размера НА стене (не в желтом прямоугольнике)

### Тест 2: Временные размеры
1. Нарисовать несколько стен
2. Начать новую стену
3. Кликнуть ЛКМ (поставить конец)
4. **Ожидается:** Появились голубые размерные линии до всех соседних стен
5. Подвинуть мышь
6. **Ожидается:** Временные размеры исчезли

### Тест 3: Углы
1. Рисовать стену под углом
2. **Ожидается:** Дуга с текстом угла (например "45.00°")

### Тест 4: Масштабирование
1. Нарисовать стену
2. Zoom Out (прокрутка вниз)
3. **Ожидается:** Размеры уменьшаются вместе со стеной
4. Zoom In (прокрутка вверх)
5. **Ожидается:** Размеры увеличиваются

---

## ДОПОЛНИТЕЛЬНЫЕ УЛУЧШЕНИЯ (по желанию) ??

### 1. Цвета как в AutoCAD
- Стены: белый контур
- Размеры: оранжевый/желтый
- Временные размеры: голубой
- Фон: темно-серый (#2B2B2B)

### 2. Толщина линий
- Стены: 1.5px
- Размеры: 1.0px
- Пунктир: 0.8px

### 3. Шрифты
- Размеры: Arial, размер зависит от zoom
- Минимум: 10px, Максимум: 20px

---

## КОНТАКТЫ

Если возникнут проблемы при реализации - обращайтесь!
Все файлы уже созданы, нужно только интегрировать их.

**Удачи! ??**
