add_library(agent SHARED
    agent.cpp
    agent_initializer.hpp
    agent_initializer.cpp
    object_tracker.hpp
    object_tracker.cpp
    object_path.hpp
    object_path.cpp
    adb_socket_proxy.hpp
    adb_socket_proxy.cpp
)

set(RO_REP_FILE "${CMAKE_SOURCE_DIR}/examples/remote_object_host/RemoteObjectExample.rep")
qt5_generate_repc(RO_REPLICA_SRCS ${RO_REP_FILE} REPLICA)
target_sources(agent PRIVATE ${RO_REPLICA_SRCS})
target_include_directories(agent PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(agent PRIVATE
    Qt5::Core
    Qt5::CorePrivate
    Qt5::Widgets
    Qt5::Quick
    Qt5::RemoteObjects
    Qt5::Network
)

# On Windows the DLL goes to bin/ so the injector can find it easily;
# on POSIX it stays in lib/.
if(WIN32)
    set_target_properties(agent PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    foreach(CFG IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(TOUPPER ${CFG} CFG_UP)
        set_target_properties(agent PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CFG_UP} "${CMAKE_BINARY_DIR}/lib"
        )
    endforeach()
endif()
