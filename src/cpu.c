//
// Created by davidg on 03.07.25.
//

#include <cpu.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REGISTER_OPCODE(code, func) opcode_table[code] = func

static opcode_func_t opcode_table[256];
static void execute_opcode(CPU* cpu, uint8_t opcode);
static void init_opcode_table(void);

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

    init_opcode_table();
}

void op_ld_r_d8(CPU* cpu, uint8_t* reg) {
    *reg = mem_read(cpu->pc++);
    cpu->cycles += 8;
}

void op_nop(CPU* cpu) {
    cpu->cycles += 4;
}

void op_ld_a_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->a); }
void op_ld_b_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->b); }
void op_ld_c_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->c); }
void op_ld_d_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->d); }
void op_ld_e_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->e); }
void op_ld_h_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->h); }
void op_ld_l_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->l); }

void op_xor_a(CPU* cpu) {
    cpu->a ^= cpu->a; // XOR A with itself results in 0
    cpu->f = FLAG_Z; // Set Z flag
    cpu->cycles += 4;
}

void op_jp_a16(CPU* cpu) {
    uint16_t address = mem_read(cpu->pc) | (mem_read(cpu->pc + 1) << 8);
    cpu->pc += 2;
    cpu->pc = address;
    cpu->cycles += 16;
}

void op_call_a16(CPU* cpu) {
    uint16_t address = mem_read(cpu->pc) | (mem_read(cpu->pc + 1) << 8);
    cpu->sp -= 2; // Stack pointer decrement
    mem_write(cpu->sp, cpu->pc & 0xFF); // Low byte
    mem_write(cpu->sp + 1, (cpu->pc >> 8) & 0xFF); // High byte
    cpu->pc = address; // Jump to address
    cpu->cycles += 24;
}

void op_ret(CPU* cpu) {
    cpu->pc = mem_read(cpu->sp) | (mem_read(cpu->sp + 1) << 8); // Load PC from stack
    cpu->sp += 2; // Stack pointer increment
    cpu->cycles += 16;
}

void op_cp_d8(CPU* cpu) {
    uint8_t value = mem_read(cpu->pc++);
    uint8_t result = cpu->a - value;

    // Set flags
    cpu->f = 0;
    if (result == 0) cpu->f |= FLAG_Z;
    cpu->f |= FLAG_N;
    if ((cpu->a & 0x0F) < (value & 0x0F)) cpu->f |= FLAG_H;
    if (cpu->a < value) cpu->f |= FLAG_C;

    cpu->cycles += 8;
}

void op_add_a_d8(CPU* cpu) {
    uint8_t value = mem_read(cpu->pc++);
    uint16_t result = cpu->a + value;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) + (value & 0x0F) > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
    cpu->cycles += 8;
}

void op_add_a_a(CPU* cpu) {
    uint16_t result = cpu->a + cpu->a;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) + (cpu->a & 0x0F) > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
    cpu->cycles += 4;
}

void op_jr_nz_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    if (!(cpu->f & FLAG_Z)) { // Z flag not set
        cpu->pc += offset; // Jump
        cpu->cycles += 12;
    } else {
        cpu->cycles += 8; // No jump
    }
}
void op_load_b_b(CPU* cpu) { cpu->b = cpu->b; cpu->cycles += 4; }
void op_load_b_c(CPU* cpu) { cpu->b = cpu->c; cpu->cycles += 4; }
void op_load_b_d(CPU* cpu) { cpu->b = cpu->d; cpu->cycles += 4; }
void op_load_b_e(CPU* cpu) { cpu->b = cpu->e; cpu->cycles += 4; }
void op_load_b_h(CPU* cpu) { cpu->b = cpu->h; cpu->cycles += 4; }
void op_load_b_l(CPU* cpu) { cpu->b = cpu->l; cpu->cycles += 4; }
// TODO HL
void op_load_b_a(CPU* cpu) { cpu->b = cpu->a; cpu->cycles += 4; }

void op_load_c_b(CPU* cpu) { cpu->c = cpu->b; cpu->cycles += 4; }
void op_load_c_c(CPU* cpu) { cpu->c = cpu->c; cpu->cycles += 4; }
void op_load_c_d(CPU* cpu) { cpu->c = cpu->d; cpu->cycles += 4; }
void op_load_c_e(CPU* cpu) { cpu->c = cpu->e; cpu->cycles += 4; }
void op_load_c_h(CPU* cpu) { cpu->c = cpu->h; cpu->cycles += 4; }
void op_load_c_l(CPU* cpu) { cpu->c = cpu->l; cpu->cycles += 4; }
// TODO HL
void op_load_c_a(CPU* cpu) { cpu->c = cpu->a; cpu->cycles += 4; }

void init_opcode_table() {
    memset(opcode_table, 0, sizeof(opcode_table));

    REGISTER_OPCODE(0x00, op_nop);
    REGISTER_OPCODE(0x3E, op_ld_a_d8);
    REGISTER_OPCODE(0x06, op_ld_b_d8);
    REGISTER_OPCODE(0x0E, op_ld_c_d8);
    REGISTER_OPCODE(0x16, op_ld_d_d8);
    REGISTER_OPCODE(0x1E, op_ld_e_d8);
    REGISTER_OPCODE(0x26, op_ld_h_d8);
    REGISTER_OPCODE(0x2E, op_ld_l_d8);
    REGISTER_OPCODE(0xAF, op_xor_a);
    REGISTER_OPCODE(0xC3, op_jp_a16);
    REGISTER_OPCODE(0xCD, op_call_a16);
    REGISTER_OPCODE(0xC9, op_ret);
    REGISTER_OPCODE(0xFE, op_cp_d8);
    REGISTER_OPCODE(0xC6, op_add_a_d8);
    REGISTER_OPCODE(0x87, op_add_a_a);
    REGISTER_OPCODE(0x20, op_jr_nz_r8);
    REGISTER_OPCODE(0x40, op_load_b_b);
    REGISTER_OPCODE(0x41, op_load_b_c);
    REGISTER_OPCODE(0x42, op_load_b_d);
    REGISTER_OPCODE(0x43, op_load_b_e);
    REGISTER_OPCODE(0x44, op_load_b_h);
    REGISTER_OPCODE(0x45, op_load_b_l);
    REGISTER_OPCODE(0x47, op_load_b_a);
    REGISTER_OPCODE(0x48, op_load_c_b);
    REGISTER_OPCODE(0x49, op_load_c_c);
    REGISTER_OPCODE(0x4A, op_load_c_d);
    REGISTER_OPCODE(0x4B, op_load_c_e);
    REGISTER_OPCODE(0x4C, op_load_c_h);
    REGISTER_OPCODE(0x4D, op_load_c_l);
    REGISTER_OPCODE(0x4F, op_load_c_a);
}

void cpu_step(CPU* cpu) {
    uint8_t opcode = mem_read(cpu->pc++);
    execute_opcode(cpu, opcode);
}

void execute_opcode(CPU* cpu, uint8_t opcode) {
    opcode_func_t handler = opcode_table[opcode];

    if (handler != NULL) {
        handler(cpu);
    } else {
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
