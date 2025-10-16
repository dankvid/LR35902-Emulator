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

static void op_ld_r_d8(CPU* cpu, uint8_t* reg) {
    *reg = mem_read(cpu->pc++);
    cpu->cycles += 8;
}

static void op_nop(CPU* cpu) {
    cpu->cycles += 4;
}

static void op_ld_a_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->a); }
static void op_ld_b_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->b); }
static void op_ld_c_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->c); }
static void op_ld_d_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->d); }
static void op_ld_e_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->e); }
static void op_ld_h_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->h); }
static void op_ld_l_d8(CPU* cpu) { op_ld_r_d8(cpu, &cpu->l); }

static void op_xor_a(CPU* cpu) {
    cpu->a ^= cpu->a; // XOR A with itself results in 0
    cpu->f = FLAG_Z; // Set Z flag
    cpu->cycles += 4;
}

static void op_jp_a16(CPU* cpu) {
    uint16_t address = mem_read(cpu->pc) | (mem_read(cpu->pc + 1) << 8);
    cpu->pc += 2;
    cpu->pc = address;
    cpu->cycles += 16;
}

static void op_call_a16(CPU* cpu) {
    uint16_t address = mem_read(cpu->pc) | (mem_read(cpu->pc + 1) << 8);
    cpu->sp -= 2; // Stack pointer decrement
    mem_write(cpu->sp, cpu->pc & 0xFF); // Low byte
    mem_write(cpu->sp + 1, (cpu->pc >> 8) & 0xFF); // High byte
    cpu->pc = address; // Jump to address
    cpu->cycles += 24;
}

static void op_ret(CPU* cpu) {
    cpu->pc = mem_read(cpu->sp) | (mem_read(cpu->sp + 1) << 8); // Load PC from stack
    cpu->sp += 2; // Stack pointer increment
    cpu->cycles += 16;
}

static void op_cp_d8(CPU* cpu) {
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

static void op_add_a_d8(CPU* cpu) {
    uint8_t value = mem_read(cpu->pc++);
    uint16_t result = cpu->a + value;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) + (value & 0x0F) > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
    cpu->cycles += 8;
}

static void op_add_a_a(CPU* cpu) {
    uint16_t result = cpu->a + cpu->a;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) + (cpu->a & 0x0F) > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
    cpu->cycles += 4;
}

static void op_jr_nz_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    if (!(cpu->f & FLAG_Z)) { // Z flag not set
        cpu->pc += offset; // Jump
        cpu->cycles += 12;
    } else {
        cpu->cycles += 8; // No jump
    }
}
static void op_load_b_b(CPU* cpu) { cpu->b = cpu->b; cpu->cycles += 4; }
static void op_load_b_c(CPU* cpu) { cpu->b = cpu->c; cpu->cycles += 4; }
static void op_load_b_d(CPU* cpu) { cpu->b = cpu->d; cpu->cycles += 4; }
static void op_load_b_e(CPU* cpu) { cpu->b = cpu->e; cpu->cycles += 4; }
static void op_load_b_h(CPU* cpu) { cpu->b = cpu->h; cpu->cycles += 4; }
static void op_load_b_l(CPU* cpu) { cpu->b = cpu->l; cpu->cycles += 4; }
static void op_load_b_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->b = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_b_a(CPU* cpu) { cpu->b = cpu->a; cpu->cycles += 4; }

static void op_load_c_b(CPU* cpu) { cpu->c = cpu->b; cpu->cycles += 4; }
static void op_load_c_c(CPU* cpu) { cpu->c = cpu->c; cpu->cycles += 4; }
static void op_load_c_d(CPU* cpu) { cpu->c = cpu->d; cpu->cycles += 4; }
static void op_load_c_e(CPU* cpu) { cpu->c = cpu->e; cpu->cycles += 4; }
static void op_load_c_h(CPU* cpu) { cpu->c = cpu->h; cpu->cycles += 4; }
static void op_load_c_l(CPU* cpu) { cpu->c = cpu->l; cpu->cycles += 4; }
static void op_load_c_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->c = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_c_a(CPU* cpu) { cpu->c = cpu->a; cpu->cycles += 4; }

