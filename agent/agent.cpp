#include <QObject>
#include <QCoreApplication>
#include <QWidget>
#include <QQuickItem>
#include <QTimer>
#include <QHash>
#include <QMutex>
#include <private/qhooks_p.h>
#include <cassert>
#include <iostream>

#include "object_path.hpp"

// ── original hook pointers (for delegation) ──────────────────────────────────
// [EN] Saved before we overwrite the slots.  May be nullptr (no previous hook)
//      or point to another tool's callback (e.g. GammaRay).
// [RU] Сохраняются до перезаписи слотов.  Могут быть nullptr (хука не было)
//      или указывать на callback другого инструмента (напр. GammaRay).
static void (*g_nextStartup)() = nullptr;
static void (*g_nextAddObject)(QObject *) = nullptr;
static void (*g_nextRemoveObject)(QObject *) = nullptr;

// ── cached object info / кэшированная информация об объектах ─────────────────
// [EN] We cache class name and path at the moment the object is fully
//      constructed (deferred ADD).  This cache is then used in the DEL
//      handler, where vtable has already unwound to QObject and we can
//      no longer obtain the real class name.
//
//      We also record whether the object is a GUI component (QWidget or
//      QQuickItem).  Non-GUI objects are silently ignored.
//
// [RU] Кэшируем имя класса и путь в момент полного конструирования объекта
//      (отложенный ADD).  Этот кэш используется в обработчике DEL, где
//      vtable уже откатился до QObject и получить реальное имя класса
//      невозможно.
//
//      Также запоминаем, является ли объект GUI-компонентом (QWidget или
//      QQuickItem).  Не-GUI объекты молча игнорируются.

struct CachedObjectInfo {
    QString className;
    QString objectName;
    QString path;
};

static QMutex g_mutex;
static QSet<const QObject *> g_pendingObjects;
static QHash<const QObject *, CachedObjectInfo> g_objectCache;

// [EN] Returns true if the object is a GUI component: QWidget (Widgets) or
//      QQuickItem (Quick/QML).  Must be called only when the object is fully
//      constructed (i.e. from the deferred handler, not from the hook).
//
// [RU] Возвращает true, если объект - GUI-компонент: QWidget (Widgets) или
//      QQuickItem (Quick/QML).  Вызывать только когда объект полностью
//      сконструирован (т.е. из отложенного обработчика, не из хука).
static bool isGuiObject(QObject *obj) noexcept
{
    return qobject_cast<QWidget *>(obj) != nullptr
           || qobject_cast<QQuickItem *>(obj) != nullptr;
}

enum class HookEvent { Add, Remove };

static void logEvent(HookEvent event, const QString &cls,
                     const QString &name, const QString &path) noexcept
{
    const auto *tag = (event == HookEvent::Add) ? "[+] ADD  " : "[-] DEL  ";
    const auto displayName = name.isEmpty() ? QStringLiteral("-") : name;

    std::cerr << tag
              << cls.toStdString()
              << " name=" << displayName.toStdString()
              << " path=" << path.toStdString()
              << std::endl;
}

static void hookStartup()
{
    std::cerr << "[agent] startup hook fired" << std::endl;
    if (g_nextStartup)
        g_nextStartup();
}

// [EN] Deferred handler - called from event loop when the object is fully
//      constructed.  At this point vtable is correct, metaObject() returns
//      the real class, objectName() is set, parent/children are complete.
//      We check if the object is a GUI component; if not - skip silently.
//      If yes - cache the info and print the ADD message.
//
// [RU] Отложенный обработчик - вызывается из event loop, когда объект
//      полностью сконструирован.  В этот момент vtable корректен,
//      metaObject() возвращает реальный класс, objectName() задан,
//      parent/children сформированы.
//      Проверяем, является ли объект GUI-компонентом; если нет - пропускаем.
//      Если да - кэшируем информацию и выводим ADD.
static void processDeferredAdd(QObject *obj)
{
    {
        QMutexLocker lock(&g_mutex);
        if (!g_pendingObjects.remove(obj))
            return; // already removed or processed
    }

    if (!isGuiObject(obj))
        return;

    const auto cls  = agent::getCorrectClassName(obj);
    const auto name = obj->objectName();
    const auto path = agent::objectPath(obj);

    {
        QMutexLocker lock(&g_mutex);
        g_objectCache.insert(obj, { cls, name, path });
    }

    logEvent(HookEvent::Add, cls, name, path);
}

// [EN] AddQObject hook - called from QObject base constructor.
//      We only record the pointer and schedule deferred processing.
//      No class info is read here.
//
// [RU] Хук AddQObject - вызывается из конструктора базового QObject.
//      Только запоминаем указатель и планируем отложенную обработку.
//      Информацию о классе здесь не читаем.
static void hookAddObject(QObject *obj)
{
    if (obj) {
        if (QCoreApplication::instance()) {
            {
                QMutexLocker lock(&g_mutex);
                g_pendingObjects.insert(obj);
            }
            QTimer::singleShot(0, [obj]() {
                processDeferredAdd(obj);
            });
        }
        // [EN] No event loop yet - skip. Objects created before QCoreApplication
        //      (like QCoreApplication itself) are not GUI components anyway.
        // [RU] Event loop ещё нет - пропускаем. Объекты, создаваемые до
        //      QCoreApplication (например, сам QCoreApplication), не являются
        //      GUI-компонентами в любом случае.
    }
    if (g_nextAddObject)
        g_nextAddObject(obj);
}

