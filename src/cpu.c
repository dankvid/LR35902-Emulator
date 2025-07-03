//
// Created by davidg on 03.07.25.
//

#include <cpu.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void execute_opcode(CPU* cpu, uint8_t opcode);

void cpu_init(CPU* cpu) {
    memset(cpu, 0, sizeof(CPU));

    cpu->a = 0x01;
    cpu->f = 0xB0;
    cpu->b = 0x00;
    cpu->c = 0x13;
    cpu->d = 0x00;
    cpu->e = 0xD8;
    cpu->h = 0x01;
    cpu->l = 0x4D;

    cpu->sp = 0xFFFE; // Initial stack pointer
    cpu->pc = 0x0100; // Initial program counter

    cpu->halted = 0;
    cpu->ime = 0; // Interrupt Master Enable
    cpu->cycles = 0; // Cycle count
}

void cpu_step(CPU* cpu) {
    uint8_t opcode = mem_read(cpu->pc++);
    execute_opcode(cpu, opcode);
}

void execute_opcode(CPU* cpu, uint8_t opcode) {
    switch (opcode) {
        case 0x00: // NOP
            cpu->cycles += 4;
        break;

        case 0x3E: // LD A, d8
            cpu->a = mem_read(cpu->pc++);
        cpu->cycles += 8;
        break;

        default:
            printf("Unbekannter Opcode: 0x%02X bei PC: 0x%04X\n", opcode, cpu->pc - 1);
        exit(1);
    }
}

void cpu_print_state(const CPU* cpu) {
    printf("PC=%04X SP=%04X AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X\n",
        cpu->pc, cpu->sp,
        cpu->a, cpu->f, cpu->b, cpu->c,
        cpu->d, cpu->e, cpu->h, cpu->l);
}
