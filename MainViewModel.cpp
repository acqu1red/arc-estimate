#include "pch.h"
#include "MainViewModel.h"
#include "MainViewModel.g.cpp"

#include <format>

namespace winrt::estimate1::implementation
{
    MainViewModel::MainViewModel()
    {
        // Инициализация модели представления
    }

    // Реализация INotifyPropertyChanged
    winrt::event_token MainViewModel::PropertyChanged(
        Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void MainViewModel::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

    void MainViewModel::RaisePropertyChanged(hstring const& propertyName)
    {
        m_propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
    }

    // Свойство текущего инструмента
    estimate1::DrawingTool MainViewModel::CurrentTool()
    {
        return m_currentTool;
    }

    void MainViewModel::CurrentTool(estimate1::DrawingTool value)
    {
        if (m_currentTool != value)
        {
            m_currentTool = value;
            RaisePropertyChanged(L"CurrentTool");
            RaisePropertyChanged(L"StatusText");
        }
    }

    // Свойство активного вида
    estimate1::PlanView MainViewModel::ActiveView()
    {
        return m_activeView;
    }

    void MainViewModel::ActiveView(estimate1::PlanView value)
    {
        if (m_activeView != value)
        {
            m_activeView = value;
            RaisePropertyChanged(L"ActiveView");
            RaisePropertyChanged(L"StatusText");
        }
    }

    // Координаты курсора
    double MainViewModel::CursorX()
    {
        return m_cursorX;
    }

    void MainViewModel::CursorX(double value)
    {
        if (m_cursorX != value)
        {
            m_cursorX = value;
            RaisePropertyChanged(L"CursorX");
            RaisePropertyChanged(L"StatusText");
        }
    }

    double MainViewModel::CursorY()
    {
        return m_cursorY;
    }

    void MainViewModel::CursorY(double value)
    {
        if (m_cursorY != value)
        {
            m_cursorY = value;
            RaisePropertyChanged(L"CursorY");
            RaisePropertyChanged(L"StatusText");
        }
    }

    // Текст статусной строки - формируется автоматически
    hstring MainViewModel::StatusText()
    {
        // Получаем название текущего инструмента
        hstring toolName;
        switch (m_currentTool)
        {
        case estimate1::DrawingTool::None:
            toolName = L"—";
            break;
        case estimate1::DrawingTool::Select:
            toolName = L"Выбор";
            break;
        case estimate1::DrawingTool::Wall:
            toolName = L"Стена";
            break;
        case estimate1::DrawingTool::Door:
            toolName = L"Дверь";
            break;
        case estimate1::DrawingTool::Window:
            toolName = L"Окно";
            break;
        case estimate1::DrawingTool::Dimension:
            toolName = L"Размер";
            break;
        }

        // Форматируем координаты и инструмент
        wchar_t buffer[256];
        swprintf_s(buffer, L"X: %.0f мм  Y: %.0f мм  |  Инструмент: %s",
            m_cursorX, m_cursorY, toolName.c_str());

        return hstring(buffer);
    }

    // Флаг несохранённых изменений
    bool MainViewModel::HasUnsavedChanges()
    {
        return m_hasUnsavedChanges;
    }

    void MainViewModel::HasUnsavedChanges(bool value)
    {
        if (m_hasUnsavedChanges != value)
        {
            m_hasUnsavedChanges = value;
            RaisePropertyChanged(L"HasUnsavedChanges");
        }
    }

    // Название проекта
    hstring MainViewModel::ProjectName()
    {
        return m_projectName;
    }

    void MainViewModel::ProjectName(hstring const& value)
    {
        if (m_projectName != value)
        {
            m_projectName = value;
            RaisePropertyChanged(L"ProjectName");
        }
    }
}
