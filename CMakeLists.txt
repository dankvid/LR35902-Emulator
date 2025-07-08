cmake_minimum_required(VERSION 3.30)
project(LR35902_Emulator C)

set(CMAKE_C_STANDARD 11)

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