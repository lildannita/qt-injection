set(SOURCE_REPS RemoteObjectExample.rep)
qt5_generate_repc(SOURCE_SRCS ${SOURCE_REPS} SOURCE)

add_executable(ro_host
    main.cpp
    RemoteObjectExample.hpp
    ${SOURCE_SRCS}
)
target_link_libraries(ro_host PRIVATE Qt5::Core Qt5::RemoteObjects)
target_include_directories(ro_host PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