static void op_load_d_b(CPU* cpu) { cpu->d = cpu->b; cpu->cycles += 4; }
static void op_load_d_c(CPU* cpu) { cpu->d = cpu->c; cpu->cycles += 4; }
static void op_load_d_d(CPU* cpu) { cpu->d = cpu->d; cpu->cycles += 4; }
static void op_load_d_e(CPU* cpu) { cpu->d = cpu->e; cpu->cycles += 4; }
static void op_load_d_h(CPU* cpu) { cpu->d = cpu->h; cpu->cycles += 4; }
static void op_load_d_l(CPU* cpu) { cpu->d = cpu->l; cpu->cycles += 4; }
static void op_load_d_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->d = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_d_a(CPU* cpu) { cpu->d = cpu->a; cpu->cycles += 4; }

static void op_load_e_b(CPU* cpu) { cpu->e = cpu->b; cpu->cycles += 4; }
static void op_load_e_c(CPU* cpu) { cpu->e = cpu->c; cpu->cycles += 4; }
static void op_load_e_d(CPU* cpu) { cpu->e = cpu->d; cpu->cycles += 4; }
static void op_load_e_e(CPU* cpu) { cpu->e = cpu->e; cpu->cycles += 4; }
static void op_load_e_h(CPU* cpu) { cpu->e = cpu->h; cpu->cycles += 4; }
static void op_load_e_l(CPU* cpu) { cpu->e = cpu->l; cpu->cycles += 4; }
static void op_load_e_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->e = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_e_a(CPU* cpu) { cpu->e = cpu->a; cpu->cycles += 4; }

static void op_load_h_b(CPU* cpu) { cpu->h = cpu->b; cpu->cycles += 4; }
static void op_load_h_c(CPU* cpu) { cpu->h = cpu->c; cpu->cycles += 4; }
static void op_load_h_d(CPU* cpu) { cpu->h = cpu->d; cpu->cycles += 4; }
static void op_load_h_e(CPU* cpu) { cpu->h = cpu->e; cpu->cycles += 4; }
static void op_load_h_h(CPU* cpu) { cpu->h = cpu->h; cpu->cycles += 4; }
static void op_load_h_l(CPU* cpu) { cpu->h = cpu->l; cpu->cycles += 4; }
static  op_load_h_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->h = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_h_a(CPU* cpu) { cpu->h = cpu->a; cpu->cycles += 4; }

static void op_load_l_b(CPU* cpu) { cpu->l = cpu->b; cpu->cycles += 4; }
static void op_load_l_c(CPU* cpu) { cpu->l = cpu->c; cpu->cycles += 4; }
static void op_load_l_d(CPU* cpu) { cpu->l = cpu->d; cpu->cycles += 4; }
static void op_load_l_e(CPU* cpu) { cpu->l = cpu->e; cpu->cycles += 4; }
static void op_load_l_h(CPU* cpu) { cpu->l = cpu->h; cpu->cycles += 4; }
static void op_load_l_l(CPU* cpu) { cpu->l = cpu->l; cpu->cycles += 4; }
static void op_load_l_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->l = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_l_a(CPU* cpu) { cpu->l = cpu->a; cpu->cycles += 4; }

static void op_load_hlp_b(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->b);
    cpu->cycles += 8;
}

static void op_load_hlp_c(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->c);
    cpu->cycles += 8;
}

static void op_load_hlp_d(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->d);
    cpu->cycles += 8;
}

static void op_load_hlp_e(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->e);
    cpu->cycles += 8;
}

static void op_load_hlp_h(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->h);
    cpu->cycles += 8;
}

static void op_load_hlp_l(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->l);
    cpu->cycles += 8;
}

static void op_halt(CPU* cpu) {
    cpu->halted = 1;
    cpu->cycles += 4;
}

static void op_load_hlp_a(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    mem_write(hl, cpu->a);
    cpu->cycles += 8;
}

