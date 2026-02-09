#include "agent_initializer.hpp"
#include "object_tracker.hpp"

#include <QCoreApplication>
#include <QThread>

#include <iostream>

namespace agent {

AgentInitializer::AgentInitializer() noexcept
{
    // [EN] Move this object to the application's main thread.
    //      The Startup hook may be called from any thread context during
    //      QCoreApplication construction.  By moving to the main thread,
    //      we ensure that initAgent() (invoked via QueuedConnection) will
    //      execute in the main thread where it's safe to install event
    //      filters and interact with the widget tree.
    //
    // [RU] Переносим этот объект в главный поток приложения.
    //      Хук Startup может вызываться из любого контекста потока при
    //      конструировании QCoreApplication.  Перенос в главный поток
    //      гарантирует, что initAgent() (вызванный через QueuedConnection)
    //      выполнится в главном потоке, где безопасно устанавливать event
    //      filter и взаимодействовать с деревом виджетов.
    if (QCoreApplication::instance()) {
        moveToThread(QCoreApplication::instance()->thread());
    }

    // [EN] Schedule initAgent() to run on the next event loop iteration.
    //      Qt::QueuedConnection posts an internal QMetaCallEvent to the
    //      event queue.  This event will be processed only when the event
    //      loop is running - i.e. after QCoreApplication is fully constructed,
    //      after main() has set up the UI, and after app.exec() is called.
    //
    //      This is fundamentally different from QTimer::singleShot(0, ...):
    //      - QTimer::singleShot creates a QObject internally → triggers
    //        the AddQObject hook → infinite recursion if called from a hook.
    //      - QMetaObject::invokeMethod with QueuedConnection only posts
    //        an event to an EXISTING object (this) - no new QObject created.
    //
    // [RU] Планируем initAgent() на следующую итерацию event loop.
    //      Qt::QueuedConnection помещает внутренний QMetaCallEvent в очередь
    //      событий.  Это событие будет обработано только когда event loop
    //      запущен - т.е. после полного конструирования QCoreApplication,
    //      после настройки UI в main(), и после вызова app.exec().
    //
    //      Это принципиально отличается от QTimer::singleShot(0, ...):
    //      - QTimer::singleShot создаёт QObject внутри → вызывает хук
    //        AddQObject → бесконечная рекурсия при вызове из хука.
    //      - QMetaObject::invokeMethod с QueuedConnection только помещает
    //        событие в СУЩЕСТВУЮЩИЙ объект (this) - новый QObject не создаётся.
    QMetaObject::invokeMethod(this, "initAgent", Qt::QueuedConnection);
}

void AgentInitializer::initAgent() noexcept
{
    // [EN] Double-check that QCoreApplication is alive.
    //      In rare cases (very early shutdown), it might have been destroyed.
    // [RU] Перепроверяем, что QCoreApplication жив.
    //      В редких случаях (очень ранее завершение) он мог быть уничтожен.
    if (!QCoreApplication::instance()) {
        std::cerr << "[agent] WARNING: QCoreApplication is gone, "
                     "cannot install event tracker." << std::endl;
        deleteLater();
        return;
    }

    // [EN] Verify we're in the main thread - event filter installation
    //      must happen from the thread that owns QCoreApplication.
    // [RU] Проверяем, что мы в главном потоке - установка event filter
    //      должна происходить из потока, владеющего QCoreApplication.
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());

    // [EN] Install the EventTracker - from this point on, all user
    //      interaction events are logged with full object identification.
    // [RU] Устанавливаем EventTracker - с этого момента все события
    //      пользовательского взаимодействия логируются с полной
    //      идентификацией объектов.
    agent::installEventTracker();

    // [EN] This initializer has done its job - schedule destruction.
    //      deleteLater() is safe here because we're in the event loop.
    // [RU] Инициализатор выполнил свою задачу - планируем уничтожение.
    //      deleteLater() безопасен, т.к. мы в event loop.
    deleteLater();
}

} // namespace agent
