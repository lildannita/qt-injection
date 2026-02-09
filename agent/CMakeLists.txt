add_library(agent SHARED
    agent.cpp
    agent_initializer.hpp
    agent_initializer.cpp
    object_tracker.hpp
    object_tracker.cpp
    object_path.hpp
    object_path.cpp
)

target_link_libraries(agent PRIVATE
    Qt5::Core
    Qt5::CorePrivate
    Qt5::Widgets
    Qt5::Quick
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
