#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QAction>
#include <QTimer>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("WidgetsTestApp");

    QMainWindow window;
    window.setObjectName("mainWindow");
    window.setWindowTitle("Qt Widgets - Injection Test Stand");
    window.resize(480, 360);

    // ── Menu bar ─────────────────────────────────────────────────────────
    QMenu *fileMenu = window.menuBar()->addMenu("&File");
    fileMenu->setObjectName("menuFile");

    QAction *actQuit = fileMenu->addAction("&Quit");
    actQuit->setObjectName("actionQuit");
    QObject::connect(actQuit, &QAction::triggered, &app, &QApplication::quit);

    QMenu *helpMenu = window.menuBar()->addMenu("&Help");
    helpMenu->setObjectName("menuHelp");

    QAction *actAbout = helpMenu->addAction("&About");
    actAbout->setObjectName("actionAbout");
    QObject::connect(actAbout, &QAction::triggered, [&]() {
        QMessageBox::about(&window, "About", "Injection test stand - Qt Widgets app.");
    });

    // ── Toolbar ──────────────────────────────────────────────────────────
    QToolBar *toolbar = window.addToolBar("MainToolBar");
    toolbar->setObjectName("mainToolBar");
    toolbar->addAction(actQuit);

    // ── Central widget ───────────────────────────────────────────────────
    QWidget *central = new QWidget;
    central->setObjectName("centralWidget");

    QVBoxLayout *layout = new QVBoxLayout(central);

    QLabel *label = new QLabel("Agent diagnostic output goes to stderr (console).");
    label->setObjectName("infoLabel");
    layout->addWidget(label);

    QLineEdit *edit = new QLineEdit;
    edit->setObjectName("inputEdit");
    edit->setPlaceholderText("Type something…");
    layout->addWidget(edit);

    QPushButton *btnGreet = new QPushButton("Greet");
    layout->addWidget(btnGreet);

    QObject::connect(btnGreet, &QPushButton::clicked, [&]() {
        label->setText("Hello from " + edit->text().trimmed());
    });

    QPushButton *btnSpawn = new QPushButton("Spawn + Destroy widget (timer 2 s)");
    layout->addWidget(btnSpawn);

    // Clicking the button creates a temporary QLabel, then destroys it after 2s
    QObject::connect(btnSpawn, &QPushButton::clicked, [&]() {
        QLabel *tmp = new QLabel("I am temporary!", central);
        tmp->setObjectName("tmpLabel");
        tmp->show();
        layout->addWidget(tmp);
        QTimer::singleShot(2000, tmp, &QObject::deleteLater);
    });

    window.setCentralWidget(central);

    // ── Status bar ───────────────────────────────────────────────────────
    window.statusBar()->setObjectName("statusBar");
    window.statusBar()->showMessage("Ready");

    window.show();
    return app.exec();
}
