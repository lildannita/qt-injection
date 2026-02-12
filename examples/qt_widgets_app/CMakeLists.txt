set(qt_widgets_app_SOURCES
    main.cpp
)

if(ANDROID)
    add_library(qt_widgets_app SHARED ${qt_widgets_app_SOURCES})
    add_qt_android_apk(qt_widgets_app_apk qt_widgets_app
        NAME "qt_widgets_app"
        PACKAGE_NAME "ru.lildannita.qt_widgets_app"
        PACKAGE_SOURCES ${CMAKE_SOURCE_DIR}/platforms/android
        APK_BUILD_TYPE "${APK_BUILD_TYPE}"
    )
else()
    add_executable(qt_widgets_app ${qt_widgets_app_SOURCES})
endif()

target_link_libraries(qt_widgets_app PRIVATE Qt5::Core Qt5::Widgets)
