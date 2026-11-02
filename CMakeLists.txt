cmake_minimum_required(VERSION 3.20)
project(MiniCompiler VERSION 1.0.0 LANGUAGES CXX)

add_executable(minic
    src/main.cpp src/lexer.cpp src/parser.cpp src/ast.cpp
    src/semantic.cpp src/optimizer.cpp src/codegen.cpp
)
target_include_directories(minic PRIVATE include)
target_compile_features(minic PRIVATE cxx_std_17)
set_target_properties(minic PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(minic PRIVATE /W4)
else()
    target_compile_options(minic PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(MINIC_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(MINIC_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(minic PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(minic PRIVATE -fsanitize=address,undefined)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    find_program(MINIC_CLANG NAMES clang REQUIRED)
    add_test(NAME compiler-integration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compiler.py"
                --compiler $<TARGET_FILE:minic> --clang "${MINIC_CLANG}")
    set_tests_properties(compiler-integration PROPERTIES TIMEOUT 180)
endif()

install(TARGETS minic RUNTIME DESTINATION bin)
