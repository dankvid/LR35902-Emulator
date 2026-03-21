cmake_minimum_required(VERSION 3.30)
project(LR35902_Emulator C)

set(CMAKE_C_STANDARD 11)

enable_testing()

add_executable(LR35902_Emulator
        src/main.c
        src/cpu.c
        src/memory.c
        src/util.c
)

add_executable(test_rom_generator
        tests/test_rom_generator.c
)

target_include_directories(LR35902_Emulator PRIVATE include)

add_executable(opcode_non_cb_tests
        tests/opcode_non_cb_tests.c
        src/cpu.c
        src/memory.c
)

target_include_directories(opcode_non_cb_tests PRIVATE include)

add_test(NAME opcode_non_cb_tests COMMAND opcode_non_cb_tests)

add_executable(opcode_cb_tests
        tests/opcode_cb_tests.c
        src/cpu.c
        src/memory.c
)

target_include_directories(opcode_cb_tests PRIVATE include)

add_test(NAME opcode_cb_tests COMMAND opcode_cb_tests)

