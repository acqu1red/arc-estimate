#include "pch.h"
#include "Controls/RibbonButton.h"
#if __has_include("Controls.RibbonButton.g.cpp")
#include "Controls.RibbonButton.g.cpp"
#endif

namespace winrt::estimate1::Controls::implementation
{
    using winrt::Microsoft::UI::Xaml::DependencyProperty;
    using winrt::Microsoft::UI::Xaml::DependencyObject;
    using winrt::Microsoft::UI::Xaml::PropertyMetadata;
    using winrt::Microsoft::UI::Xaml::PropertyChangedCallback;
    using winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs;
    using winrt::Microsoft::UI::Xaml::RoutedEventArgs;
    using winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage;
    using winrt::Microsoft::UI::Xaml::Visibility;
    using winrt::Microsoft::UI::Xaml::Style;
    using winrt::Windows::Foundation::Uri;
    using winrt::Windows::UI::ColorHelper;
    using winrt::Windows::UI::Colors;
    // Статические DependencyProperty
    DependencyProperty RibbonButton::m_labelProperty = nullptr;
    DependencyProperty RibbonButton::m_iconSourceProperty = nullptr;
    DependencyProperty RibbonButton::m_buttonSizeProperty = nullptr;
    DependencyProperty RibbonButton::m_commandProperty = nullptr;
    DependencyProperty RibbonButton::m_isActiveProperty = nullptr;

    RibbonButton::RibbonButton()
    {
        InitializeComponent();
        InitializeDependencyProperties();

        // По умолчанию маленькая кнопка
        ButtonSize(L"Small");
    }

