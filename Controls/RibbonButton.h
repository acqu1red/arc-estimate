#pragma once

// Включаем .g.h для базового класса
#include "Controls.RibbonButton.g.h"

// Включаем xaml.g.h для XAML поддержки
// Это переопределит RibbonButtonT из .g.h
#include "Controls/RibbonButton.xaml.g.h"

namespace winrt::estimate1::Controls::implementation
{
    struct RibbonButton : RibbonButtonT<RibbonButton>
    {
        RibbonButton();

        // DependencyProperty для Label (текст кнопки)
        static Microsoft::UI::Xaml::DependencyProperty LabelProperty();
        hstring Label();
        void Label(hstring const& value);

        // DependencyProperty для IconSource (путь к иконке)
        static Microsoft::UI::Xaml::DependencyProperty IconSourceProperty();
        hstring IconSource();
        void IconSource(hstring const& value);

        // DependencyProperty для ButtonSize (Large/Small)
        static Microsoft::UI::Xaml::DependencyProperty ButtonSizeProperty();
        hstring ButtonSize();
        void ButtonSize(hstring const& value);

        // DependencyProperty для Command
        static Microsoft::UI::Xaml::DependencyProperty CommandProperty();
        Microsoft::UI::Xaml::Input::ICommand Command();
        void Command(Microsoft::UI::Xaml::Input::ICommand const& value);

        // DependencyProperty для IsActive (подсветка активной кнопки)
        static Microsoft::UI::Xaml::DependencyProperty IsActiveProperty();
        bool IsActive();
        void IsActive(bool value);

        // Обработчик клика
        void OnButtonClick(Windows::Foundation::IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const& e);

    private:
        static Microsoft::UI::Xaml::DependencyProperty m_labelProperty;
        static Microsoft::UI::Xaml::DependencyProperty m_iconSourceProperty;
        static Microsoft::UI::Xaml::DependencyProperty m_buttonSizeProperty;
        static Microsoft::UI::Xaml::DependencyProperty m_commandProperty;
        static Microsoft::UI::Xaml::DependencyProperty m_isActiveProperty;

        // Обновление визуального состояния при изменении свойств
        void OnLabelChanged();
        void OnIconSourceChanged();
        void OnButtonSizeChanged();
        void OnIsActiveChanged();

        // Инициализация DependencyProperty
        static void InitializeDependencyProperties();
    };
}

namespace winrt::estimate1::Controls::factory_implementation
{
    struct RibbonButton : RibbonButtonT<RibbonButton, implementation::RibbonButton>
    {
    };
}
