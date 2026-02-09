# qt-injection

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white) ![Qt](https://img.shields.io/badge/Qt-5.15%20LTS-41CD52?style=flat-square&logo=qt&logoColor=white) ![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows-6f42c1?style=flat-square) ![License](https://img.shields.io/badge/License-GPLv3-dc3545?style=flat-square)

[![RU](https://img.shields.io/badge/README-RU-007acc?style=flat-square)](docs/README_RU.md)

Minimal monorepo for experimental verification of dynamic code injection as a method for non-invasive automated GUI testing of Qt 5.15 applications.

A test agent (shared library) is injected into the target application's process and [hooks](agent/agent.cpp) into Qt's internal object model via `qtHookData`. An [event filter](agent/object_tracker.cpp) installed on `QCoreApplication` intercepts user interaction events (mouse clicks, key presses, focus changes) and logs the target element's real class name, `objectName`, and hierarchical path — confirming that the internal object model is observable at runtime without modifying the application's source code.

---

## Repository structure

```
├── CMakeLists.txt                  # root build
├── agent/
│   ├── CMakeLists.txt
│   ├── agent.cpp                   # hook installation & deferred init ★
│   ├── agent_initializer.hpp/cpp   # QueuedConnection-based deferred setup
│   ├── object_tracker.hpp/cpp      # event filter — interaction logging
│   └── object_path.hpp/cpp         # hierarchical path computation
├── examples/
│   ├── qt_widgets_app/				# Qt Widgets test app
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── qt_quick_app/				# Qt Quick test app
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── qml.qrc
│       └── qml/main.qml
├── windows_injector/
│   ├── CMakeLists.txt
│   └── main.cpp                    # DLL injector (Windows only)
├── run.py                          # build + launch helper
└── builds/                         # created by the build (gitignored)
    └── <compiler>/
        ├── bin/                    # executables
        └── lib/                    # agent library
```

## How it works

1. **Injection** — the agent library is loaded into the target process:
   - Linux: `LD_PRELOAD`
   - macOS: `DYLD_INSERT_LIBRARIES`
   - Windows: `CreateProcessW(CREATE_SUSPENDED)` + `CreateRemoteThread(LoadLibraryW)`

2. **Hook installation** — `Q_COREAPP_STARTUP_FUNCTION` registers hook setup to run during `QCoreApplication` construction. Hooks are installed into `qtHookData` slots.

3. **Deferred initialization** — the `Startup` hook creates an `AgentInitializer` object that defers `EventTracker` installation to the next event loop iteration via `QMetaObject::invokeMethod(Qt::QueuedConnection)`. This avoids operating on a half-constructed `QCoreApplication`.

4. **Event tracking** — `EventTracker` (event filter on `QCoreApplication`) intercepts user interaction events. For each event on a GUI object (`QWidget` or `QQuickItem`), it logs the event type, real class name, `objectName`, and hierarchical path.

## Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| Qt         | 5.15 LTS | Widgets, Quick, Core (+ private headers) |
| CMake      | ≥ 3.16 | |
| C++ compiler | C++17 | GCC, Clang, MSVC, or MinGW |
| Python     | ≥ 3.8 | for `run.py` (optional) |
| Ninja      | any | recommended (auto-detected by `run.py`) |

## Building and running

### Quickest way (all platforms)

```bash
python run.py
```

The script configures, builds (into `builds/<compiler>/`), asks which app to run, and launches it with the agent injected.

### Manual build

Manual build is available via CMake (using the Ninja generator) on Linux/macOS and Windows. The exact commands depend on the selected toolchain and environment and are intentionally omitted — see [run.py](run.py) (it performs the same steps) or the build instructions in the repository/CI.

### Manual run

**Linux:**
```bash
cd /path/to/build_dir
LD_PRELOAD=lib/libagent.so  bin/qt_widgets_app # (or bin/qt_quick_app)
```

**macOS:**
```bash
cd /path/to/build_dir
DYLD_INSERT_LIBRARIES=lib/libagent.dylib  bin/qt_widgets_app # (or bin/qt_quick_app)
```

**Windows:**
```bat
cd /path/to/build_dir
bin\windows_injector.exe  bin\qt_widgets_app.exe  lib\agent.dll # (or bin\qt_quick_app)
```

## Expected output

The agent prints to **stderr**. Interact with GUI elements to see their paths:

```
[agent] hooks installed successfully.
[agent] startup hook fired.
[agent] event tracker installed.
[agent] interact with GUI elements to see their paths.
[*] FocusIn       QLineEdit    name=nameEdit     path=/mainWindow/centralWidget/tabWidget/inputPage/nameEdit
[*] KeyPress      QLineEdit    name=nameEdit     path=/mainWindow/centralWidget/tabWidget/inputPage/nameEdit
[*] MouseButtonPress  QPushButton  name=btnGreet path=/mainWindow/centralWidget/tabWidget/inputPage/btnGreet
[*] FocusIn       QPushButton  name=btnGreet     path=/mainWindow/centralWidget/tabWidget/inputPage/btnGreet
```

## Notes

- The agent delegates to any previously installed hooks, so it is safe to chain with other tools (e.g. GammaRay).
- Path nodes use `objectName` when set, otherwise `ClassName_N` (index among same-class siblings). UI text is never part of the path.
- QML class name suffixes (`_QMLTYPE_N`, `_QML_N`) are stripped for path stability.
