//
// Created by davidg on 03.07.25.
//

#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

/**
 * GameBoy CPU structure
 */
typedef struct {
    // 8-bit
    uint8_t a, f;
    uint8_t b, c;
    uint8_t d, e;
    uint8_t h, l;

    // 16-bit
    uint16_t sp;
    uint16_t pc;

    // CPU Status
    bool halted;
    bool ime;
    uint32_t cycles;
} CPU;

typedef void (*opcode_func_t)(CPU* cpu);

void cpu_init(CPU* cpu);

void cpu_step(CPU* cpu);

void cpu_print_state(const CPU* cpu);

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

#define SET_FLAG(cpu, flag)   ((cpu)->f |= (flag))
#define CLEAR_FLAG(cpu, flag) ((cpu)->f &= ~(flag))
#define GET_FLAG(cpu, flag)   (((cpu)->f & (flag)) != 0)

#define REG_AF(cpu) (((cpu)->a << 8) | (cpu)->f)
#define REG_BC(cpu) (((cpu)->b << 8) | (cpu)->c)
#define REG_DE(cpu) (((cpu)->d << 8) | (cpu)->e)
#define REG_HL(cpu) (((cpu)->h << 8) | (cpu)->l)

#endif // CPU_H
