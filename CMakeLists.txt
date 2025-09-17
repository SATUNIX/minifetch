cmake_minimum_required(VERSION 3.16)
project(minifetch C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(WARN_FLAGS
    -Wall
    -Wextra
    -Werror
    -pedantic
    -D_POSIX_C_SOURCE=200809L
)

set(SRC_BASE
    src/main.c
    src/cli.c
    src/core.c
    src/linux_extras.c
    src/compat.c
    src/term.c
    src/hidden.c
)

set(LOGO_TXT ${CMAKE_CURRENT_SOURCE_DIR}/frames/logo.txt)
set(LOGO_GEN ${CMAKE_CURRENT_BINARY_DIR}/logo_data.c)

add_custom_command(
    OUTPUT ${LOGO_GEN}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E env sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_logo.sh ${LOGO_TXT} ${LOGO_GEN}
    DEPENDS ${LOGO_TXT} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_logo.sh
    COMMENT "Embedding logo into ${LOGO_GEN}"
    VERBATIM
)

set(COMMON_SOURCES ${SRC_BASE} ${LOGO_GEN})

add_executable(minifetch ${COMMON_SOURCES})
target_include_directories(minifetch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(minifetch PRIVATE ${WARN_FLAGS})
target_link_libraries(minifetch PRIVATE m)

add_executable(minifetch-linux ${COMMON_SOURCES})
target_include_directories(minifetch-linux PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(minifetch-linux PRIVATE ${WARN_FLAGS})
target_compile_definitions(minifetch-linux PRIVATE MINIFETCH_LINUX_EXT=1)
target_link_libraries(minifetch-linux PRIVATE m)

add_custom_target(smoke
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$ENV{PATH}"
        "MINIFETCH_BIN=$<TARGET_FILE:minifetch>"
        "MINIFETCH_LINUX_BIN=$<TARGET_FILE:minifetch-linux>"
        sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh
    DEPENDS minifetch minifetch-linux
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running smoke tests"
)

enable_testing()
add_test(NAME smoke
    COMMAND ${CMAKE_COMMAND} -E env
        "PATH=$ENV{PATH}"
        "MINIFETCH_BIN=$<TARGET_FILE:minifetch>"
        "MINIFETCH_LINUX_BIN=$<TARGET_FILE:minifetch-linux>"
        sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh
)
