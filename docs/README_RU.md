# qt-injection

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white) ![Qt](https://img.shields.io/badge/Qt-5.15%20LTS-41CD52?style=flat-square&logo=qt&logoColor=white) ![Platforms](https://img.shields.io/badge/Platforms-Linux%20%7C%20macOS%20%7C%20Windows-6f42c1?style=flat-square) ![License](https://img.shields.io/badge/License-GPLv3-dc3545?style=flat-square)

[![EN](https://img.shields.io/badge/README-EN-007acc?style=flat-square)](../README.md)

Минимальный монорепозиторий для экспериментальной проверки метода динамической инъекции кода как способа неинвазивного автоматизированного тестирования GUI приложений на Qt 5.15.

Тестовый агент (динамическая библиотека) внедряется в процесс целевого приложения и [подключается](../agent/agent.cpp) к внутренней объектной модели Qt через `qtHookData`. [Фильтр событий](../agent/object_tracker.cpp), установленный на `QCoreApplication`, перехватывает события пользовательского взаимодействия (клики мыши, нажатия клавиш, изменения фокуса) и выводит тип события, реальное имя класса, `objectName` и иерархический путь целевого элемента — подтверждая наблюдаемость внутренней объектной модели интерфейса во время выполнения без модификации исходного кода приложения.

---

## Структура репозитория

```
├── CMakeLists.txt                  # корневая сборка
├── agent/
│   ├── CMakeLists.txt
│   ├── agent.cpp                   # установка хуков и отложенная инициализация ★
│   ├── agent_initializer.hpp/cpp   # отложенная настройка через QueuedConnection
│   ├── object_tracker.hpp/cpp      # фильтр событий — логирование взаимодействий
│   └── object_path.hpp/cpp         # вычисление иерархического пути
├── examples/
│   ├── qt_widgets_app/				# тестовое приложение Qt Widgets
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   └── qt_quick_app/				# тестовое приложение Qt Quick
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── qml.qrc
│       └── qml/main.qml
├── windows_injector/
│   ├── CMakeLists.txt
│   └── main.cpp                    # инжектор DLL (только Windows)
├── run.py                          # скрипт сборки и запуска
└── builds/                         # создаётся при сборке (в .gitignore)
    └── <компилятор>/
        ├── bin/                    # исполняемые файлы
        └── lib/                    # библиотека агента
```

## Принцип работы

1. **Инъекция** — библиотека агента загружается в процесс целевого приложения:
   - Linux: `LD_PRELOAD`
   - macOS: `DYLD_INSERT_LIBRARIES`
   - Windows: `CreateProcessW(CREATE_SUSPENDED)` + `CreateRemoteThread(LoadLibraryW)`

2. **Установка хуков** — `Q_COREAPP_STARTUP_FUNCTION` регистрирует функцию установки хуков для вызова при конструировании `QCoreApplication`. Хуки устанавливаются в слоты `qtHookData`.

3. **Отложенная инициализация** — хук `Startup` создаёт объект `AgentInitializer`, который откладывает установку `EventTracker` на следующую итерацию event loop через `QMetaObject::invokeMethod(Qt::QueuedConnection)`. Это исключает работу с полусконструированным `QCoreApplication`.

4. **Отслеживание событий** — `EventTracker` (фильтр событий на `QCoreApplication`) перехватывает события пользовательского взаимодействия. Для каждого события, доставленного GUI-объекту (`QWidget` или `QQuickItem`), выводит тип события, реальное имя класса, `objectName` и иерархический путь.

## Зависимости

| Зависимость | Версия | Примечание |
|-------------|--------|------------|
| Qt          | 5.15 LTS | Widgets, Quick, Core (+ приватные заголовки) |
| CMake       | ≥ 3.16 | |
| Компилятор C++ | C++17 | GCC, Clang, MSVC или MinGW |
| Python      | ≥ 3.8 | для `run.py` (опционально) |
| Ninja       | любая | рекомендуется (автоопределяется `run.py`) |

## Сборка и запуск

### Быстрый способ (все платформы)

```bash
python run.py
```

Скрипт конфигурирует, собирает (в `builds/<компилятор>/`), предлагает выбрать приложение и запускает его с внедрённым агентом.

### Ручная сборка

Ручная сборка возможна через CMake на Linux/macOS и Windows. Команды зависят от выбранного компилятора и окружения и намеренно опущены — см. [run.py](../run.py) (он выполняет те же шаги), либо инструкции в репозитории/CI.

### Ручной запуск

**Linux:**
```bash
cd /path/to/build_dir
LD_PRELOAD=lib/libagent.so  bin/qt_widgets_app # (или bin/qt_quick_app)
```

**macOS:**
```bash
cd /path/to/build_dir
DYLD_INSERT_LIBRARIES=lib/libagent.dylib  bin/qt_widgets_app # (или bin/qt_quick_app)
```

**Windows:**
```bat
cd /path/to/build_dir
bin\windows_injector.exe  bin\qt_widgets_app.exe  lib\agent.dll # (или bin\qt_quick_app)
```

## Ожидаемый вывод

Агент пишет в **stderr**. Взаимодействуйте с элементами интерфейса, чтобы увидеть их пути:

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

## Примечания

- Агент делегирует ранее установленным хукам, поэтому безопасно работает в цепочке с другими инструментами (напр. GammaRay).
- Узлы пути используют `objectName` если задан, иначе `ClassName_N` (индекс среди одноклассных siblings). Текст UI никогда не является частью пути.
- QML-суффиксы имён классов (`_QMLTYPE_N`, `_QML_N`) удаляются для стабильности путей.
