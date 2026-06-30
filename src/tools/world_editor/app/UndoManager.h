/*
 * UndoManager - global undo/redo stack for the editor's data models.
 *
 * Every mutation site in MainWindow wraps its model edit in
 * `record(label, [&]{ ...mutate... })`.  The manager captures the model's
 * full state before and after via the model's captureState/restoreState
 * pair, then pushes a frame onto the undo stack and clears the redo
 * stack.  Ctrl+Z pops the top frame, restores the "before" snapshot,
 * and pushes the inverse onto the redo stack.
 *
 * The manager is model-agnostic -- it only sees opaque restore-fn
 * closures, so every model (Annotation, Spawn, Waypoint, Areatrigger,
 * Graveyard, SmartScript) plugs in the same way.  A single global stack
 * means Ctrl+Z always undoes the most recent action regardless of which
 * model was touched, which matches operator expectations from other
 * editors.
 *
 * After every undo / redo the manager fires `refresh()` so MainWindow
 * can push every model back to the viewer + diagnostics docks.
 */

#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

namespace world_editor::app
{

class UndoManager final : public QObject
{
    Q_OBJECT

public:
    explicit UndoManager(QObject* parent = nullptr);

    // Record a mutation.  `mutate` runs synchronously.  `captureBefore`
    // and `captureAfter` are typically lambdas that call the model's
    // captureState() and store the snapshot in a shared_ptr so the
    // restore closures can keep them alive.  `restoreFn` takes a void
    // pointer to the snapshot and applies it via restoreState().
    //
    // Convenience helpers in `recordOn<Model>()` below build these
    // closures from a model's captureState / restoreState pair.
    void push(QString const& label,
              std::function<void()> undoFn,
              std::function<void()> redoFn);

    // Convenience: wrap a mutation on a single model.  `Model` must
    // provide:
    //   StateSnapshot Model::captureState() const;
    //   void          Model::restoreState(StateSnapshot const&);
    // The snapshot type is auto-deduced.  `mutate` is run once
    // synchronously; the frame is pushed atomically with before+after
    // snapshots captured around the call.
    template<typename Model, typename Mutate>
    void recordOn(Model* model, QString const& label, Mutate&& mutate);

    // Variant for mutations that may return bool indicating whether a
    // real change occurred (most model edit functions do).  Skips the
    // frame push entirely when the mutate fn returns false so the undo
    // stack doesn't fill up with no-op refreshes.  Returns mutate()'s
    // result so the caller can decide whether to push downstream
    // refreshes.
    template<typename Model, typename Mutate>
    bool recordIf(Model* model, QString const& label, Mutate&& mutate);

    void undo();
    void redo();
    void clear();
    [[nodiscard]] bool canUndo() const noexcept { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redo.empty(); }
    [[nodiscard]] QString topUndoLabel() const;
    [[nodiscard]] QString topRedoLabel() const;
    [[nodiscard]] size_t  undoDepth() const noexcept { return m_undo.size(); }
    [[nodiscard]] size_t  redoDepth() const noexcept { return m_redo.size(); }

signals:
    // Fired after undo / redo / push so MainWindow can refresh viewer
    // + docks.  Carries the active label for status-bar reporting.
    void stateChanged(QString const& label);

private:
    struct Frame
    {
        QString               label;
        std::function<void()> undoFn;
        std::function<void()> redoFn;
    };
    std::vector<Frame> m_undo;
    std::vector<Frame> m_redo;
    // Cap the stack to keep memory bounded on long sessions.  A full
    // continent has < 100K rows total across all models, so 500 frames
    // is well under 100MB of snapshot RAM in the worst case.
    static constexpr size_t kMaxDepth = 500;
};

template<typename Model, typename Mutate>
void UndoManager::recordOn(Model* model, QString const& label, Mutate&& mutate)
{
    if (!model) { mutate(); return; }
    using Snapshot = decltype(model->captureState());
    auto before = std::make_shared<Snapshot>(model->captureState());
    mutate();
    auto after  = std::make_shared<Snapshot>(model->captureState());
    push(label,
         [model, before]() { model->restoreState(*before); },
         [model, after]()  { model->restoreState(*after);  });
}

template<typename Model, typename Mutate>
bool UndoManager::recordIf(Model* model, QString const& label, Mutate&& mutate)
{
    if (!model) return mutate();
    using Snapshot = decltype(model->captureState());
    auto before = std::make_shared<Snapshot>(model->captureState());
    bool const changed = mutate();
    if (!changed) return false;
    auto after = std::make_shared<Snapshot>(model->captureState());
    push(label,
         [model, before]() { model->restoreState(*before); },
         [model, after]()  { model->restoreState(*after);  });
    return true;
}

} // namespace world_editor::app
