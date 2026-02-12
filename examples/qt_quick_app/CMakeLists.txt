set(qt_quick_app_SOURCES
    main.cpp
    qml.qrc
)

if(ANDROID)
    add_library(qt_quick_app SHARED ${qt_quick_app_SOURCES})
    add_qt_android_apk(qt_quick_app_apk qt_quick_app
        NAME "qt_quick_app"
        PACKAGE_NAME "ru.lildannita.qt_quick_app"
        PACKAGE_SOURCES ${CMAKE_SOURCE_DIR}/platforms/android
        APK_BUILD_TYPE "${APK_BUILD_TYPE}"
    )
else()
    add_executable(qt_quick_app ${qt_quick_app_SOURCES})
endif()

target_link_libraries(qt_quick_app PRIVATE Qt5::Core Qt5::Quick)
