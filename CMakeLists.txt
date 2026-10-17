cmake_minimum_required(VERSION 3.16)
project(MouseFix VERSION 1.1.0 LANGUAGES C)

option(MOUSEFIX_TESTS "Build the filter tests" ON)

add_library(debounce STATIC src/debounce.c)
target_include_directories(debounce PUBLIC src)

if(WIN32)
    enable_language(RC)
    add_executable(MouseFix WIN32
        src/main.c src/settings.c src/strings.c res/mousefix.rc)
    target_link_libraries(MouseFix PRIVATE debounce user32 shell32 advapi32)
    # Vista headers for the larger tray structure; main.c only uses it after
    # checking the running Windows version, and imports nothing newer than XP.
    target_compile_definitions(MouseFix PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0600)
    if(MSVC)
        # UTF-8 sources; static CRT for an .exe with no dependencies.
        target_compile_options(MouseFix PRIVATE /utf-8 /W4)
        target_compile_options(debounce PRIVATE /W4)
        set_property(TARGET MouseFix debounce PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # The manifest comes from the .rc file; do not generate another one.
        target_link_options(MouseFix PRIVATE /MANIFEST:NO)
    else()
        # MinGW links against msvcrt.dll, which every Windows since XP has.
        # The PE header must declare 5.1 or Windows XP refuses to start it.
        target_compile_options(MouseFix PRIVATE -Wall -Wextra)
        target_link_options(MouseFix PRIVATE -static -s
            -Wl,--major-os-version,5,--minor-os-version,1
            -Wl,--major-subsystem-version,5,--minor-subsystem-version,1
            -Wl,--dynamicbase,--nxcompat)
    endif()
endif()

if(MOUSEFIX_TESTS)
    enable_testing()
    add_executable(test_debounce tests/test_debounce.c)
    target_link_libraries(test_debounce PRIVATE debounce)
    add_test(NAME debounce COMMAND test_debounce)
endif()
