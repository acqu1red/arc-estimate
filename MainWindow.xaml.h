#pragma once

#include "MainWindow.g.h"
#include "MainViewModel.h"
#include "Camera.h"
#include "GridRenderer.h"
#include "Layer.h"
#include "Element.h"
#include "WallRenderer.h"
#include "DrawingTools.h"
#include "DimensionRenderer.h"
#include "AutoDimensionManager.h"
#include "WallJoinManager.h"
#include "WallJoinSystem.h"
#include "DxfReference.h"
#include "DxfReferenceRenderer.h"
#include "IfcReference.h"
#include "IfcReferenceRenderer.h"
#include "WallSnapSystem.h"
#include "WallSnapRenderer.h"
#include "ProjectSerializer.h"
#include "UndoManager.h"
#include "WallTypeEditor.h"
#include "EstimationEngine.h"
#include "ExcelExporter.h"
#include "PdfExporter.h"
#include "Tests.h"
#include "OpeningRenderer.h"
#include "OpeningTools.h"
#include "EditTools.h"
#include "StructureTools.h"
#include "RoomRenderer.h"

namespace winrt::estimate1::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // Свойство модели представления
        estimate1::MainViewModel ViewModel();

        // Обработчики событий инструментов
        void OnSelectToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnWallToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnDoorToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnWindowToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnDimensionToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Edit Tools (R2.5)
        void OnTrimExtendToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnSplitToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Structure Tools (R6)
        void OnColumnToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnSlabToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void OnBeamToolClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Обработчик переключения вкладок видов
        void OnViewTabChanged(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // Обработчики событий холста Win2D
        void OnCanvasDraw(
            Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
            Microsoft::Graphics::Canvas::UI::Xaml::CanvasDrawEventArgs const& args);
        
        void OnCanvasCreateResources(
            Microsoft::Graphics::Canvas::UI::Xaml::CanvasControl const& sender,
            Microsoft::Graphics::Canvas::UI::CanvasCreateResourcesEventArgs const& args);

        void OnCanvasPointerPressed(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void OnCanvasPointerMoved(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void OnCanvasPointerReleased(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void OnCanvasPointerWheelChanged(
            Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

            void OnCanvasSizeChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::SizeChangedEventArgs const& e);

            // Обработчик клавиатуры
            void OnCanvasKeyDown(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);

            // Обработчик изменения видимости слоя
            void OnLayerVisibilityChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // Свойства выбранной стены
            void OnWallTypeChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnWallThicknessChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e);

        void OnWallWorkStateChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnWallLocationLineChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void OnDimensionLockChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnDimensionOffsetChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e);

            // Обработчики меню Вид
            void OnDimensionsToggleClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnAutoDimensionsToggleClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // M5: Обработчик импорта DXF
            void OnImportDxfClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5.5: Обработчик импорта IFC
            void OnImportIfcClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5.6: Обработчик изменения режима привязки
            void OnSnapModeChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

            // M6: Обработчики сохранения/загрузки проекта
            void OnNewProjectClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnOpenProjectClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnSaveProjectClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnSaveProjectAsClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M9: Undo/Redo handlers
            void OnUndoClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnRedoClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M6: Wall Type Editor dialog
            void OnWallTypeEditorClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            winrt::Windows::Foundation::IAsyncAction ShowWallTypeEditorAsync();

            // M5/M5.5: Convert imported elements to native walls
            void OnConvertDxfToWallsClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnConvertIfcToWallsClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M7: Экспорт сметы
            void OnExportEstimateClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnExportEstimateCsvClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnShowEstimateSummaryClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            winrt::Windows::Foundation::IAsyncAction ShowEstimateExportDialogAsync();

            // M8: Экспорт чертежа в PDF
            void OnExportPdfClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            winrt::Windows::Foundation::IAsyncAction ShowPdfExportDialogAsync();

            // M9.5: Тесты
            void OnRunTestsClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnAboutClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5: Диалог настроек импорта DXF
            winrt::Windows::Foundation::IAsyncAction ShowDxfImportDialogAsync(const std::wstring& filePath);

            // M5.5: Диалог настроек импорта IFC
            winrt::Windows::Foundation::IAsyncAction ShowIfcImportDialogAsync(const std::wstring& filePath);

        // M5: DXF import (safe entry point that doesn't rely on implementation lifetime after picker)
        winrt::Windows::Foundation::IAsyncAction ShowDxfImportDialogWithXamlRootAsync(const std::wstring& filePath, Microsoft::UI::Xaml::XamlRoot const& xamlRoot);

        private:
        // Обновление визуального состояния кнопок инструментов
        void UpdateToolButtonStates();

        // Перерисовка холста
        void InvalidateCanvas();

        // Обновление видимости слоёв при смене вида
        void UpdateLayerVisibility();

        // Синхронизация UI чекбоксов с состоянием слоёв
        void SyncLayerCheckboxes();

        // Обновление панели свойств выбранного элемента
        void UpdateSelectedElementUI();

        // Заполнение списка типов стен (M3.1)
        void RebuildWallTypeCombo();

        // Обработка инструментов
        void HandleToolClick(const WorldPoint& worldPoint);

        // Модель представления
        estimate1::MainViewModel m_viewModel{ nullptr };

        // Камера для преобразования координат
        Camera m_camera;

        // Рендерер сетки
        GridRenderer m_gridRenderer;

        // Менеджер слоёв
        LayerManager m_layerManager;

        // Модель документа
        DocumentModel m_document;

        // Рендерер стен
        WallRenderer m_wallRenderer;

        // Рендерер размеров (M3.5)
        DimensionRenderer m_dimensionRenderer;

        // Инструменты
        WallTool m_wallTool;
        SelectTool m_selectTool;
        DimensionTool m_dimensionTool;
        SnapManager m_snapManager;

        // Авторазмеры (M3.5)
        AutoDimensionManager m_autoDimensionManager;

        // M4: Соединения стен (старый менеджер)
        WallJoinManager m_wallJoinManager;

        // R2: Новая система соединений с митра-углами
        WallJoinSystem m_wallJoinSystem;
        WallJoinRenderer m_wallJoinRenderer;
        std::optional<JoinInfo> m_previewJoin;  // Превью соединения при рисовании

        // Флаг показа размеров
        bool m_showDimensions{ true };

        // R4: Инструменты дверей и окон
        DoorPlacementTool m_doorTool;
        WindowPlacementTool m_windowTool;
        
        // R6: Инструменты конструкций
        ColumnTool m_columnTool;
        SlabTool m_slabTool;
        BeamTool m_beamTool;

        // Текущая точка привязки
        SnapResult m_currentSnap;

        // Состояние панорамирования
        bool m_isPanning{ false };
        ScreenPoint m_lastPointerPosition;

        // Drag размеров (M3.5)
        bool m_isDraggingDimension{ false };
        uint64_t m_dragDimensionId{ 0 };
        DimensionHandle m_dragHandle{ DimensionHandle::None };
        WorldPoint m_dragBaseP1{ 0, 0 };
        WorldPoint m_dragBaseP2{ 0, 0 };
        double m_dragStartOffset{ 0.0 };
        WorldPoint m_dragStartWorld{ 0, 0 };

        // Hover ручек (M3.5)
        uint64_t m_hoverDimensionId{ 0 };
        DimensionHandle m_hoverHandle{ DimensionHandle::None };

        // M9: Hover над стенами
        uint64_t m_hoverWallId{ 0 };

        // Флаг для предотвращения рекурсии при обновлении чекбоксов
        bool m_updatingLayerCheckboxes{ false };

        // M5: Менеджер DXF-подложек
        DxfReferenceManager m_dxfManager;

        // M5.5: Менеджер IFC-подложек
            IfcReferenceManager m_ifcManager;

            // M5.6: Система привязки стен
            WallSnapSystem m_wallSnapSystem;
            WallSnapCandidate m_currentWallSnap;

            // M6: Данные проекта
            ProjectMetadata m_projectMetadata;
            std::wstring m_currentFilePath;  // Путь к текущему файлу проекта

            // M9: Undo/Redo
            UndoManager m_undoManager;

            // R4: Инструменты и рендерер дверей/окон
            OpeningRenderer m_openingRenderer;
            uint64_t m_hoverOpeningId{ 0 };  // Hover над дверью/окном

            // R5.2: Рендерер помещений
            RoomRenderer m_roomRenderer;
            uint64_t m_hoverRoomId{ 0 };  // Hover над помещением

            // R2.5: Инструменты редактирования
            TrimExtendTool m_trimExtendTool;
            SplitTool m_splitTool;

            // M6: Асинхронные операции сохранения/загрузки
            winrt::Windows::Foundation::IAsyncAction SaveProjectAsync(bool saveAs);
            winrt::Windows::Foundation::IAsyncAction OpenProjectAsync();
            winrt::Windows::Foundation::IAsyncAction ConfirmNewProjectAsync();
        };
}

namespace winrt::estimate1::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
