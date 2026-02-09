#include <QObject>
#include <QCoreApplication>
#include <private/qhooks_p.h>
#include <iostream>

#include "agent_initializer.hpp"

// [EN] This is the entry point of the agent shared library.  It hooks into
//      Qt's qtHookData mechanism and uses the Startup hook to create an
//      AgentInitializer, which defers EventTracker installation to the next
//      event loop iteration via Qt::QueuedConnection.
//
// Architecture:
//
//   Q_COREAPP_STARTUP_FUNCTION
//     └─ installHooks()
//          └─ overwrites qtHookData slots
//
//   QCoreApplication constructor calls Startup hook:
//     └─ hookStartup()
//          └─ creates AgentInitializer (→ moveToThread → QueuedConnection)
//
//   Event loop starts (app.exec()):
//     └─ AgentInitializer::initAgent() fires
//          └─ installEventTracker()
//               └─ EventTracker::eventFilter() now active
//
//   User clicks a button:
//     └─ QCoreApplication::notify() → EventTracker::eventFilter()
//          └─ logs: [*] MouseButtonPress  QPushButton  name=btnGreet  path=...
//
// [RU] Точка входа разделяемой библиотеки агента.  Подключается к механизму
//      qtHookData Qt и использует хук Startup для создания AgentInitializer,
//      который откладывает установку EventTracker на следующую итерацию
//      event loop через Qt::QueuedConnection.
//
// Архитектура:
//
//   Q_COREAPP_STARTUP_FUNCTION
//     └─ installHooks()
//          └─ перезаписывает слоты qtHookData
//
//   Конструктор QCoreApplication вызывает хук Startup:
//     └─ hookStartup()
//          └─ создаёт AgentInitializer (→ moveToThread → QueuedConnection)
//
//   Event loop запускается (app.exec()):
//     └─ AgentInitializer::initAgent() срабатывает
//          └─ installEventTracker()
//               └─ EventTracker::eventFilter() теперь активен
//
//   Пользователь кликает на кнопку:
//     └─ QCoreApplication::notify() → EventTracker::eventFilter()
//          └─ логирует: [*] MouseButtonPress  QPushButton  name=btnGreet  path=...

// ── original hook pointers ───────────────────────────────────────────────────
// [EN] Saved before we overwrite the slots.  May be nullptr (no previous hook)
//      or point to another tool's callback (e.g. GammaRay).
// [RU] Сохраняются до перезаписи слотов.  Могут быть nullptr (хука не было)
//      или указывать на callback другого инструмента (напр. GammaRay).
static void (*g_nextStartup)()              = nullptr;
static void (*g_nextAddObject)(QObject*)    = nullptr;
static void (*g_nextRemoveObject)(QObject*) = nullptr;

// ── hook callbacks ───────────────────────────────────────────────────────────

// [EN] Startup hook - fired during QCoreApplication construction.
//      Creates the AgentInitializer which will install the EventTracker
//      on the next event loop iteration.
//
//      Why create AgentInitializer here and not install EventTracker directly?
//      Because QCoreApplication is still being constructed - its event
//      dispatching mechanism is not yet fully operational.  The initializer
//      uses QueuedConnection to defer to a safe point.
//
// [RU] Хук Startup - срабатывает при конструировании QCoreApplication.
//      Создаёт AgentInitializer, который установит EventTracker на
//      следующей итерации event loop.
//
//      Почему создаём AgentInitializer, а не устанавливаем EventTracker
//      напрямую?  Потому что QCoreApplication ещё конструируется - его
//      механизм диспетчеризации событий ещё не полностью работоспособен.
//      Инициализатор использует QueuedConnection для отложенного вызова
//      в безопасный момент.
static void hookStartup()
{
    std::cerr << "[agent] startup hook fired." << std::endl;

    // [EN] AgentInitializer moves itself to the main thread and schedules
    //      initAgent() via QueuedConnection.  It will be destroyed by
    //      deleteLater() after initAgent() completes.
    //      The `new` without storing the pointer is intentional - the object
    //      manages its own lifetime.
    //
    // [RU] AgentInitializer переносит себя в главный поток и планирует
    //      initAgent() через QueuedConnection.  Он будет уничтожен через
    //      deleteLater() после завершения initAgent().
    //      `new` без сохранения указателя - намеренно: объект управляет
    //      своим временем жизни.

    new agent::AgentInitializer();

    if (g_nextStartup)
        g_nextStartup();
}

