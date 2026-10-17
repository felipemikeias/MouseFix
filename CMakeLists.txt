cmake_minimum_required(VERSION 3.16)
project(MouseFix VERSION 1.0.0 LANGUAGES C)

option(MOUSEFIX_TESTS "Compilar os testes do filtro" ON)

add_library(debounce STATIC src/debounce.c)
target_include_directories(debounce PUBLIC src)

if(WIN32)
    enable_language(RC)
    add_executable(MouseFix WIN32 src/main.c src/settings.c res/mousefix.rc)
    target_link_libraries(MouseFix PRIVATE debounce user32 shell32 advapi32)
    target_compile_definitions(MouseFix PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
    if(MSVC)
        # Fontes em UTF-8; CRT estático para um .exe sem dependências.
        target_compile_options(MouseFix PRIVATE /utf-8 /W4)
        target_compile_options(debounce PRIVATE /W4)
        set_property(TARGET MouseFix debounce PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # O manifesto vem do .rc; não gerar outro.
        target_link_options(MouseFix PRIVATE /MANIFEST:NO)
    else()
        target_compile_options(MouseFix PRIVATE -Wall -Wextra)
        target_link_options(MouseFix PRIVATE -static)
    endif()
endif()

if(MOUSEFIX_TESTS)
    enable_testing()
    add_executable(test_debounce tests/test_debounce.c)
    target_link_libraries(test_debounce PRIVATE debounce)
    if(MSVC)
        target_compile_options(test_debounce PRIVATE /utf-8)
    endif()
    add_test(NAME debounce COMMAND test_debounce)
endif()
