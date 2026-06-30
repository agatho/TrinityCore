#include "UndoManager.h"

namespace world_editor::app
{

UndoManager::UndoManager(QObject* parent)
    : QObject(parent)
{}

void UndoManager::push(QString const& label,
                       std::function<void()> undoFn,
                       std::function<void()> redoFn)
{
    m_undo.push_back(Frame{ label, std::move(undoFn), std::move(redoFn) });
    if (m_undo.size() > kMaxDepth)
        m_undo.erase(m_undo.begin(), m_undo.begin() + (m_undo.size() - kMaxDepth));
    m_redo.clear();
    emit stateChanged(label);
}

void UndoManager::undo()
{
    if (m_undo.empty()) return;
    Frame f = std::move(m_undo.back());
    m_undo.pop_back();
    f.undoFn();
    QString const label = f.label;
    m_redo.push_back(std::move(f));
    emit stateChanged(QStringLiteral("undo: ") + label);
}

void UndoManager::redo()
{
    if (m_redo.empty()) return;
    Frame f = std::move(m_redo.back());
    m_redo.pop_back();
    f.redoFn();
    QString const label = f.label;
    m_undo.push_back(std::move(f));
    emit stateChanged(QStringLiteral("redo: ") + label);
}

void UndoManager::clear()
{
    m_undo.clear();
    m_redo.clear();
    emit stateChanged(QString());
}

QString UndoManager::topUndoLabel() const
{
    return m_undo.empty() ? QString() : m_undo.back().label;
}

QString UndoManager::topRedoLabel() const
{
    return m_redo.empty() ? QString() : m_redo.back().label;
}

} // namespace world_editor::app
