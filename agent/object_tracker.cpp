#include "object_tracker.hpp"

#include <QCoreApplication>
#include <QWidget>
#include <QQuickItem>
#include <iostream>
#include <QRemoteObjectNode>
#include <QRemoteObjectReplica>

#include "object_path.hpp"
#include "rep_RemoteObjectExample_replica.h"

#ifdef Q_OS_ANDROID
#include "adb_socket_proxy.hpp"
#endif

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
#ifdef Q_OS_ANDROID
    // [EN] On Android we cannot use TCP (requires INTERNET permission)
    //      and "local:" sockets won't reach the PC.
    //
    //      Solution: AdbSocketProxy creates:
    //        - An abstract Unix domain socket "ro_agent" (AF_UNIX, no permission needed)
    //        - A QLocalServer "ro_demo" bridged to it
    //
    //      ADB on the PC side:
    //        adb forward tcp:65511 localabstract:ro_agent
    //
    //      ro_host on PC listens on tcp://0.0.0.0:65511.
    //      The proxy transparently bridges ADB↔QLocalServer.
    //      Our QRemoteObjectNode connects to "local:ro_demo" — pure IPC,
    //      no INTERNET permission required.
    //
    // [RU] На Android нельзя использовать TCP (требуется INTERNET permission),
    //      а "local:" сокеты не достигнут ПК.
    //
    //      Решение: AdbSocketProxy создаёт:
    //        - Абстрактный Unix domain socket "ro_agent" (AF_UNIX, permission не нужен)
    //        - QLocalServer "ro_demo", связанный мостом с ним
    //
    //      ADB на стороне ПК:
    //        adb forward tcp:65511 localabstract:ro_agent
    //
    //      ro_host на ПК слушает на tcp://0.0.0.0:65511.
    //      Прокси прозрачно связывает ADB↔QLocalServer.
    //      Наш QRemoteObjectNode подключается к "local:ro_demo" — чистый IPC,
    //      INTERNET permission не требуется.

    m_proxy = new AdbSocketProxy(
        QStringLiteral("ro_agent"),   // abstract socket name for ADB
        QStringLiteral("ro_demo"),    // QLocalServer name for QtRO replica
        this);

    QObject::connect(m_proxy, &AdbSocketProxy::ready, this, [this]() {
        qInfo() << "[agent] proxy ready, connecting RemoteObjects to local:ro_demo";
        connectRemoteObjects();
    });
    QObject::connect(m_proxy, &AdbSocketProxy::errorOccurred, this, [](const QString &msg) {
        qWarning() << "[agent] proxy error:" << msg;
    });
    m_proxy->start();

#else
    // [EN] On desktop: direct connection via local: or tcp:// as before.
    //      Controlled by AGENT_RO_URL env var, default "local:demo".
    // [RU] На десктопе: прямое подключение через local: или tcp:// как раньше.
    //      Управляется переменной окружения AGENT_RO_URL, по умолчанию "local:demo".
    connectRemoteObjects();
#endif
}

void EventTracker::connectRemoteObjects()
{
    QString urlStr = qEnvironmentVariable("AGENT_RO_URL");
    if (urlStr.isEmpty()) {
#ifdef Q_OS_ANDROID
        // [EN] On Android the proxy provides a local server at "ro_demo".
        // [RU] На Android прокси предоставляет локальный сервер "ro_demo".
        urlStr = QStringLiteral("local:ro_demo");
#else
        urlStr = QStringLiteral("local:demo");
#endif
    }

    const QUrl roUrl(urlStr);
    qInfo() << "[agent] connecting RemoteObjects to" << roUrl;

    m_roNode = new QRemoteObjectNode(this);
    m_roNode->connectToNode(roUrl);

    m_roReplica.reset(m_roNode->acquire<ROExampleReplica>());
    if (m_roReplica) {
        QObject::connect(m_roReplica.get(), &QRemoteObjectReplica::initialized, this, [this]() {
            qInfo() << "[agent] ROExampleReplica initialized";
            m_roReplica->pong(QStringLiteral("agent connected"));
        });

        QObject::connect(m_roReplica.get(), &ROExampleReplica::messageChanged, this, [this]() {
            qInfo() << "[agent] RO message:" << m_roReplica->message();
        });
        QObject::connect(m_roReplica.get(), &ROExampleReplica::ping, this, [](int seq) {
            qInfo() << "[agent] RO ping:" << seq;
        });

        QObject::connect(m_roReplica.get(), &QRemoteObjectReplica::stateChanged, this,
            [](QRemoteObjectReplica::State newState, QRemoteObjectReplica::State oldState) {
                qInfo() << "[agent] RO replica state:"
                        << oldState << "->" << newState;
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
