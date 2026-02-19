#pragma once

// [EN] ADB ↔ QLocalServer proxy.
//
//      On Android, opening a TCP socket (even to 127.0.0.1) requires
//      the INTERNET permission in AndroidManifest.xml.  We want zero
//      modifications to the target APK's manifest.
//
//      Solution: use a Linux abstract Unix domain socket, which is
//      AF_UNIX (not AF_INET) and requires no Android permissions.
//      ADB can forward a TCP port on the PC into an abstract socket
//      on the device:
//
//          adb forward tcp:65511 localabstract:ro_agent
//
//      This class:
//        1. Creates a QLocalServer listening on a private name
//           (e.g. "ro_demo") inside the app sandbox.
//        2. Spawns a background thread that binds an abstract Unix
//           domain socket named "\0ro_agent" and accepts one connection
//           from ADB.
//        3. Bidirectionally proxies bytes between the ADB connection
//           and a QLocalSocket connected to the QLocalServer.
//
//      The QtRemoteObjects replica then connects to "local:ro_demo"
//      as usual — everything stays inside the process, no INTERNET
//      permission needed.
//
// [RU] Прокси ADB ↔ QLocalServer.
//
//      На Android открытие TCP-сокета (даже к 127.0.0.1) требует
//      permission INTERNET в AndroidManifest.xml.  Мы хотим ноль
//      изменений в манифесте целевого APK.
//
//      Решение: использовать абстрактный Unix domain socket (Linux),
//      который является AF_UNIX (не AF_INET) и не требует Android
//      permissions.  ADB умеет перенаправлять TCP-порт на ПК в
//      абстрактный сокет на устройстве:
//
//          adb forward tcp:65511 localabstract:ro_agent
//
//      Этот класс:
//        1. Создаёт QLocalServer, слушающий на приватном имени
//           (напр. "ro_demo") внутри песочницы приложения.
//        2. Запускает фоновый поток, который bind'ит абстрактный
//           Unix domain socket с именем "\0ro_agent" и принимает
//           одно подключение от ADB.
//        3. Двунаправленно проксирует байты между ADB-соединением
//           и QLocalSocket, подключённым к QLocalServer.
//
//      Реплика QtRemoteObjects подключается к "local:ro_demo"
//      как обычно — всё остаётся внутри процесса, permission
//      INTERNET не нужен.

#include <QObject>
#include <QThread>
#include <QLocalServer>
#include <atomic>

namespace agent {

// [EN] Background worker that runs in a dedicated QThread.
//      Manages the abstract socket listener and the byte-forwarding loop.
//
// [RU] Фоновый рабочий объект, работающий в отдельном QThread.
//      Управляет слушателем абстрактного сокета и циклом пересылки байтов.
class AdbProxyWorker : public QObject
{
    Q_OBJECT

public:
    // [EN] @param abstractName  Name for the abstract socket (without \0 prefix).
    //                           ADB will forward to "localabstract:<abstractName>".
    // [EN] @param localName     Name for QLocalServer.
    //                           QtRemoteObjects connects to "local:<localName>".
    //
    // [RU] @param abstractName  Имя абстрактного сокета (без префикса \0).
    //                           ADB будет форвардить в "localabstract:<abstractName>".
    // [RU] @param localName     Имя для QLocalServer.
    //                           QtRemoteObjects подключается к "local:<localName>".
    explicit AdbProxyWorker(const QString &abstractName,
                            const QString &localName,
                            QObject *parent = nullptr);
    ~AdbProxyWorker() override;

public slots:
    // [EN] Main entry point — called when the worker's thread starts.
    // [RU] Главная точка входа — вызывается при старте потока рабочего.
    void run();

    // [EN] Request graceful shutdown from any thread.
    // [RU] Запросить корректное завершение из любого потока.
    void stop();

signals:
    // [EN] Emitted when the proxy is ready (QLocalServer listening AND
    //      abstract socket bound).  The replica can now connect.
    // [RU] Испускается, когда прокси готов (QLocalServer слушает И
    //      абстрактный сокет привязан).  Реплика теперь может подключаться.
    void ready();

    // [EN] Emitted on fatal error.
    // [RU] Испускается при фатальной ошибке.
    void errorOccurred(const QString &msg);

private:
    QString m_abstractName;
    QString m_localName;
    std::atomic<bool> m_stopRequested{false};
    int m_listenFd = -1;   // abstract socket fd
};


// [EN] Convenience wrapper: creates the worker + thread pair.
//      Call start() to launch, stop() to tear down.
//      Typically created once from EventTracker's constructor.
//
// [RU] Удобная обёртка: создаёт пару worker + thread.
//      Вызовите start() для запуска, stop() для остановки.
//      Обычно создаётся один раз из конструктора EventTracker.
class AdbSocketProxy : public QObject
{
    Q_OBJECT

public:
    explicit AdbSocketProxy(const QString &abstractName,
                            const QString &localName,
                            QObject *parent = nullptr);
    ~AdbSocketProxy() override;

    void start();
    void stop();

signals:
    void ready();
    void errorOccurred(const QString &msg);

private:
    QThread m_thread;
    AdbProxyWorker *m_worker = nullptr;
};

} // namespace agent
