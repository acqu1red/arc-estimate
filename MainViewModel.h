#pragma once

#include "MainViewModel.g.h"

namespace winrt::estimate1::implementation
{
    // Модель представления главного окна - управляет состоянием UI
    struct MainViewModel : MainViewModelT<MainViewModel>
    {
        MainViewModel();

        // Реализация INotifyPropertyChanged
        winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

        // Свойства текущего инструмента
        estimate1::DrawingTool CurrentTool();
        void CurrentTool(estimate1::DrawingTool value);

        // Свойства активного вида
        estimate1::PlanView ActiveView();
        void ActiveView(estimate1::PlanView value);

        // Координаты курсора
        double CursorX();
        void CursorX(double value);
        double CursorY();
        void CursorY(double value);

        // Текст статусной строки
        hstring StatusText();

        // Флаг несохранённых изменений
        bool HasUnsavedChanges();
        void HasUnsavedChanges(bool value);

        // Название проекта
        hstring ProjectName();
        void ProjectName(hstring const& value);

    private:
        // Вспомогательный метод для уведомления об изменении свойства
        void RaisePropertyChanged(hstring const& propertyName);

        // Поля данных
        estimate1::DrawingTool m_currentTool{ estimate1::DrawingTool::Select };
        estimate1::PlanView m_activeView{ estimate1::PlanView::Measure };
        double m_cursorX{ 0.0 };
        double m_cursorY{ 0.0 };
        bool m_hasUnsavedChanges{ false };
        hstring m_projectName{ L"Новый проект" };

        // Событие изменения свойства
        winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
    };
}

namespace winrt::estimate1::factory_implementation
{
    struct MainViewModel : MainViewModelT<MainViewModel, implementation::MainViewModel>
    {
    };
}
