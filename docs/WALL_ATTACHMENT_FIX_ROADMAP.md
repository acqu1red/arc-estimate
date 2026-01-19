# ROADMAP: Исправление системы привязки стен (Wall Attachment System Fix)

**Дата:** 2026-01-20
**Статус:** В работе
**Цель:** Полностью переделать систему привязки и дорисовки стыков стен

---

## 🔍 ВЫЯВЛЕННЫЕ ПРОБЛЕМЫ

### Критические проблемы текущей реализации:

1. **❌ UI панель привязки НЕ существует**
   - В MainWindow.xaml НЕТ панели для выбора режима привязки перед рисованием
   - Есть только ComboBox `LocationLineComboBox` в свойствах ВЫБРАННОЙ стены (строка 508-518)
   - Панель должна появляться СРАЗУ при выборе инструмента Wall

2. **❌ WallTool использует старый LocationLineMode**
   - DrawingTools.h, строка 295: `LocationLineMode m_locationLineMode`
   - Должен использовать `WallAttachmentMode` из новой системы
   - Нет синхронизации с UI панелью

3. **❌ WallRenderer использует старый WallJoinSystem**
   - WallRenderer.h, строка 136: `WallJoinSystem* m_joinSystem`
   - Должен использовать `WallAttachmentSystem`
   - Рендеринг стыков работает неправильно

4. **❌ Нет показа панели при выборе инструмента**
   - MainWindow.xaml.cpp, строка 870-876: обработчик клика НЕ устанавливает режим привязки
   - Панель не показывается/скрывается при смене инструмента

5. **❌ Нет автоматического обновления предпросмотра**
   - При изменении режима привязки в процессе рисования предпросмотр не обновляется
   - Нет InvalidateCanvas() после смены режима

---

## 📋 ДЕТАЛЬНЫЙ ПЛАН РЕАЛИЗАЦИИ

### ЭТАП 1: Создание UI панели привязки (XAML)

**Файл:** `MainWindow.xaml`

**Цель:** Создать панель с тремя RadioButton для выбора режима привязки

**Задачи:**

1.1. Найти место для размещения панели (над Canvas, после Ribbon)

1.2. Создать StackPanel с именем `WallAttachmentModePanel`

1.3. Добавить три RadioButton:
   - `AttachmentModeCore` - "Осевая линия (Центр)"
   - `AttachmentModeExterior` - "Чистовая поверхность: наружная"
   - `AttachmentModeInterior` - "Чистовая поверхность: внутренняя"

1.4. Добавить визуальный индикатор (тонкая цветная линия) для каждого режима

**Код XAML:**

```xml
<!-- Панель привязки стен (показывается только при выборе инструмента Wall) -->
<Border x:Name="WallAttachmentModePanel"
        Visibility="Collapsed"
        Background="#F5F5F5"
        BorderBrush="#CCCCCC"
        BorderThickness="0,1,0,1"
        Padding="12,8"
        Height="48">
    <StackPanel Orientation="Horizontal" Spacing="16">
        <TextBlock Text="Привязка:"
                   VerticalAlignment="Center"
                   FontWeight="SemiBold"
                   FontSize="13"/>

        <!-- Осевая линия (Центр) -->
        <RadioButton x:Name="AttachmentModeCore"
                     GroupName="WallAttachment"
                     IsChecked="True"
                     Click="OnWallAttachmentModeChanged"
                     VerticalAlignment="Center">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <Rectangle Width="24" Height="3" Fill="#0078D4"/>
                <TextBlock Text="Осевая линия" VerticalAlignment="Center"/>
            </StackPanel>
        </RadioButton>

        <!-- Чистовая наружная -->
        <RadioButton x:Name="AttachmentModeExterior"
                     GroupName="WallAttachment"
                     Click="OnWallAttachmentModeChanged"
                     VerticalAlignment="Center">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <Rectangle Width="24" Height="3" Fill="#00B400"/>
                <TextBlock Text="Чистовая наружная" VerticalAlignment="Center"/>
            </StackPanel>
        </RadioButton>

        <!-- Чистовая внутренняя -->
        <RadioButton x:Name="AttachmentModeInterior"
                     GroupName="WallAttachment"
                     Click="OnWallAttachmentModeChanged"
                     VerticalAlignment="Center">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <Rectangle Width="24" Height="3" Fill="#C80000"/>
                <TextBlock Text="Чистовая внутренняя" VerticalAlignment="Center"/>
            </StackPanel>
        </RadioButton>
    </StackPanel>
</Border>
```

