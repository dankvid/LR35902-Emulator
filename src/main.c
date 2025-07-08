#include <stdio.h>
#include "cpu.h"
#include "memory.h"

int main(void) {
    CPU cpu;
    memory_init();
    cpu_init(&cpu);

    // Test-ROM laden
    load_rom("test.bin");

    printf("=== LR35902 Emulator Test ===\n");

    // 20 Schritte ausführen
    for (int i = 0; i < 20; i++) {
        printf("Schritt %d: ", i + 1);
        cpu_print_state(&cpu);
        cpu_step(&cpu);
    }

    return 0;
}