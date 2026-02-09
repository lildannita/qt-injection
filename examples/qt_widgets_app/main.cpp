// Qt Widgets test application for the injection test stand.
//
// Provides a variety of interactive GUI elements for testing the agent:
//   - Menu bar with actions
//   - Toolbar
//   - Text input fields
//   - Buttons
//   - Checkbox, radio buttons
//   - Combo box
//   - Tab widget with multiple pages
//   - Status bar
//
// Some elements have objectName set, some don't - this lets us verify
// that the agent correctly falls back to ClassName_index path notation
// for unnamed objects.

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QAction>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("WidgetsTestApp");

    QMainWindow window;
    window.setObjectName("mainWindow");
    window.setWindowTitle("Qt Widgets - Injection Test Stand");
    window.resize(520, 440);

    // ── Menu bar ─────────────────────────────────────────────────────────
    auto *fileMenu = window.menuBar()->addMenu("&File");
    fileMenu->setObjectName("menuFile");

    auto *actQuit = fileMenu->addAction("&Quit");
    actQuit->setObjectName("actionQuit");
    QObject::connect(actQuit, &QAction::triggered, &app, &QApplication::quit);

    auto *helpMenu = window.menuBar()->addMenu("&Help");
    helpMenu->setObjectName("menuHelp");

    auto *actAbout = helpMenu->addAction("&About");
    actAbout->setObjectName("actionAbout");
    QObject::connect(actAbout, &QAction::triggered, [&]() {
        QMessageBox::about(&window, "About",
                           "Injection test stand - Qt Widgets app.\n"
                           "Interact with elements and watch stderr output.");
    });

    // ── Toolbar ──────────────────────────────────────────────────────────
    auto *toolbar = window.addToolBar("MainToolBar");
    toolbar->setObjectName("mainToolBar");
    toolbar->addAction(actAbout);
    toolbar->addAction(actQuit);

    // ── Central widget with tabs ─────────────────────────────────────────
    auto *central = new QWidget;
    central->setObjectName("centralWidget");
    auto *mainLayout = new QVBoxLayout(central);

    auto *infoLabel = new QLabel(
        "Agent output goes to stderr. Interact with elements below.");
    infoLabel->setObjectName("infoLabel");
    mainLayout->addWidget(infoLabel);

    auto *tabs = new QTabWidget;
    tabs->setObjectName("tabWidget");

    // ── Tab 1: Input ─────────────────────────────────────────────────────
    auto *inputPage = new QWidget;
    inputPage->setObjectName("inputPage");
    auto *inputLayout = new QVBoxLayout(inputPage);

    auto *nameEdit = new QLineEdit;
    nameEdit->setObjectName("nameEdit");
    nameEdit->setPlaceholderText("Enter your name...");
    inputLayout->addWidget(nameEdit);

    auto *messageEdit = new QLineEdit;
    messageEdit->setObjectName("messageEdit");
    messageEdit->setPlaceholderText("Enter a message...");
    inputLayout->addWidget(messageEdit);

    auto *greetLabel = new QLabel;
    greetLabel->setObjectName("greetLabel");
    inputLayout->addWidget(greetLabel);

    auto *btnGreet = new QPushButton("Greet");
    btnGreet->setObjectName("btnGreet");
    inputLayout->addWidget(btnGreet);

    QObject::connect(btnGreet, &QPushButton::clicked, [=]() {
        greetLabel->setText(nameEdit->text().trimmed() + " says: "
                            + messageEdit->text().trimmed());
    });

    inputLayout->addStretch();
    tabs->addTab(inputPage, "Input");

    // ── Tab 2: Controls ──────────────────────────────────────────────────
    auto *controlsPage = new QWidget;
    controlsPage->setObjectName("controlsPage");
    auto *controlsLayout = new QVBoxLayout(controlsPage);

    // Checkbox - intentionally unnamed (tests fallback path notation)
    auto *checkbox = new QCheckBox("Enable feature");
    controlsLayout->addWidget(checkbox);

    // Radio buttons in a group box
    auto *radioGroup = new QGroupBox("Select option");
    radioGroup->setObjectName("radioGroup");
    auto *radioLayout = new QVBoxLayout(radioGroup);

    auto *radioA = new QRadioButton("Option A");
    radioA->setObjectName("radioA");
    radioA->setChecked(true);
    radioLayout->addWidget(radioA);

    auto *radioB = new QRadioButton("Option B");
    radioB->setObjectName("radioB");
    radioLayout->addWidget(radioB);

    // Intentionally unnamed radio button
    auto *radioC = new QRadioButton("Option C");
    radioLayout->addWidget(radioC);

    controlsLayout->addWidget(radioGroup);

    // Combo box
    auto *combo = new QComboBox;
    combo->setObjectName("comboBox");
    combo->addItems({"First", "Second", "Third"});
    controlsLayout->addWidget(combo);

    // Status label for controls page
    auto *controlsStatus = new QLabel("Select controls above.");
    controlsStatus->setObjectName("controlsStatus");
    controlsLayout->addWidget(controlsStatus);

    QObject::connect(checkbox, &QCheckBox::toggled, [=](bool checked) {
        controlsStatus->setText(checked ? "Feature enabled" : "Feature disabled");
    });

    controlsLayout->addStretch();
    tabs->addTab(controlsPage, "Controls");

    // ── Tab 3: Buttons ───────────────────────────────────────────────────
    auto *buttonsPage = new QWidget;
    buttonsPage->setObjectName("buttonsPage");
    auto *buttonsLayout = new QVBoxLayout(buttonsPage);

    auto *clickCount = new QLabel("Clicks: 0");
    clickCount->setObjectName("clickCount");
    buttonsLayout->addWidget(clickCount);

    auto *btnRow = new QHBoxLayout;
    int counter = 0;

    // Three buttons - mix of named and unnamed
    auto *btn1 = new QPushButton("Button 1");
    btn1->setObjectName("btn1");
    btnRow->addWidget(btn1);

    auto *btn2 = new QPushButton("Button 2");
    btnRow->addWidget(btn2);  // intentionally unnamed

    auto *btn3 = new QPushButton("Button 3");
    btn3->setObjectName("btn3");
    btnRow->addWidget(btn3);

    buttonsLayout->addLayout(btnRow);

    auto updateCounter = [=]() mutable {
        ++counter;
        clickCount->setText(QString("Clicks: %1").arg(counter));
    };
    QObject::connect(btn1, &QPushButton::clicked, updateCounter);
    QObject::connect(btn2, &QPushButton::clicked, updateCounter);
    QObject::connect(btn3, &QPushButton::clicked, updateCounter);

    auto *btnReset = new QPushButton("Reset");
    btnReset->setObjectName("btnReset");
    buttonsLayout->addWidget(btnReset);
    QObject::connect(btnReset, &QPushButton::clicked, [=]() mutable {
        counter = 0;
        clickCount->setText("Clicks: 0");
    });

    buttonsLayout->addStretch();
    tabs->addTab(buttonsPage, "Buttons");

    mainLayout->addWidget(tabs);
    window.setCentralWidget(central);

    // ── Status bar ───────────────────────────────────────────────────────
    window.statusBar()->setObjectName("statusBar");
    window.statusBar()->showMessage("Ready - interact with elements to see agent output");

    window.show();
    return app.exec();
}