**Критерии завершения:**
- ✅ Панель добавлена в XAML
- ✅ Три RadioButton с визуальными индикаторами
- ✅ Панель имеет Visibility="Collapsed" по умолчанию
- ✅ Обработчик `OnWallAttachmentModeChanged` указан

---

### ЭТАП 2: Добавление обработчиков в MainWindow.xaml.h

**Файл:** `MainWindow.xaml.h`

**Задачи:**

2.1. Добавить поле `m_currentAttachmentMode` типа `WallAttachmentMode`

2.2. Объявить обработчик `OnWallAttachmentModeChanged()`

2.3. Объявить методы `ShowAttachmentModePanel()` и `HideAttachmentModePanel()`

**Код:**

```cpp
// В секции private (после m_currentWallSnap):

// R-WALL: Текущий режим привязки стен
WallAttachmentMode m_currentAttachmentMode{ WallAttachmentMode::Core };

// Обработчики UI панели привязки
void OnWallAttachmentModeChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& e);

void ShowAttachmentModePanel();
void HideAttachmentModePanel();
```

**Критерии завершения:**
- ✅ Поле `m_currentAttachmentMode` добавлено
- ✅ Методы объявлены в .h файле

---

### ЭТАП 3: Реализация обработчиков в MainWindow.xaml.cpp

**Файл:** `MainWindow.xaml.cpp`

**Задачи:**

3.1. Реализовать `ShowAttachmentModePanel()` - показывает панель

3.2. Реализовать `HideAttachmentModePanel()` - скрывает панель

3.3. Реализовать `OnWallAttachmentModeChanged()` - обрабатывает смену режима

3.4. Обновить обработчик выбора инструмента для показа панели при выборе Wall

**Код:**

```cpp
// =====================================================
// R-WALL: Показать панель выбора режима привязки
// =====================================================
void MainWindow::ShowAttachmentModePanel()
{
    WallAttachmentModePanel().Visibility(Visibility::Visible);
}

void MainWindow::HideAttachmentModePanel()
{
    WallAttachmentModePanel().Visibility(Visibility::Collapsed);
}

// =====================================================
// R-WALL: Обработчик изменения режима привязки
// =====================================================
void MainWindow::OnWallAttachmentModeChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& e)
{
    (void)sender;
    (void)e;

    // Определить выбранный режим
    if (AttachmentModeCore().IsChecked().GetBoolean())
    {
        m_currentAttachmentMode = WallAttachmentMode::Core;
    }
    else if (AttachmentModeExterior().IsChecked().GetBoolean())
    {
        m_currentAttachmentMode = WallAttachmentMode::FinishExterior;
    }
    else if (AttachmentModeInterior().IsChecked().GetBoolean())
    {
        m_currentAttachmentMode = WallAttachmentMode::FinishInterior;
    }

    // КРИТИЧНО: Обновить режим в WallTool
    m_wallTool.SetAttachmentMode(m_currentAttachmentMode);

    // КРИТИЧНО: Перерисовать canvas для обновления предпросмотра
    InvalidateCanvas();
}
```

**Обновить HandleMenuItemClick (обработчик выбора инструмента):**

```cpp
void MainWindow::HandleMenuItemClick(IInspectable const& sender, RoutedEventArgs const& e)
{
    // ... существующий код ...

    if (tag == L"Wall")
    {
        m_viewModel.CurrentTool(DrawingTool::Wall);

        // R-WALL: Показать панель привязки
        ShowAttachmentModePanel();

        ToolsMenuButton().Content(box_value(L"Стена"));
    }
    else if (tag == L"Select")
    {
        m_viewModel.CurrentTool(DrawingTool::Select);

        // R-WALL: Скрыть панель привязки
        HideAttachmentModePanel();

        ToolsMenuButton().Content(box_value(L"Выбор"));
    }
    // ... остальные инструменты - скрывают панель ...
    else
    {
        // Для всех остальных инструментов скрыть панель
        HideAttachmentModePanel();
    }
}
```

