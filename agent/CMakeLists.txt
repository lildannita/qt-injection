add_library(agent SHARED agent.cpp object_path.hpp object_path.cpp)

target_link_libraries(agent PRIVATE Qt5::Core Qt5::CorePrivate Qt5::Quick Qt5::Widgets)

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