// [EN] RemoveQObject hook - called from QObject destructor.
//      vtable has unwound to QObject, so we CANNOT read the real class.
//      Instead we look up the cached info from the deferred ADD.
//      If the object was never cached (not a GUI component, or was
//      destroyed before the deferred ADD fired), we skip silently.
//
// [RU] Хук RemoveQObject - вызывается из деструктора QObject.
//      vtable откатился до QObject, поэтому реальный класс прочитать
//      НЕЛЬЗЯ.  Вместо этого берём кэшированную информацию из
//      отложенного ADD.  Если объект не был закэширован (не GUI-компонент
//      или был удалён до срабатывания отложенного ADD) - пропускаем.
static void hookRemoveObject(QObject *obj)
{
    if (obj) {
        QMutexLocker lock(&g_mutex);

        // [EN] If still pending - never got to deferred ADD. Remove silently.
        // [RU] Если ещё в pending - отложенный ADD не сработал. Убираем молча.
        g_pendingObjects.remove(obj);

        auto it = g_objectCache.find(obj);
        if (it != g_objectCache.end()) {
            const auto info = it.value(); // copy before erase
            g_objectCache.erase(it);
            lock.unlock();
            logEvent(HookEvent::Remove, info.className, info.objectName, info.path);
        }
    }
    if (g_nextRemoveObject)
        g_nextRemoveObject(obj);
}

// [EN] Reads the current values from qtHookData (saving them for delegation),
//      then overwrites the slots with our callbacks.
//
//      qtHookData is a plain C array of quintptr (unsigned integer the size
//      of a pointer).  The first two elements are metadata:
//        [HookDataVersion] - must be >= 1 (we understand this version)
//        [HookDataSize]    - must be >= 6 (array has at least 6 elements)
//      The remaining elements are function pointers cast to quintptr:
//        [Startup]       - StartupCallback:       void (*)()
//        [AddQObject]    - AddQObjectCallback:    void (*)(QObject*)
//        [RemoveQObject] - RemoveQObjectCallback: void (*)(QObject*)
//
// [RU] Считывает текущие значения из qtHookData (сохраняя их для делегирования),
//      затем перезаписывает слоты нашими callback'ами.
//
//      qtHookData - обычный C-массив quintptr (беззнаковое целое размером
//      с указатель).  Первые два элемента - метаданные:
//        [HookDataVersion] - должен быть >= 1 (мы понимаем эту версию)
//        [HookDataSize]    - должен быть >= 6 (массив содержит минимум 6 элементов)
//      Остальные элементы - указатели на функции, приведённые к quintptr:
//        [Startup]       - StartupCallback:       void (*)()
//        [AddQObject]    - AddQObjectCallback:    void (*)(QObject*)
//        [RemoveQObject] - RemoveQObjectCallback: void (*)(QObject*)
static void installHooksInternal()
{
    if (qtHookData[QHooks::HookDataVersion] < 1 ||
        qtHookData[QHooks::HookDataSize] < 6) {
        std::cerr << "[agent] WARNING: qtHookData version/size mismatch "
                     "- hooks NOT installed." << std::endl;
        return;
    }

    g_nextStartup = reinterpret_cast<QHooks::StartupCallback>(
        qtHookData[QHooks::Startup]);
    g_nextAddObject = reinterpret_cast<QHooks::AddQObjectCallback>(
        qtHookData[QHooks::AddQObject]);
    g_nextRemoveObject = reinterpret_cast<QHooks::RemoveQObjectCallback>(
        qtHookData[QHooks::RemoveQObject]);

    qtHookData[QHooks::Startup]       = reinterpret_cast<quintptr>(&hookStartup);
    qtHookData[QHooks::AddQObject]    = reinterpret_cast<quintptr>(&hookAddObject);
    qtHookData[QHooks::RemoveQObject] = reinterpret_cast<quintptr>(&hookRemoveObject);

    std::cerr << "[agent] hooks installed successfully." << std::endl;
}

// [EN] Guard against double installation - if we're already hooked, skip.
// [RU] Защита от повторной установки - если уже установлены, пропускаем.
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
//      the construction of the first QCoreApplication (or QApplication /
//      QGuiApplication, which inherit from it).  This macro expands to a
//      static initializer that appends our function to an internal list;
//      QCoreApplication's constructor iterates this list and calls each entry.
//
// [RU] Q_COREAPP_STARTUP_FUNCTION регистрирует installHooks для вызова во
//      время конструирования первого QCoreApplication (или QApplication /
//      QGuiApplication, наследующих от него).  Макрос раскрывается в
//      статический инициализатор, добавляющий нашу функцию во внутренний
//      список; конструктор QCoreApplication проходит по этому списку и
//      вызывает каждую запись.
Q_COREAPP_STARTUP_FUNCTION(installHooks)
