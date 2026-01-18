#pragma once

// ARC-Estimate Undo/Redo System (M9 - brought forward)
// Система отмены/повтора действий

#include "pch.h"
#include "Camera.h"
#include "Element.h"
#include <vector>
#include <memory>
#include <stack>
#include <functional>

namespace winrt::estimate1
{
    // Тип команды
    enum class CommandType
    {
        AddWall,
        RemoveWall,
        ModifyWall,
        AddDimension,
        RemoveDimension,
        ModifyDimension,
        // Можно добавить другие типы по мере необходимости
    };

    // Базовый класс команды для Undo/Redo
    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual CommandType GetType() const = 0;
        virtual std::wstring GetDescription() const = 0;
    };

    // Команда добавления стены
    class AddWallCommand : public ICommand
    {
    public:
        AddWallCommand(
            DocumentModel& document,
            const WorldPoint& start,
            const WorldPoint& end,
            double thickness,
            WorkStateNative workState,
            LocationLineMode locationLine,
            const std::wstring& wallTypeName = L"")
            : m_document(document)
            , m_start(start)
            , m_end(end)
            , m_thickness(thickness)
            , m_workState(workState)
            , m_locationLine(locationLine)
            , m_wallTypeName(wallTypeName)
        {
        }

        void Execute() override
        {
            Wall* wall = m_document.AddWall(m_start, m_end, m_thickness);
            if (wall)
            {
                wall->SetWorkState(m_workState);
                wall->SetLocationLineMode(m_locationLine);
                if (!m_wallTypeName.empty())
                {
                    auto wt = m_document.GetWallTypeByName(m_wallTypeName);
                    if (wt) wall->SetType(wt);
                }
                m_wallId = wall->GetId();
            }
        }

        void Undo() override
        {
            if (m_wallId != 0)
            {
                m_document.RemoveWall(m_wallId);
            }
        }

        CommandType GetType() const override { return CommandType::AddWall; }
        std::wstring GetDescription() const override { return L"Добавить стену"; }

        uint64_t GetWallId() const { return m_wallId; }

    private:
        DocumentModel& m_document;
        WorldPoint m_start;
        WorldPoint m_end;
        double m_thickness;
        WorkStateNative m_workState;
        LocationLineMode m_locationLine;
        std::wstring m_wallTypeName;
        uint64_t m_wallId{ 0 };
    };

    // Команда удаления стены
    class RemoveWallCommand : public ICommand
    {
    public:
        RemoveWallCommand(DocumentModel& document, uint64_t wallId)
            : m_document(document)
            , m_wallId(wallId)
        {
            // Сохраняем данные стены для возможного восстановления
            Wall* wall = m_document.GetWall(wallId);
            if (wall)
            {
                m_start = wall->GetStartPoint();
                m_end = wall->GetEndPoint();
                m_thickness = wall->GetThickness();
                m_height = wall->GetHeight();
                m_workState = wall->GetWorkState();
                m_locationLine = wall->GetLocationLineMode();
                m_allowJoinStart = wall->IsJoinAllowedAtStart();
                m_allowJoinEnd = wall->IsJoinAllowedAtEnd();
                if (wall->GetType())
                {
                    m_wallTypeName = wall->GetType()->GetName();
                }
            }
        }

        void Execute() override
        {
            m_document.RemoveWall(m_wallId);
        }

        void Undo() override
        {
            // Восстанавливаем стену
            Wall* wall = m_document.AddWall(m_start, m_end, m_thickness);
            if (wall)
            {
                wall->SetHeight(m_height);
                wall->SetWorkState(m_workState);
                wall->SetLocationLineMode(m_locationLine);
                wall->SetJoinAllowedAtStart(m_allowJoinStart);
                wall->SetJoinAllowedAtEnd(m_allowJoinEnd);
                if (!m_wallTypeName.empty())
                {
                    auto wt = m_document.GetWallTypeByName(m_wallTypeName);
                    if (wt) wall->SetType(wt);
                }
                // Присваиваем тот же ID (если возможно — для связи с размерами)
                // Note: В текущей реализации ID генерируется автоматически
            }
        }

        CommandType GetType() const override { return CommandType::RemoveWall; }
        std::wstring GetDescription() const override { return L"Удалить стену"; }

    private:
        DocumentModel& m_document;
        uint64_t m_wallId;
        WorldPoint m_start;
        WorldPoint m_end;
        double m_thickness{ 150.0 };
        double m_height{ 2700.0 };
        WorkStateNative m_workState{ WorkStateNative::Existing };
        LocationLineMode m_locationLine{ LocationLineMode::WallCenterline };
        bool m_allowJoinStart{ true };
        bool m_allowJoinEnd{ true };
        std::wstring m_wallTypeName;
    };

    // Команда изменения стены
    class ModifyWallCommand : public ICommand
    {
    public:
        ModifyWallCommand(DocumentModel& document, uint64_t wallId)
            : m_document(document)
            , m_wallId(wallId)
        {
            // Сохраняем текущее состояние стены
            Wall* wall = m_document.GetWall(wallId);
            if (wall)
            {
                m_oldStart = wall->GetStartPoint();
                m_oldEnd = wall->GetEndPoint();
                m_oldThickness = wall->GetThickness();
                m_oldWorkState = wall->GetWorkState();
                m_oldLocationLine = wall->GetLocationLineMode();
                if (wall->GetType())
                {
                    m_oldWallTypeName = wall->GetType()->GetName();
                }
            }
        }

        void SetNewState(
            const WorldPoint& start,
            const WorldPoint& end,
            double thickness,
            WorkStateNative workState,
            LocationLineMode locationLine,
            const std::wstring& wallTypeName)
        {
            m_newStart = start;
            m_newEnd = end;
            m_newThickness = thickness;
            m_newWorkState = workState;
            m_newLocationLine = locationLine;
            m_newWallTypeName = wallTypeName;
        }

        void Execute() override
        {
            Wall* wall = m_document.GetWall(m_wallId);
            if (wall)
            {
                wall->SetStartPoint(m_newStart);
                wall->SetEndPoint(m_newEnd);
                wall->SetThickness(m_newThickness);
                wall->SetWorkState(m_newWorkState);
                wall->SetLocationLineMode(m_newLocationLine);
                if (!m_newWallTypeName.empty())
                {
                    auto wt = m_document.GetWallTypeByName(m_newWallTypeName);
                    if (wt) wall->SetType(wt);
                }
            }
        }

        void Undo() override
        {
            Wall* wall = m_document.GetWall(m_wallId);
            if (wall)
            {
                wall->SetStartPoint(m_oldStart);
                wall->SetEndPoint(m_oldEnd);
                wall->SetThickness(m_oldThickness);
                wall->SetWorkState(m_oldWorkState);
                wall->SetLocationLineMode(m_oldLocationLine);
                if (!m_oldWallTypeName.empty())
                {
                    auto wt = m_document.GetWallTypeByName(m_oldWallTypeName);
                    if (wt) wall->SetType(wt);
                }
            }
        }

        CommandType GetType() const override { return CommandType::ModifyWall; }
        std::wstring GetDescription() const override { return L"Изменить стену"; }

    private:
        DocumentModel& m_document;
        uint64_t m_wallId;
        
        // Старое состояние
        WorldPoint m_oldStart;
        WorldPoint m_oldEnd;
        double m_oldThickness{ 150.0 };
        WorkStateNative m_oldWorkState{ WorkStateNative::Existing };
        LocationLineMode m_oldLocationLine{ LocationLineMode::WallCenterline };
        std::wstring m_oldWallTypeName;
        
        // Новое состояние
        WorldPoint m_newStart;
        WorldPoint m_newEnd;
        double m_newThickness{ 150.0 };
        WorkStateNative m_newWorkState{ WorkStateNative::Existing };
        LocationLineMode m_newLocationLine{ LocationLineMode::WallCenterline };
        std::wstring m_newWallTypeName;
    };

    // Менеджер Undo/Redo
    class UndoManager
    {
    public:
        UndoManager() = default;

        // Выполнить команду и добавить её в историю
        void Execute(std::unique_ptr<ICommand> command)
        {
            if (!command) return;
            
            command->Execute();
            m_undoStack.push(std::move(command));
            
            // Очищаем Redo стек при новом действии
            while (!m_redoStack.empty())
            {
                m_redoStack.pop();
            }
            
            NotifyChanged();
        }

        // Отменить последнее действие
        bool Undo()
        {
            if (m_undoStack.empty()) return false;
            
            auto command = std::move(m_undoStack.top());
            m_undoStack.pop();
            
            command->Undo();
            m_redoStack.push(std::move(command));
            
            NotifyChanged();
            return true;
        }

        // Повторить отменённое действие
        bool Redo()
        {
            if (m_redoStack.empty()) return false;
            
            auto command = std::move(m_redoStack.top());
            m_redoStack.pop();
            
            command->Execute();
            m_undoStack.push(std::move(command));
            
            NotifyChanged();
            return true;
        }

        // Проверка доступности операций
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        // Очистить всю историю
        void Clear()
        {
            while (!m_undoStack.empty()) m_undoStack.pop();
            while (!m_redoStack.empty()) m_redoStack.pop();
            NotifyChanged();
        }

        // Получить описание следующей операции Undo
        std::wstring GetUndoDescription() const
        {
            if (m_undoStack.empty()) return L"";
            return m_undoStack.top()->GetDescription();
        }

        // Получить описание следующей операции Redo
        std::wstring GetRedoDescription() const
        {
            if (m_redoStack.empty()) return L"";
            return m_redoStack.top()->GetDescription();
        }

        // Колбэк при изменении стеков
        void SetOnChanged(std::function<void()> callback)
        {
            m_onChanged = callback;
        }

    private:
        void NotifyChanged()
        {
            if (m_onChanged) m_onChanged();
        }

        std::stack<std::unique_ptr<ICommand>> m_undoStack;
        std::stack<std::unique_ptr<ICommand>> m_redoStack;
        std::function<void()> m_onChanged;
    };
}
