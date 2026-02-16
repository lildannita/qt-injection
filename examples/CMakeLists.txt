if(ANDROID)
    include(${CMAKE_SOURCE_DIR}/3rdparty/QtAndroidCMake/AddQtAndroidApk.cmake)
endif()

add_subdirectory(qt_quick_app)
add_subdirectory(qt_widgets_app)
add_subdirectory(remote_object_host)