    void RibbonButton::InitializeDependencyProperties()
    {
        if (m_labelProperty == nullptr)
        {
            m_labelProperty = DependencyProperty::Register(
                L"Label",
                xaml_typename<hstring>(),
                xaml_typename<Controls::RibbonButton>(),
                PropertyMetadata(box_value(L"Button"),
                    PropertyChangedCallback([](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
                    {
                        if (auto btn = d.try_as<Controls::RibbonButton>())
                        {
                            get_self<RibbonButton>(btn)->OnLabelChanged();
                        }
                    })));
        }

        if (m_iconSourceProperty == nullptr)
        {
            m_iconSourceProperty = DependencyProperty::Register(
                L"IconSource",
                xaml_typename<hstring>(),
                xaml_typename<Controls::RibbonButton>(),
                PropertyMetadata(box_value(L""),
                    PropertyChangedCallback([](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
                    {
                        if (auto btn = d.try_as<Controls::RibbonButton>())
                        {
                            get_self<RibbonButton>(btn)->OnIconSourceChanged();
                        }
                    })));
        }

        if (m_buttonSizeProperty == nullptr)
        {
            m_buttonSizeProperty = DependencyProperty::Register(
                L"ButtonSize",
                xaml_typename<hstring>(),
                xaml_typename<Controls::RibbonButton>(),
                PropertyMetadata(box_value(L"Small"),
                    PropertyChangedCallback([](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
                    {
                        if (auto btn = d.try_as<Controls::RibbonButton>())
                        {
                            get_self<RibbonButton>(btn)->OnButtonSizeChanged();
                        }
                    })));
        }

        if (m_commandProperty == nullptr)
        {
            m_commandProperty = DependencyProperty::Register(
                L"Command",
                xaml_typename<Microsoft::UI::Xaml::Input::ICommand>(),
                xaml_typename<Controls::RibbonButton>(),
                PropertyMetadata(nullptr));
        }

        if (m_isActiveProperty == nullptr)
        {
            m_isActiveProperty = DependencyProperty::Register(
                L"IsActive",
                xaml_typename<bool>(),
                xaml_typename<Controls::RibbonButton>(),
                PropertyMetadata(box_value(false),
                    PropertyChangedCallback([](DependencyObject const& d, DependencyPropertyChangedEventArgs const&)
                    {
                        if (auto btn = d.try_as<Controls::RibbonButton>())
                        {
                            get_self<RibbonButton>(btn)->OnIsActiveChanged();
                        }
                    })));
        }
    }

    DependencyProperty RibbonButton::LabelProperty() { return m_labelProperty; }
    hstring RibbonButton::Label() { return unbox_value<hstring>(GetValue(m_labelProperty)); }
    void RibbonButton::Label(hstring const& value) { SetValue(m_labelProperty, box_value(value)); }

    DependencyProperty RibbonButton::IconSourceProperty() { return m_iconSourceProperty; }
    hstring RibbonButton::IconSource() { return unbox_value<hstring>(GetValue(m_iconSourceProperty)); }
    void RibbonButton::IconSource(hstring const& value) { SetValue(m_iconSourceProperty, box_value(value)); }

    DependencyProperty RibbonButton::ButtonSizeProperty() { return m_buttonSizeProperty; }
    hstring RibbonButton::ButtonSize() { return unbox_value<hstring>(GetValue(m_buttonSizeProperty)); }
    void RibbonButton::ButtonSize(hstring const& value) { SetValue(m_buttonSizeProperty, box_value(value)); }

    DependencyProperty RibbonButton::CommandProperty() { return m_commandProperty; }
    Microsoft::UI::Xaml::Input::ICommand RibbonButton::Command() 
    { 
        auto value = GetValue(m_commandProperty);
        return value ? value.as<Microsoft::UI::Xaml::Input::ICommand>() : nullptr;
    }
    void RibbonButton::Command(Microsoft::UI::Xaml::Input::ICommand const& value) { SetValue(m_commandProperty, value); }

    DependencyProperty RibbonButton::IsActiveProperty() { return m_isActiveProperty; }
    bool RibbonButton::IsActive() { return unbox_value<bool>(GetValue(m_isActiveProperty)); }
    void RibbonButton::IsActive(bool value) { SetValue(m_isActiveProperty, box_value(value)); }

    void RibbonButton::OnLabelChanged()
    {
        hstring label = Label();
        if (LargeText())
            LargeText().Text(label);
        if (SmallText())
            SmallText().Text(label);
    }

    void RibbonButton::OnIconSourceChanged()
    {
        hstring iconPath = IconSource();
        if (!iconPath.empty())
        {
            BitmapImage bitmap;
            bitmap.UriSource(Uri(iconPath));

            if (LargeIcon())
                LargeIcon().Source(bitmap);
            if (SmallIcon())
                SmallIcon().Source(bitmap);
        }
    }

    void RibbonButton::OnButtonSizeChanged()
    {
        hstring size = ButtonSize();
        
        if (size == L"Large")
        {
            // Показываем большой layout
            if (LargeLayout())
                LargeLayout().Visibility(Visibility::Visible);
            if (SmallLayout())
                SmallLayout().Visibility(Visibility::Collapsed);
            
            // Применяем стиль большой кнопки
            if (RibbonBtn())
            {
                auto style = Resources().TryLookup(box_value(L"LargeRibbonButtonStyle"));
                if (style)
                    RibbonBtn().Style(style.as<Style>());
            }
        }
        else // Small
        {
            // Показываем маленький layout
            if (SmallLayout())
                SmallLayout().Visibility(Visibility::Visible);
            if (LargeLayout())
                LargeLayout().Visibility(Visibility::Collapsed);
            
            // Применяем стиль маленькой кнопки
            if (RibbonBtn())
            {
                auto style = Resources().TryLookup(box_value(L"SmallRibbonButtonStyle"));
                if (style)
                    RibbonBtn().Style(style.as<Style>());
            }
        }
    }

    void RibbonButton::OnIsActiveChanged()
    {
        // Обновляем внешний вид кнопки при изменении IsActive
        if (RibbonBtn())
        {
            if (IsActive())
            {
                // Активная кнопка - голубой фон
                RibbonBtn().Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::ColorHelper::FromArgb(100, 0, 120, 215)));
                RibbonBtn().BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::ColorHelper::FromArgb(255, 0, 120, 215)));
            }
            else
            {
                // Неактивная кнопка - прозрачная
                RibbonBtn().Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::Colors::Transparent()));
                RibbonBtn().BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::Colors::Transparent()));
            }
        }
    }

    void RibbonButton::OnButtonClick(Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        // Выполняем команду если есть
        auto command = Command();
        if (command && command.CanExecute(nullptr))
        {
            command.Execute(nullptr);
        }
    }
}