**Критерии завершения:**
- ✅ Панель показывается при выборе инструмента Wall
- ✅ Панель скрывается при выборе любого другого инструмента
- ✅ Обработчик `OnWallAttachmentModeChanged` обновляет m_currentAttachmentMode
- ✅ Вызывается `InvalidateCanvas()` для обновления предпросмотра

---

### ЭТАП 4: Обновление WallTool для WallAttachmentMode

**Файл:** `DrawingTools.h`

**Задачи:**

4.1. Заменить `LocationLineMode m_locationLineMode` на `WallAttachmentMode m_attachmentMode`

4.2. Добавить методы `GetAttachmentMode()` и `SetAttachmentMode()`

4.3. Обновить метод `OnClick()` для использования нового режима

**Код:**

```cpp
class WallTool
{
public:
    // ... существующие методы ...

    // R-WALL: Получить текущий режим привязки
    WallAttachmentMode GetAttachmentMode() const
    {
        return m_attachmentMode;
    }

    // R-WALL: Установить режим привязки
    void SetAttachmentMode(WallAttachmentMode mode)
    {
        m_attachmentMode = mode;
    }

private:
    WallToolState m_state{ WallToolState::Idle };
    WorldPoint m_startPoint{ 0, 0 };
    WorldPoint m_currentPoint{ 0, 0 };
    double m_thickness{ 150.0 };
    WorkStateNative m_workState{ WorkStateNative::Existing };

    // СТАРОЕ: LocationLineMode m_locationLineMode{ LocationLineMode::WallCenterline };
    // НОВОЕ:
    WallAttachmentMode m_attachmentMode{ WallAttachmentMode::Core };

    bool m_isFlipped{ false };
    std::function<void(Wall*)> m_onWallCreated;
};
```

**Обновить OnClick() для использования нового режима:**

```cpp
bool OnClick(
    const WorldPoint& clickPoint,
    DocumentModel& document,
    SnapManager& snapManager,
    LayerManager& layerManager,
    const Camera& camera)
{
    if (m_state == WallToolState::Idle)
    {
        // Начало рисования
        m_startPoint = clickPoint;
        m_currentPoint = clickPoint;
        m_state = WallToolState::Drawing;
        return false;
    }
    else if (m_state == WallToolState::Drawing || m_state == WallToolState::ChainDrawing)
    {
        // Завершение текущей стены
        Wall* newWall = new Wall(m_startPoint, m_currentPoint, m_thickness);

        // R-WALL: Установить режим привязки через конвертацию
        newWall->SetLocationLineMode(
            WallAttachmentSystem::ToLocationLineMode(m_attachmentMode));

        newWall->SetWorkState(m_workState);
        document.AddWall(newWall);

        if (m_onWallCreated)
            m_onWallCreated(newWall);

        // Переход в режим цепного рисования
        m_startPoint = m_currentPoint;
        m_state = WallToolState::ChainDrawing;
        return true;
    }

    return false;
}
```

**Критерии завершения:**
- ✅ `m_attachmentMode` заменяет `m_locationLineMode`
- ✅ Методы `GetAttachmentMode()` и `SetAttachmentMode()` реализованы
- ✅ `OnClick()` использует `WallAttachmentSystem::ToLocationLineMode()`
- ✅ Новые стены создаются с правильным режимом

---

### ЭТАП 5: Обновление WallRenderer для WallAttachmentSystem

**Файл:** `WallRenderer.h`

**Задачи:**

5.1. Заменить `WallJoinSystem* m_joinSystem` на `WallAttachmentSystem* m_attachmentSystem`

5.2. Обновить конструктор и SetJoinSystem

5.3. Обновить метод `DrawWallWithJoins()` для использования новой системы

5.4. Добавить рисование индикатора линии привязки в DrawPreview

**Код:**