static void op_load_a_b(CPU* cpu) { cpu->a = cpu->b; cpu->cycles += 4; }
static void op_load_a_c(CPU* cpu) { cpu->a = cpu->c; cpu->cycles += 4; }
static void op_load_a_d(CPU* cpu) { cpu->a = cpu->d; cpu->cycles += 4; }
static void op_load_a_e(CPU* cpu) { cpu->a = cpu->e; cpu->cycles += 4; }
static void op_load_a_h(CPU* cpu) { cpu->a = cpu->h; cpu->cycles += 4; }
static void op_load_a_l(CPU* cpu) { cpu->a = cpu->l; cpu->cycles += 4; }
static void op_load_a_hlp(CPU* cpu) {
    uint16_t hl = (cpu->h << 8) | cpu->l;
    cpu->a = mem_read(hl);
    cpu->cycles += 8;
}
static void op_load_a_a(CPU* cpu) { cpu->a = cpu->a; cpu->cycles += 4; }

static void alu_add(CPU* cpu, uint8_t value) {
    uint16_t result = cpu->a + value;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (((cpu->a & 0x0F) + (value & 0x0F)) > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
}

static void op_add_a_b(CPU* cpu) { alu_add(cpu, cpu->b); cpu->cycles += 4; }
static void op_add_a_c(CPU* cpu) { alu_add(cpu, cpu->c); cpu->cycles += 4; }
static void op_add_a_d(CPU* cpu) { alu_add(cpu, cpu->d); cpu->cycles += 4; }
static void op_add_a_e(CPU* cpu) { alu_add(cpu, cpu->e); cpu->cycles += 4; }
static void op_add_a_h(CPU* cpu) { alu_add(cpu, cpu->h); cpu->cycles += 4; }
static void op_add_a_l(CPU* cpu) { alu_add(cpu, cpu->l); cpu->cycles += 4; }
static void op_add_a_hlp(CPU* cpu) {
    uint16_t hl = cpu->h << 8 | cpu->l;
    alu_add(cpu, mem_read(hl));
    cpu->cycles += 8;
}

static void alu_adc(CPU* cpu, uint8_t value) {
    uint8_t carry = cpu->f & FLAG_C ? 1 : 0;
    uint16_t result = cpu->a + value + carry;

    cpu->f = 0;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) + (value & 0x0F) + carry > 0x0F) cpu->f |= FLAG_H;
    if (result > 0xFF) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
}

static void op_adc_a_b(CPU* cpu) { alu_adc(cpu, cpu->b); cpu->cycles += 4; }
static void op_adc_a_c(CPU* cpu) { alu_adc(cpu, cpu->c); cpu->cycles += 4; }
static void op_adc_a_d(CPU* cpu) { alu_adc(cpu, cpu->d); cpu->cycles += 4; }
static void op_adc_a_e(CPU* cpu) { alu_adc(cpu, cpu->e); cpu->cycles += 4; }
static void op_adc_a_h(CPU* cpu) { alu_adc(cpu, cpu->h); cpu->cycles += 4; }
static void op_adc_a_l(CPU* cpu) { alu_adc(cpu, cpu->l); cpu->cycles += 4; }
static void op_adc_a_hlp(CPU* cpu) {
    uint16_t hl = cpu->h << 8 | cpu->l;
    alu_adc(cpu, mem_read(hl));
    cpu->cycles += 8;
}
static void op_adc_a_a(CPU* cpu) {alu_adc(cpu, cpu->a); cpu->cycles += 4; }




