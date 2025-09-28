cmake_minimum_required(VERSION 3.18)
set(LLVM_MOS_PLATFORM c64)
find_package(llvm-mos-sdk REQUIRED)
project(c64jp_c VERSION 1.0.0)

# Build configuration
add_compile_options(-Oz -Wall -flto)

# jtxt library
add_library(jtxt STATIC
    src/jtxt.c
    src/jtxt_text.c
    src/jtxt_bitmap.c
    src/jtxt_charset.c
    src/jtxt_resource.c
)

target_include_directories(jtxt PUBLIC include)

# IME library translated from Prog8 ime.p8
add_library(ime STATIC
    src/ime.c
)

target_link_libraries(ime PUBLIC jtxt)
target_include_directories(ime PUBLIC include)

# Test programs
add_executable(hello_c src/hello.c)
target_link_libraries(hello_c jtxt ime)

add_executable(test_jtxt test/test_jtxt.c)
target_link_libraries(test_jtxt jtxt)

add_executable(ime_test_c src/ime_test.c)
target_link_libraries(ime_test_c jtxt ime)

# Generate PRG files
add_custom_command(
    TARGET hello_c POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:hello_c> ${CMAKE_BINARY_DIR}/hello_c.prg
)

add_custom_command(
    TARGET test_jtxt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:test_jtxt> ${CMAKE_BINARY_DIR}/test_jtxt.prg
)

add_custom_command(
    TARGET ime_test_c POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ime_test_c> ${CMAKE_BINARY_DIR}/ime_test_c.prg
)