```cpp
class WallRenderer
{
public:
    WallRenderer() = default;

    // R-WALL: Установить систему привязки
    void SetAttachmentSystem(WallAttachmentSystem* system)
    {
        m_attachmentSystem = system;
    }

    // Для обратной совместимости (если нужно)
    void SetJoinSystem(WallJoinSystem* system)
    {
        // Игнорируем старую систему
        (void)system;
    }

    // ... остальные методы ...

private:
    // СТАРОЕ: WallJoinSystem* m_joinSystem{ nullptr };
    // НОВОЕ:
    WallAttachmentSystem* m_attachmentSystem{ nullptr };

    // Обновленный метод рисования с соединениями
    void DrawWallWithJoins(
        const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
        const Camera& camera,
        const Wall& wall,
        const LayerManager& layerManager,
        bool isPreview = false,
        bool isHovered = false)
    {
        Windows::UI::Color baseColor = layerManager.GetColorForWorkState(wall.GetWorkState());

        if (isPreview)
            baseColor.A = 100;

        if (isHovered && !wall.IsSelected())
        {
            baseColor.R = static_cast<uint8_t>((std::min)(255, baseColor.R + 40));
            baseColor.G = static_cast<uint8_t>((std::min)(255, baseColor.G + 40));
            baseColor.B = static_cast<uint8_t>((std::min)(255, baseColor.B + 40));
        }

        // R-WALL: Использовать новую систему привязки
        if (m_attachmentSystem)
        {
            // Получить все стены из документа (нужно передать через параметр)
            // Для preview wall используем временный пустой вектор
            std::vector<std::unique_ptr<Wall>> emptyWalls;

            auto joins = m_attachmentSystem->FindJoins(wall, emptyWalls, 50.0);

            if (!joins.empty())
            {
                auto contour = m_attachmentSystem->BuildWallContour(wall, joins);

                if (contour.size() >= 3)
                {
                    DrawContour(session, camera, contour, baseColor, wall.GetWorkState(),
                               isPreview, wall.IsSelected());

                    if (wall.IsSelected())
                        DrawSelectionHandles(session, camera, wall);

                    if (wall.IsSelected() || isPreview)
                        DrawAxisLine(session, camera, wall);

                    return;
                }
            }
        }

        // Fallback: рисовать без соединений
        DrawSimpleWall(session, camera, wall, baseColor, isPreview);
    }
};
```

**Добавить рисование индикатора линии привязки в DrawPreview:**

```cpp
void DrawPreview(
    const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
    const Camera& camera,
    const Wall& previewWall)
{
    // ... существующий код рисования preview wall ...

    // R-WALL: Нарисовать линию привязки
    AttachmentLine attachLine = WallAttachmentSystem::GetAttachmentLine(previewWall);
    DrawAttachmentLineIndicator(session, camera, attachLine);
}

// Новый метод для рисования индикатора линии привязки
void DrawAttachmentLineIndicator(
    const Microsoft::Graphics::Canvas::CanvasDrawingSession& session,
    const Camera& camera,
    const AttachmentLine& line)
{
    ScreenPoint start = camera.WorldToScreen(line.Start);
    ScreenPoint end = camera.WorldToScreen(line.End);

    Windows::UI::Color color;
    switch (line.Mode)
    {
        case WallAttachmentMode::Core:
            color = Windows::UI::ColorHelper::FromArgb(180, 0, 120, 215);  // Синий
            break;
        case WallAttachmentMode::FinishExterior:
            color = Windows::UI::ColorHelper::FromArgb(180, 0, 180, 0);    // Зелёный
            break;
        case WallAttachmentMode::FinishInterior:
            color = Windows::UI::ColorHelper::FromArgb(180, 200, 0, 0);    // Красный
            break;
    }

    auto style = Microsoft::Graphics::Canvas::Geometry::CanvasStrokeStyle();
    style.DashStyle(Microsoft::Graphics::Canvas::Geometry::CanvasDashStyle::Dash);

    session.DrawLine(
        Windows::Foundation::Numerics::float2(start.X, start.Y),
        Windows::Foundation::Numerics::float2(end.X, end.Y),
        color, 2.0f, style);
}
```

**Критерии завершения:**
- ✅ `m_attachmentSystem` заменяет `m_joinSystem`
- ✅ `DrawWallWithJoins()` использует `WallAttachmentSystem::FindJoins()`
- ✅ `DrawPreview()` рисует индикатор линии привязки
- ✅ Цвет индикатора зависит от режима (синий/зелёный/красный)

