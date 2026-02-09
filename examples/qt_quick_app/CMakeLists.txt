add_executable(qt_quick_app main.cpp qml.qrc)
target_link_libraries(qt_quick_app PRIVATE Qt5::Core Qt5::Quick)
