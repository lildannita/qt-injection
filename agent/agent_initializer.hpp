#pragma once

#include <QObject>

// [EN] Created in the Startup hook (during QCoreApplication construction).
//      Immediately moves itself to the application's main thread and schedules
//      initAgent() via QMetaObject::invokeMethod with Qt::QueuedConnection.
//
//      Qt::QueuedConnection means the call is posted as an event to the main
//      thread's event queue.  It will execute on the NEXT event loop iteration,
//      after QCoreApplication construction is fully complete and the event loop
//      is running.
//
//      Once initAgent() runs, it creates the EventTracker (event filter) and
//      deletes this initializer object (deleteLater).
//
//      Why not just install the event filter in the Startup hook directly?
//          - The Startup hook fires during QCoreApplication construction.
//          - QCoreApplication is not yet fully initialized at that point.
//          - Installing an event filter on a half-constructed QCoreApplication
//          can cause crashes or missed events.
//          - QueuedConnection guarantees we run after construction is complete.
//
// [RU] Создаётся в хуке Startup (при конструировании QCoreApplication).
//      Немедленно переносится в главный поток приложения и планирует вызов
//      initAgent() через QMetaObject::invokeMethod с Qt::QueuedConnection.
//
//      Qt::QueuedConnection означает, что вызов помещается как событие в
//      очередь событий главного потока.  Он выполнится на СЛЕДУЮЩЕЙ итерации
//      event loop, после полного завершения конструирования QCoreApplication
//      и запуска event loop.
//
//      Когда initAgent() выполняется, он создаёт EventTracker (event filter)
//      и удаляет этот объект-инициализатор (deleteLater).
//
//      Почему нельзя установить event filter прямо в хуке Startup?
//          - Хук Startup срабатывает при конструировании QCoreApplication.
//          - QCoreApplication ещё не полностью инициализирован в этот момент.
//          - Установка event filter на полуконструированный QCoreApplication
//          может вызвать ошибки или пропуск событий.
//          - QueuedConnection гарантирует выполнение после завершения конструирования.

namespace agent {
class AgentInitializer final : public QObject
{
    Q_OBJECT

public:
    // [EN] Constructor: moves to main thread and schedules initAgent().
    //      Must be called from the Startup hook.
    // [RU] Конструктор: переносит в главный поток и планирует initAgent().
    //      Должен вызываться из хука Startup.
    AgentInitializer() noexcept;

private slots:
    // [EN] Called on the next event loop iteration (Qt::QueuedConnection).
    //      Installs the EventTracker and destroys this initializer.
    // [RU] Вызывается на следующей итерации event loop (Qt::QueuedConnection).
    //      Устанавливает EventTracker и уничтожает этот инициализатор.
    void initAgent() noexcept;
};
} // namespace agent