---

### ЭТАП 6: Интеграция WallAttachmentSystem в MainWindow

**Файл:** `MainWindow.xaml.h` и `MainWindow.xaml.cpp`

**Задачи:**

6.1. Добавить поле `WallAttachmentSystem m_attachmentSystem` в MainWindow

6.2. Передать указатель на `m_attachmentSystem` в `WallRenderer`

6.3. Обеспечить доступ к `m_document.GetWalls()` для поиска соединений

**Код в MainWindow.xaml.h:**

```cpp
private:
    // ... существующие поля ...

    // R-WALL: Система привязки и соединения стен
    WallAttachmentSystem m_attachmentSystem;
```

**Код в MainWindow() конструкторе (MainWindow.xaml.cpp):**

```cpp
MainWindow::MainWindow()
{
    InitializeComponent();

    // ... существующий код ...

    // R-WALL: Подключить систему привязки к рендереру
    m_wallRenderer.SetAttachmentSystem(&m_attachmentSystem);
}
```

**Обновить DrawWallWithJoins для передачи всех стен:**

```cpp
// В OnCanvasDraw(), в цикле рисования стен:
for (const auto& wall : m_document.GetWalls())
{
    if (!wall || !m_layerManager.IsWorkStateVisible(wall->GetWorkState()))
        continue;

    // R-WALL: Найти соединения для текущей стены
    auto joins = m_attachmentSystem.FindJoins(*wall, m_document.GetWalls(), 50.0);

    // Передать в renderer
    m_wallRenderer.DrawWallWithAttachmentJoins(
        session, m_camera, *wall, m_layerManager, joins,
        false, hoveredElement == wall.get());
}
```

**Критерии завершения:**
- ✅ `m_attachmentSystem` создан в MainWindow
- ✅ Указатель передан в `m_wallRenderer`
- ✅ Соединения вычисляются для каждой стены перед рисованием
- ✅ Все стены передаются в `FindJoins()`

---

### ЭТАП 7: Добавление горячих клавиш

**Файл:** `MainWindow.xaml.cpp`

**Задачи:**

7.1. Обновить обработчик `OnKeyDown()` для клавиш 1, 2, 3

7.2. Синхронизировать состояние RadioButton при нажатии клавиш

**Код:**

```cpp
void MainWindow::OnKeyDown(KeyRoutedEventArgs const& args)
{
    auto key = args.Key();

    // R-WALL: Горячие клавиши для режимов привязки (только если инструмент Wall активен)
    if (m_viewModel.CurrentTool() == DrawingTool::Wall)
    {
        if (key == VirtualKey::Number1)
        {
            AttachmentModeCore().IsChecked(true);
            m_currentAttachmentMode = WallAttachmentMode::Core;
            m_wallTool.SetAttachmentMode(m_currentAttachmentMode);
            InvalidateCanvas();
            args.Handled(true);
            return;
        }
        else if (key == VirtualKey::Number2)
        {
            AttachmentModeExterior().IsChecked(true);
            m_currentAttachmentMode = WallAttachmentMode::FinishExterior;
            m_wallTool.SetAttachmentMode(m_currentAttachmentMode);
            InvalidateCanvas();
            args.Handled(true);
            return;
        }
        else if (key == VirtualKey::Number3)
        {
            AttachmentModeInterior().IsChecked(true);
            m_currentAttachmentMode = WallAttachmentMode::FinishInterior;
            m_wallTool.SetAttachmentMode(m_currentAttachmentMode);
            InvalidateCanvas();
            args.Handled(true);
            return;
        }
    }

    // ... остальная обработка клавиш ...
}
```

**Критерии завершения:**
- ✅ Клавиши 1/2/3 переключают режим привязки
- ✅ RadioButton обновляется при нажатии клавиш
- ✅ Вызывается `InvalidateCanvas()` для обновления предпросмотра
- ✅ Горячие клавиши работают только при активном инструменте Wall

---

### ЭТАП 8: Обновление рендеринга в OnCanvasDraw

**Файл:** `MainWindow.xaml.cpp`

**Задачи:**

8.1. Обновить цикл рисования стен для использования AttachmentJoinInfo

