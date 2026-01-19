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

        // �������� ������ �������������
        estimate1::MainViewModel ViewModel();

        // ����������� ������� ������������
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

        // ���������� ������������ ������� �����
        void OnViewTabChanged(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // ����������� ������� ������ Win2D
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

            // ���������� ����������
            void OnCanvasKeyDown(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);

            // ���������� ��������� ��������� ����
            void OnLayerVisibilityChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // �������� ��������� �����
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

        // R-WALL: Обработчик изменения режима привязки стен
        void OnWallAttachmentModeChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& e);

        void OnDimensionLockChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnDimensionOffsetChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& e);

            // ����������� ���� ���
            void OnDimensionsToggleClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnAutoDimensionsToggleClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

        // M5: ���������� ������� DXF
            void OnImportDxfClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5.5: ���������� ������� IFC
            void OnImportIfcClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5.6: ���������� ��������� ������ ��������
            void OnSnapModeChanged(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

            // M6: ����������� ����������/�������� �������
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

            // M7: ������� �����
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

            // M8: ������� ������� � PDF
            void OnExportPdfClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            winrt::Windows::Foundation::IAsyncAction ShowPdfExportDialogAsync();

            // M9.5: �����
            void OnRunTestsClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            void OnAboutClick(
                Windows::Foundation::IInspectable const& sender,
                Microsoft::UI::Xaml::RoutedEventArgs const& e);

            // M5: ������ �������� ������� DXF
            winrt::Windows::Foundation::IAsyncAction ShowDxfImportDialogAsync(const std::wstring& filePath);

            // M5.5: ������ �������� ������� IFC
            winrt::Windows::Foundation::IAsyncAction ShowIfcImportDialogAsync(const std::wstring& filePath);

        // M5: DXF import (safe entry point that doesn't rely on implementation lifetime after picker)
        winrt::Windows::Foundation::IAsyncAction ShowDxfImportDialogWithXamlRootAsync(const std::wstring& filePath, Microsoft::UI::Xaml::XamlRoot const& xamlRoot);

        private:
        // ���������� ����������� ��������� ������ ������������
        void UpdateToolButtonStates();

        // ����������� ������
        void InvalidateCanvas();

        // ���������� ��������� ���� ��� ����� ����
        void UpdateLayerVisibility();

        // ������������� UI ��������� � ���������� ����
        void SyncLayerCheckboxes();

        // ���������� ������ ������� ���������� ��������
        void UpdateSelectedElementUI();

        // ���������� ������ ����� ���� (M3.1)
        void RebuildWallTypeCombo();

        // R-WALL: ������ ������ �������� ����
        void ShowAttachmentModePanel();
        void HideAttachmentModePanel();

        // ��������� ������������
        void HandleToolClick(const WorldPoint& worldPoint);

        // ������ �������������
        estimate1::MainViewModel m_viewModel{ nullptr };

        // ������ ��� �������������� ���������
        Camera m_camera;

        // �������� �����
        GridRenderer m_gridRenderer;

        // �������� ����
        LayerManager m_layerManager;

        // ������ ���������
        DocumentModel m_document;

        // �������� ����
        WallRenderer m_wallRenderer;

        // �������� �������� (M3.5)
        DimensionRenderer m_dimensionRenderer;

        // �����������
        WallTool m_wallTool;
        SelectTool m_selectTool;
        DimensionTool m_dimensionTool;
        SnapManager m_snapManager;

        // ����������� (M3.5)
        AutoDimensionManager m_autoDimensionManager;

        // M4: ���������� ���� (������ ��������)
        WallJoinManager m_wallJoinManager;

        // R2: ����� ������� ���������� � �����-������
        WallJoinSystem m_wallJoinSystem;
        WallJoinRenderer m_wallJoinRenderer;
        std::optional<JoinInfo> m_previewJoin;  // ������ ���������� ��� ���������

        // ���� ������ ��������
        bool m_showDimensions{ true };

        // R4: ����������� ������ � ����
        DoorPlacementTool m_doorTool;
        WindowPlacementTool m_windowTool;
        
        // R6: ����������� �����������
        ColumnTool m_columnTool;
        SlabTool m_slabTool;
        BeamTool m_beamTool;

        // ������� ����� ��������
        SnapResult m_currentSnap;

        // ��������� ���������������
        bool m_isPanning{ false };
        ScreenPoint m_lastPointerPosition;

        // Drag �������� (M3.5)
        bool m_isDraggingDimension{ false };
        uint64_t m_dragDimensionId{ 0 };
        DimensionHandle m_dragHandle{ DimensionHandle::None };
        WorldPoint m_dragBaseP1{ 0, 0 };
        WorldPoint m_dragBaseP2{ 0, 0 };
        double m_dragStartOffset{ 0.0 };
        WorldPoint m_dragStartWorld{ 0, 0 };

        // Hover ����� (M3.5)
        uint64_t m_hoverDimensionId{ 0 };
        DimensionHandle m_hoverHandle{ DimensionHandle::None };

        // M9: Hover ��� �������
        uint64_t m_hoverWallId{ 0 };

        // ���� ��� �������������� �������� ��� ���������� ���������
        bool m_updatingLayerCheckboxes{ false };

        // M5: �������� DXF-��������
        DxfReferenceManager m_dxfManager;

        // M5.5: �������� IFC-��������
            IfcReferenceManager m_ifcManager;

            // M5.6: ������� �������� ����
            WallSnapSystem m_wallSnapSystem;
            WallSnapCandidate m_currentWallSnap;

            // R-WALL: ������� ����� �������� ����
            WallAttachmentMode m_currentAttachmentMode{ WallAttachmentMode::Core };

            // M6: ������ �������
            ProjectMetadata m_projectMetadata;
            std::wstring m_currentFilePath;  // ���� � �������� ����� �������

            // M9: Undo/Redo
            UndoManager m_undoManager;

            // R4: ����������� � �������� ������/����
            OpeningRenderer m_openingRenderer;
            uint64_t m_hoverOpeningId{ 0 };  // Hover ��� ������/�����

            // R5.2: �������� ���������
            RoomRenderer m_roomRenderer;
            uint64_t m_hoverRoomId{ 0 };  // Hover ��� ����������

            // R2.5: ����������� ��������������
            TrimExtendTool m_trimExtendTool;
            SplitTool m_splitTool;

            // M6: ����������� �������� ����������/��������
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
