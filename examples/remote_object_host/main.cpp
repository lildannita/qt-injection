#include <QCoreApplication>
#include <QTimer>
#include <QRemoteObjectHost>
#include <QDateTime>
#include <iostream>

#include "RemoteObjectExample.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ROExample source;
    QRemoteObjectHost hostNode(QUrl(QStringLiteral("local:demo")));
    if (!hostNode.enableRemoting(&source)) {
        std::cerr << "[HOST] ERROR: enableRemoting failed!" << std::endl;
        return 1;
    }
    std::cout << "[HOST] listening on local:demo" << std::endl;

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