8.2. Обновить рисование preview wall

8.3. Убедиться, что индикатор линии привязки рисуется

**Код в OnCanvasDraw():**

```cpp
void MainWindow::OnCanvasDraw(
    CanvasControl const& sender,
    CanvasDrawEventArgs const& args)
{
    auto session = args.DrawingSession();

    // ... рисование сетки, DXF, IFC, комнат ...

    // R-WALL: Рисование стен с новой системой соединений
    Element* hoveredElement = nullptr;
    if (m_currentHover.hasHover)
        hoveredElement = m_currentHover.element;

    for (const auto& wall : m_document.GetWalls())
    {
        if (!wall || !m_layerManager.IsWorkStateVisible(wall->GetWorkState()))
            continue;

        // Вычислить соединения
        auto joins = m_attachmentSystem.FindJoins(*wall, m_document.GetWalls(), 50.0);

        // Построить контур
        auto contour = m_attachmentSystem.BuildWallContour(*wall, joins);

        // Нарисовать стену
        if (contour.size() >= 3)
        {
            m_wallRenderer.DrawWallContour(
                session, m_camera, *wall, contour, m_layerManager,
                false, hoveredElement == wall.get());
        }
        else
        {
            // Fallback: простая стена без соединений
            m_wallRenderer.DrawSimpleWall(session, m_camera, *wall, m_layerManager,
                false, hoveredElement == wall.get());
        }
    }

    // ... рисование других элементов ...

    // R-WALL: Рисование preview wall с индикатором линии привязки
    if (m_viewModel.CurrentTool() == DrawingTool::Wall && m_wallTool.ShouldDrawPreview())
    {
        WorldPoint startPt, endPt;
        m_wallTool.GetEffectivePoints(startPt, endPt);

        Wall previewWall(startPt, endPt, m_wallTool.GetThickness());
        previewWall.SetLocationLineMode(
            WallAttachmentSystem::ToLocationLineMode(m_currentAttachmentMode));

        // Нарисовать preview стену
        m_wallRenderer.DrawPreview(session, m_camera, previewWall);

        // Нарисовать индикатор линии привязки
        AttachmentLine attachLine = WallAttachmentSystem::GetAttachmentLine(previewWall);
        m_wallRenderer.DrawAttachmentLineIndicator(session, m_camera, attachLine);
    }

    // ... остальное рисование ...
}
```

**Критерии завершения:**
- ✅ Все стены рисуются с соединениями через WallAttachmentSystem
- ✅ Preview wall рисуется с индикатором линии привязки
- ✅ Индикатор имеет правильный цвет в зависимости от режима
- ✅ Нет визуальных артефактов

---

### ЭТАП 9: Тестирование и отладка

**Задачи:**

9.1. Компиляция проекта

9.2. Исправление ошибок компиляции

9.3. Визуальное тестирование всех трёх режимов

**Тестовые сценарии:**

#### Сценарий 1: Показ панели привязки
1. Запустить приложение
2. Выбрать инструмент "Стена"
3. **Ожидаемый результат:** Панель привязки появляется над холстом
4. Выбрать инструмент "Выбор"
5. **Ожидаемый результат:** Панель скрывается

#### Сценарий 2: Рисование в режиме Core
1. Выбрать инструмент "Стена"
2. Выбрать режим "Осевая линия"
3. Нарисовать две стены под 90°
4. **Ожидаемый результат:**
   - Индикатор линии привязки синий
   - Угол формируется на пересечении центральных осей
   - Стыки визуально правильные

#### Сценарий 3: Рисование в режиме FinishExterior
1. Выбрать режим "Чистовая наружная"
2. Нарисовать две стены под 90°
3. **Ожидаемый результат:**
   - Индикатор зелёный
   - Угол формируется на пересечении внешних граней
   - Вторая стена начинается от внешней грани первой

#### Сценарий 4: Рисование в режиме FinishInterior
1. Выбрать режим "Чистовая внутренняя"
2. Нарисовать две стены под 90°
3. **Ожидаемый результат:**
   - Индикатор красный
   - Угол формируется на пересечении внутренних граней
   - Вторая стена начинается от внутренней грани первой

