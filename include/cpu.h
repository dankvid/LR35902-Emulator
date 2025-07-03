//
// Created by davidg on 03.07.25.
//

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct {
    // 8-bit
    uint8_t a, f;
    uint8_t b, c;
    uint8_t d, e;
    uint8_t h, l;

    // 16-bit
    uint16_t sp;
    uint16_t pc;

    int halted;
    int ime;
    int cycles;
} CPU;

void cpu_init(CPU* cpu);

void cpu_step(CPU* cpu);

void cpu_print_state(const CPU* cpu);

#endif // CPU_H
