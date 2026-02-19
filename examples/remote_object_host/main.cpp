#include <QCoreApplication>
#include <QTimer>
#include <QRemoteObjectHost>
#include <QTcpSocket>
#include <QDateTime>
#include <iostream>

#include "RemoteObjectExample.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ROExample source;
    const bool useLocal = app.arguments().contains(QStringLiteral("--local"));

    QRemoteObjectHost hostNode;

    if (useLocal) {
        // ── Desktop mode ─────────────────────────────────────────────────
        // [EN] Classic local socket — both ro_host and agent run on the
        //      same machine.  Agent uses LD_PRELOAD and connects to
        //      "local:demo".
        //
        // [RU] Классический локальный сокет — и ro_host, и agent работают
        //      на одной машине.  Агент использует LD_PRELOAD и подключается
        //      к "local:demo".
        hostNode.setHostUrl(QUrl(QStringLiteral("local:demo")));
        if (!hostNode.enableRemoting(&source)) {
            std::cerr << "[HOST] ERROR: enableRemoting failed!" << std::endl;
            return 1;
        }
        std::cout << "[HOST] listening on local:demo" << std::endl;

    } else {
        // ── Android mode (via ADB) ──────────────────────────────────────
        // [EN] The agent on Android listens on an abstract Unix domain
        //      socket "ro_agent" (no INTERNET permission needed).
        //      ADB forwards a TCP port on the PC into that socket:
        //
        //          adb forward tcp:65511 localabstract:ro_agent
        //
        //      ro_host connects to localhost:65511 as a TCP CLIENT.
        //      ADB tunnels this to the abstract socket on the device,
        //      where the agent's AdbSocketProxy accepts the connection
        //      and bridges it to a QLocalServer.
        //
        //      We use addHostSideConnection() to attach the host to
        //      an externally-managed QIODevice (our QTcpSocket), rather
        //      than having QRemoteObjectHost listen on a URL.
        //
        // [RU] Агент на Android слушает абстрактный Unix domain socket
        //      "ro_agent" (INTERNET permission не нужен).
        //      ADB пробрасывает TCP-порт на ПК в этот сокет:
        //
        //          adb forward tcp:65511 localabstract:ro_agent
        //
        //      ro_host подключается к localhost:65511 как TCP КЛИЕНТ.
        //      ADB туннелирует это в абстрактный сокет на устройстве,
        //      где AdbSocketProxy агента принимает подключение и
        //      мостит его к QLocalServer.
        //
        //      Используем addHostSideConnection() для привязки хоста к
        //      внешне управляемому QIODevice (нашему QTcpSocket), вместо
        //      того чтобы QRemoteObjectHost слушал на URL.

        // [EN] QRemoteObjectHost needs a valid URL before enableRemoting().
        //      We use a local socket as the "primary" address — it won't
        //      be used by anyone, but satisfies the internal check.
        //      The real transport is the QTcpSocket added below.
        //
        // [RU] QRemoteObjectHost требует валидный URL перед enableRemoting().
        //      Используем локальный сокет как "первичный" адрес — к нему
        //      никто не будет подключаться, но он удовлетворяет внутреннюю
        //      проверку.  Реальный транспорт — QTcpSocket, добавленный ниже.
        hostNode.setHostUrl(QUrl(QStringLiteral("local:ro_host_internal")));
        if (!hostNode.enableRemoting(&source)) {
            std::cerr << "[HOST] ERROR: enableRemoting failed!" << std::endl;
            return 1;
        }

        const quint16 port = 65511;
        auto *socket = new QTcpSocket(&app);
        auto *reconnecting = new bool(false);

        auto scheduleReconnect = [socket, port, reconnecting]() {
            if (*reconnecting)
                return;
            *reconnecting = true;
            socket->abort();
            QTimer::singleShot(2000, socket, [socket, port, reconnecting]() {
                *reconnecting = false;
                socket->connectToHost(QStringLiteral("127.0.0.1"), port);
            });
        };

        QObject::connect(socket, &QTcpSocket::connected, [&hostNode, socket]() {
            std::cout << "[HOST] connected to ADB forward tunnel" << std::endl;
            hostNode.addHostSideConnection(socket);
        });

        QObject::connect(socket, &QTcpSocket::disconnected, [scheduleReconnect]() {
            std::cerr << "[HOST] disconnected from ADB tunnel, reconnecting..."
                      << std::endl;
            scheduleReconnect();
        });

        QObject::connect(socket,
                         QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
                         [socket, scheduleReconnect](QAbstractSocket::SocketError) {
                             std::cerr << "[HOST] socket error: "
                                       << socket->errorString().toStdString()
                                       << "  -- retrying in 2s..." << std::endl;
                             scheduleReconnect();
                         });

        std::cout << "[HOST] connecting to ADB forward on tcp://127.0.0.1:"
                  << port << " ..." << std::endl;
        socket->connectToHost(QStringLiteral("127.0.0.1"), port);
    }

    // ── Periodic message sender (same for both modes) ────────────────────
    int seq = 0;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        ++seq;
        const auto msg = QStringLiteral("hello #%1 at %2")
                             .arg(seq)
                             .arg(QDateTime::currentDateTime()
                                      .toString(Qt::ISODate));
        source.setMessage(msg);
        emit source.ping(seq);
        std::cout << "[HOST] sent message: " << msg.toStdString()
                  << std::endl;
    });
    timer.start(2000);

    return app.exec();
}
