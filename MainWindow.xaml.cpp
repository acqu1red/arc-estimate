#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.h>
#include <Shobjidl.h>
#include <microsoft.ui.xaml.window.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::Graphics::Canvas::UI::Xaml;

namespace winrt::estimate1::implementation
{
    MainWindow::MainWindow()
    {
        // Инициализация модели представления
        m_viewModel = winrt::make<MainViewModel>();
        
        // Инициализация компонентов XAML
        InitializeComponent();
        
        // Обновляем состояние кнопок инструментов
        UpdateToolButtonStates();

        // Настраиваем LayerManager для обновления холста при изменениях
        m_layerManager.SetOnLayerChanged([this]() {
            InvalidateCanvas();
        });

        // R2: Связываем систему соединений с рендерером стен
        m_wallRenderer.SetJoinSystem(&m_wallJoinSystem);
        // Применяем настройки соединений из документа
        m_wallJoinSystem.SetSettings(m_document.GetJoinSettings());

        // Настраиваем WallTool для обновления UI при создании стены
            m_wallTool.SetOnWallCreated([this](Wall* wall) {
                // R2: Используем новую систему соединений
                if (wall)
                {
                    m_wallJoinSystem.ProcessNewWall(*wall, const_cast<std::vector<std::unique_ptr<Wall>>&>(m_document.GetWalls()));
                }
            
                m_viewModel.HasUnsavedChanges(true);
                UpdateSelectedElementUI();
                InvalidateCanvas();
            });

            // Настраиваем DimensionTool для обновления UI при создании размера
            m_dimensionTool.SetOnDimensionCreated([this](Dimension* dim) {
                m_viewModel.HasUnsavedChanges(true);
                m_document.SetSelectedElement(dim);
                UpdateSelectedElementUI();
                InvalidateCanvas();
            });

            // Применяем начальную видимость слоёв (Обмерный вид по умолчанию)
            UpdateLayerVisibility();

            // M3.1: загружаем каталог типов стен в UI
            RebuildWallTypeCombo();

            // M3.5: первичная генерация авторазмеров
            m_autoDimensionManager.Rebuild(m_document);

            // M5.6: Подписываемся на событие изменения режима привязки
            // (делаем это после InitializeComponent чтобы избежать вызова до инициализации)
            if (SnapModeComboBox())
            {
                SnapModeComboBox().SelectionChanged({ this, &MainWindow::OnSnapModeChanged });
            }

            // R4: Настраиваем инструмент двери
            m_doorTool.SetOnDoorCreated([this](std::shared_ptr<Door> door) {
                if (door)
                {
                    m_document.AddDoor(door);
                    m_document.SetSelectedElement(door.get());
                    UpdateSelectedElementUI();
                }
            });

            // R4: Настраиваем инструмент окна
            m_windowTool.SetOnWindowCreated([this](std::shared_ptr<Window> window) {
                if (window)
                {
                    m_document.AddWindow(window);
                    m_document.SetSelectedElement(window.get());
                    UpdateSelectedElementUI();
                }
            });
        }

        void MainWindow::RebuildWallTypeCombo()
        {
            if (!WallTypeComboBox())
                return;

        WallTypeComboBox().Items().Clear();

        const auto& types = m_document.GetWallTypes();
        for (const auto& t : types)
        {
            if (!t)
                continue;
            auto item = Microsoft::UI::Xaml::Controls::ComboBoxItem();
            item.Content(winrt::box_value(winrt::hstring(t->GetName())));
            WallTypeComboBox().Items().Append(item);
        }
    }

    // Получение модели представления
    estimate1::MainViewModel MainWindow::ViewModel()
    {
        return m_viewModel;
    }

    // Обработчик клика по инструменту "Выбор"
    void MainWindow::OnSelectToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Отменяем текущее рисование стены
        m_wallTool.Cancel();
        