// [EN] AddQObject hook - called from QObject's base constructor.
//
//      Left empty (delegation only) in this test stand because:
//
//      1. vtable points to QObject, not the derived class - className(),
//         objectName(), and parent hierarchy are all unreliable here.
//
//      2. Deferred identification (timer queue, background thread) adds
//         complexity unjustified for a minimal stand: thread safety,
//         cache invalidation, handling premature destruction.
//
//      3. The EventTracker identifies objects at interaction time, when
//         they are fully constructed.  This is sufficient for our goal:
//         verify that injection works and paths are computed correctly.
//
//      A production tool would queue the pointer here and process it
//      later (after derived constructors complete) to maintain a live
//      object tree.
//
// [RU] Хук AddQObject - вызывается из конструктора базового QObject.
//
//      Оставлен пустым (только делегирование) в этом стенде, потому что:
//
//      1. vtable указывает на QObject, не на производный класс - className(),
//         objectName() и иерархия parent ненадёжны в этой точке.
//
//      2. Отложенная идентификация (очередь с таймером, фоновый поток)
//         добавляет сложность, не оправданную для минимального стенда:
//         потокобезопасность, инвалидация кэша, обработка преждевременного
//         удаления.
//
//      3. EventTracker идентифицирует объекты в момент взаимодействия,
//         когда они полностью сконструированы.  Этого достаточно для нашей
//         цели: проверить, что инъекция работает и пути вычисляются корректно.
//
//      В продакшен-инструменте здесь бы ставился указатель в очередь
//      для обработки позже (после завершения конструкторов производных
//      классов) для поддержания живого дерева объектов.
static void hookAddObject(QObject *obj)
{
    if (g_nextAddObject)
        g_nextAddObject(obj);
}

// [EN] RemoveQObject hook - called from QObject's destructor.
//
//      Empty for the same reasons as AddQObject: no object tree to
//      maintain, and vtable has already unwound to QObject - real class
//      name is unavailable without a cache from creation time.
//
//      The EventTracker needs no destruction notification - it simply
//      won't receive events for a destroyed object.
//
//      A production tool would remove the object from its known set,
//      clean up pending queues, and notify dependent components.
//
// [RU] Хук RemoveQObject - вызывается из деструктора QObject.
//
//      Пуст по тем же причинам, что и AddQObject: нет дерева объектов
//      для поддержания, и vtable уже откатился до QObject - реальное
//      имя класса недоступно без кэша, заполненного при создании.
//
//      EventTracker не нуждается в уведомлении об удалении - он просто
//      перестанет получать события для удалённого объекта.
//
//      В продакшен-инструменте здесь удалялся бы объект из множества
//      известных, чистились очереди ожидания и уведомлялись зависимые
//      компоненты.
static void hookRemoveObject(QObject *obj)
{
    if (g_nextRemoveObject)
        g_nextRemoveObject(obj);
}

// ── hook installation ────────────────────────────────────────────────────────

static void installHooksInternal()
{
    if (qtHookData[QHooks::HookDataVersion] < 1 ||
        qtHookData[QHooks::HookDataSize] < 6) {
        std::cerr << "[agent] WARNING: qtHookData version/size mismatch "
                     "- hooks NOT installed." << std::endl;
        return;
    }

    // [EN] Save originals for delegation chain.
    // [RU] Сохраняем оригиналы для цепочки делегирования.
    g_nextStartup = reinterpret_cast<QHooks::StartupCallback>(
        qtHookData[QHooks::Startup]);
    g_nextAddObject = reinterpret_cast<QHooks::AddQObjectCallback>(
        qtHookData[QHooks::AddQObject]);
    g_nextRemoveObject = reinterpret_cast<QHooks::RemoveQObjectCallback>(
        qtHookData[QHooks::RemoveQObject]);

    // [EN] Install our hooks.
    // [RU] Устанавливаем наши хуки.
    qtHookData[QHooks::Startup]       = reinterpret_cast<quintptr>(&hookStartup);
    qtHookData[QHooks::AddQObject]    = reinterpret_cast<quintptr>(&hookAddObject);
    qtHookData[QHooks::RemoveQObject] = reinterpret_cast<quintptr>(&hookRemoveObject);

    std::cerr << "[agent] hooks installed successfully." << std::endl;
}

// [EN] Guard against double installation.
// [RU] Защита от повторной установки.
static bool alreadyInstalled() noexcept
{
    return qtHookData[QHooks::AddQObject] == reinterpret_cast<quintptr>(&hookAddObject);
}

static void installHooks()
{
    if (!alreadyInstalled())
        installHooksInternal();
}

// [EN] Q_COREAPP_STARTUP_FUNCTION registers installHooks to be called during
//      QCoreApplication construction.  The macro expands to a static
//      initializer that appends our function to Qt's internal startup list.
//
// [RU] Q_COREAPP_STARTUP_FUNCTION регистрирует installHooks для вызова при
//      конструировании QCoreApplication.  Макрос раскрывается в статический
//      инициализатор, добавляющий нашу функцию во внутренний список Qt.
Q_COREAPP_STARTUP_FUNCTION(installHooks)