static void init_opcode_table() {
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
    REGISTER_OPCODE(0x46, op_load_b_hlp);
    REGISTER_OPCODE(0x47, op_load_b_a);
    REGISTER_OPCODE(0x48, op_load_c_b);
    REGISTER_OPCODE(0x49, op_load_c_c);
    REGISTER_OPCODE(0x4A, op_load_c_d);
    REGISTER_OPCODE(0x4B, op_load_c_e);
    REGISTER_OPCODE(0x4C, op_load_c_h);
    REGISTER_OPCODE(0x4D, op_load_c_l);
    REGISTER_OPCODE(0x4E, op_load_c_hlp);
    REGISTER_OPCODE(0x4F, op_load_c_a);
    REGISTER_OPCODE(0x50, op_load_d_b);
    REGISTER_OPCODE(0x51, op_load_d_c);
    REGISTER_OPCODE(0x52, op_load_d_d);
    REGISTER_OPCODE(0x53, op_load_d_e);
    REGISTER_OPCODE(0x54, op_load_d_h);
    REGISTER_OPCODE(0x55, op_load_d_l);
    REGISTER_OPCODE(0x56, op_load_d_hlp);
    REGISTER_OPCODE(0x57, op_load_d_a);
    REGISTER_OPCODE(0x58, op_load_e_b);
    REGISTER_OPCODE(0x59, op_load_e_c);
    REGISTER_OPCODE(0x5A, op_load_e_d);
    REGISTER_OPCODE(0x5B, op_load_e_e);
    REGISTER_OPCODE(0x5C, op_load_e_h);
    REGISTER_OPCODE(0x5D, op_load_e_l);
    REGISTER_OPCODE(0x5E, op_load_e_hlp);
    REGISTER_OPCODE(0x5F, op_load_e_a);
    REGISTER_OPCODE(0x60, op_load_h_b);
    REGISTER_OPCODE(0x61, op_load_h_c);
    REGISTER_OPCODE(0x62, op_load_h_d);
    REGISTER_OPCODE(0x63, op_load_h_e);
    REGISTER_OPCODE(0x64, op_load_h_h);
    REGISTER_OPCODE(0x65, op_load_h_l);
    REGISTER_OPCODE(0x66, op_load_h_hlp);
    REGISTER_OPCODE(0x67, op_load_h_a);
    REGISTER_OPCODE(0x68, op_load_l_b);
    REGISTER_OPCODE(0x69, op_load_l_c);
    REGISTER_OPCODE(0x6A, op_load_l_d);
    REGISTER_OPCODE(0x6B, op_load_l_e);
    REGISTER_OPCODE(0x6C, op_load_l_h);
    REGISTER_OPCODE(0x6D, op_load_l_l);
    REGISTER_OPCODE(0x6E, op_load_l_hlp);
    REGISTER_OPCODE(0x6F, op_load_l_a);
    REGISTER_OPCODE(0x70, op_load_hlp_b);
    REGISTER_OPCODE(0x71, op_load_hlp_c);
    REGISTER_OPCODE(0x72, op_load_hlp_d);
    REGISTER_OPCODE(0x73, op_load_hlp_e);
    REGISTER_OPCODE(0x74, op_load_hlp_h);
    REGISTER_OPCODE(0x75, op_load_hlp_l);
    REGISTER_OPCODE(0x76, op_halt);
    REGISTER_OPCODE(0x77, op_load_hlp_a);
    REGISTER_OPCODE(0x78, op_load_a_b);
    REGISTER_OPCODE(0x79, op_load_a_c);
    REGISTER_OPCODE(0x7A, op_load_a_d);
    REGISTER_OPCODE(0x7B, op_load_a_e);
    REGISTER_OPCODE(0x7C, op_load_a_h);
    REGISTER_OPCODE(0x7D, op_load_a_l);
    REGISTER_OPCODE(0x7E, op_load_a_hlp);
    REGISTER_OPCODE(0x7F, op_load_a_a);
    REGISTER_OPCODE(0x80, op_add_a_b);
    REGISTER_OPCODE(0x81, op_add_a_c);
    REGISTER_OPCODE(0x82, op_add_a_d);
    REGISTER_OPCODE(0x83, op_add_a_e);
    REGISTER_OPCODE(0x84, op_add_a_h);
    REGISTER_OPCODE(0x85, op_add_a_l);
    REGISTER_OPCODE(0x86, op_add_a_hlp);
    REGISTER_OPCODE(0x88, op_adc_a_b);
    REGISTER_OPCODE(0x89, op_adc_a_c);
    REGISTER_OPCODE(0x8A, op_adc_a_d);
    REGISTER_OPCODE(0x8B, op_adc_a_e);
    REGISTER_OPCODE(0x8C, op_adc_a_h);
    REGISTER_OPCODE(0x8D, op_adc_a_l);
    REGISTER_OPCODE(0x8E, op_adc_a_hlp);
    REGISTER_OPCODE(0x8F, op_adc_a_a);
}

void cpu_step(CPU* cpu) {
    if (cpu->halted) {
        cpu->cycles += 4; // maybe do nothing or wait for an interrupt
        return;
    }

    uint8_t opcode = mem_read(cpu->pc++);
    execute_opcode(cpu, opcode);
}

void execute_opcode(CPU* cpu, uint8_t opcode) {
    opcode_func_t handler = opcode_table[opcode];

    // NULL
    if (handler != 0) {
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