        m_viewModel.CurrentTool(DrawingTool::Select);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    // Обработчик клика по инструменту "Стена"
    void MainWindow::OnWallToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_viewModel.CurrentTool(DrawingTool::Wall);
        UpdateToolButtonStates();
    }

    // Обработчик клика по инструменту "Дверь"
    void MainWindow::OnDoorToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_viewModel.CurrentTool(DrawingTool::Door);
        UpdateToolButtonStates();
    }

    // Обработчик клика по инструменту "Окно"
    void MainWindow::OnWindowToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_viewModel.CurrentTool(DrawingTool::Window);
        UpdateToolButtonStates();
    }

    // Обработчик клика по инструменту "Размер"
    void MainWindow::OnDimensionToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Отменяем текущее рисование стены
        m_wallTool.Cancel();
        
        m_viewModel.CurrentTool(DrawingTool::Dimension);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    // R2.5: Trim/Extend Tool
    void MainWindow::OnTrimExtendToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_wallTool.Cancel();
        m_trimExtendTool.Reset(); // Reset internal state
        m_viewModel.CurrentTool(DrawingTool::TrimExtend);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    // R2.5: Split Tool
    void MainWindow::OnSplitToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_wallTool.Cancel();
        m_viewModel.CurrentTool(DrawingTool::Split);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    // R6: Structural Tools
    void MainWindow::OnColumnToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_wallTool.Cancel();
        m_viewModel.CurrentTool(DrawingTool::Column);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    void MainWindow::OnSlabToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_wallTool.Cancel();
        m_viewModel.CurrentTool(DrawingTool::Slab);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    void MainWindow::OnBeamToolClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_wallTool.Cancel();
        m_viewModel.CurrentTool(DrawingTool::Beam);
        UpdateToolButtonStates();
        InvalidateCanvas();
    }

    // Обработчики меню Вид
    void MainWindow::OnDimensionsToggleClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        m_showDimensions = DimensionsToggle().IsChecked();
        InvalidateCanvas();
    }

    void MainWindow::OnAutoDimensionsToggleClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        bool enabled = AutoDimensionsToggle().IsChecked();
        m_document.SetAutoDimensionsEnabled(enabled);
        InvalidateCanvas();
    }

    // Обработчик переключения вкладок видов
    void MainWindow::OnViewTabChanged(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        auto radioButton = sender.try_as<RadioButton>();
        if (radioButton)
        {
            auto name = radioButton.Name();
            if (name == L"MeasureViewTab")
            {
                m_viewModel.ActiveView(PlanView::Measure);
            }
            else if (name == L"DemolitionViewTab")
            {
                m_viewModel.ActiveView(PlanView::Demolition);
            }
            else if (name == L"ConstructionViewTab")
            {
                m_viewModel.ActiveView(PlanView::Construction);
            }
        }
        
        // Обновляем видимость слоёв при смене вида
        UpdateLayerVisibility();
        
        // Перерисовываем холст при смене вида
        InvalidateCanvas();
    }

    // Обновление видимости слоёв при смене вида
    void MainWindow::UpdateLayerVisibility()
    {
        PlanView activeView = m_viewModel.ActiveView();
        PlanViewNative nativeView;

        switch (activeView)
        {
        case PlanView::Measure:
            nativeView = PlanViewNative::Measure;
            break;
        case PlanView::Demolition:
            nativeView = PlanViewNative::Demolition;
            break;
        case PlanView::Construction:
            nativeView = PlanViewNative::Construction;
            break;
        default:
            nativeView = PlanViewNative::Measure;
            break;
        }

        m_layerManager.ApplyViewVisibility(nativeView);

        // Синхронизируем UI чекбоксы с состоянием слоёв
        SyncLayerCheckboxes();
    }

    // Синхронизация UI чекбоксов с состоянием слоёв
    void MainWindow::SyncLayerCheckboxes()
    {
        m_updatingLayerCheckboxes = true;

        Layer* existingLayer = m_layerManager.GetLayerByWorkState(WorkStateNative::Existing);
        Layer* demolishLayer = m_layerManager.GetLayerByWorkState(WorkStateNative::Demolish);
        Layer* newLayer = m_layerManager.GetLayerByWorkState(WorkStateNative::New);

        if (ExistingLayerCheckBox() && existingLayer)
            ExistingLayerCheckBox().IsChecked(existingLayer->IsVisible());
        
        if (DemolishLayerCheckBox() && demolishLayer)
            DemolishLayerCheckBox().IsChecked(demolishLayer->IsVisible());
        
        if (NewLayerCheckBox() && newLayer)
            NewLayerCheckBox().IsChecked(newLayer->IsVisible());

        m_updatingLayerCheckboxes = false;
    }

    // Обработчик изменения видимости слоя из UI
    void MainWindow::OnLayerVisibilityChanged(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Предотвращаем рекурсию при программном обновлении чекбоксов
        if (m_updatingLayerCheckboxes)
            return;

        auto checkbox = sender.try_as<Microsoft::UI::Xaml::Controls::CheckBox>();
        if (!checkbox)
            return;

        auto name = checkbox.Name();
        bool isChecked = checkbox.IsChecked().Value();

        Layer* layer = nullptr;
        if (name == L"ExistingLayerCheckBox")
        {
            layer = m_layerManager.GetLayerByWorkState(WorkStateNative::Existing);
        }
        else if (name == L"DemolishLayerCheckBox")
        {
            layer = m_layerManager.GetLayerByWorkState(WorkStateNative::Demolish);
        }
        else if (name == L"NewLayerCheckBox")
        {
            layer = m_layerManager.GetLayerByWorkState(WorkStateNative::New);
        }

        if (layer)
        {
            layer->SetVisible(isChecked);
            InvalidateCanvas();
        }
    }

    // Обновление визуального состояния кнопок инструментов
    void MainWindow::UpdateToolButtonStates()
    {
        auto currentTool = m_viewModel.CurrentTool();

        // Сбрасываем стиль всех кнопок и устанавливаем акцентный для выбранного
        auto accentStyle = Application::Current().Resources().Lookup(
            winrt::box_value(L"AccentButtonStyle")).as<Style>();
        auto defaultStyle = Application::Current().Resources().Lookup(
            winrt::box_value(L"DefaultButtonStyle")).as<Style>();

        SelectToolButton().Style(currentTool == DrawingTool::Select ? accentStyle : defaultStyle);
        WallToolButton().Style(currentTool == DrawingTool::Wall ? accentStyle : defaultStyle);
        DoorToolButton().Style(currentTool == DrawingTool::Door ? accentStyle : defaultStyle);
        WindowToolButton().Style(currentTool == DrawingTool::Window ? accentStyle : defaultStyle);
        DimensionToolButton().Style(currentTool == DrawingTool::Dimension ? accentStyle : defaultStyle);
        
        // R2.5: Кнопки Trim и Split (если они есть в XAML, пока предполагаем что добавим их)
        if (TrimExtendToolButton())
            TrimExtendToolButton().Style(currentTool == DrawingTool::TrimExtend ? accentStyle : defaultStyle);
        if (SplitToolButton())
            SplitToolButton().Style(currentTool == DrawingTool::Split ? accentStyle : defaultStyle);

        // R6: Structural tools
        if (ColumnToolButton())
            ColumnToolButton().Style(currentTool == DrawingTool::Column ? accentStyle : defaultStyle);
        if (SlabToolButton())
            SlabToolButton().Style(currentTool == DrawingTool::Slab ? accentStyle : defaultStyle);
        if (BeamToolButton())
            BeamToolButton().Style(currentTool == DrawingTool::Beam ? accentStyle : defaultStyle);
    }

    // Перерисовка холста
    void MainWindow::InvalidateCanvas()
    {
        if (DrawingCanvas())
        {
            DrawingCanvas().Invalidate();
        }
    }

    // Обработчик отрисовки холста Win2D
    void MainWindow::OnCanvasDraw(
        [[maybe_unused]] CanvasControl const& sender,
        CanvasDrawEventArgs const& args)
    {
        auto session = args.DrawingSession();

        // Очищаем холст светло-серым фоном
        session.Clear(Windows::UI::ColorHelper::FromArgb(255, 250, 250, 250));

        // M5: Рисуем DXF-подложки (под сеткой или над — зависит от предпочтений)
        DxfReferenceRenderer::Draw(session, m_camera, m_dxfManager);

        // M5.5: Рисуем IFC-подложки
        IfcReferenceRenderer::Draw(session, m_camera, m_ifcManager);

        // Рисуем сетку
        m_gridRenderer.Draw(args, m_camera);

        // R5.2: Рисуем помещения (под стенами)
        m_roomRenderer.Draw(session, m_camera, m_document, m_hoverRoomId);

        // R6.2: Рисуем перекрытия (между помещениями и стенами)
        {
            uint64_t hoverSlabId = (m_viewModel.CurrentTool() == DrawingTool::Select) ? m_hoverWallId : 0; // Temporarily reusing simple hover or check real hover logic
            // Actually need separate hover var for slab, or generic hover var. 
            // Reuse m_hoverWallId logic if generic, but m_hoverWallId is specific.
            // Let's rely on selection highlight mostly, or implement GetHoverElement in OnPointerMoved.
            StructureRenderer::DrawSlabs(session, m_camera, m_document.GetSlabs(), 0);
        }

        // Рисуем стены
        uint64_t effectiveHoverId = m_hoverWallId;
        // R2.5: Подсветка границы при Trim/Extend
        if (m_viewModel.CurrentTool() == DrawingTool::TrimExtend && m_trimExtendTool.GetState() == TrimExtendState::PickSubject)
        {
             effectiveHoverId = m_trimExtendTool.GetBoundaryID();
        }
        m_wallRenderer.Draw(session, m_camera, m_document, m_layerManager, effectiveHoverId);

        // R4: Рисуем двери и окна (поверх стен)
        {
            // Преобразуем unique_ptr стены в shared_ptr для рендерера (временный вектор)
            std::vector<std::shared_ptr<Wall>> wallsForRender;
            for (const auto& w : m_document.GetWalls())
            {
                if (w) wallsForRender.push_back(std::shared_ptr<Wall>(w.get(), [](Wall*){}));
            }
            
            uint64_t selectedId = 0;
            if (auto sel = m_document.GetSelectedElement())
            {
                selectedId = sel->GetId();
            }
            
            m_openingRenderer.DrawDoors(
                DrawingCanvas(), session, m_camera,
                m_document.GetDoors(), wallsForRender,
                selectedId, m_hoverOpeningId);
                
            m_openingRenderer.DrawWindows(
                DrawingCanvas(), session, m_camera,
                m_document.GetWindows(), wallsForRender,
                selectedId, m_hoverOpeningId);
        }

        // R6.1: Колонны (поверх стен)
        StructureRenderer::DrawColumns(session, m_camera, m_document.GetColumns(), 0);

        // R6.5: Балки (поверх стен)
        StructureRenderer::DrawBeams(session, m_camera, m_document.GetBeams(), 0);

        // M5.6: Рисуем линии привязки для стен при использовании инструмента стены
        if (m_viewModel.CurrentTool() == DrawingTool::Wall)
        {
            for (const auto& wall : m_document.GetWalls())
            {
                if (wall)
                {
                    WallSnapRenderer::DrawWallReferenceLines(session, m_camera, *wall, m_wallSnapSystem, false);
                }
            }
        }

        // Рисуем размеры (поверх стен) если включены
        if (m_showDimensions)
        {
            m_dimensionRenderer.SetHover(m_hoverDimensionId, m_hoverHandle);
            m_dimensionRenderer.Draw(session, m_camera, m_document, m_layerManager);
        }

        // Рисуем превью стены (если рисуем)
        if (m_viewModel.CurrentTool() == DrawingTool::Wall && m_wallTool.ShouldDrawPreview())
        {
            // M5.6: Получаем текущую точку с учётом расширенной привязки к стенам
            WorldPoint endPoint = m_wallTool.GetCurrentPoint();
            if (m_currentWallSnap.IsValid)
            {
                endPoint = m_currentWallSnap.ProjectedPoint;
            }
            else if (m_currentSnap.hasSnap)
            {
                endPoint = m_currentSnap.point;
            }
            
            m_wallRenderer.DrawPreview(
                session, m_camera,
                m_wallTool.GetStartPoint(),
                endPoint,
                m_wallTool.GetThickness(),
                m_wallTool.GetWorkState(),
                m_layerManager,
                m_wallTool.IsFlipped(),
                m_wallTool.GetLocationLineMode());

            // R2: Рисуем превью соединения (угол, тип)
            if (m_previewJoin.has_value() && m_previewJoin->IsValid())
            {
                m_wallJoinRenderer.DrawJoinPreview(session, m_camera, *m_previewJoin, true);
                m_wallJoinRenderer.DrawJoinTypeBadge(session, m_camera, *m_previewJoin);
            }

            // R3: Рисуем угол от горизонтали при рисовании стены
            WorldPoint startPt = m_wallTool.GetStartPoint();
            if (startPt.Distance(endPoint) > 50.0)
            {
                m_dimensionRenderer.DrawAngleFromHorizontal(session, m_camera, startPt, endPoint);
            }
        }

        // R4: Рисуем превью двери (если инструмент двери активен)
        if (m_viewModel.CurrentTool() == DrawingTool::Door && m_doorTool.HasValidPreview())
        {
            auto hit = m_doorTool.GetPreviewHit();
            if (hit.wall)
            {
                WorldPoint wallDir = m_doorTool.GetPreviewPosition();
                wallDir = WorldPoint(
                    hit.wall->GetEndPoint().X - hit.wall->GetStartPoint().X,
                    hit.wall->GetEndPoint().Y - hit.wall->GetStartPoint().Y
                );
                double len = std::sqrt(wallDir.X * wallDir.X + wallDir.Y * wallDir.Y);
                if (len > 0.001) { wallDir.X /= len; wallDir.Y /= len; }
                
                m_openingRenderer.DrawDoorPreview(
                    session, m_camera,
                    hit.hitPoint, wallDir,
                    m_doorTool.GetWidth(),
                    hit.wall->GetThickness(),
                    true);
            }
        }

        // R4: Рисуем превью окна (если инструмент окна активен)
        if (m_viewModel.CurrentTool() == DrawingTool::Window && m_windowTool.HasValidPreview())
        {
            auto hit = m_windowTool.GetPreviewHit();
            if (hit.wall)
            {
                WorldPoint wallDir(
                    hit.wall->GetEndPoint().X - hit.wall->GetStartPoint().X,
                    hit.wall->GetEndPoint().Y - hit.wall->GetStartPoint().Y
                );
                double len = std::sqrt(wallDir.X * wallDir.X + wallDir.Y * wallDir.Y);
                if (len > 0.001) { wallDir.X /= len; wallDir.Y /= len; }
                
                m_openingRenderer.DrawWindowPreview(
                    session, m_camera,
                    hit.hitPoint, wallDir,
                    m_windowTool.GetWidth(),
                    hit.wall->GetThickness(),
                    true);
            }
        }

        // Рисуем превью размера (если рисуем)
        if (m_viewModel.CurrentTool() == DrawingTool::Dimension && m_dimensionTool.ShouldDrawPreview())
        {
            WorldPoint endPoint = m_currentSnap.hasSnap ? m_currentSnap.point : m_dimensionTool.GetCurrentPoint();
            
            m_dimensionRenderer.DrawPreview(
                session, m_camera,
                m_dimensionTool.GetStartPoint(),
                endPoint,
                m_dimensionTool.GetOffset());
        }

        // R6: Превью колонны
        if (m_viewModel.CurrentTool() == DrawingTool::Column && m_columnTool.m_previewColumn)
        {
             std::vector<std::shared_ptr<Column>> preview = { 
                 std::shared_ptr<Column>(m_columnTool.m_previewColumn.get(), [](Column*){})
             };
             StructureRenderer::DrawColumns(session, m_camera, preview, 0);
        }

        // R6: Превью перекрытия
        if (m_viewModel.CurrentTool() == DrawingTool::Slab && m_slabTool.IsActive())
        {
            CanvasPathBuilder builder(session);
            auto pts = m_slabTool.GetPreviewPoints();
             if (pts.size() > 1) {
                 ScreenPoint p0 = m_camera.WorldToScreen(pts[0]);
                 builder.BeginFigure(p0.X, p0.Y);
                 for (size_t i = 1; i < pts.size(); ++i) {
                     ScreenPoint p = m_camera.WorldToScreen(pts[i]);
                     builder.AddLine(p.X, p.Y);
                 }
                 builder.EndFigure(CanvasFigureLoop::Open);
                 auto geometry = CanvasGeometry::CreatePath(builder);
                 session.DrawGeometry(geometry, Microsoft::UI::Colors::Blue(), 1.0f);
             }
        }

        // R6.5: Превью балки
        if (m_viewModel.CurrentTool() == DrawingTool::Beam && m_beamTool.IsDrawing())
        {
            WorldPoint endPoint = m_beamTool.GetCurrentPoint();
            // TODO: Use better constructor-like preview
             // Create temporary beam for preview
             auto beam = std::make_shared<Beam>();
             beam->SetStartPoint(m_beamTool.GetStartPoint());
             beam->SetEndPoint(endPoint);
             beam->SetWidth(m_beamTool.GetWidth());
             
             std::vector<std::shared_ptr<Beam>> preview = { beam };
             StructureRenderer::DrawBeams(session, m_camera, preview, 0);
        }

        // Рисуем точку привязки
                if (m_currentSnap.hasSnap)
                {
                    m_wallRenderer.DrawSnapPoint(session, m_camera, m_currentSnap.point, true);
                }

                // M5.6: Рисуем расширенный индикатор привязки к стене
                if (m_currentWallSnap.IsValid)
                {
                    WallSnapRenderer::DrawSnapIndicator(session, m_camera, m_currentWallSnap);
                    WallSnapRenderer::DrawSnapTooltip(session, m_camera, m_currentWallSnap);
                    
                    // Если размещаем стену - показать линию соединения
                    if (m_viewModel.CurrentTool() == DrawingTool::Wall && m_wallTool.ShouldDrawPreview())
                    {
                        WallSnapRenderer::DrawSnapConnectionLine(session, m_camera, 
                            m_wallTool.GetStartPoint(), m_currentWallSnap);
                    }
                }

                // Рисуем информацию о масштабе в углу
                double zoom = m_camera.GetZoom();
                double mmPerPixel = 1.0 / zoom;
        
                wchar_t scaleText[64];
        if (mmPerPixel >= 10)
        {
            swprintf_s(scaleText, L"1 px = %.0f мм", mmPerPixel);
        }
        else if (mmPerPixel >= 1)
        {
            swprintf_s(scaleText, L"1 px = %.1f мм", mmPerPixel);
        }
        else
        {
            swprintf_s(scaleText, L"%.1f px = 1 мм", zoom);
        }

        session.DrawText(
            scaleText,
            10.0f, 10.0f,
            Windows::UI::ColorHelper::FromArgb(180, 80, 80, 80));

        // Отображаем информацию о видимых слоях
        float layerInfoY = 30.0f;
        const auto& layers = m_layerManager.GetLayers();
        for (const auto& layer : layers)
        {
            if (layer.IsVisible())
            {
                Windows::UI::Color layerColor = layer.GetColor();
                std::wstring layerName = layer.GetName();
                
                // Рисуем индикатор цвета слоя
                session.FillRectangle(
                    Windows::Foundation::Rect(10.0f, layerInfoY, 12.0f, 12.0f),
                    layerColor);
                
                // Рисуем название слоя
                session.DrawText(
                    layerName.c_str(),
                    26.0f, layerInfoY - 2.0f,
                    Windows::UI::ColorHelper::FromArgb(150, 80, 80, 80));
                
                layerInfoY += 18.0f;
            }
        }

        // Показываем количество стен
        wchar_t wallCountText[64];
        swprintf_s(wallCountText, L"Стен: %zu", m_document.GetWallCount());
        session.DrawText(
            wallCountText,
            10.0f, layerInfoY + 10.0f,
            Windows::UI::ColorHelper::FromArgb(150, 80, 80, 80));

        // M5.6: Показываем режим привязки при использовании инструмента стены
        if (m_viewModel.CurrentTool() == DrawingTool::Wall)
        {
            float canvasWidth = m_camera.GetCanvasWidth();
            WallSnapRenderer::DrawSnapModeIndicator(
                session,
                m_wallSnapSystem.GetReferenceMode(),
                canvasWidth - 180.0f, 10.0f);
        }
    }

    // Обработчик создания ресурсов холста
    void MainWindow::OnCanvasCreateResources(
        [[maybe_unused]] CanvasControl const& sender,
        [[maybe_unused]] Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesEventArgs const& args)
    {
        // Здесь можно инициализировать ресурсы (кисти, шрифты и т.д.)
    }

    // Обработчик нажатия указателя на холсте
    void MainWindow::OnCanvasPointerPressed(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& e)
    {
        auto element = sender.as<Microsoft::UI::Xaml::UIElement>();
        auto point = e.GetCurrentPoint(element);
        auto position = point.Position();

        // Запоминаем позицию для панорамирования
        m_lastPointerPosition = ScreenPoint(
            static_cast<float>(position.X), 
            static_cast<float>(position.Y));

        // Проверяем, какой кнопкой мыши нажали
        auto props = point.Properties();
        
        // Средняя или правая кнопка - панорамирование
        if (props.IsMiddleButtonPressed() || props.IsRightButtonPressed())
        {
            m_isPanning = true;
            element.CapturePointer(e.Pointer());
        }
        // Левая кнопка - действие инструмента
        else if (props.IsLeftButtonPressed())
        {
            // Преобразуем экранные координаты в мировые
            WorldPoint worldPos = m_camera.ScreenToWorld(m_lastPointerPosition);

            // M3.5: если кликаем по размеру — начинаем drag.
            // Drag доступен из Select и Dimension (когда появится инструмент), чтобы можно было быстро раздвигать размеры.
            {
                // Сначала выбираем (может попасть по линии), но drag стартуем только по ручке.
                double tolerance = 20.0;
                Element* hit = m_document.HitTest(worldPos, tolerance, m_layerManager);
                Dimension* hitDim = dynamic_cast<Dimension*>(hit);
                if (hitDim)
                {
                    // Требуем попадание по ручкам. Точность в world зависит от zoom.
                    double handleTolWorld = 10.0 / m_camera.GetZoom(); // ~10px
                    auto handleKind = hitDim->HitTestHandleKind(worldPos, handleTolWorld);
                    if (handleKind == DimensionHandle::None)
                    {
                        // Просто выделяем (без drag)
                        m_document.SetSelectedElement(hitDim);
                        UpdateSelectedElementUI();
                        InvalidateCanvas();
                    }
                    else
                    {
                    // Сейчас разрешаем drag только за среднюю ручку: это безопасно для авторазмеров.
                    // Концевые ручки зарезервированы под ручные размеры/перепривязку.
                    if (handleKind != DimensionHandle::Middle)
                    {
                        m_document.SetSelectedElement(hitDim);
                        UpdateSelectedElementUI();
                        m_hoverDimensionId = hitDim->GetId();
                        m_hoverHandle = handleKind;
                        InvalidateCanvas();
                        return;
                    }

                    // Выбираем размер
                    m_document.SetSelectedElement(hitDim);
                    UpdateSelectedElementUI();

                    m_isDraggingDimension = true;
                    m_dragDimensionId = hitDim->GetId();
                    m_dragHandle = handleKind;
                    m_dragBaseP1 = hitDim->GetP1();
                    m_dragBaseP2 = hitDim->GetP2();
                    m_dragStartOffset = hitDim->GetOffset();
                    m_dragStartWorld = worldPos;

                    // Обновляем hover, чтобы сразу подсветить активную ручку
                    m_hoverDimensionId = hitDim->GetId();
                    m_hoverHandle = handleKind;

                    element.CapturePointer(e.Pointer());
                    e.Handled(true);
                    InvalidateCanvas();
                    return;
                    }
                }
            }
            
            // Обработка текущего инструмента
            HandleToolClick(worldPos);
        }

        e.Handled(true);
    }

    // Обработка клика инструментом
    void MainWindow::HandleToolClick(const WorldPoint& worldPoint)
    {
        DrawingTool currentTool = m_viewModel.CurrentTool();

        switch (currentTool)
        {
        case DrawingTool::Select:
            {
                Element* selected = m_selectTool.OnClick(worldPoint, m_document, m_layerManager);
                UpdateSelectedElementUI();
                InvalidateCanvas();
            }
                    break;

                case DrawingTool::Wall:
                    {
                        // Устанавливаем WorkState для новой стены в зависимости от активного вида
                        PlanView activeView = m_viewModel.ActiveView();
                        WorkStateNative wallWorkState = WorkStateNative::Existing;
                
                        switch (activeView)
                        {
                        case PlanView::Measure:
                            wallWorkState = WorkStateNative::Existing;
                            break;
                        case PlanView::Demolition:
                            wallWorkState = WorkStateNative::Demolish;
                            break;
                        case PlanView::Construction:
                            wallWorkState = WorkStateNative::New;
                            break;
                        }
                
                        m_wallTool.SetWorkState(wallWorkState);
                
                        // M5.6: Используем точку привязки от wall snap системы если доступна
                        WorldPoint clickPoint = worldPoint;
                        if (m_currentWallSnap.IsValid)
                        {
                            clickPoint = m_currentWallSnap.ProjectedPoint;
                        }
                        else if (m_currentSnap.hasSnap)
                        {
                            clickPoint = m_currentSnap.point;
                        }
                
                        // Обрабатываем клик
                        bool wallCreated = m_wallTool.OnClick(
                            clickPoint, m_document, m_snapManager, m_layerManager, m_camera);
                
                        if (wallCreated)
                        {
                            UpdateSelectedElementUI();
                        }
                
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Dimension:
                    {
                        // Обрабатываем клик инструмента размера
                        bool dimCreated = m_dimensionTool.OnClick(
                            worldPoint, m_document, m_snapManager, m_layerManager, m_camera);
                
                        if (dimCreated)
                        {
                            UpdateSelectedElementUI();
                        }
                
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Door:
                    {
                        // R4: Размещение двери
                        if (m_doorTool.TryPlace())
                        {
                            m_viewModel.HasUnsavedChanges(true);
                            UpdateSelectedElementUI();
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Window:
                    {
                        // R4: Размещение окна
                        if (m_windowTool.TryPlace())
                        {
                            m_viewModel.HasUnsavedChanges(true);
                            UpdateSelectedElementUI();
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::TrimExtend:
                    {
                        // R2.5: Trim/Extend
                        if (m_trimExtendTool.HandleClick(worldPoint, m_document, m_layerManager))
                        {
                            m_viewModel.HasUnsavedChanges(true);
                            // Visual feedback is handled by InvalidateCanvas
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Split:
                    {
                        // R2.5: Split
                        if (m_splitTool.HandleClick(worldPoint, m_document, m_layerManager))
                        {
                            m_viewModel.HasUnsavedChanges(true);
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Column:
                    {
                        WorldPoint clickPoint = m_currentSnap.hasSnap ? m_currentSnap.point : worldPoint;
                        
                        // Задаём WorkState
                        PlanView activeView = m_viewModel.ActiveView();
                        WorkStateNative state = WorkStateNative::New;
                        if (activeView == PlanView::Measure) state = WorkStateNative::Existing;
                        else if (activeView == PlanView::Demolition) state = WorkStateNative::Demolish;
                        
                        m_columnTool.m_workState = state;
                        m_columnTool.UpdatePreview(clickPoint);
                        
                        if (m_columnTool.TryPlace())
                        {
                            auto col = m_columnTool.GetLastCreated();
                            if (col) m_document.AddColumn(col);
                            m_viewModel.HasUnsavedChanges(true);
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Slab:
                    {
                        WorldPoint clickPoint = m_currentSnap.hasSnap ? m_currentSnap.point : worldPoint;
                        if (m_slabTool.OnClick(clickPoint))
                        {
                            auto slab = m_slabTool.GetLastCreated();
                            if (slab) m_document.AddSlab(slab);
                            m_viewModel.HasUnsavedChanges(true);
                        }
                        InvalidateCanvas();
                    }
                    break;

                case DrawingTool::Beam:
                    {
                        WorldPoint clickPoint = m_currentSnap.hasSnap ? m_currentSnap.point : worldPoint;
                        if (m_beamTool.OnClick(clickPoint))
                        {
                            auto beam = m_beamTool.GetLastCreated();
                            // Inherit work state
                            PlanView activeView = m_viewModel.ActiveView();
                            WorkStateNative state = WorkStateNative::New;
                            if (activeView == PlanView::Measure) state = WorkStateNative::Existing;
                            else if (activeView == PlanView::Demolition) state = WorkStateNative::Demolish;
                            beam->SetWorkState(state);
                            
                            if (beam) m_document.AddBeam(beam);
                            m_viewModel.HasUnsavedChanges(true);
                        }
                        InvalidateCanvas();
                    }
                    break;

                default:
                    break;
                }
            }

            // Обновление панели свойств выбранного элемента
            void MainWindow::UpdateSelectedElementUI()
            {
        Element* selected = m_document.GetSelectedElement();

        // Защита от висячих указателей: если элемент удалён, сбрасываем выбор
        if (selected && !m_document.IsElementAlive(selected))
        {
            m_document.ClearSelection();
            selected = nullptr;
        }
        
        if (selected)
        {
            // Показываем информацию о выбранном элементе
            Wall* wall = dynamic_cast<Wall*>(selected);
            if (wall)
            {
                wchar_t info[128];
                swprintf_s(info, L"Стена (%.0f мм)", wall->GetLength());
                SelectedElementInfo().Text(info);
                
                // Показываем панель свойств стены
                WallPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
                DimensionPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);

                // 1) Тип стены
                if (WallTypeComboBox())
                {
                    auto selectedType = wall->GetType();
                    int targetIndex = -1;
                    const auto& types = m_document.GetWallTypes();
                    for (size_t i = 0; i < types.size(); ++i)
                    {
                        if (types[i] == selectedType)
                        {
                            targetIndex = static_cast<int>(i);
                            break;
                        }
                    }
                    WallTypeComboBox().SelectedIndex(targetIndex);
                }

                // 2) Состав слоёв
                if (WallLayersText())
                {
                    std::wstring layersText;
                    if (auto t = wall->GetType())
                    {
                        for (const auto& layer : t->GetLayers())
                        {
                            if (!layersText.empty())
                                layersText += L"\n";
                            layersText += layer.Name;
                            layersText += L": ";
                            wchar_t buf[64];
                            swprintf_s(buf, L"%.0f мм", layer.Thickness);
                            layersText += buf;
                            if (layer.MaterialRef)
                            {
                                layersText += L" (";
                                layersText += layer.MaterialRef->Name;
                                layersText += L")";
                            }
                        }
                    }
                    else
                    {
                        layersText = L"(нет типа)";
                    }

                    WallLayersText().Text(winrt::hstring(layersText));
                }

                // 3) Толщина (если тип задан - синхронизирована)
                WallThicknessBox().Value(wall->GetThickness());

                // 4) WorkState
                int workStateIndex = 0;
                switch (wall->GetWorkState())
                {
                case WorkStateNative::Existing: workStateIndex = 0; break;
                case WorkStateNative::Demolish: workStateIndex = 1; break;
                case WorkStateNative::New: workStateIndex = 2; break;
                }
                WorkStateComboBox().SelectedIndex(workStateIndex);

                // 5) LocationLine
                int locIndex = 0;
                switch (wall->GetLocationLineMode())
                {
                case LocationLineMode::WallCenterline: locIndex = 0; break;
                case LocationLineMode::CoreCenterline: locIndex = 1; break;
                case LocationLineMode::FinishFaceExterior: locIndex = 2; break;
                case LocationLineMode::FinishFaceInterior: locIndex = 3; break;
                case LocationLineMode::CoreFaceExterior: locIndex = 4; break;
                case LocationLineMode::CoreFaceInterior: locIndex = 5; break;
                }
                LocationLineComboBox().SelectedIndex(locIndex);
            }
            else
            {
                // Может быть размер
                Dimension* dim = dynamic_cast<Dimension*>(selected);
                if (dim)
                {
                    wchar_t info[128];
                    swprintf_s(info, L"Размер (%.0f мм)", dim->GetValueMm());
                    SelectedElementInfo().Text(info);

                    WallPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                    DimensionPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
                    DimensionLockCheckBox().IsChecked(dim->IsLocked());
                    DimensionOffsetBox().Value(dim->GetOffset());
                }
                else
                {
                    // R5.2: Проверяем, помещение ли это
                    Room* room = dynamic_cast<Room*>(selected);
                    if (room)
                    {
                        wchar_t info[256];
                        swprintf_s(info, L"Помещение %ls: %ls (%.2f м²)", 
                            room->GetNumber().c_str(),
                            room->GetName().c_str(),
                            room->GetAreaSqM());
                        SelectedElementInfo().Text(info);
                        
                        WallPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                        DimensionPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                        // TODO: RoomPropertiesPanel для R5.3
                    }
                    else
                    {
                        SelectedElementInfo().Text(L"Выбран элемент");
                        WallPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                        DimensionPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                        
                        // R6.6: Свойства колонн и перекрытий (пока просто отображаем инфо)
                        Column* col = dynamic_cast<Column*>(selected);
                        if (col)
                        {
                            wchar_t info[128];
                            swprintf_s(info, L"Колонна %ls", col->GetShape() == ColumnShape::Rectangular ? L"Прямоуг." : L"Круглая");
                            SelectedElementInfo().Text(info);
                        }
                        
                        Slab* slab = dynamic_cast<Slab*>(selected);
                        if (slab)
                        {
                            wchar_t info[128];
                            swprintf_s(info, L"Перекрытие (%.0f мм)", slab->GetThickness());
                            SelectedElementInfo().Text(info);
                        }

                        Beam* beam = dynamic_cast<Beam*>(selected);
                        if (beam)
                        {
                             wchar_t info[128];
                             swprintf_s(info, L"Балка %.0f x %.0f", beam->GetWidth(), beam->GetHeight());
                             SelectedElementInfo().Text(info);
                        }
                    }
                }
            }
        }
        else
        {
            SelectedElementInfo().Text(L"Ничего не выбрано");
            WallPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            DimensionPropertiesPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        }
    }

    void MainWindow::OnDimensionLockChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        (void)sender;
        (void)e;

        auto selected = m_document.GetSelectedElement();
        auto dim = dynamic_cast<Dimension*>(selected);
        if (!dim)
            return;

        bool isLocked = DimensionLockCheckBox().IsChecked().Value();
        dim->SetLocked(isLocked);
        m_viewModel.HasUnsavedChanges(true);
        InvalidateCanvas();
    }

    void MainWindow::OnDimensionOffsetChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e)
    {
        (void)sender;

        auto selected = m_document.GetSelectedElement();
        auto dim = dynamic_cast<Dimension*>(selected);
        if (!dim)
            return;

        dim->SetOffset(e.NewValue());
        // Чтобы offset сохранялся при перестроениях — логично зафиксировать.
        // Пользователь может снять lock вручную.
        dim->SetLocked(true);

        m_viewModel.HasUnsavedChanges(true);
        InvalidateCanvas();
    }

    void MainWindow::OnWallTypeChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        (void)sender;
        (void)e;

        auto selected = m_document.GetSelectedElement();
        auto wall = dynamic_cast<Wall*>(selected);
        if (!wall)
            return;

        int index = WallTypeComboBox().SelectedIndex();
        const auto& types = m_document.GetWallTypes();
        if (index >= 0 && static_cast<size_t>(index) < types.size())
        {
            wall->SetType(types[static_cast<size_t>(index)]);
            m_document.NotifyWallChanged(wall->GetId());
            m_viewModel.HasUnsavedChanges(true);
            UpdateSelectedElementUI();
            InvalidateCanvas();
        }
    }

    void MainWindow::OnWallThicknessChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e)
    {
        (void)sender;
        auto selected = m_document.GetSelectedElement();
        auto wall = dynamic_cast<Wall*>(selected);
        if (!wall)
            return;

        // Если пользователь меняет толщину вручную - отвязываем от типа (см. Wall::SetThickness)
        wall->SetThickness(e.NewValue());
        m_document.NotifyWallChanged(wall->GetId());
        m_viewModel.HasUnsavedChanges(true);
        UpdateSelectedElementUI();
        InvalidateCanvas();
    }

    void MainWindow::OnWallWorkStateChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        (void)sender;
        (void)e;

        auto selected = m_document.GetSelectedElement();
        auto wall = dynamic_cast<Wall*>(selected);
        if (!wall)
            return;

        int idx = WorkStateComboBox().SelectedIndex();
        WorkStateNative state = WorkStateNative::Existing;
        if (idx == 1) state = WorkStateNative::Demolish;
        else if (idx == 2) state = WorkStateNative::New;

        wall->SetWorkState(state);
        m_document.NotifyWallChanged(wall->GetId());
        m_viewModel.HasUnsavedChanges(true);
        InvalidateCanvas();
    }

    void MainWindow::OnWallLocationLineChanged(
        Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
    {
        (void)sender;
        (void)e;

        auto selected = m_document.GetSelectedElement();
        auto wall = dynamic_cast<Wall*>(selected);
        if (!wall)
            return;

        int idx = LocationLineComboBox().SelectedIndex();
        LocationLineMode mode = LocationLineMode::WallCenterline;
        switch (idx)
        {
        case 0: mode = LocationLineMode::WallCenterline; break;
        case 1: mode = LocationLineMode::CoreCenterline; break;
        case 2: mode = LocationLineMode::FinishFaceExterior; break;
        case 3: mode = LocationLineMode::FinishFaceInterior; break;
        case 4: mode = LocationLineMode::CoreFaceExterior; break;
        case 5: mode = LocationLineMode::CoreFaceInterior; break;
        }

        wall->SetLocationLineMode(mode);
        m_document.NotifyWallChanged(wall->GetId());
        m_viewModel.HasUnsavedChanges(true);
        InvalidateCanvas();
    }

    // Обработчик перемещения указателя по холсту
    void MainWindow::OnCanvasPointerMoved(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& e)
    {
        auto element = sender.as<Microsoft::UI::Xaml::UIElement>();
        auto point = e.GetCurrentPoint(element);
        auto position = point.Position();

        ScreenPoint currentPosition(
            static_cast<float>(position.X), 
            static_cast<float>(position.Y));

        // Преобразуем в мировые координаты и обновляем статус
        WorldPoint worldPos = m_camera.ScreenToWorld(currentPosition);
        m_viewModel.CursorX(worldPos.X);
        m_viewModel.CursorY(worldPos.Y);

        // Если drag'аем размер
        if (m_isDraggingDimension)
        {
            auto selected = m_document.GetSelectedElement();
            auto dim = dynamic_cast<Dimension*>(selected);
            if (dim && dim->GetId() == m_dragDimensionId)
            {
                if (m_dragHandle == DimensionHandle::Middle)
                {
                    // Пересчёт offset: проектируем вектор курсора на нормаль базовой линии P1-P2.
                    WorldPoint p1 = m_dragBaseP1;
                    WorldPoint p2 = m_dragBaseP2;
                    double dx = p2.X - p1.X;
                    double dy = p2.Y - p1.Y;
                    double len = std::sqrt(dx * dx + dy * dy);
                    if (len > 0.001)
                    {
                        double nx = -dy / len;
                        double ny = dx / len;
                        WorldPoint delta(worldPos.X - m_dragStartWorld.X, worldPos.Y - m_dragStartWorld.Y);
                        double proj = delta.X * nx + delta.Y * ny;
                        double newOffset = m_dragStartOffset + proj;
                        if (newOffset < 0.0) newOffset = 0.0;

                        // Если размер в цепочке -> обновляем всю цепочку
                        uint64_t chainId = dim->GetChainId();
                        if (chainId != 0)
                        {
                            // Обновляем все размеры с этим chainId
                            for (const auto& otherDim : m_document.GetDimensions())
                            {
                                if (otherDim && otherDim->GetChainId() == chainId)
                                {
                                    otherDim->SetOffset(newOffset);
                                    otherDim->SetLocked(true);
                                }
                            }
                        }
                        else
                        {
                            dim->SetOffset(newOffset);
                            dim->SetLocked(true);
                        }

                        DimensionOffsetBox().Value(newOffset);
                        m_viewModel.HasUnsavedChanges(true);
                    }
                }
            }

            InvalidateCanvas();
        }
        // Если панорамируем - перемещаем камеру
        else if (m_isPanning)
        {
            float deltaX = currentPosition.X - m_lastPointerPosition.X;
            float deltaY = currentPosition.Y - m_lastPointerPosition.Y;

            m_camera.Pan(deltaX, deltaY);
            m_lastPointerPosition = currentPosition;

            InvalidateCanvas();
        }
        else
        {
            // Hover ручек размеров (только когда не панорамируем и не drag'аем)
            uint64_t newHoverId = 0;
            DimensionHandle newHoverHandle = DimensionHandle::None;

            // В world-tolerance используем ~10px
            double handleTolWorld = 10.0 / m_camera.GetZoom();
            for (const auto& d : m_document.GetDimensions())
            {
                if (!d)
                    continue;
                auto kind = d->HitTestHandleKind(worldPos, handleTolWorld);
                if (kind != DimensionHandle::None)
                {
                    newHoverId = d->GetId();
                    newHoverHandle = kind;
                    break;
                }
                        }

                        if (newHoverId != m_hoverDimensionId || newHoverHandle != m_hoverHandle)
                        {
                            m_hoverDimensionId = newHoverId;
                            m_hoverHandle = newHoverHandle;

                            InvalidateCanvas();
                        }

                        // M9: Hover над стенами (для инструмента Select)
                        if (m_viewModel.CurrentTool() == DrawingTool::Select)
                        {
                            uint64_t newHoverWallId = 0;
                            uint64_t newHoverRoomId = 0;
                            double hitTolerance = 5.0 / m_camera.GetZoom();
                            
                            // Сначала проверяем стены (приоритет)
                            for (const auto& wall : m_document.GetWalls())
                            {
                                if (wall && wall->HitTest(worldPos, hitTolerance))
                                {
                                    newHoverWallId = wall->GetId();
                                    break;
                                }
                            }
                            
                            // R5.2: Если не попали в стену, проверяем помещения
                            if (newHoverWallId == 0)
                            {
                                for (const auto& room : m_document.GetRooms())
                                {
                                    if (room && room->HitTest(worldPos, hitTolerance))
                                    {
                                        newHoverRoomId = room->GetId();
                                        break;
                                    }
                                }
                            }
                            
                            bool needRedraw = false;
                            if (newHoverWallId != m_hoverWallId)
                            {
                                m_hoverWallId = newHoverWallId;
                                needRedraw = true;
                            }
                            if (newHoverRoomId != m_hoverRoomId)
                            {
                                m_hoverRoomId = newHoverRoomId;
                                needRedraw = true;
                            }
                            
                            if (needRedraw)
                                InvalidateCanvas();
                        }

                        // Обновляем привязку и превью для инструмента стены
                        if (m_viewModel.CurrentTool() == DrawingTool::Wall)
                        {
                            // Ищем точку привязки (базовая)
                            m_currentSnap = m_snapManager.FindSnap(
                                worldPos, m_document, m_layerManager, m_camera);

                            // M5.6: Расширенная привязка к стенам
                            m_currentWallSnap = m_wallSnapSystem.FindBestSnap(
                                worldPos, m_document.GetWalls(), m_camera.GetZoom());
                            
                            // Обновляем индикатор режима в статусной строке
                            if (SnapIndicatorText())
                            {
                                if (m_currentWallSnap.IsValid)
                                {
                                    std::wstring snapText = WallSnapSystem::GetSnapPlaneShortName(m_currentWallSnap.Plane);
                                    SnapIndicatorText().Text(winrt::hstring(snapText));
                                }
                                else
                                {
                                    SnapIndicatorText().Text(L"");
                                }
                            }

                            // Обновляем текущую точку инструмента
                            m_wallTool.OnMouseMove(worldPos);

                            // R2: Обновляем превью соединения
                            if (m_wallTool.ShouldDrawPreview())
                            {
                                WorldPoint endPt = m_currentWallSnap.IsValid ? m_currentWallSnap.ProjectedPoint :
                                                   (m_currentSnap.hasSnap ? m_currentSnap.point : worldPos);
                                m_previewJoin = m_wallJoinSystem.FindPreviewJoin(
                                    m_wallTool.GetStartPoint(),
                                    endPt,
                                    m_wallTool.GetThickness(),
                                    m_document.GetWalls());
                            }
                            else
                            {
                                m_previewJoin = std::nullopt;
                            }

                            // Перерисовываем для обновления превью
                            InvalidateCanvas();
                        }
                        // Обновляем привязку и превью для инструмента размера
                        else if (m_viewModel.CurrentTool() == DrawingTool::Dimension)
                        {
                            // Ищем точку привязки
                            m_currentSnap = m_snapManager.FindSnap(
                                worldPos, m_document, m_layerManager, m_camera);

                            // Обновляем текущую точку инструмента
                            m_dimensionTool.OnMouseMove(worldPos);

                            // Перерисовываем для обновления превью
                            InvalidateCanvas();
                        }
                        // R4: Обновляем превью для инструмента двери
                        else if (m_viewModel.CurrentTool() == DrawingTool::Door)
                        {
                            // Преобразуем unique_ptr в shared_ptr для инструмента
                            std::vector<std::shared_ptr<Wall>> wallsForTool;
                            for (const auto& w : m_document.GetWalls())
                            {
                                if (w) wallsForTool.push_back(std::shared_ptr<Wall>(w.get(), [](Wall*){}));
                            }
                            m_doorTool.UpdatePreview(worldPos, wallsForTool);
                            InvalidateCanvas();
                        }
                        // R4: Обновляем превью для инструмента окна
                        else if (m_viewModel.CurrentTool() == DrawingTool::Window)
                        {
                            std::vector<std::shared_ptr<Wall>> wallsForTool;
                            for (const auto& w : m_document.GetWalls())
                            {
                                if (w) wallsForTool.push_back(std::shared_ptr<Wall>(w.get(), [](Wall*){}));
                            }
                            m_windowTool.UpdatePreview(worldPos, wallsForTool);
                            InvalidateCanvas();
                        }
                        else if (m_viewModel.CurrentTool() == DrawingTool::TrimExtend)
                        {
                            m_trimExtendTool.OnMouseMove(worldPos);
                            InvalidateCanvas();
                        }
                        else if (m_viewModel.CurrentTool() == DrawingTool::Split)
                        {
                            m_splitTool.OnMouseMove(worldPos);
                            InvalidateCanvas();
                        }
                        else if (m_viewModel.CurrentTool() == DrawingTool::Column)
                        {
                            m_currentSnap = m_snapManager.FindSnap(worldPos, m_document, m_layerManager, m_camera);
                            WorldPoint pos = m_currentSnap.hasSnap ? m_currentSnap.point : worldPos;
                            m_columnTool.UpdatePreview(pos);
                            InvalidateCanvas();
                        }
                        else if (m_viewModel.CurrentTool() == DrawingTool::Slab)
                        {
                            m_currentSnap = m_snapManager.FindSnap(worldPos, m_document, m_layerManager, m_camera);
                            WorldPoint pos = m_currentSnap.hasSnap ? m_currentSnap.point : worldPos;
                            m_slabTool.OnMouseMove(pos);
                            InvalidateCanvas();
                        }
                        else if (m_viewModel.CurrentTool() == DrawingTool::Beam)
                        {
                            m_currentSnap = m_snapManager.FindSnap(worldPos, m_document, m_layerManager, m_camera);
                            WorldPoint pos = m_currentSnap.hasSnap ? m_currentSnap.point : worldPos;
                            m_beamTool.OnMouseMove(pos);
                            InvalidateCanvas();
                        }
                        else
                        {
                            // Сбрасываем привязку если не рисуем
                            m_currentSnap.hasSnap = false;
                        }
                    }

                    e.Handled(true);
                }

                // Обработчик отпускания указателя
                void MainWindow::OnCanvasPointerReleased(
                    Windows::Foundation::IInspectable const& sender,
                    PointerRoutedEventArgs const& e)
                {
                    auto element = sender.as<Microsoft::UI::Xaml::UIElement>();

        if (m_isDraggingDimension)
        {
            m_isDraggingDimension = false;
            m_dragDimensionId = 0;
            m_dragHandle = DimensionHandle::None;
            element.ReleasePointerCapture(e.Pointer());
            e.Handled(true);
            return;
        }

        if (m_isPanning)
        {
            m_isPanning = false;
            element.ReleasePointerCapture(e.Pointer());
        }

        e.Handled(true);
    }

    // Обработчик прокрутки колёсика мыши (масштабирование)
    void MainWindow::OnCanvasPointerWheelChanged(
        Windows::Foundation::IInspectable const& sender,
        PointerRoutedEventArgs const& e)
    {
        auto element = sender.as<Microsoft::UI::Xaml::UIElement>();
        auto point = e.GetCurrentPoint(element);
        auto position = point.Position();
        auto props = point.Properties();

        // Получаем дельту прокрутки
        int delta = props.MouseWheelDelta();

        // Вычисляем коэффициент масштабирования
        double zoomFactor = delta > 0 ? 1.15 : (1.0 / 1.15);

        // Масштабируем относительно позиции курсора
        ScreenPoint screenPoint(
            static_cast<float>(position.X), 
            static_cast<float>(position.Y));
        
        m_camera.ZoomAt(screenPoint, zoomFactor);

        // Обновляем координаты курсора (после масштабирования мировая позиция изменится)
        WorldPoint worldPos = m_camera.ScreenToWorld(screenPoint);
        m_viewModel.CursorX(worldPos.X);
        m_viewModel.CursorY(worldPos.Y);

        InvalidateCanvas();
        e.Handled(true);
    }

    // Обработчик изменения размера холста
        void MainWindow::OnCanvasSizeChanged(
            [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
            SizeChangedEventArgs const& e)
        {
            auto newSize = e.NewSize();
            m_camera.SetCanvasSize(
                static_cast<float>(newSize.Width), 
                static_cast<float>(newSize.Height));

            InvalidateCanvas();
        }

        // Обработчик клавиатуры (M4: Spacebar flip, Escape cancel)
        void MainWindow::OnCanvasKeyDown(
            [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
        {
            auto key = e.Key();

            // Spacebar - переворот стены при рисовании
            if (key == Windows::System::VirtualKey::Space)
            {
                if (m_viewModel.CurrentTool() == DrawingTool::Wall && m_wallTool.ShouldDrawPreview())
                {
                    m_wallTool.ToggleFlip();
                    InvalidateCanvas();
                    e.Handled(true);
                }
            }
            // Tab - цикл режима привязки к стенам (M5.6)
            else if (key == Windows::System::VirtualKey::Tab)
            {
                if (m_viewModel.CurrentTool() == DrawingTool::Wall)
                {
                    m_wallSnapSystem.CycleReferenceMode();
                    
                    // Обновляем ComboBox если доступен
                    if (SnapModeComboBox())
                    {
                        int modeIndex = static_cast<int>(m_wallSnapSystem.GetReferenceMode());
                        SnapModeComboBox().SelectedIndex(modeIndex);
                    }
                    
                    // Показываем индикатор текущего режима
                    if (SnapIndicatorText())
                    {
                        std::wstring modeName = WallSnapSystem::GetReferenceModeName(m_wallSnapSystem.GetReferenceMode());
                        SnapIndicatorText().Text(winrt::hstring(L"[" + modeName + L"]"));
                    }
                    
                    InvalidateCanvas();
                    e.Handled(true);
                }
            }
            // Escape - отмена текущей операции
            else if (key == Windows::System::VirtualKey::Escape)
            {
                if (m_viewModel.CurrentTool() == DrawingTool::Wall)
                {
                    m_wallTool.Cancel();
                    InvalidateCanvas();
                    e.Handled(true);
                }
                else if (m_viewModel.CurrentTool() == DrawingTool::Dimension)
                {
                    m_dimensionTool.Cancel();
                    InvalidateCanvas();
                    e.Handled(true);
                }
            }
            // V - инструмент выбора
            else if (key == Windows::System::VirtualKey::V)
            {
                m_wallTool.Cancel();
                m_dimensionTool.Cancel();
                m_viewModel.CurrentTool(DrawingTool::Select);
                UpdateToolButtonStates();
                InvalidateCanvas();
                e.Handled(true);
            }
            // W - инструмент стены
            else if (key == Windows::System::VirtualKey::W)
            {
                m_dimensionTool.Cancel();
                m_viewModel.CurrentTool(DrawingTool::Wall);
                UpdateToolButtonStates();
                InvalidateCanvas();
                e.Handled(true);
            }
            // R - инструмент размера
            else if (key == Windows::System::VirtualKey::R)
            {
                m_wallTool.Cancel();
                m_viewModel.CurrentTool(DrawingTool::Dimension);
                UpdateToolButtonStates();
                InvalidateCanvas();
                e.Handled(true);
            }
            // D - инструмент двери (заглушка)
            else if (key == Windows::System::VirtualKey::D)
            {
                m_wallTool.Cancel();
                m_dimensionTool.Cancel();
                m_viewModel.CurrentTool(DrawingTool::Door);
                UpdateToolButtonStates();
                InvalidateCanvas();
                e.Handled(true);
            }
            // O - инструмент окна (заглушка)
            else if (key == Windows::System::VirtualKey::O)
            {
                m_wallTool.Cancel();
                m_dimensionTool.Cancel();
                m_viewModel.CurrentTool(DrawingTool::Window);
                UpdateToolButtonStates();
                InvalidateCanvas();
                e.Handled(true);
            }
            // Delete - удаление выбранного элемента
            else if (key == Windows::System::VirtualKey::Delete)
            {
                Element* selected = m_document.GetSelectedElement();
                if (selected)
                {
                    // Проверяем тип элемента
                    Wall* wall = dynamic_cast<Wall*>(selected);
                    if (wall)
                    {
                        m_document.RemoveWall(wall->GetId());
                        m_document.ClearSelection();
                        m_viewModel.HasUnsavedChanges(true);
                        UpdateSelectedElementUI();
                        InvalidateCanvas();
                        e.Handled(true);
                    }
                    else
                    {
                        Dimension* dim = dynamic_cast<Dimension*>(selected);
                        if (dim && dim->IsManual())
                        {
                            m_document.RemoveManualDimension(dim->GetId());
                            m_document.ClearSelection();
                            m_viewModel.HasUnsavedChanges(true);
                            UpdateSelectedElementUI();
                            InvalidateCanvas();
                            e.Handled(true);
                        }
                    }
                }
            }
        }

        // ============================================================================
        // M5.6: Привязка к стенам (Wall Snap System)
        // ============================================================================

        void MainWindow::OnSnapModeChanged(
            [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e)
        {
            // Проверяем, что ComboBox инициализирован
            if (!SnapModeComboBox())
                return;

            int index = SnapModeComboBox().SelectedIndex();
            if (index < 0)
                return;

            SnapReferenceMode mode = SnapReferenceMode::Auto;

            switch (index)
            {
            case 0: mode = SnapReferenceMode::Auto; break;
            case 1: mode = SnapReferenceMode::Centerline; break;
            case 2: mode = SnapReferenceMode::FinishExterior; break;
            case 3: mode = SnapReferenceMode::FinishInterior; break;
            case 4: mode = SnapReferenceMode::CoreExterior; break;
            case 5: mode = SnapReferenceMode::CoreInterior; break;
            }

            m_wallSnapSystem.SetReferenceMode(mode);
            InvalidateCanvas();
        }

        // ============================================================================
        // M5: Импорт DXF
        // ============================================================================

        void MainWindow::OnImportDxfClick(
            [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
        {
            auto xamlRoot = Content().XamlRoot();

            // Асинхронный диалог выбора файла с HWND, полученным от окна
            // Keep a strong ref to the projected window across co_await.
            auto asyncOp = [window = winrt::estimate1::MainWindow(*this), xamlRoot]() -> winrt::Windows::Foundation::IAsyncAction
            {
                // Получаем HWND через IWindowNative от проекции окна
                HWND hwnd{ nullptr };
                auto windowNative = window.try_as<IWindowNative>();
                if (windowNative)
                {
                    windowNative->get_WindowHandle(&hwnd);
                }

                auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();

                if (hwnd)
                {
                    auto init = picker.as<IInitializeWithWindow>();
                    if (init)
                    {
                        init->Initialize(hwnd);
                    }
                }

                picker.FileTypeFilter().Append(L".dxf");
                picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
                picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::List);

                winrt::Windows::Storage::StorageFile file{ nullptr };
                try
                {
                    file = co_await picker.PickSingleFileAsync();
                }
                catch (...)
                {
                    co_return;
                }

                if (file)
                {
                    std::wstring filePath = file.Path().c_str();
                    // Call implementation safely via captured XamlRoot (no Content()/this access needed after picker completes).
                    auto impl = winrt::get_self<winrt::estimate1::implementation::MainWindow>(window);
                    co_await impl->ShowDxfImportDialogWithXamlRootAsync(filePath, xamlRoot);
                }
            };
            asyncOp();
        }

        winrt::Windows::Foundation::IAsyncAction MainWindow::ShowDxfImportDialogAsync(const std::wstring& filePath)
        {
            co_await ShowDxfImportDialogWithXamlRootAsync(filePath, Content().XamlRoot());
        }

        winrt::Windows::Foundation::IAsyncAction MainWindow::ShowDxfImportDialogWithXamlRootAsync(
            const std::wstring& filePath,
            Microsoft::UI::Xaml::XamlRoot const& xamlRoot)
        {
            auto parseResult = DxfParser::ParseFile(filePath);

            if (!parseResult.Success || !parseResult.Document)
            {
                ContentDialog errorDialog;
                errorDialog.XamlRoot(xamlRoot);
                errorDialog.Title(winrt::box_value(L"Ошибка импорта DXF"));
                errorDialog.Content(winrt::box_value(winrt::hstring(parseResult.ErrorMessage)));
                errorDialog.CloseButtonText(L"OK");
                co_await errorDialog.ShowAsync();
                co_return;
            }

            auto& doc = parseResult.Document;

            // Формируем информацию о файле
            std::wstring infoText = L"Найдено сущностей: " + std::to_wstring(doc->TotalEntityCount);
            infoText += L"\n\nЛиний: " + std::to_wstring(doc->LineCount);
            infoText += L"\nПолилиний: " + std::to_wstring(doc->PolylineCount);
            infoText += L"\nОкружностей: " + std::to_wstring(doc->CircleCount);
            infoText += L"\nДуг: " + std::to_wstring(doc->ArcCount);
            infoText += L"\nТекстов: " + std::to_wstring(doc->TextCount);

            if (doc->HasBounds)
            {
                wchar_t boundsText[256];
                swprintf_s(boundsText, L"\n\nРазмеры: %.0f ? %.0f",
                    doc->MaxBounds.X - doc->MinBounds.X,
                    doc->MaxBounds.Y - doc->MinBounds.Y);
                infoText += boundsText;
            }

            // Единицы
            std::wstring unitsName = L"Не указаны";
            switch (doc->Units)
            {
            case 1: unitsName = L"Дюймы"; break;
            case 2: unitsName = L"Футы"; break;
            case 4: unitsName = L"Миллиметры"; break;
            case 5: unitsName = L"Сантиметры"; break;
            case 6: unitsName = L"Метры"; break;
            }
            infoText += L"\nЕдиницы: " + unitsName;

            // Создаём диалог с настройками импорта
            ContentDialog importDialog;
            importDialog.XamlRoot(xamlRoot);
            importDialog.Title(winrt::box_value(L"Импорт DXF"));
            importDialog.PrimaryButtonText(L"Импортировать");
            importDialog.CloseButtonText(L"Отмена");
            importDialog.DefaultButton(ContentDialogButton::Primary);

            // Создаём контент диалога
            StackPanel panel;
            panel.Spacing(8);

            // Информация о файле
            TextBlock infoBlock;
            infoBlock.Text(winrt::hstring(infoText));
            infoBlock.TextWrapping(TextWrapping::Wrap);
            panel.Children().Append(infoBlock);

            // Разделитель
            Border separator;
            separator.Height(1);
            separator.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200)));
            separator.Margin(ThicknessHelper::FromLengths(0, 8, 0, 8));
            panel.Children().Append(separator);

            // Масштаб
            TextBlock scaleLabel;
            scaleLabel.Text(L"Масштаб импорта:");
            panel.Children().Append(scaleLabel);

            NumberBox scaleBox;
            scaleBox.Value(1.0);
            scaleBox.Minimum(0.001);
            scaleBox.Maximum(1000.0);
            scaleBox.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Compact);
            panel.Children().Append(scaleBox);

            // Единицы
            TextBlock unitsLabel;
            unitsLabel.Text(L"Единицы исходного файла:");
            panel.Children().Append(unitsLabel);

            ComboBox unitsCombo;
            unitsCombo.Items().Append(winrt::box_value(L"Авто (из файла)"));
            unitsCombo.Items().Append(winrt::box_value(L"Миллиметры"));
            unitsCombo.Items().Append(winrt::box_value(L"Сантиметры"));
            unitsCombo.Items().Append(winrt::box_value(L"Метры"));
            unitsCombo.Items().Append(winrt::box_value(L"Дюймы"));
            unitsCombo.Items().Append(winrt::box_value(L"Футы"));
            unitsCombo.SelectedIndex(0);
            unitsCombo.HorizontalAlignment(HorizontalAlignment::Stretch);
            panel.Children().Append(unitsCombo);

            // Прозрачность
            TextBlock opacityLabel;
            opacityLabel.Text(L"Прозрачность подложки:");
            panel.Children().Append(opacityLabel);

            Slider opacitySlider;
            opacitySlider.Minimum(10);
            opacitySlider.Maximum(255);
            opacitySlider.Value(128);
            opacitySlider.HorizontalAlignment(HorizontalAlignment::Stretch);
            panel.Children().Append(opacitySlider);

            importDialog.Content(panel);

            // Показываем диалог
            auto result = co_await importDialog.ShowAsync();

            if (result == ContentDialogResult::Primary)
            {
                // Получаем настройки
                DxfImportSettings settings;
                settings.Scale = scaleBox.Value();

                // Единицы
                int unitsIndex = unitsCombo.SelectedIndex();
                switch (unitsIndex)
                {
                case 0: settings.OverrideUnits = -1; break;  // Авто
                case 1: settings.OverrideUnits = 4; break;   // мм
                case 2: settings.OverrideUnits = 5; break;   // см
                case 3: settings.OverrideUnits = 6; break;   // м
                case 4: settings.OverrideUnits = 1; break;   // дюймы
                case 5: settings.OverrideUnits = 2; break;   // футы
                }

                // Импортируем
                auto importResult = m_dxfManager.ImportFile(filePath, settings);

                if (importResult.Success)
                {
                    // Устанавливаем прозрачность
                    if (auto* layer = m_dxfManager.GetLayer(importResult.LayerIndex))
                    {
                        layer->SetOpacity(static_cast<uint8_t>(opacitySlider.Value()));
                    }

                    // Центрируем вид на импортированной геометрии
                    WorldPoint minPt, maxPt;
                    if (m_dxfManager.GetCombinedBounds(minPt, maxPt))
                    {
                        WorldPoint center{
                            (minPt.X + maxPt.X) / 2.0,
                            (minPt.Y + maxPt.Y) / 2.0
                        };

                        // Сбрасываем камеру на центр импортированной геометрии
                        m_camera.SetOffset(-center.X, -center.Y);

                        // Подбираем масштаб, чтобы вписать геометрию
                        double width = maxPt.X - minPt.X;
                        double height = maxPt.Y - minPt.Y;
                        if (width > 0 && height > 0)
                        {
                            double zoomX = m_camera.GetCanvasWidth() / width * 0.8;
                            double zoomY = m_camera.GetCanvasHeight() / height * 0.8;
                            double zoom = (std::min)(zoomX, zoomY);
                            zoom = (std::max)(zoom, 0.01);
                            zoom = (std::min)(zoom, 10.0);
                            m_camera.SetZoom(zoom);
                        }
                    }

                    m_viewModel.HasUnsavedChanges(true);
                    InvalidateCanvas();

                    // Показываем сообщение об успехе
                    wchar_t successMsg[256];
                    swprintf_s(successMsg, L"Импортировано %zu сущностей", importResult.EntityCount);

                    ContentDialog successDialog;
                    successDialog.XamlRoot(xamlRoot);
                    successDialog.Title(winrt::box_value(L"Импорт завершён"));
                    successDialog.Content(winrt::box_value(winrt::hstring(successMsg)));
                    successDialog.CloseButtonText(L"OK");
                    co_await successDialog.ShowAsync();
                }
                else
                {
                    ContentDialog errorDialog;
                    errorDialog.XamlRoot(xamlRoot);
                    errorDialog.Title(winrt::box_value(L"Ошибка импорта"));
                    errorDialog.Content(winrt::box_value(winrt::hstring(importResult.ErrorMessage)));
                    errorDialog.CloseButtonText(L"OK");
                    co_await errorDialog.ShowAsync();
                }
            }
        }

        // ============================================================================
        // M5.5: Импорт IFC
        // ============================================================================

        void MainWindow::OnImportIfcClick(
            [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
        {
            auto xamlRoot = Content().XamlRoot();

            // Асинхронный диалог выбора файла
            auto asyncOp = [window = winrt::estimate1::MainWindow(*this), xamlRoot]() -> winrt::Windows::Foundation::IAsyncAction
            {
                // Получаем HWND
                HWND hwnd{ nullptr };
                auto windowNative = window.try_as<IWindowNative>();
                if (windowNative)
                {
                    windowNative->get_WindowHandle(&hwnd);
                }

                auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();

                if (hwnd)
                {
                    auto init = picker.as<IInitializeWithWindow>();
                    if (init)
                    {
                        init->Initialize(hwnd);
                    }
                }

                picker.FileTypeFilter().Append(L".ifc");
                picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
                picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::List);

                winrt::Windows::Storage::StorageFile file{ nullptr };
                try
                {
                    file = co_await picker.PickSingleFileAsync();
                }
                catch (...)
                {
                    co_return;
                }

                if (file)
                {
                    std::wstring filePath = file.Path().c_str();
                    auto impl = winrt::get_self<winrt::estimate1::implementation::MainWindow>(window);
                    co_await impl->ShowIfcImportDialogAsync(filePath);
                }
            };
            asyncOp();
        }

        winrt::Windows::Foundation::IAsyncAction MainWindow::ShowIfcImportDialogAsync(const std::wstring& filePath)
        {
            auto xamlRoot = Content().XamlRoot();

            // Парсим файл
            auto parseResult = IfcParser::ParseFile(filePath);

            if (!parseResult.Success || !parseResult.Document)
            {
                ContentDialog errorDialog;
                errorDialog.XamlRoot(xamlRoot);
                errorDialog.Title(winrt::box_value(L"Ошибка импорта IFC"));
                errorDialog.Content(winrt::box_value(winrt::hstring(parseResult.ErrorMessage)));
                errorDialog.CloseButtonText(L"OK");
                co_await errorDialog.ShowAsync();
                co_return;
            }

            auto& doc = parseResult.Document;

            // Формируем информацию о файле
            std::wstring infoText = L"Схема: " + doc->Schema;
            if (!doc->ProjectName.empty())
            {
                infoText += L"\nПроект: " + doc->ProjectName;
            }
            
            infoText += L"\n\nНайдено элементов:";
            infoText += L"\n  Стен: " + std::to_wstring(doc->Walls.size());
            infoText += L"\n  Дверей: " + std::to_wstring(doc->Doors.size());
            infoText += L"\n  Окон: " + std::to_wstring(doc->Windows.size());
            infoText += L"\n  Помещений: " + std::to_wstring(doc->Spaces.size());
            infoText += L"\n  Этажей: " + std::to_wstring(doc->Storeys.size());
            infoText += L"\n  Всего сущностей: " + std::to_wstring(doc->TotalEntityCount);

            if (doc->HasBounds)
            {
                wchar_t boundsText[256];
                swprintf_s(boundsText, L"\n\nРазмеры: %.0f ? %.0f мм",
                    doc->MaxBounds.X - doc->MinBounds.X,
                    doc->MaxBounds.Y - doc->MinBounds.Y);
                infoText += boundsText;
            }

            // Единицы
            infoText += L"\nЕдиницы: " + (doc->LengthUnitName.empty() ? L"мм" : doc->LengthUnitName);
            infoText += L" (масштаб: " + std::to_wstring(doc->LengthUnitScale) + L")";

            // Создаём диалог с настройками импорта
            ContentDialog importDialog;
            importDialog.XamlRoot(xamlRoot);
            importDialog.Title(winrt::box_value(L"Импорт IFC"));
            importDialog.PrimaryButtonText(L"Импортировать");
            importDialog.CloseButtonText(L"Отмена");
            importDialog.DefaultButton(ContentDialogButton::Primary);

            // Создаём контент диалога
            StackPanel panel;
            panel.Spacing(8);

            // Информация о файле
            TextBlock infoBlock;
            infoBlock.Text(winrt::hstring(infoText));
            infoBlock.TextWrapping(TextWrapping::Wrap);
            panel.Children().Append(infoBlock);

            // Разделитель
            Border separator;
            separator.Height(1);
            separator.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200)));
            separator.Margin(ThicknessHelper::FromLengths(0, 8, 0, 8));
            panel.Children().Append(separator);

            // Чекбоксы для типов элементов
            TextBlock elementsLabel;
            elementsLabel.Text(L"Импортировать:");
            panel.Children().Append(elementsLabel);

            CheckBox wallsCheck;
            wallsCheck.Content(winrt::box_value(L"Стены"));
            wallsCheck.IsChecked(true);
            panel.Children().Append(wallsCheck);

            CheckBox doorsCheck;
            doorsCheck.Content(winrt::box_value(L"Двери"));
            doorsCheck.IsChecked(true);
            panel.Children().Append(doorsCheck);

            CheckBox windowsCheck;
            windowsCheck.Content(winrt::box_value(L"Окна"));
            windowsCheck.IsChecked(true);
            panel.Children().Append(windowsCheck);

            CheckBox spacesCheck;
            spacesCheck.Content(winrt::box_value(L"Помещения"));
            spacesCheck.IsChecked(true);
            panel.Children().Append(spacesCheck);

            // Разделитель
            Border separator2;
            separator2.Height(1);
            separator2.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200)));
            separator2.Margin(ThicknessHelper::FromLengths(0, 8, 0, 8));
            panel.Children().Append(separator2);

            // Масштаб
            TextBlock scaleLabel;
            scaleLabel.Text(L"Дополнительный масштаб:");
            panel.Children().Append(scaleLabel);

            NumberBox scaleBox;
            scaleBox.Value(1.0);
            scaleBox.Minimum(0.001);
            scaleBox.Maximum(1000.0);
            scaleBox.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Compact);
            panel.Children().Append(scaleBox);

            // Прозрачность
            TextBlock opacityLabel;
            opacityLabel.Text(L"Прозрачность подложки:");
            panel.Children().Append(opacityLabel);

            Slider opacitySlider;
            opacitySlider.Minimum(10);
            opacitySlider.Maximum(255);
            opacitySlider.Value(180);
            opacitySlider.HorizontalAlignment(HorizontalAlignment::Stretch);
            panel.Children().Append(opacitySlider);

            importDialog.Content(panel);

            // Показываем диалог
            auto result = co_await importDialog.ShowAsync();

            if (result == ContentDialogResult::Primary)
            {
                // Получаем настройки
                IfcImportSettings settings;
                settings.Scale = scaleBox.Value();
                settings.ImportWalls = wallsCheck.IsChecked().Value();
                settings.ImportDoors = doorsCheck.IsChecked().Value();
                settings.ImportWindows = windowsCheck.IsChecked().Value();
                settings.ImportSpaces = spacesCheck.IsChecked().Value();

                // Импортируем
                auto importResult = m_ifcManager.ImportFile(filePath, settings);

                if (importResult.Success)
                {
                    // Устанавливаем прозрачность
                    if (auto* layer = m_ifcManager.GetLayer(importResult.LayerIndex))
                    {
                        layer->SetOpacity(static_cast<uint8_t>(opacitySlider.Value()));
                        layer->SetShowSpaces(settings.ImportSpaces);
                    }

                    // Центрируем вид на импортированной геометрии
                    if (auto* layer = m_ifcManager.GetLayer(importResult.LayerIndex))
                    {
                        WorldPoint minPt = layer->GetMinBounds();
                        WorldPoint maxPt = layer->GetMaxBounds();

                        if (maxPt.X > minPt.X && maxPt.Y > minPt.Y)
                        {
                            WorldPoint center{
                                (minPt.X + maxPt.X) / 2.0,
                                (minPt.Y + maxPt.Y) / 2.0
                            };

                            // Сбрасываем камеру на центр импортированной геометрии
                            m_camera.SetOffset(-center.X, -center.Y);

                            // Подбираем масштаб
                            double width = maxPt.X - minPt.X;
                            double height = maxPt.Y - minPt.Y;
                            if (width > 0 && height > 0)
                            {
                                double zoomX = m_camera.GetCanvasWidth() / width * 0.8;
                                double zoomY = m_camera.GetCanvasHeight() / height * 0.8;
                                double zoom = (std::min)(zoomX, zoomY);
                                zoom = (std::max)(zoom, 0.01);
                                zoom = (std::min)(zoom, 10.0);
                                m_camera.SetZoom(zoom);
                            }
                        }
                    }

                    m_viewModel.HasUnsavedChanges(true);
                    InvalidateCanvas();

                    // Показываем сообщение об успехе
                    wchar_t successMsg[512];
                    swprintf_s(successMsg, 
                        L"Импортировано:\n  Стен: %zu\n  Дверей: %zu\n  Окон: %zu\n  Помещений: %zu\n\nВсего сущностей: %zu",
                        importResult.WallCount,
                        importResult.DoorCount,
                        importResult.WindowCount,
                        importResult.SpaceCount,
                        importResult.TotalEntityCount);

                    ContentDialog successDialog;
                    successDialog.XamlRoot(xamlRoot);
                    successDialog.Title(winrt::box_value(L"Импорт IFC завершён"));
                    successDialog.Content(winrt::box_value(winrt::hstring(successMsg)));
                    successDialog.CloseButtonText(L"OK");
                    co_await successDialog.ShowAsync();
                }
                else
                {
                    ContentDialog errorDialog;
                    errorDialog.XamlRoot(xamlRoot);
                    errorDialog.Title(winrt::box_value(L"Ошибка импорта IFC"));
                    errorDialog.Content(winrt::box_value(winrt::hstring(importResult.ErrorMessage)));
                    errorDialog.CloseButtonText(L"OK");
                    co_await errorDialog.ShowAsync();
                }
            }
        }
    

    // =========================================================================
    // M6: Сохранение/загрузка проекта
    // =========================================================================

    void MainWindow::OnNewProjectClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        ConfirmNewProjectAsync();
    }

    void MainWindow::OnOpenProjectClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        OpenProjectAsync();
    }

    void MainWindow::OnSaveProjectClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        SaveProjectAsync(false);
    }

    void MainWindow::OnSaveProjectAsClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        SaveProjectAsync(true);
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::ConfirmNewProjectAsync()
    {
        auto xamlRoot = this->Content().XamlRoot();
        if (!xamlRoot)
            co_return;

        // Если есть несохранённые изменения — спрашиваем
        if (m_viewModel.HasUnsavedChanges())
        {
            ContentDialog confirmDialog;
            confirmDialog.XamlRoot(xamlRoot);
            confirmDialog.Title(winrt::box_value(L"Несохранённые изменения"));
            confirmDialog.Content(winrt::box_value(L"В проекте есть несохранённые изменения.\nСохранить перед созданием нового?"));
            confirmDialog.PrimaryButtonText(L"Сохранить");
            confirmDialog.SecondaryButtonText(L"Не сохранять");
            confirmDialog.CloseButtonText(L"Отмена");
            confirmDialog.DefaultButton(ContentDialogButton::Primary);

            auto result = co_await confirmDialog.ShowAsync();
            
            if (result == ContentDialogResult::Primary)
            {
                co_await SaveProjectAsync(false);
            }
            else if (result == ContentDialogResult::None)
            {
                co_return; // Отмена
            }
        }

        // Очищаем документ
        m_document.Clear();
        m_dxfManager.Clear();
        m_ifcManager.Clear();
        
        // Сбрасываем метаданные
        m_projectMetadata = ProjectMetadata();
        m_currentFilePath.clear();
        
        // Сбрасываем камеру
        m_camera.Reset();
        
        // Инициализируем дефолтные данные
        m_document.InitializeDefaults();
        m_wallJoinSystem.SetSettings(m_document.GetJoinSettings());
        RebuildWallTypeCombo();
        
        // Сбрасываем выделение
        m_document.ClearSelection();
        UpdateSelectedElementUI();
        
        // Обновляем состояние
        m_viewModel.HasUnsavedChanges(false);
        m_viewModel.ProjectName(L"Новый проект");
        
        InvalidateCanvas();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::SaveProjectAsync(bool saveAs)
    {
        auto xamlRoot = this->Content().XamlRoot();
        if (!xamlRoot)
            co_return;

        std::wstring filePath = m_currentFilePath;

        // Если нет файла или запрошено "Сохранить как..." — показываем диалог
        if (saveAs || filePath.empty())
        {
            // Получаем HWND для диалога
            auto windowNative = this->try_as<::IWindowNative>();
            if (!windowNative)
                co_return;

            HWND hwnd = nullptr;
            windowNative->get_WindowHandle(&hwnd);

            // Создаём SaveFileDialog через COM
            IFileSaveDialog* pFileSave = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));

            if (SUCCEEDED(hr) && pFileSave)
            {
                // Устанавливаем фильтры
                COMDLG_FILTERSPEC fileTypes[] = {
                    { L"ARC-Estimate Project (*.arcp)", L"*.arcp" },
                    { L"Все файлы (*.*)", L"*.*" }
                };
                pFileSave->SetFileTypes(2, fileTypes);
                pFileSave->SetFileTypeIndex(1);
                pFileSave->SetDefaultExtension(L"arcp");

                // Устанавливаем имя по умолчанию
                std::wstring defaultName = m_projectMetadata.Name.empty() ? L"Проект" : m_projectMetadata.Name;
                pFileSave->SetFileName(defaultName.c_str());

                // Показываем диалог
                hr = pFileSave->Show(hwnd);
                if (SUCCEEDED(hr))
                {
                    IShellItem* pItem = nullptr;
                    hr = pFileSave->GetResult(&pItem);
                    if (SUCCEEDED(hr) && pItem)
                    {
                        PWSTR pszFilePath = nullptr;
                        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                        if (SUCCEEDED(hr) && pszFilePath)
                        {
                            filePath = pszFilePath;
                            CoTaskMemFree(pszFilePath);
                        }
                        pItem->Release();
                    }
                }
                pFileSave->Release();
            }

            if (filePath.empty())
                co_return;
        }

        // Сохраняем проект
        auto result = ProjectSerializer::SaveProject(
            filePath,
            m_projectMetadata,
            m_camera,
            m_layerManager,
            m_document,
            &m_dxfManager,
            &m_ifcManager);

        if (result.Success)
        {
            m_currentFilePath = filePath;
            m_viewModel.HasUnsavedChanges(false);
            
            // Обновляем имя проекта из пути файла
            size_t lastSlash = filePath.find_last_of(L"\\/");
            size_t lastDot = filePath.find_last_of(L'.');
            if (lastSlash != std::wstring::npos && lastDot != std::wstring::npos && lastDot > lastSlash)
            {
                m_viewModel.ProjectName(winrt::hstring(filePath.substr(lastSlash + 1, lastDot - lastSlash - 1)));
            }
        }
        else
        {
            ContentDialog errorDialog;
            errorDialog.XamlRoot(xamlRoot);
            errorDialog.Title(winrt::box_value(L"Ошибка сохранения"));
            errorDialog.Content(winrt::box_value(winrt::hstring(result.ErrorMessage)));
            errorDialog.CloseButtonText(L"OK");
            co_await errorDialog.ShowAsync();
        }
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::OpenProjectAsync()
    {
        auto xamlRoot = this->Content().XamlRoot();
        if (!xamlRoot)
            co_return;

        // Если есть несохранённые изменения — спрашиваем
        if (m_viewModel.HasUnsavedChanges())
        {
            ContentDialog confirmDialog;
            confirmDialog.XamlRoot(xamlRoot);
            confirmDialog.Title(winrt::box_value(L"Несохранённые изменения"));
            confirmDialog.Content(winrt::box_value(L"В проекте есть несохранённые изменения.\nСохранить перед открытием другого проекта?"));
            confirmDialog.PrimaryButtonText(L"Сохранить");
            confirmDialog.SecondaryButtonText(L"Не сохранять");
            confirmDialog.CloseButtonText(L"Отмена");
            confirmDialog.DefaultButton(ContentDialogButton::Primary);

            auto result = co_await confirmDialog.ShowAsync();
            
            if (result == ContentDialogResult::Primary)
            {
                co_await SaveProjectAsync(false);
            }
            else if (result == ContentDialogResult::None)
            {
                co_return; // Отмена
            }
        }

        // Получаем HWND для диалога
        auto windowNative = this->try_as<::IWindowNative>();
        if (!windowNative)
            co_return;

        HWND hwnd = nullptr;
        windowNative->get_WindowHandle(&hwnd);

        // Создаём OpenFileDialog через COM
        std::wstring filePath;

        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr) && pFileOpen)
        {
            // Устанавливаем фильтры
            COMDLG_FILTERSPEC fileTypes[] = {
                { L"ARC-Estimate Project (*.arcp)", L"*.arcp" },
                { L"Все файлы (*.*)", L"*.*" }
            };
            pFileOpen->SetFileTypes(2, fileTypes);
            pFileOpen->SetFileTypeIndex(1);

            // Показываем диалог
            hr = pFileOpen->Show(hwnd);
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem = nullptr;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr) && pItem)
                {
                    PWSTR pszFilePath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr) && pszFilePath)
                    {
                        filePath = pszFilePath;
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }

        if (filePath.empty())
            co_return;

        // Очищаем текущие данные
        m_dxfManager.Clear();
        m_ifcManager.Clear();

        // Загружаем проект
        auto result = ProjectSerializer::LoadProject(
            filePath,
            m_projectMetadata,
            m_camera,
            m_layerManager,
            m_document,
            &m_dxfManager,
            &m_ifcManager);

        if (result.Success)
        {
            m_currentFilePath = filePath;
            m_viewModel.HasUnsavedChanges(false);
            // Применяем настройки соединений из загруженного документа
            m_wallJoinSystem.SetSettings(m_document.GetJoinSettings());
            
            // Обновляем имя проекта
            if (!m_projectMetadata.Name.empty())
            {
                m_viewModel.ProjectName(winrt::hstring(m_projectMetadata.Name));
            }
            else
            {
                size_t lastSlash = filePath.find_last_of(L"\\/");
                size_t lastDot = filePath.find_last_of(L'.');
                if (lastSlash != std::wstring::npos && lastDot != std::wstring::npos && lastDot > lastSlash)
                {
                    m_viewModel.ProjectName(winrt::hstring(filePath.substr(lastSlash + 1, lastDot - lastSlash - 1)));
                }
            }

            // Обновляем UI
            RebuildWallTypeCombo();
            SyncLayerCheckboxes();
            UpdateSelectedElementUI();
            InvalidateCanvas();
        }
        else
        {
            ContentDialog errorDialog;
            errorDialog.XamlRoot(xamlRoot);
            errorDialog.Title(winrt::box_value(L"Ошибка открытия"));
            errorDialog.Content(winrt::box_value(winrt::hstring(result.ErrorMessage)));
            errorDialog.CloseButtonText(L"OK");
            co_await errorDialog.ShowAsync();
        }
    }

    // =========================================================================
    // M9: Undo/Redo
    // =========================================================================

    void MainWindow::OnUndoClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (m_undoManager.Undo())
        {
            m_viewModel.HasUnsavedChanges(true);
            m_document.ClearSelection();
            UpdateSelectedElementUI();
            InvalidateCanvas();
        }
    }

    void MainWindow::OnRedoClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (m_undoManager.Redo())
        {
            m_viewModel.HasUnsavedChanges(true);
            m_document.ClearSelection();
            UpdateSelectedElementUI();
            InvalidateCanvas();
        }
    }

    // =========================================================================
    // M6: Wall Type Editor Dialog
    // =========================================================================

    void MainWindow::OnWallTypeEditorClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        ShowWallTypeEditorAsync();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::ShowWallTypeEditorAsync()
    {
        auto xamlRoot = Content().XamlRoot();
        if (!xamlRoot)
            co_return;

        WallTypeEditorController controller(m_document);

        // Создаём основную панель
        StackPanel mainPanel;
        mainPanel.Spacing(12);
        mainPanel.MinWidth(500);

        // Заголовок
        TextBlock headerText;
        headerText.Text(L"Редактор типов стен");
        headerText.FontSize(18);
        // FontWeight будет по умолчанию Normal
        mainPanel.Children().Append(headerText);

        // Список типов стен
        TextBlock typesLabel;
        typesLabel.Text(L"Типы стен:");
        mainPanel.Children().Append(typesLabel);

        ListView typesListView;
        typesListView.Height(150);
        typesListView.SelectionMode(ListViewSelectionMode::Single);

        // Заполняем список
        for (const auto& wt : controller.GetTypes())
        {
            if (!wt) continue;
            auto item = winrt::box_value(winrt::hstring(FormatWallTypeSummary(*wt)));
            typesListView.Items().Append(item);
        }

        mainPanel.Children().Append(typesListView);

        // Кнопки управления типами
        StackPanel typeButtonsPanel;
        typeButtonsPanel.Orientation(Orientation::Horizontal);
        typeButtonsPanel.Spacing(8);

        Button addTypeButton;
        addTypeButton.Content(winrt::box_value(L"Добавить"));
        typeButtonsPanel.Children().Append(addTypeButton);

        Button deleteTypeButton;
        deleteTypeButton.Content(winrt::box_value(L"Удалить"));
        typeButtonsPanel.Children().Append(deleteTypeButton);

        mainPanel.Children().Append(typeButtonsPanel);

        // Разделитель
        Border separator;
        separator.Height(1);
        separator.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200)));
        separator.Margin(ThicknessHelper::FromLengths(0, 8, 0, 8));
        mainPanel.Children().Append(separator);

        // Детали выбранного типа
        TextBlock detailsLabel;
        detailsLabel.Text(L"Слои выбранного типа:");
        mainPanel.Children().Append(detailsLabel);

        TextBlock layersText;
        layersText.TextWrapping(TextWrapping::Wrap);
        layersText.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::ColorHelper::FromArgb(255, 100, 100, 100)));
        layersText.Text(L"Выберите тип стены для просмотра слоёв");
        mainPanel.Children().Append(layersText);

        // Кнопки управления слоями
        StackPanel layerButtonsPanel;
        layerButtonsPanel.Orientation(Orientation::Horizontal);
        layerButtonsPanel.Spacing(8);

        Button addLayerButton;
        addLayerButton.Content(winrt::box_value(L"+ Слой"));
        addLayerButton.IsEnabled(false);
        layerButtonsPanel.Children().Append(addLayerButton);

        Button removeLayerButton;
        removeLayerButton.Content(winrt::box_value(L"- Слой"));
        removeLayerButton.IsEnabled(false);
        layerButtonsPanel.Children().Append(removeLayerButton);

        mainPanel.Children().Append(layerButtonsPanel);

        // Разделитель
        Border separator2;
        separator2.Height(1);
        separator2.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::ColorHelper::FromArgb(255, 200, 200, 200)));
        separator2.Margin(ThicknessHelper::FromLengths(0, 8, 0, 8));
        mainPanel.Children().Append(separator2);

        // Редактирование слоя
        TextBlock editLayerLabel;
        editLayerLabel.Text(L"Добавить новый слой:");
        mainPanel.Children().Append(editLayerLabel);

        Grid layerEditGrid;
        layerEditGrid.ColumnSpacing(8);
        layerEditGrid.RowSpacing(4);

        // Столбцы
        auto col1 = ColumnDefinition();
        col1.Width(GridLengthHelper::FromPixels(100));
        layerEditGrid.ColumnDefinitions().Append(col1);
        auto col2 = ColumnDefinition();
        col2.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        layerEditGrid.ColumnDefinitions().Append(col2);

        // Строки
        for (int i = 0; i < 3; i++)
        {
            layerEditGrid.RowDefinitions().Append(RowDefinition());
        }

        // Название слоя
        TextBlock layerNameLabel;
        layerNameLabel.Text(L"Название:");
        layerNameLabel.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetRow(layerNameLabel, 0);
        Grid::SetColumn(layerNameLabel, 0);
        layerEditGrid.Children().Append(layerNameLabel);

        TextBox layerNameBox;
        layerNameBox.PlaceholderText(L"Несущий");
        Grid::SetRow(layerNameBox, 0);
        Grid::SetColumn(layerNameBox, 1);
        layerEditGrid.Children().Append(layerNameBox);

        // Толщина
        TextBlock layerThicknessLabel;
        layerThicknessLabel.Text(L"Толщина (мм):");
        layerThicknessLabel.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetRow(layerThicknessLabel, 1);
        Grid::SetColumn(layerThicknessLabel, 0);
        layerEditGrid.Children().Append(layerThicknessLabel);

        NumberBox layerThicknessBox;
        layerThicknessBox.Value(100);
        layerThicknessBox.Minimum(1);
        layerThicknessBox.Maximum(1000);
        layerThicknessBox.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Compact);
        Grid::SetRow(layerThicknessBox, 1);
        Grid::SetColumn(layerThicknessBox, 1);
        layerEditGrid.Children().Append(layerThicknessBox);

        // Материал
        TextBlock layerMaterialLabel;
        layerMaterialLabel.Text(L"Материал:");
        layerMaterialLabel.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetRow(layerMaterialLabel, 2);
        Grid::SetColumn(layerMaterialLabel, 0);
        layerEditGrid.Children().Append(layerMaterialLabel);

        ComboBox layerMaterialCombo;
        layerMaterialCombo.Items().Append(winrt::box_value(L"(нет)"));
        for (const auto& mat : controller.GetMaterials())
        {
            if (!mat) continue;
            layerMaterialCombo.Items().Append(winrt::box_value(winrt::hstring(mat->Name)));
        }
        layerMaterialCombo.SelectedIndex(0);
        Grid::SetRow(layerMaterialCombo, 2);
        Grid::SetColumn(layerMaterialCombo, 1);
        layerEditGrid.Children().Append(layerMaterialCombo);

        mainPanel.Children().Append(layerEditGrid);

        // Обработчик выбора типа
        typesListView.SelectionChanged([&controller, &layersText, &addLayerButton, &removeLayerButton](
            [[maybe_unused]] IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args)
        {
            auto listView = sender.as<ListView>();
            int index = listView.SelectedIndex();
            controller.SelectType(index);
            
            auto selectedType = controller.GetSelectedType();
            if (selectedType)
            {
                layersText.Text(winrt::hstring(FormatWallTypeLayers(*selectedType)));
                addLayerButton.IsEnabled(true);
                removeLayerButton.IsEnabled(selectedType->GetLayerCount() > 0);
            }
            else
            {
                layersText.Text(L"Выберите тип стены для просмотра слоёв");
                addLayerButton.IsEnabled(false);
                removeLayerButton.IsEnabled(false);
            }
        });

        // Обработчик добавления слоя
        addLayerButton.Click([&controller, &layersText, &layerNameBox, &layerThicknessBox, &layerMaterialCombo, &removeLayerButton](
            [[maybe_unused]] IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& args)
        {
            std::wstring name = layerNameBox.Text().c_str();
            if (name.empty()) name = L"Слой";
            
            double thickness = layerThicknessBox.Value();
            
            std::shared_ptr<Material> material = nullptr;
            int matIndex = layerMaterialCombo.SelectedIndex();
            if (matIndex > 0 && static_cast<size_t>(matIndex - 1) < controller.GetMaterials().size())
            {
                material = controller.GetMaterials()[matIndex - 1];
            }
            
            if (controller.AddLayerToSelected(name, thickness, material))
            {
                auto selectedType = controller.GetSelectedType();
                if (selectedType)
                {
                    layersText.Text(winrt::hstring(FormatWallTypeLayers(*selectedType)));
                    removeLayerButton.IsEnabled(true);
                }
            }
        });

        // Обработчик удаления слоя
        removeLayerButton.Click([&controller, &layersText, &removeLayerButton](
            [[maybe_unused]] IInspectable const& sender,
            [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& args)
        {
            auto selectedType = controller.GetSelectedType();
            if (selectedType && selectedType->GetLayerCount() > 0)
            {
                // Удаляем последний слой
                controller.RemoveLayerFromSelected(static_cast<int>(selectedType->GetLayerCount()) - 1);
                layersText.Text(winrt::hstring(FormatWallTypeLayers(*selectedType)));
                removeLayerButton.IsEnabled(selectedType->GetLayerCount() > 0);
            }
        });

        // Создаём диалог
        ContentDialog dialog;
        dialog.XamlRoot(xamlRoot);
        dialog.Title(winrt::box_value(L"Редактор типов стен"));
        dialog.Content(mainPanel);
        dialog.PrimaryButtonText(L"Закрыть");
        dialog.DefaultButton(ContentDialogButton::Primary);

        co_await dialog.ShowAsync();

        // Обновляем UI после закрытия
        RebuildWallTypeCombo();
        m_viewModel.HasUnsavedChanges(true);
        InvalidateCanvas();
    }

    // =========================================================================
    // M5/M5.5: Конвертация импортированных элементов в стены
    // =========================================================================

    void MainWindow::OnConvertDxfToWallsClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Конвертируем линии DXF в стены
        size_t wallsCreated = 0;
        double defaultThickness = 150.0; // Толщина по умолчанию

        // Получаем текущий WorkState из активного вида
        WorkStateNative targetWorkState = WorkStateNative::Existing;
        switch (m_viewModel.ActiveView())
        {
        case PlanView::Measure: targetWorkState = WorkStateNative::Existing; break;
        case PlanView::Demolition: targetWorkState = WorkStateNative::Demolish; break;
        case PlanView::Construction: targetWorkState = WorkStateNative::New; break;
        }

        // Проходим по всем DXF слоям
        for (const auto& dxfLayer : m_dxfManager.GetLayers())
        {
            if (!dxfLayer) continue;

            double scale = dxfLayer->GetScale();
            WorldPoint offset = dxfLayer->GetOffset();

            // Проходим по всем сущностям
            for (const auto& entity : dxfLayer->GetEntities())
            {
                if (!entity) continue;

                // Конвертируем линии
                if (entity->Type == DxfEntityType::Line)
                {
                    const DxfLine* line = static_cast<const DxfLine*>(entity.get());
                    WorldPoint start{
                        line->Start.X * scale + offset.X,
                        line->Start.Y * scale + offset.Y
                    };
                    WorldPoint end{
                        line->End.X * scale + offset.X,
                        line->End.Y * scale + offset.Y
                    };

                    // Пропускаем слишком короткие линии
                    if (start.Distance(end) < 50.0) continue;

                    Wall* wall = m_document.AddWall(start, end, defaultThickness);
                    if (wall)
                    {
                        wall->SetWorkState(targetWorkState);
                        wallsCreated++;
                    }
                }
                // Конвертируем полилинии
                else if (entity->Type == DxfEntityType::LWPolyline || entity->Type == DxfEntityType::Polyline)
                {
                    const DxfPolyline* polyline = static_cast<const DxfPolyline*>(entity.get());
                    if (polyline->Vertices.size() < 2) continue;

                    for (size_t i = 0; i + 1 < polyline->Vertices.size(); ++i)
                    {
                        WorldPoint start{
                            polyline->Vertices[i].Point.X * scale + offset.X,
                            polyline->Vertices[i].Point.Y * scale + offset.Y
                        };
                        WorldPoint end{
                            polyline->Vertices[i + 1].Point.X * scale + offset.X,
                            polyline->Vertices[i + 1].Point.Y * scale + offset.Y
                        };

                        if (start.Distance(end) < 50.0) continue;

                        Wall* wall = m_document.AddWall(start, end, defaultThickness);
                        if (wall)
                        {
                            wall->SetWorkState(targetWorkState);
                            wallsCreated++;
                        }
                    }

                    // Замыкаем если полилиния замкнутая
                    if (polyline->IsClosed && polyline->Vertices.size() >= 3)
                    {
                        WorldPoint start{
                            polyline->Vertices.back().Point.X * scale + offset.X,
                            polyline->Vertices.back().Point.Y * scale + offset.Y
                        };
                        WorldPoint end{
                            polyline->Vertices.front().Point.X * scale + offset.X,
                            polyline->Vertices.front().Point.Y * scale + offset.Y
                        };

                        if (start.Distance(end) >= 50.0)
                        {
                            Wall* wall = m_document.AddWall(start, end, defaultThickness);
                            if (wall)
                            {
                                wall->SetWorkState(targetWorkState);
                                wallsCreated++;
                            }
                        }
                    }
                }
            }
        }

        if (wallsCreated > 0)
        {
            m_viewModel.HasUnsavedChanges(true);
            InvalidateCanvas();
        }

        // Показываем результат
        auto showResult = [this, wallsCreated]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            wchar_t msg[128];
            swprintf_s(msg, L"Создано стен из DXF: %zu", wallsCreated);

            ContentDialog dialog;
            dialog.XamlRoot(xamlRoot);
            dialog.Title(winrt::box_value(L"Конвертация DXF"));
            dialog.Content(winrt::box_value(winrt::hstring(msg)));
            dialog.CloseButtonText(L"OK");
            co_await dialog.ShowAsync();
        };
        showResult();
    }

    void MainWindow::OnConvertIfcToWallsClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Конвертируем элементы IFC в нативные стены
        size_t wallsCreated = 0;

        // Получаем текущий WorkState из активного вида
        WorkStateNative targetWorkState = WorkStateNative::Existing;
        switch (m_viewModel.ActiveView())
        {
        case PlanView::Measure: targetWorkState = WorkStateNative::Existing; break;
        case PlanView::Demolition: targetWorkState = WorkStateNative::Demolish; break;
        case PlanView::Construction: targetWorkState = WorkStateNative::New; break;
        }

        // Проходим по всем IFC слоям
        for (const auto& ifcLayer : m_ifcManager.GetLayers())
        {
            if (!ifcLayer) continue;

            double scale = ifcLayer->GetScale();
            WorldPoint offset = ifcLayer->GetOffset();

            // Конвертируем стены
            for (const auto& ifcWall : ifcLayer->GetWalls())
            {
                if (!ifcWall) continue;

                // IFC стены имеют контур в Contours, берём первый контур
                if (ifcWall->Contours.empty() || ifcWall->Contours[0].Points.size() < 2)
                {
                    // Если нет контура, используем StartPoint/EndPoint
                    WorldPoint start{
                        ifcWall->StartPoint.X * scale + offset.X,
                        ifcWall->StartPoint.Y * scale + offset.Y
                    };
                    WorldPoint end{
                        ifcWall->EndPoint.X * scale + offset.X,
                        ifcWall->EndPoint.Y * scale + offset.Y
                    };

                    double thickness = ifcWall->Thickness > 0 ? ifcWall->Thickness * scale : 150.0;
                    if (thickness < 50.0) thickness = 50.0;
                    if (thickness > 1000.0) thickness = 1000.0;

                    if (start.Distance(end) >= 50.0)
                    {
                        Wall* wall = m_document.AddWall(start, end, thickness);
                        if (wall)
                        {
                            wall->SetWorkState(targetWorkState);
                            if (!ifcWall->Name.empty())
                            {
                                wall->SetName(ifcWall->Name);
                            }
                            wallsCreated++;
                        }
                    }
                    continue;
                }

                const auto& contour = ifcWall->Contours[0].Points;
                
                // Вычисляем центральную линию стены из контура (4 точки = прямоугольник)
                if (contour.size() >= 4)
                {
                    WorldPoint start{
                        (contour[0].X + contour[3].X) / 2.0 * scale + offset.X,
                        (contour[0].Y + contour[3].Y) / 2.0 * scale + offset.Y
                    };
                    WorldPoint end{
                        (contour[1].X + contour[2].X) / 2.0 * scale + offset.X,
                        (contour[1].Y + contour[2].Y) / 2.0 * scale + offset.Y
                    };

                    // Вычисляем толщину из контура
                    double d1 = std::sqrt(
                        std::pow(contour[0].X - contour[3].X, 2) +
                        std::pow(contour[0].Y - contour[3].Y, 2));
                    double thickness = d1 * scale;
                    if (thickness < 50.0) thickness = ifcWall->Thickness > 0 ? ifcWall->Thickness * scale : 150.0;
                    if (thickness < 50.0) thickness = 50.0;
                    if (thickness > 1000.0) thickness = 1000.0;

                    if (start.Distance(end) >= 50.0)
                    {
                        Wall* wall = m_document.AddWall(start, end, thickness);
                        if (wall)
                        {
                            wall->SetWorkState(targetWorkState);
                            if (!ifcWall->Name.empty())
                            {
                                wall->SetName(ifcWall->Name);
                            }
                            wallsCreated++;
                        }
                    }
                }
                else if (contour.size() >= 2)
                {
                    // Простая линия из 2 точек
                    WorldPoint start{
                        contour[0].X * scale + offset.X,
                        contour[0].Y * scale + offset.Y
                    };
                    WorldPoint end{
                        contour[1].X * scale + offset.X,
                        contour[1].Y * scale + offset.Y
                    };

                    double thickness = ifcWall->Thickness > 0 ? ifcWall->Thickness * scale : 150.0;
                    if (thickness < 50.0) thickness = 50.0;
                    if (thickness > 1000.0) thickness = 1000.0;

                    if (start.Distance(end) >= 50.0)
                    {
                        Wall* wall = m_document.AddWall(start, end, thickness);
                        if (wall)
                        {
                            wall->SetWorkState(targetWorkState);
                            if (!ifcWall->Name.empty())
                            {
                                wall->SetName(ifcWall->Name);
                            }
                            wallsCreated++;
                        }
                    }
                }
            }
        }

        if (wallsCreated > 0)
        {
            m_viewModel.HasUnsavedChanges(true);
            InvalidateCanvas();
        }

        // Показываем результат
        auto showResult = [this, wallsCreated]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            wchar_t msg[128];
            swprintf_s(msg, L"Создано стен из IFC: %zu", wallsCreated);

            ContentDialog dialog;
            dialog.XamlRoot(xamlRoot);
            dialog.Title(winrt::box_value(L"Конвертация IFC"));
            dialog.Content(winrt::box_value(winrt::hstring(msg)));
            dialog.CloseButtonText(L"OK");
            co_await dialog.ShowAsync();
        };
        showResult();
    }

    // =========================================================================
    // M7: Экспорт сметы
    // =========================================================================

    void MainWindow::OnExportEstimateClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        ShowEstimateExportDialogAsync();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::ShowEstimateExportDialogAsync()
    {
        auto xamlRoot = Content().XamlRoot();
        if (!xamlRoot) co_return;

        // Расчёт сметы
        EstimationEngine engine;
        EstimationSettings settings;
        settings.IncludeExisting = false;
        settings.IncludeDemolition = true;
        settings.IncludeNew = true;
        settings.GroupByWallType = true;

        EstimationResult result = engine.Calculate(m_document, settings);
        result.ProjectName = m_projectMetadata.Name;

        // Проверяем наличие данных
        if (result.Items.empty())
        {
            ContentDialog emptyDialog;
            emptyDialog.XamlRoot(xamlRoot);
            emptyDialog.Title(winrt::box_value(L"Экспорт сметы"));
            emptyDialog.Content(winrt::box_value(L"Нет данных для экспорта.\nДобавьте стены с WorkState = Снос или Новое."));
            emptyDialog.CloseButtonText(L"OK");
            co_await emptyDialog.ShowAsync();
            co_return;
        }

        // Показываем предварительную сводку
        std::wstring summary = EstimationFormatter::FormatSummary(result);

        // Создаём диалог с опциями экспорта
        StackPanel mainPanel;
        mainPanel.Spacing(12);
        mainPanel.Padding({ 0, 12, 0, 0 });

        // Сводка
        TextBlock summaryText;
        summaryText.Text(summary);
        summaryText.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
        summaryText.FontSize(12);
        mainPanel.Children().Append(summaryText);

        // Настройки экспорта
        StackPanel optionsPanel;
        optionsPanel.Spacing(8);
        optionsPanel.Padding({ 0, 12, 0, 0 });

        TextBlock optionsHeader;
        optionsHeader.Text(L"Настройки экспорта:");
        optionsHeader.FontSize(14);
        optionsPanel.Children().Append(optionsHeader);

        CheckBox groupByTypeCheck;
        groupByTypeCheck.Content(winrt::box_value(L"Группировать по типам стен"));
        groupByTypeCheck.IsChecked(true);
        optionsPanel.Children().Append(groupByTypeCheck);

        CheckBox groupByMaterialCheck;
        groupByMaterialCheck.Content(winrt::box_value(L"Группировать по материалам"));
        groupByMaterialCheck.IsChecked(false);
        optionsPanel.Children().Append(groupByMaterialCheck);

        mainPanel.Children().Append(optionsPanel);

        ContentDialog dialog;
        dialog.XamlRoot(xamlRoot);
        dialog.Title(winrt::box_value(L"Экспорт сметы в Excel"));
        dialog.Content(mainPanel);
        dialog.PrimaryButtonText(L"Экспорт");
        dialog.CloseButtonText(L"Отмена");

        auto dialogResult = co_await dialog.ShowAsync();
        if (dialogResult != ContentDialogResult::Primary) co_return;

        // Обновляем настройки из диалога
        settings.GroupByWallType = groupByTypeCheck.IsChecked().Value();
        settings.GroupByMaterial = groupByMaterialCheck.IsChecked().Value();

        // Пересчитываем с новыми настройками
        result = engine.Calculate(m_document, settings);
        result.ProjectName = m_projectMetadata.Name;

        // Получаем HWND
        HWND hwnd{ nullptr };
        auto windowNative = this->try_as<IWindowNative>();
        if (windowNative)
        {
            windowNative->get_WindowHandle(&hwnd);
        }

        // Выбор файла для сохранения
        Windows::Storage::Pickers::FileSavePicker picker;
        if (hwnd)
        {
            auto init = picker.as<IInitializeWithWindow>();
            if (init) init->Initialize(hwnd);
        }
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.SuggestedFileName(L"Смета");
        picker.FileTypeChoices().Insert(L"Excel файл", winrt::single_threaded_vector<winrt::hstring>({ L".xlsx" }));

        auto file = co_await picker.PickSaveFileAsync();
        if (!file) co_return;

        std::wstring filePath = file.Path().c_str();

        // Экспорт
        bool success = ExcelExporter::Export(result, filePath);

        // Результат
        ContentDialog resultDialog;
        resultDialog.XamlRoot(xamlRoot);
        
        if (success)
        {
            resultDialog.Title(winrt::box_value(L"Экспорт завершён"));
            resultDialog.Content(winrt::box_value(L"Смета успешно экспортирована в Excel."));
        }
        else
        {
            resultDialog.Title(winrt::box_value(L"Ошибка экспорта"));
            resultDialog.Content(winrt::box_value(L"Не удалось создать файл Excel.\nПопробуйте экспорт в CSV."));
        }
        
        resultDialog.CloseButtonText(L"OK");
        co_await resultDialog.ShowAsync();
    }

    void MainWindow::OnExportEstimateCsvClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        auto exportAsync = [this]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            // Расчёт сметы
            EstimationEngine engine;
            EstimationSettings settings;
            EstimationResult result = engine.Calculate(m_document, settings);
            result.ProjectName = m_projectMetadata.Name;

            if (result.Items.empty())
            {
                ContentDialog emptyDialog;
                emptyDialog.XamlRoot(xamlRoot);
                emptyDialog.Title(winrt::box_value(L"Экспорт сметы"));
                emptyDialog.Content(winrt::box_value(L"Нет данных для экспорта."));
                emptyDialog.CloseButtonText(L"OK");
                co_await emptyDialog.ShowAsync();
                co_return;
            }

            // Получаем HWND
            HWND hwnd{ nullptr };
            auto windowNative = this->try_as<IWindowNative>();
            if (windowNative)
            {
                windowNative->get_WindowHandle(&hwnd);
            }

            // Выбор файла
            Windows::Storage::Pickers::FileSavePicker picker;
            if (hwnd)
            {
                auto init = picker.as<IInitializeWithWindow>();
                if (init) init->Initialize(hwnd);
            }
            picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.SuggestedFileName(L"Смета");
            picker.FileTypeChoices().Insert(L"CSV файл", winrt::single_threaded_vector<winrt::hstring>({ L".csv" }));

            auto file = co_await picker.PickSaveFileAsync();
            if (!file) co_return;

            std::wstring filePath = file.Path().c_str();

            // Экспорт
            bool success = CsvExporter::Export(result, filePath);

            // Результат
            ContentDialog resultDialog;
            resultDialog.XamlRoot(xamlRoot);
            
            if (success)
            {
                resultDialog.Title(winrt::box_value(L"Экспорт завершён"));
                resultDialog.Content(winrt::box_value(L"Смета успешно экспортирована в CSV.\nФайл можно открыть в Excel."));
            }
            else
            {
                resultDialog.Title(winrt::box_value(L"Ошибка экспорта"));
                resultDialog.Content(winrt::box_value(L"Не удалось создать файл."));
            }
            
            resultDialog.CloseButtonText(L"OK");
            co_await resultDialog.ShowAsync();
        };
        exportAsync();
    }

    void MainWindow::OnShowEstimateSummaryClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        auto showAsync = [this]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            // Расчёт сметы
            EstimationEngine engine;
            EstimationSettings settings;
            EstimationResult result = engine.Calculate(m_document, settings);

            // Форматируем сводку
            std::wstring summary = EstimationFormatter::FormatSummary(result);

            // Создаём ScrollViewer для длинного текста
            ScrollViewer scrollViewer;
            scrollViewer.MaxHeight(400);
            scrollViewer.MaxWidth(500);

            TextBlock summaryText;
            summaryText.Text(summary);
            summaryText.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
            summaryText.FontSize(13);
            summaryText.TextWrapping(Microsoft::UI::Xaml::TextWrapping::NoWrap);
            summaryText.IsTextSelectionEnabled(true);
            scrollViewer.Content(summaryText);

            ContentDialog dialog;
            dialog.XamlRoot(xamlRoot);
            dialog.Title(winrt::box_value(L"Сводка по смете"));
            dialog.Content(scrollViewer);
            dialog.CloseButtonText(L"Закрыть");
            dialog.PrimaryButtonText(L"Экспорт в Excel...");

            auto dialogResult = co_await dialog.ShowAsync();
            
            if (dialogResult == ContentDialogResult::Primary)
            {
                // Запускаем экспорт
                co_await ShowEstimateExportDialogAsync();
            }
        };
        showAsync();
    }

    // =========================================================================
    // M8: Экспорт чертежа в PDF
    // =========================================================================

    void MainWindow::OnExportPdfClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        ShowPdfExportDialogAsync();
    }

    winrt::Windows::Foundation::IAsyncAction MainWindow::ShowPdfExportDialogAsync()
    {
        auto xamlRoot = Content().XamlRoot();
        if (!xamlRoot) co_return;

        // Проверяем наличие элементов
        if (m_document.GetWalls().empty())
        {
            ContentDialog emptyDialog;
            emptyDialog.XamlRoot(xamlRoot);
            emptyDialog.Title(winrt::box_value(L"Экспорт PDF"));
            emptyDialog.Content(winrt::box_value(L"Нет элементов для экспорта.\nДобавьте стены на чертёж."));
            emptyDialog.CloseButtonText(L"OK");
            co_await emptyDialog.ShowAsync();
            co_return;
        }

        // Создаём диалог настроек
        StackPanel mainPanel;
        mainPanel.Spacing(12);
        mainPanel.Padding({ 0, 8, 0, 0 });

        // Масштаб
        TextBlock scaleLabel;
        scaleLabel.Text(L"Масштаб:");
        mainPanel.Children().Append(scaleLabel);

        ComboBox scaleCombo;
        scaleCombo.Items().Append(winrt::box_value(L"1:50"));
        scaleCombo.Items().Append(winrt::box_value(L"1:100"));
        scaleCombo.Items().Append(winrt::box_value(L"1:200"));
        scaleCombo.Items().Append(winrt::box_value(L"1:500"));
        scaleCombo.SelectedIndex(1);  // 1:100 по умолчанию
        scaleCombo.HorizontalAlignment(Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
        mainPanel.Children().Append(scaleCombo);

        // Размер бумаги
        TextBlock paperLabel;
        paperLabel.Text(L"Размер бумаги:");
        mainPanel.Children().Append(paperLabel);

        ComboBox paperCombo;
        paperCombo.Items().Append(winrt::box_value(L"A4 (210?297 мм)"));
        paperCombo.Items().Append(winrt::box_value(L"A3 (297?420 мм)"));
        paperCombo.Items().Append(winrt::box_value(L"A2 (420?594 мм)"));
        paperCombo.Items().Append(winrt::box_value(L"A1 (594?841 мм)"));
        paperCombo.SelectedIndex(1);  // A3 по умолчанию
        paperCombo.HorizontalAlignment(Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
        mainPanel.Children().Append(paperCombo);

        // Ориентация
        TextBlock orientLabel;
        orientLabel.Text(L"Ориентация:");
        mainPanel.Children().Append(orientLabel);

        ComboBox orientCombo;
        orientCombo.Items().Append(winrt::box_value(L"Альбомная"));
        orientCombo.Items().Append(winrt::box_value(L"Книжная"));
        orientCombo.SelectedIndex(0);  // Альбомная по умолчанию
        orientCombo.HorizontalAlignment(Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
        mainPanel.Children().Append(orientCombo);

        // Опции
        TextBlock optionsLabel;
        optionsLabel.Text(L"Опции:");
        optionsLabel.Margin({ 0, 8, 0, 0 });
        mainPanel.Children().Append(optionsLabel);

        CheckBox showDimensionsCheck;
        showDimensionsCheck.Content(winrt::box_value(L"Показывать размеры"));
        showDimensionsCheck.IsChecked(true);
        mainPanel.Children().Append(showDimensionsCheck);

        CheckBox showTitleBlockCheck;
        showTitleBlockCheck.Content(winrt::box_value(L"Показывать штамп"));
        showTitleBlockCheck.IsChecked(true);
        mainPanel.Children().Append(showTitleBlockCheck);

        CheckBox blackWhiteCheck;
        blackWhiteCheck.Content(winrt::box_value(L"Чёрно-белый режим"));
        blackWhiteCheck.IsChecked(true);
        mainPanel.Children().Append(blackWhiteCheck);

        // Название чертежа
        TextBlock titleLabel;
        titleLabel.Text(L"Название чертежа:");
        titleLabel.Margin({ 0, 8, 0, 0 });
        mainPanel.Children().Append(titleLabel);

        TextBox titleBox;
        titleBox.Text(L"План этажа");
        titleBox.HorizontalAlignment(Microsoft::UI::Xaml::HorizontalAlignment::Stretch);
        mainPanel.Children().Append(titleBox);

        ContentDialog dialog;
        dialog.XamlRoot(xamlRoot);
        dialog.Title(winrt::box_value(L"Экспорт чертежа в PDF"));
        dialog.Content(mainPanel);
        dialog.PrimaryButtonText(L"Экспорт");
        dialog.CloseButtonText(L"Отмена");

        auto dialogResult = co_await dialog.ShowAsync();
        if (dialogResult != ContentDialogResult::Primary) co_return;

        // Собираем настройки
        PdfExportSettings settings;

        // Масштаб
        int scaleIdx = scaleCombo.SelectedIndex();
        switch (scaleIdx)
        {
        case 0: settings.Scale = 50; break;
        case 1: settings.Scale = 100; break;
        case 2: settings.Scale = 200; break;
        case 3: settings.Scale = 500; break;
        }

        // Размер бумаги
        int paperIdx = paperCombo.SelectedIndex();
        switch (paperIdx)
        {
        case 0: settings.Paper = PaperSize::A4; break;
        case 1: settings.Paper = PaperSize::A3; break;
        case 2: settings.Paper = PaperSize::A2; break;
        case 3: settings.Paper = PaperSize::A1; break;
        }

        // Ориентация
        settings.Orientation = (orientCombo.SelectedIndex() == 0)
            ? PaperOrientation::Landscape
            : PaperOrientation::Portrait;

        // Опции
        settings.ShowDimensions = showDimensionsCheck.IsChecked().Value();
        settings.ShowTitleBlock = showTitleBlockCheck.IsChecked().Value();
        settings.BlackAndWhite = blackWhiteCheck.IsChecked().Value();

        // Метаданные
        settings.ProjectName = m_projectMetadata.Name;
        settings.DrawingTitle = std::wstring(titleBox.Text());

        // Получаем HWND
        HWND hwnd{ nullptr };
        auto windowNative = this->try_as<IWindowNative>();
        if (windowNative)
        {
            windowNative->get_WindowHandle(&hwnd);
        }

        // Выбор файла
        Windows::Storage::Pickers::FileSavePicker picker;
        if (hwnd)
        {
            auto init = picker.as<IInitializeWithWindow>();
            if (init) init->Initialize(hwnd);
        }
        picker.SuggestedStartLocation(Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
        picker.SuggestedFileName(L"Чертёж");
        picker.FileTypeChoices().Insert(L"PDF файл", winrt::single_threaded_vector<winrt::hstring>({ L".pdf" }));

        auto file = co_await picker.PickSaveFileAsync();
        if (!file) co_return;

        std::wstring filePath = file.Path().c_str();

        // Собираем размеры для экспорта
        std::vector<Dimension> exportDimensions;
        if (settings.ShowDimensions)
        {
            // Добавляем все размеры из документа (ручные и авто)
            for (const auto& dim : m_document.GetDimensions())
            {
                if (dim) exportDimensions.push_back(*dim);
            }
        }

        // Экспорт
        PlanPdfExporter exporter;
        bool success = exporter.Export(m_document, exportDimensions, settings, filePath);

        // Результат
        ContentDialog resultDialog;
        resultDialog.XamlRoot(xamlRoot);

        if (success)
        {
            resultDialog.Title(winrt::box_value(L"Экспорт завершён"));
            resultDialog.Content(winrt::box_value(L"Чертёж успешно экспортирован в PDF."));
        }
        else
        {
            resultDialog.Title(winrt::box_value(L"Ошибка экспорта"));
            resultDialog.Content(winrt::box_value(L"Не удалось создать файл PDF."));
        }

        resultDialog.CloseButtonText(L"OK");
        co_await resultDialog.ShowAsync();
    }

    // =========================================================================
    // M9.5: Тесты
    // =========================================================================

    void MainWindow::OnRunTestsClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        // Запускаем тесты
        auto results = tests::RunAllTests();
        
        // Показываем результаты в диалоге
        auto showAsync = [this, results]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            // Создаём текстовый блок с результатами
            TextBlock resultsText;
            resultsText.Text(winrt::hstring(results.GetSummary()));
            resultsText.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
            resultsText.FontSize(12);
            resultsText.IsTextSelectionEnabled(true);

            ScrollViewer scrollViewer;
            scrollViewer.Content(resultsText);
            scrollViewer.MaxHeight(400);
            scrollViewer.HorizontalScrollBarVisibility(Microsoft::UI::Xaml::Controls::ScrollBarVisibility::Auto);

            ContentDialog dialog;
            dialog.XamlRoot(xamlRoot);
            
            if (results.TotalFailed == 0)
            {
                dialog.Title(winrt::box_value(L"? Все тесты пройдены"));
            }
            else
            {
                dialog.Title(winrt::box_value(L"? Есть ошибки в тестах"));
            }
            
            dialog.Content(scrollViewer);
            dialog.CloseButtonText(L"Закрыть");

            co_await dialog.ShowAsync();
        };
        showAsync();
    }

    void MainWindow::OnAboutClick(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        auto showAsync = [this]() -> winrt::Windows::Foundation::IAsyncAction
        {
            auto xamlRoot = Content().XamlRoot();
            if (!xamlRoot) co_return;

            StackPanel panel;
            panel.Spacing(8);

            TextBlock title;
            title.Text(L"ARC-Estimate");
            title.FontSize(24);
            panel.Children().Append(title);

            TextBlock version;
            version.Text(L"Версия 1.0.0");
            version.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::ColorHelper::FromArgb(255, 100, 100, 100)));
            panel.Children().Append(version);

            TextBlock description;
            description.Text(L"2D инструмент архитектурного проектирования\nс автоматической сметой");
            description.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
            panel.Children().Append(description);

            TextBlock stack;
            stack.Text(L"WinUI 3 + C++/WinRT + Win2D");
            stack.FontSize(11);
            stack.Margin({ 0, 16, 0, 0 });
            panel.Children().Append(stack);

            ContentDialog dialog;
            dialog.XamlRoot(xamlRoot);
            dialog.Title(winrt::box_value(L"О программе"));
            dialog.Content(panel);
            dialog.CloseButtonText(L"OK");

            co_await dialog.ShowAsync();
        };
        showAsync();
    }
}



