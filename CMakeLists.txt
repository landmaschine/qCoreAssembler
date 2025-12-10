cmake_minimum_required(VERSION 3.18)
project(sbasm LANGUAGES C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

flex_target(LEXER lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lexer.h)

bison_target(PARSER parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)

add_flex_bison_dependency(LEXER PARSER)

add_library(assembler_lib STATIC
    ${BISON_PARSER_OUTPUTS}
    ${FLEX_LEXER_OUTPUTS}
    InstructionEncoder.cpp
    ast.h
    common.h
    SymbolTable.h
    InstructionEncoder.h
)

target_include_directories(assembler_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    assembler_lib
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -O3
    -g
)

# target_link_options(${PROJECT_NAME} PRIVATE 
#     -static-libstdc++
#     -static-libgcc
#     -static
# )

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
