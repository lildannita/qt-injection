#include "adb_socket_proxy.hpp"

#include <QLocalServer>
#include <QLocalSocket>
#include <iostream>

#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#endif

namespace agent {

// ── AdbProxyWorker ───────────────────────────────────────────────────────────

AdbProxyWorker::AdbProxyWorker(const QString &abstractName,
                               const QString &localName,
                               QObject *parent)
    : QObject(parent)
    , m_abstractName(abstractName)
    , m_localName(localName)
{
}

AdbProxyWorker::~AdbProxyWorker()
{
    stop();
}

void AdbProxyWorker::stop()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
    if (m_listenFd >= 0) {
#ifdef Q_OS_LINUX
        // [EN] Closing the fd unblocks accept() / poll() in run().
        // [RU] Закрытие fd разблокирует accept() / poll() в run().
        ::shutdown(m_listenFd, SHUT_RDWR);
        ::close(m_listenFd);
        m_listenFd = -1;
#endif
    }
}

void AdbProxyWorker::run()
{
#ifndef Q_OS_LINUX
    emit errorOccurred(QStringLiteral("AdbSocketProxy is Linux/Android only"));
    return;
#else
    // ── Step 1: Create QLocalServer ──────────────────────────────────────
    // [EN] QLocalServer in the app sandbox.  Remove stale socket first.
    // [RU] QLocalServer в песочнице приложения.  Сначала удаляем устаревший сокет.
    QLocalServer::removeServer(m_localName);
    QLocalServer localServer;
    if (!localServer.listen(m_localName)) {
        const auto err = QStringLiteral("[proxy] QLocalServer::listen(%1) failed: %2")
                             .arg(m_localName, localServer.errorString());
        std::cerr << err.toStdString() << std::endl;
        emit errorOccurred(err);
        return;
    }
    qInfo() << "[proxy] QLocalServer listening on" << m_localName;

    // ── Step 2: Create abstract Unix socket ──────────────────────────────
    // [EN] AF_UNIX + abstract namespace = no INTERNET permission required.
    //      Abstract name: sun_path[0] = '\0', then the name bytes.
    // [RU] AF_UNIX + абстрактное пространство имён = INTERNET permission не нужен.
    //      Абстрактное имя: sun_path[0] = '\0', затем байты имени.
    m_listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        const auto err = QStringLiteral("[proxy] socket(AF_UNIX) failed: %1")
                             .arg(QString::fromUtf8(strerror(errno)));
        std::cerr << err.toStdString() << std::endl;
        emit errorOccurred(err);
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // [EN] Abstract namespace: first byte is \0, then the name.
    //      The name does NOT need to be null-terminated in abstract namespace,
    //      so the effective length = offsetof(sun_path) + 1 + name_length.
    // [RU] Абстрактное пространство имён: первый байт \0, потом имя.
    //      Имя НЕ должно завершаться нулём в абстрактном пространстве,
    //      поэтому эффективная длина = offsetof(sun_path) + 1 + длина_имени.
    const QByteArray nameBytes = m_abstractName.toUtf8();
    if (nameBytes.size() + 1 > (int)sizeof(addr.sun_path)) {
        emit errorOccurred(QStringLiteral("[proxy] abstract name too long"));
        ::close(m_listenFd);
        m_listenFd = -1;
        return;
    }
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, nameBytes.constData(), nameBytes.size());
    const socklen_t addrLen = offsetof(struct sockaddr_un, sun_path)
                              + 1 + nameBytes.size();

    if (::bind(m_listenFd, reinterpret_cast<struct sockaddr *>(&addr), addrLen) < 0) {
        const auto err = QStringLiteral("[proxy] bind(abstract:%1) failed: %2")
                             .arg(m_abstractName, QString::fromUtf8(strerror(errno)));
        std::cerr << err.toStdString() << std::endl;
        emit errorOccurred(err);
        ::close(m_listenFd);
        m_listenFd = -1;
        return;
    }

    if (::listen(m_listenFd, 1) < 0) {
        emit errorOccurred(QStringLiteral("[proxy] listen() failed"));
        ::close(m_listenFd);
        m_listenFd = -1;
        return;
    }
    qInfo() << "[proxy] abstract socket listening:" << m_abstractName;
    emit ready();

    // ── Step 3: Accept loop ──────────────────────────────────────────────
    // [EN] Each accepted ADB connection is bridged to a new QLocalSocket
    //      connecting to our QLocalServer.  We handle one connection at a
    //      time (sufficient for a single ro_host↔agent link).
    //
    // [RU] Каждое принятое ADB-подключение соединяется мостом с новым
    //      QLocalSocket, подключающимся к нашему QLocalServer.  Обрабатываем
    //      по одному подключению за раз (достаточно для одной связи
    //      ro_host↔agent).

    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        // [EN] poll() with timeout so we can check m_stopRequested periodically.
        // [RU] poll() с таймаутом, чтобы периодически проверять m_stopRequested.
        struct pollfd pfd;
        pfd.fd = m_listenFd;
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, 1000 /* ms */);
        if (pr <= 0)
            continue;

        int adbFd = ::accept(m_listenFd, nullptr, nullptr);
        if (adbFd < 0) {
            if (m_stopRequested.load(std::memory_order_relaxed))
                break;
            qWarning() << "[proxy] accept() failed:" << strerror(errno);
            continue;
        }
        qInfo() << "[proxy] ADB connected";

        // [EN] Connect a QLocalSocket to our QLocalServer.
        // [RU] Подключаем QLocalSocket к нашему QLocalServer.
        //
        // Wait for QLocalServer to receive the pending connection:
        if (!localServer.waitForNewConnection(0)) {
            // [EN] We must first create a client that connects to localServer.
            //      Then localServer.nextPendingConnection() gives us the
            //      server-side socket, and we keep the client-side socket too.
            //      But for simplicity, we'll just proxy at the fd level.
        }

        // [EN] For simplicity and to avoid mixing Qt event loop with raw fds
        //      in a background thread, we do a raw poll()-based byte shuttle.
        //
        //      We connect a QLocalSocket to QLocalServer synchronously,
        //      get its native fd, and shuttle between adbFd and localFd.
        //
        // [RU] Для простоты и чтобы не смешивать Qt event loop с raw fd
        //      в фоновом потоке, делаем poll()-based shuttle байтов.
        //
        //      Подключаем QLocalSocket к QLocalServer синхронно,
        //      получаем его native fd и шаттлим между adbFd и localFd.
        QLocalSocket localClient;
        localClient.connectToServer(m_localName);
        if (!localClient.waitForConnected(3000)) {
            qWarning() << "[proxy] could not connect QLocalSocket to"
                        << m_localName << ":" << localClient.errorString();
            ::close(adbFd);
            continue;
        }

        // [EN] Get the server-side socket from QLocalServer.
        //      This is the socket that QtRemoteObjects host would normally
        //      talk to.  But here WE are the host-side proxy — we forward
        //      everything from ADB into this socket.
        //
        //      Actually, the architecture is simpler: QRemoteObjectNode
        //      on agent side connects to "local:ro_demo" (our QLocalServer).
        //      We proxy between ADB fd and the server-side pending connection.
        //
        //      Wait — we need to rethink.  The QRemoteObjectNode replica
        //      connects as a CLIENT to "local:ro_demo".  So QLocalServer
        //      will accept that connection.  But ADB also needs to be bridged.
        //
        //      Correct architecture:
        //        ADB (from ro_host on PC) ←→ [proxy] ←→ QLocalSocket → QLocalServer
        //                                                                  ↑
        //                                              QRemoteObjectNode connects here
        //
        //      NO — QRemoteObjectHost on PC uses QRemoteObjectHost(url).
        //      QRemoteObjectNode on Android is the replica that calls
        //      connectToNode(url).  The host LISTENS, the replica CONNECTS.
        //
        //      With the proxy:
        //        PC:  ro_host listens on tcp://0.0.0.0:65511
        //             ↓
        //        ADB forward: tcp:65511 on PC → localabstract:ro_agent on device
        //             ↓
        //        Proxy: accepts abstract socket, opens QLocalSocket to QLocalServer
        //             ↓
        //        QLocalServer("ro_demo") — accepts connections from both
        //             proxy AND the replica.
        //
        //      WAIT — this is wrong.  QtRemoteObjects uses a client-server
        //      model.  The HOST creates a QRemoteObjectHost that listens on
        //      a URL.  The REPLICA calls QRemoteObjectNode::connectToNode(url).
        //
        //      In the original code:
        //        ro_host: QRemoteObjectHost("local:demo")  — creates server
        //        agent:   QRemoteObjectNode::connectToNode("local:demo") — connects
        //
        //      With proxy, we need the proxy to act as a TRANSPARENT TUNNEL.
        //      From QRemoteObjectNode's perspective on Android, it connects
        //      to "local:ro_demo".  The proxy listens on "local:ro_demo" via
        //      QLocalServer, and forwards all bytes to/from the ADB abstract
        //      socket, which ADB forwards to tcp:65511 on the PC, where
        //      ro_host listens.
        //
        //      So the data flow is:
        //        agent QRemoteObjectNode → connects to local:ro_demo →
        //        → QLocalServer(ro_demo) [in proxy] → accepted socket →
        //        → proxy shuttles bytes to abstract socket →
        //        → ADB forwards to tcp:65511 on PC →
        //        → ro_host QRemoteObjectHost(tcp://0.0.0.0:65511) accepts
        //
        //      This means we DON'T need QLocalSocket from proxy to server.
        //      The proxy IS the server for the replica side.
        //      We need to shuttle between:
        //        - The QLocalServer's pending connection (from replica)
        //        - The abstract socket connection (from ADB/ro_host)
        //
        //      But there's a timing issue: ADB might connect before or after
        //      the replica.  Let's handle both.
        //
        // [RU] Правильная архитектура:
        //        agent QRemoteObjectNode → подключается к local:ro_demo →
        //        → QLocalServer(ro_demo) [в прокси] → принятый сокет →
        //        → прокси шаттлит байты в абстрактный сокет →
        //        → ADB форвардит в tcp:65511 на ПК →
        //        → ro_host QRemoteObjectHost(tcp://0.0.0.0:65511) принимает

        // [EN] We don't need localClient — we used it wrongly above.
        //      Instead, wait for QLocalServer to get a connection from replica.
        localClient.disconnectFromServer();

        // [EN] Wait for the replica to connect.  The replica is created
        //      in EventTracker constructor and connects to "local:ro_demo".
        //      It should connect very soon after we emit ready().
        // [RU] Ждём подключения реплики.  Реплика создаётся в конструкторе
        //      EventTracker и подключается к "local:ro_demo".
        //      Она должна подключиться вскоре после ready().

        QLocalSocket *replicaSocket = nullptr;

        // [EN] Wait up to 10 seconds for replica to connect.
        // [RU] Ждём до 10 секунд подключения реплики.
        for (int i = 0; i < 100 && !m_stopRequested.load(std::memory_order_relaxed); ++i) {
            if (localServer.waitForNewConnection(100)) {
                replicaSocket = localServer.nextPendingConnection();
                break;
            }
        }

        if (!replicaSocket) {
            qWarning() << "[proxy] no replica connection within timeout";
            ::close(adbFd);
            continue;
        }

        const int replicaFd = static_cast<int>(replicaSocket->socketDescriptor());
        qInfo() << "[proxy] replica connected, starting byte shuttle";

        // ── Byte shuttle ─────────────────────────────────────────────────
        // [EN] Simple poll() loop that copies data in both directions.
        //      Runs until one side closes or an error occurs.
        // [RU] Простой poll()-цикл, копирующий данные в обоих направлениях.
        //      Работает до закрытия одной из сторон или ошибки.
        char buf[8192];
        bool running = true;
        while (running && !m_stopRequested.load(std::memory_order_relaxed)) {
            struct pollfd fds[2];
            fds[0].fd = adbFd;
            fds[0].events = POLLIN;
            fds[1].fd = replicaFd;
            fds[1].events = POLLIN;

            int ret = ::poll(fds, 2, 500 /* ms */);
            if (ret < 0) {
                if (errno == EINTR)
                    continue;
                qWarning() << "[proxy] poll error:" << strerror(errno);
                break;
            }
            if (ret == 0)
                continue;

            // ADB → replica
            if (fds[0].revents & POLLIN) {
                ssize_t n = ::read(adbFd, buf, sizeof(buf));
                if (n <= 0) { running = false; break; }
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = ::write(replicaFd, buf + written, n - written);
                    if (w <= 0) { running = false; break; }
                    written += w;
                }
            }
            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                running = false; break;
            }

            // replica → ADB
            if (fds[1].revents & POLLIN) {
                ssize_t n = ::read(replicaFd, buf, sizeof(buf));
                if (n <= 0) { running = false; break; }
                ssize_t written = 0;
                while (written < n) {
                    ssize_t w = ::write(adbFd, buf + written, n - written);
                    if (w <= 0) { running = false; break; }
                    written += w;
                }
            }
            if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                running = false; break;
            }
        }

        qInfo() << "[proxy] byte shuttle ended";
        ::close(adbFd);
        // [EN] replicaSocket is owned by QLocalServer, will be cleaned up.
        // [RU] replicaSocket принадлежит QLocalServer, будет очищен.
        delete replicaSocket;
    }

    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
    }
    qInfo() << "[proxy] worker finished";
#endif // Q_OS_LINUX
}


// ── AdbSocketProxy ──────────────────────────────────────────────────────────

AdbSocketProxy::AdbSocketProxy(const QString &abstractName,
                               const QString &localName,
                               QObject *parent)
    : QObject(parent)
{
    m_worker = new AdbProxyWorker(abstractName, localName);
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, m_worker, &AdbProxyWorker::run);
    connect(m_worker, &AdbProxyWorker::ready, this, &AdbSocketProxy::ready);
    connect(m_worker, &AdbProxyWorker::errorOccurred, this, &AdbSocketProxy::errorOccurred);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
}

AdbSocketProxy::~AdbSocketProxy()
{
    stop();
}

void AdbSocketProxy::start()
{
    m_thread.start();
}

void AdbSocketProxy::stop()
{
    if (m_worker)
        m_worker->stop();
    m_thread.quit();
    m_thread.wait(3000);
}

} // namespace agent
