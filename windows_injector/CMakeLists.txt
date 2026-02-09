add_executable(windows_injector main.cpp)

set_target_properties(windows_injector PROPERTIES WIN32_EXECUTABLE OFF)

if(MSVC)
    target_compile_options(windows_injector PRIVATE /W4 /EHsc)
elseif(MINGW)
    target_link_options(windows_injector PRIVATE -municode)
    target_compile_options(windows_injector PRIVATE -Wall -Wextra -pedantic)
endif()
