#include "object_tracker.hpp"

#include <QCoreApplication>
#include <QWidget>
#include <QQuickItem>
#include <iostream>
#include <QRemoteObjectNode>
#include <QRemoteObjectReplica>

#include "object_path.hpp"
#include "rep_RemoteObjectExample_replica.h"

namespace agent {

// [EN] Returns a human-readable name for user interaction event types.
//      Returns nullptr for event types we ignore - the caller uses this
//      as a filter: if nullptr, skip the event.
//
//      We deliberately limit ourselves to direct user interaction events
//      and visibility changes.  System events (Paint, Timer, MetaCall,
//      LayoutRequest, etc.) are too noisy and carry no useful information
//      for test script generation.
//
// [RU] Возвращает человекочитаемое имя для типов событий пользовательского
//      взаимодействия.  Возвращает nullptr для типов, которые мы игнорируем -
//      вызывающий использует это как фильтр: если nullptr, пропускаем.
//
//      Мы намеренно ограничиваемся событиями прямого пользовательского
//      взаимодействия и изменениями видимости.  Системные события (Paint,
//      Timer, MetaCall, LayoutRequest и т.д.) слишком шумные и не несут
//      полезной информации для генерации тестовых скриптов.
static const char *interactionEventName(QEvent::Type type) noexcept
{
    switch (type) {
    // ── Mouse / мышь ─────────────────────────────────────────────────────
    case QEvent::MouseButtonPress:    return "MouseButtonPress";
    case QEvent::MouseButtonRelease:  return "MouseButtonRelease";
    case QEvent::MouseButtonDblClick: return "MouseButtonDblClick";

    case QEvent::TouchBegin:          return "TouchBegin";
    case QEvent::TouchEnd:            return "TouchEnd";


    // ── Keyboard / клавиатура ────────────────────────────────────────────
    case QEvent::KeyPress:            return "KeyPress";
    case QEvent::KeyRelease:          return "KeyRelease";

    // ── Focus / фокус ────────────────────────────────────────────────────
    case QEvent::FocusIn:             return "FocusIn";
    case QEvent::FocusOut:            return "FocusOut";

    // ── Visibility / видимость ───────────────────────────────────────────
    case QEvent::Show:                return "Show";
    case QEvent::Hide:                return "Hide";
    case QEvent::Close:               return "Close";

    // ── Everything else - ignore / всё остальное - игнорируем ────────────
    default:                          return nullptr;
    }
}

// [EN] Returns true if the object is a GUI component: QWidget (Widgets)
//      or QQuickItem (Quick/QML).
//
//      This function is only called from the event filter, where the
//      object is guaranteed to be fully constructed - vtable is correct,
//      qobject_cast works properly.
//
// [RU] Возвращает true, если объект - GUI-компонент: QWidget (Widgets)
//      или QQuickItem (Quick/QML).
//
//      Функция вызывается только из event filter, где объект гарантированно
//      полностью сконструирован - vtable корректен, qobject_cast работает
//      правильно.
static bool isGuiObject(const QObject *obj) noexcept
{
    return qobject_cast<const QWidget *>(obj) != nullptr
           || qobject_cast<const QQuickItem *>(obj) != nullptr;
}

// ── EventTracker implementation / реализация EventTracker ────────────────────

EventTracker::EventTracker(QObject *parent)
    : QObject(parent)
{
    m_roNode = new QRemoteObjectNode(this);
    m_roNode->connectToNode(QUrl(QStringLiteral("local:demo")));

    m_roReplica.reset(m_roNode->acquire<ROExampleReplica>());
    if (m_roReplica) {
        QObject::connect(m_roReplica.get(), &QRemoteObjectReplica::initialized, this, [this]() {
            qInfo() << "[agent] ROExampleReplica initialized";
            // Call a remote slot on the host as a handshake.
            m_roReplica->pong(QStringLiteral("agent connected"));
        });

        QObject::connect(m_roReplica.get(), &ROExampleReplica::messageChanged, this, [this]() {
            qInfo() << "[agent] RO message:" << m_roReplica->message();
        });
        QObject::connect(m_roReplica.get(), &ROExampleReplica::ping, this, [](int seq) {
            qInfo() << "[agent] RO ping:" << seq;
        });
    }
}

bool EventTracker::eventFilter(QObject *watched, QEvent *event)
{
    // [EN] Step 1: Filter by event type.
    //      interactionEventName returns nullptr for non-interaction events.
    //      This is the fast path - the vast majority of events (Paint, Timer,
    //      MetaCall, UpdateRequest, etc.) are rejected here with a single
    //      switch-case lookup, adding negligible overhead to the application.
    //
    // [RU] Шаг 1: Фильтрация по типу события.
    //      interactionEventName возвращает nullptr для не-интерактивных событий.
    //      Это быстрый путь - подавляющее большинство событий (Paint, Timer,
    //      MetaCall, UpdateRequest и т.д.) отклоняются здесь одним switch-case,
    //      добавляя пренебрежимую нагрузку на приложение.
    const auto *eventName = interactionEventName(event->type());
    if (!eventName)
        return false;

    // [EN] Step 2: Filter by object type.
    //      We only care about GUI components - QWidget and QQuickItem.
    //      Internal Qt objects (QAction, QTimer, QShortcutMap, etc.) that
    //      occasionally receive forwarded events are ignored.
    //
    // [RU] Шаг 2: Фильтрация по типу объекта.
    //      Нас интересуют только GUI-компоненты - QWidget и QQuickItem.
    //      Внутренние объекты Qt (QAction, QTimer, QShortcutMap и т.д.),
    //      которым иногда перенаправляются события, игнорируются.
    if (!isGuiObject(watched))
        return false;

    // [EN] Step 3: Identify and log.
    //      At this point:
    //      - The object is fully constructed (events only arrive after construction)
    //      - vtable is correct → metaObject()->className() returns the real class
    //      - objectName() is set (if the developer assigned it)
    //      - parent/children hierarchy is complete
    //      - The event is a user interaction event
    //
    // [RU] Шаг 3: Идентификация и логирование.
    //      На этом этапе:
    //      - Объект полностью сконструирован (события приходят только после конструирования)
    //      - vtable корректен → metaObject()->className() возвращает реальный класс
    //      - objectName() задан (если разработчик его назначил)
    //      - иерархия parent/children сформирована
    //      - Событие является пользовательским взаимодействием
    const auto cls  = agent::getCorrectClassName(watched);
    const auto name = watched->objectName();
    const auto path = agent::objectPath(watched);

    const auto displayName = name.isEmpty() ? QStringLiteral("-") : name;

    // std::cerr << "[*] " << eventName
    //           << "  " << cls.toUtf8().constData()
    //           << "  name=" << displayName.toUtf8().constData()
    //           << "  path=" << path.toUtf8().constData()
    //           << std::endl;
    qInfo() << "[*] " << eventName
              << "  " << cls.toUtf8().constData()
              << "  name=" << displayName.toUtf8().constData()
              << "  path=" << path.toUtf8().constData();

    // [EN] Never consume the event.  We are a passive observer - the event
    //      must reach its intended handler in the application.
    // [RU] Никогда не поглощаем событие.  Мы пассивный наблюдатель - событие
    //      должно дойти до своего обработчика в приложении.
    return false;
}

// ── installation / установка ─────────────────────────────────────────────────

void installEventTracker()
{
    auto *app = QCoreApplication::instance();
    if (!app) {
        std::cerr << "[agent] WARNING: QCoreApplication not available, "
                     "event tracker NOT installed." << std::endl;
        return;
    }

    // [EN] The EventTracker is parented to QCoreApplication - automatic
    //      cleanup on application shutdown.
    //
    //      installEventFilter on QCoreApplication intercepts ALL events
    //      for ALL objects: QCoreApplication::notify() passes every event
    //      through installed filters before delivering it to the target.
    //
    // [RU] EventTracker привязан к QCoreApplication как parent -
    //      автоматическая очистка при завершении приложения.
    //
    //      installEventFilter на QCoreApplication перехватывает ВСЕ события
    //      для ВСЕХ объектов: QCoreApplication::notify() пропускает каждое
    //      событие через установленные фильтры перед доставкой получателю.
    auto *tracker = new EventTracker(app);
    app->installEventFilter(tracker);

    std::cerr << "[agent] event tracker installed.\n"
                 "[agent] interact with GUI elements to see their paths."
              << std::endl;
}

} // namespace agent