#### Сценарий 5: Смена режима в процессе рисования
1. Выбрать режим "Осевая линия"
2. Нарисовать первую стену
3. НЕ завершая рисование, сменить режим на "Чистовая наружная" (клавиша 2)
4. **Ожидаемый результат:**
   - Предпросмотр второй стены автоматически обновляется
   - Угол смещается к внешней грани
   - Индикатор меняет цвет с синего на зелёный

#### Сценарий 6: Горячие клавиши
1. Выбрать инструмент "Стена"
2. Нажать клавишу "1"
3. **Ожидаемый результат:** RadioButton "Осевая линия" активирован
4. Нажать клавишу "2"
5. **Ожидаемый результат:** RadioButton "Чистовая наружная" активирован
6. Нажать клавишу "3"
7. **Ожидаемый результат:** RadioButton "Чистовая внутренняя" активирован

#### Сценарий 7: Цепочка стен
1. Нарисовать прямоугольник из 4 стен в режиме Core
2. **Ожидаемый результат:** Все 4 угла корректны
3. Нарисовать прямоугольник в режиме FinishExterior
4. **Ожидаемый результат:** Все углы на внешних гранях
5. Нарисовать прямоугольник в режиме FinishInterior
6. **Ожидаемый результат:** Все углы на внутренних гранях

#### Сценарий 8: Разные углы
1. Нарисовать стены под углом 45°
2. **Ожидаемый результат:** Скосы корректны, не слишком длинные
3. Нарисовать стены под тупым углом (135°)
4. **Ожидаемый результат:** Скосы корректны

**Критерии завершения этапа 9:**
- ✅ Проект компилируется без ошибок
- ✅ Все 8 тестовых сценариев проходят успешно
- ✅ Нет визуальных артефактов
- ✅ Нет крашей или исключений
- ✅ Предпросмотр обновляется плавно при смене режима

---

### ЭТАП 10: Финализация и документация

**Задачи:**

10.1. Обновить CLAUDE.md с описанием новой системы

10.2. Добавить комментарии в код

10.3. Создать отчёт о завершении

**Обновление CLAUDE.md:**

```markdown
## Wall Attachment System (R-WALL)

**Status:** ✅ Fully Implemented (2026-01-20)

### Overview

A new fundamental system for wall attachment and corner joining, replacing the legacy WallJoinSystem.

### Key Features

1. **Three Attachment Modes:**
   - **Core** (Centerline) - Walls attach at their centerlines
   - **FinishExterior** (Outer Finish Face) - Walls attach at outer finish faces
   - **FinishInterior** (Inner Finish Face) - Walls attach at inner finish faces

2. **UI Panel:**
   - Appears automatically when Wall tool is selected
   - Three RadioButtons with visual indicators (blue/green/red)
   - Hot keys: 1 (Core), 2 (Exterior), 3 (Interior)

3. **Real-time Preview:**
   - Attachment line indicator shows current mode
   - Color-coded: Blue (Core), Green (Exterior), Red (Interior)
   - Preview updates automatically when mode is changed

4. **Join Algorithms:**
   - L-Join: Corner joins with miter cuts
   - T-Join: One wall ends at another's middle
   - X-Join: Two walls cross in the middle
   - Collinear: Walls on the same line

### Architecture

**Core Classes:**
- `WallAttachmentSystem` (WallAttachmentSystem.h) - Main system class
- `AttachmentLine` - Line offset from wall centerline based on mode
- `AttachmentJoinInfo` - Join information with geometry

**Integration:**
- `WallTool` - Uses `WallAttachmentMode` for new walls
- `WallRenderer` - Uses `WallAttachmentSystem` for rendering
- `MainWindow` - Shows/hides attachment mode panel based on active tool

### Usage

**Creating Walls:**
```cpp
// Select Wall tool - attachment panel appears
// Choose mode (Core/Exterior/Interior)
// Click to start wall
// Click to end wall
// Second wall automatically uses same mode
```

**Changing Mode During Drawing:**
```cpp
// While drawing, press 1/2/3 or click RadioButton
// Preview updates immediately
// New wall segment uses new mode
```

**Rendering:**
```cpp
auto joins = m_attachmentSystem.FindJoins(wall, allWalls, 50.0);
auto contour = m_attachmentSystem.BuildWallContour(wall, joins);
// Draw contour
```

### Files Modified

- `MainWindow.xaml` - Added WallAttachmentModePanel
- `MainWindow.xaml.h` - Added m_currentAttachmentMode, panel methods
- `MainWindow.xaml.cpp` - Implemented handlers, show/hide logic
- `DrawingTools.h` - WallTool uses WallAttachmentMode
- `WallRenderer.h` - Uses WallAttachmentSystem instead of WallJoinSystem
- `WallAttachmentSystem.h` - Core system implementation (750+ lines)

### Testing

All test scenarios passed:
- ✅ Panel shows/hides correctly
- ✅ All three modes render correctly
- ✅ Preview updates on mode change
- ✅ Hot keys work
- ✅ Chain drawing works
- ✅ Various angles work correctly
```

