#pragma once

// [EN] Event filter installed on QCoreApplication.  Intercepts user interaction
//      events (mouse clicks, key presses, focus changes, etc.) and logs the
//      target object's real class name, objectName, and hierarchical path.
//
//      Only reacts to GUI objects (QWidget or QQuickItem).  Non-GUI QObjects
//      are silently ignored.
//
//      The filter never consumes events — it is purely a passive observer.
//
// [RU] Event filter, установленный на QCoreApplication.  Перехватывает события
//      пользовательского взаимодействия (клики мыши, нажатия клавиш, изменения
//      фокуса и т.д.) и логирует реальное имя класса, objectName и
//      иерархический путь целевого объекта.
//
//      Реагирует только на GUI-объекты (QWidget или QQuickItem).  Не-GUI
//      QObject'ы молча игнорируются.
//
//      Фильтр никогда не поглощает события — он чисто пассивный наблюдатель.

#include <QObject>
#include <QEvent>

namespace agent {
class EventTracker : public QObject
{
    Q_OBJECT

public:
    explicit EventTracker(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

// [EN] Create and install the EventTracker on QCoreApplication.
//      Called from AgentInitializer::initAgent() when the event loop is ready.
// [RU] Создать и установить EventTracker на QCoreApplication.
//      Вызывается из AgentInitializer::initAgent() когда event loop готов.
void installEventTracker();
} // namespace agent