**Критерии завершения этапа 10:**
- ✅ CLAUDE.md обновлён
- ✅ Комментарии добавлены в код
- ✅ Отчёт о завершении создан

---

## 📊 ИТОГОВЫЙ ЧЕК-ЛИСТ

### Функциональность
- [ ] UI панель привязки создана и работает
- [ ] Панель показывается при выборе инструмента Wall
- [ ] Панель скрывается при выборе других инструментов
- [ ] Три RadioButton переключают режим
- [ ] Визуальные индикаторы (цветные линии) отображаются
- [ ] WallTool использует WallAttachmentMode
- [ ] WallRenderer использует WallAttachmentSystem
- [ ] Стыки рисуются правильно для всех режимов
- [ ] Предпросмотр показывает индикатор линии привязки
- [ ] Предпросмотр обновляется при смене режима
- [ ] Горячие клавиши 1/2/3 работают
- [ ] Цепочка стен работает корректно

### UI/UX
- [ ] Панель имеет правильный дизайн
- [ ] Цвета индикаторов правильные (синий/зелёный/красный)
- [ ] Нет визуальных артефактов
- [ ] Переключение плавное
- [ ] Горячие клавиши интуитивны

### Код
- [ ] Проект компилируется без ошибок
- [ ] Нет предупреждений (или минимум)
- [ ] Код хорошо структурирован
- [ ] Комментарии добавлены
- [ ] Нет дублирования кода

### Тестирование
- [ ] Все 8 тестовых сценариев проходят
- [ ] Нет крашей
- [ ] Нет исключений
- [ ] Производительность приемлемая

### Документация
- [ ] CLAUDE.md обновлён
- [ ] ROADMAP завершён
- [ ] Отчёт о завершении создан

---

## ⏱️ ОЦЕНКА ВРЕМЕНИ

| Этап | Описание | Время |
|------|----------|-------|
| 1 | Создание UI панели (XAML) | 30 мин |
| 2 | Обработчики в .h файле | 15 мин |
| 3 | Реализация обработчиков (.cpp) | 45 мин |
| 4 | Обновление WallTool | 30 мин |
| 5 | Обновление WallRenderer | 1 час |
| 6 | Интеграция в MainWindow | 30 мин |
| 7 | Горячие клавиши | 20 мин |
| 8 | Обновление OnCanvasDraw | 45 мин |
| 9 | Тестирование и отладка | 2 часа |
| 10 | Финализация и документация | 30 мин |
| **ИТОГО** | | **~7 часов** |

---

## 🎯 ОЖИДАЕМЫЙ РЕЗУЛЬТАТ

После завершения всех этапов:

1. ✅ **UI панель появляется СРАЗУ** при выборе инструмента "Стена"
2. ✅ **Три режима привязки работают правильно**
3. ✅ **Предпросмотр обновляется автоматически** при смене режима
4. ✅ **Визуальные индикаторы** показывают линию привязки (синий/зелёный/красный)
5. ✅ **Стыки рисуются корректно** для всех комбинаций режимов
6. ✅ **Горячие клавиши** 1/2/3 работают
7. ✅ **Цепочка стен** работает без проблем
8. ✅ **Система стабильна** - нет крашей или багов

**Система будет работать фундаментально правильно, как в Revit!**

---

**КОНЕЦ ROADMAP**

Этот ROADMAP является детальным планом для полного исправления системы привязки стен.
