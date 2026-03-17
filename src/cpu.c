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
static void alu_add(CPU* cpu, uint8_t value);
static void alu_adc(CPU* cpu, uint8_t value);
static void alu_sub(CPU* cpu, uint8_t value);
static void alu_sbc(CPU* cpu, uint8_t value);
static void alu_and(CPU* cpu, uint8_t value);
static void alu_xor(CPU* cpu, uint8_t value);
static void alu_or(CPU* cpu, uint8_t value);
static void alu_cp(CPU* cpu, uint8_t value);

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

static uint16_t cpu_get_hl(const CPU* cpu) {
    return cpu->h << 8 | cpu->l;
}

static void cpu_set_hl(CPU* cpu, uint16_t value) {
    cpu->h = (value >> 8) & 0xFF;
    cpu->l = value & 0xFF;
}

static uint16_t cpu_get_bc(const CPU* cpu) {
    return (cpu->b << 8) | cpu->c;
}

static void cpu_set_bc(CPU* cpu, uint16_t value) {
    cpu->b = (value >> 8) & 0xFF;
    cpu->c = value & 0xFF;
}

static uint16_t cpu_get_de(const CPU* cpu) {
    return (cpu->d << 8) | cpu->e;
}

static void cpu_set_de(CPU* cpu, uint16_t value) {
    cpu->d = (value >> 8) & 0xFF;
    cpu->e = value & 0xFF;
}

static uint16_t read_u16(CPU* cpu) {
    uint16_t value = mem_read(cpu->pc) | (mem_read(cpu->pc + 1) << 8);
    cpu->pc += 2;
    return value;
}

static void push_u16(CPU* cpu, uint16_t value) {
    cpu->sp -= 2;
    mem_write(cpu->sp, value & 0xFF);
    mem_write(cpu->sp + 1, (value >> 8) & 0xFF);
}

static uint16_t pop_u16(CPU* cpu) {
    uint16_t value = mem_read(cpu->sp) | (mem_read(cpu->sp + 1) << 8);
    cpu->sp += 2;
    return value;
}

static void op_stop(CPU* cpu) {
    cpu->pc++; // STOP is encoded as 0x10 0x00
    cpu->halted = 1;
    cpu->cycles += 4;
}

static void op_ld_bc_d16(CPU* cpu) { cpu_set_bc(cpu, read_u16(cpu)); cpu->cycles += 12; }
static void op_ld_de_d16(CPU* cpu) { cpu_set_de(cpu, read_u16(cpu)); cpu->cycles += 12; }
static void op_ld_hl_d16(CPU* cpu) { cpu_set_hl(cpu, read_u16(cpu)); cpu->cycles += 12; }
static void op_ld_sp_d16(CPU* cpu) { cpu->sp = read_u16(cpu); cpu->cycles += 12; }

static void op_ld_bcp_a(CPU* cpu) { mem_write(cpu_get_bc(cpu), cpu->a); cpu->cycles += 8; }
static void op_ld_dep_a(CPU* cpu) { mem_write(cpu_get_de(cpu), cpu->a); cpu->cycles += 8; }
static void op_ld_a_bcp(CPU* cpu) { cpu->a = mem_read(cpu_get_bc(cpu)); cpu->cycles += 8; }
static void op_ld_a_dep(CPU* cpu) { cpu->a = mem_read(cpu_get_de(cpu)); cpu->cycles += 8; }

static void op_ld_hli_a(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    mem_write(hl, cpu->a);
    cpu_set_hl(cpu, hl + 1);
    cpu->cycles += 8;
}

static void op_ld_hld_a(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    mem_write(hl, cpu->a);
    cpu_set_hl(cpu, hl - 1);
    cpu->cycles += 8;
}

static void op_ld_a_hli(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    cpu->a = mem_read(hl);
    cpu_set_hl(cpu, hl + 1);
    cpu->cycles += 8;
}

static void op_ld_a_hld(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    cpu->a = mem_read(hl);
    cpu_set_hl(cpu, hl - 1);
    cpu->cycles += 8;
}

static void op_ld_a16_sp(CPU* cpu) {
    uint16_t addr = read_u16(cpu);
    mem_write(addr, cpu->sp & 0xFF);
    mem_write(addr + 1, (cpu->sp >> 8) & 0xFF);
    cpu->cycles += 20;
}

static void op_inc_bc(CPU* cpu) { cpu_set_bc(cpu, cpu_get_bc(cpu) + 1); cpu->cycles += 8; }
static void op_inc_de(CPU* cpu) { cpu_set_de(cpu, cpu_get_de(cpu) + 1); cpu->cycles += 8; }
static void op_inc_hl(CPU* cpu) { cpu_set_hl(cpu, cpu_get_hl(cpu) + 1); cpu->cycles += 8; }
static void op_inc_sp(CPU* cpu) { cpu->sp++; cpu->cycles += 8; }

static void op_dec_bc(CPU* cpu) { cpu_set_bc(cpu, cpu_get_bc(cpu) - 1); cpu->cycles += 8; }
static void op_dec_de(CPU* cpu) { cpu_set_de(cpu, cpu_get_de(cpu) - 1); cpu->cycles += 8; }
static void op_dec_hl(CPU* cpu) { cpu_set_hl(cpu, cpu_get_hl(cpu) - 1); cpu->cycles += 8; }
static void op_dec_sp(CPU* cpu) { cpu->sp--; cpu->cycles += 8; }

static void op_inc_r(CPU* cpu, uint8_t* reg) {
    uint8_t old = *reg;
    uint8_t result = old + 1;
    uint8_t carry = cpu->f & FLAG_C;

    cpu->f = carry;
    if (result == 0) cpu->f |= FLAG_Z;
    if ((old & 0x0F) + 1 > 0x0F) cpu->f |= FLAG_H;
    *reg = result;
}

static void op_dec_r(CPU* cpu, uint8_t* reg) {
    uint8_t old = *reg;
    uint8_t result = old - 1;
    uint8_t carry = cpu->f & FLAG_C;

    cpu->f = carry | FLAG_N;
    if (result == 0) cpu->f |= FLAG_Z;
    if ((old & 0x0F) == 0) cpu->f |= FLAG_H;
    *reg = result;
}

static void op_inc_b(CPU* cpu) { op_inc_r(cpu, &cpu->b); cpu->cycles += 4; }
static void op_inc_c(CPU* cpu) { op_inc_r(cpu, &cpu->c); cpu->cycles += 4; }
static void op_inc_d(CPU* cpu) { op_inc_r(cpu, &cpu->d); cpu->cycles += 4; }
static void op_inc_e(CPU* cpu) { op_inc_r(cpu, &cpu->e); cpu->cycles += 4; }
static void op_inc_h(CPU* cpu) { op_inc_r(cpu, &cpu->h); cpu->cycles += 4; }
static void op_inc_l(CPU* cpu) { op_inc_r(cpu, &cpu->l); cpu->cycles += 4; }
static void op_inc_a(CPU* cpu) { op_inc_r(cpu, &cpu->a); cpu->cycles += 4; }

static void op_dec_b(CPU* cpu) { op_dec_r(cpu, &cpu->b); cpu->cycles += 4; }
static void op_dec_c(CPU* cpu) { op_dec_r(cpu, &cpu->c); cpu->cycles += 4; }
static void op_dec_d(CPU* cpu) { op_dec_r(cpu, &cpu->d); cpu->cycles += 4; }
static void op_dec_e(CPU* cpu) { op_dec_r(cpu, &cpu->e); cpu->cycles += 4; }
static void op_dec_h(CPU* cpu) { op_dec_r(cpu, &cpu->h); cpu->cycles += 4; }
static void op_dec_l(CPU* cpu) { op_dec_r(cpu, &cpu->l); cpu->cycles += 4; }
static void op_dec_a(CPU* cpu) { op_dec_r(cpu, &cpu->a); cpu->cycles += 4; }

static void op_inc_hlp(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    uint8_t value = mem_read(hl);
    op_inc_r(cpu, &value);
    mem_write(hl, value);
    cpu->cycles += 12;
}

static void op_dec_hlp(CPU* cpu) {
    uint16_t hl = cpu_get_hl(cpu);
    uint8_t value = mem_read(hl);
    op_dec_r(cpu, &value);
    mem_write(hl, value);
    cpu->cycles += 12;
}

static void op_add_hl(CPU* cpu, uint16_t value) {
    uint16_t hl = cpu_get_hl(cpu);
    uint32_t result = hl + value;
    uint8_t z = cpu->f & FLAG_Z;

    cpu->f = z;
    if (((hl & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF) cpu->f |= FLAG_H;
    if (result > 0xFFFF) cpu->f |= FLAG_C;

    cpu_set_hl(cpu, result & 0xFFFF);
}

static void op_add_hl_bc(CPU* cpu) { op_add_hl(cpu, cpu_get_bc(cpu)); cpu->cycles += 8; }
static void op_add_hl_de(CPU* cpu) { op_add_hl(cpu, cpu_get_de(cpu)); cpu->cycles += 8; }
static void op_add_hl_hl(CPU* cpu) { op_add_hl(cpu, cpu_get_hl(cpu)); cpu->cycles += 8; }
static void op_add_hl_sp(CPU* cpu) { op_add_hl(cpu, cpu->sp); cpu->cycles += 8; }

static void op_rlca(CPU* cpu) {
    uint8_t carry = (cpu->a >> 7) & 1;
    cpu->a = (cpu->a << 1) | carry;
    cpu->f = carry ? FLAG_C : 0;
    cpu->cycles += 4;
}

static void op_rrca(CPU* cpu) {
    uint8_t carry = cpu->a & 1;
    cpu->a = (cpu->a >> 1) | (carry << 7);
    cpu->f = carry ? FLAG_C : 0;
    cpu->cycles += 4;
}

static void op_rla(CPU* cpu) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint8_t carry = (cpu->a >> 7) & 1;
    cpu->a = (cpu->a << 1) | old_carry;
    cpu->f = carry ? FLAG_C : 0;
    cpu->cycles += 4;
}

static void op_rra(CPU* cpu) {
    uint8_t old_carry = (cpu->f & FLAG_C) ? 1 : 0;
    uint8_t carry = cpu->a & 1;
    cpu->a = (cpu->a >> 1) | (old_carry << 7);
    cpu->f = carry ? FLAG_C : 0;
    cpu->cycles += 4;
}

static void op_daa(CPU* cpu) {
    uint8_t correction = 0;
    uint8_t carry = cpu->f & FLAG_C;

    if (!(cpu->f & FLAG_N)) {
        if ((cpu->f & FLAG_H) || (cpu->a & 0x0F) > 0x09) correction |= 0x06;
        if (carry || cpu->a > 0x99) {
            correction |= 0x60;
            carry = FLAG_C;
        }
        cpu->a += correction;
    } else {
        if (cpu->f & FLAG_H) correction |= 0x06;
        if (carry) correction |= 0x60;
        cpu->a -= correction;
    }

    cpu->f &= FLAG_N;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
    cpu->f |= carry;
    cpu->cycles += 4;
}

static void op_cpl(CPU* cpu) {
    cpu->a = ~cpu->a;
    cpu->f = (cpu->f & (FLAG_Z | FLAG_C)) | FLAG_N | FLAG_H;
    cpu->cycles += 4;
}

static void op_scf(CPU* cpu) {
    cpu->f = (cpu->f & FLAG_Z) | FLAG_C;
    cpu->cycles += 4;
}

static void op_ccf(CPU* cpu) {
    uint8_t c = (cpu->f & FLAG_C) ? 0 : FLAG_C;
    cpu->f = (cpu->f & FLAG_Z) | c;
    cpu->cycles += 4;
}

static void op_jr_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    cpu->pc += offset;
    cpu->cycles += 12;
}

static void op_jr_z_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    if (cpu->f & FLAG_Z) {
        cpu->pc += offset;
        cpu->cycles += 12;
    } else {
        cpu->cycles += 8;
    }
}

static void op_jr_nc_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    if (!(cpu->f & FLAG_C)) {
        cpu->pc += offset;
        cpu->cycles += 12;
    } else {
        cpu->cycles += 8;
    }
}

static void op_jr_c_r8(CPU* cpu) {
    int8_t offset = (int8_t)mem_read(cpu->pc++);
    if (cpu->f & FLAG_C) {
        cpu->pc += offset;
        cpu->cycles += 12;
    } else {
        cpu->cycles += 8;
    }
}

static void op_jp_cc(CPU* cpu, bool condition) {
    uint16_t address = read_u16(cpu);
    if (condition) {
        cpu->pc = address;
        cpu->cycles += 16;
    } else {
        cpu->cycles += 12;
    }
}

static void op_jp_nz_a16(CPU* cpu) { op_jp_cc(cpu, !(cpu->f & FLAG_Z)); }
static void op_jp_z_a16(CPU* cpu) { op_jp_cc(cpu, cpu->f & FLAG_Z); }
static void op_jp_nc_a16(CPU* cpu) { op_jp_cc(cpu, !(cpu->f & FLAG_C)); }
static void op_jp_c_a16(CPU* cpu) { op_jp_cc(cpu, cpu->f & FLAG_C); }

static void op_ret_cc(CPU* cpu, bool condition) {
    if (condition) {
        cpu->pc = pop_u16(cpu);
        cpu->cycles += 20;
    } else {
        cpu->cycles += 8;
    }
}

static void op_ret_nz(CPU* cpu) { op_ret_cc(cpu, !(cpu->f & FLAG_Z)); }
static void op_ret_z(CPU* cpu) { op_ret_cc(cpu, cpu->f & FLAG_Z); }
static void op_ret_nc(CPU* cpu) { op_ret_cc(cpu, !(cpu->f & FLAG_C)); }
static void op_ret_c(CPU* cpu) { op_ret_cc(cpu, cpu->f & FLAG_C); }

static void op_call_cc(CPU* cpu, bool condition) {
    uint16_t address = read_u16(cpu);
    if (condition) {
        push_u16(cpu, cpu->pc);
        cpu->pc = address;
        cpu->cycles += 24;
    } else {
        cpu->cycles += 12;
    }
}

static void op_call_nz_a16(CPU* cpu) { op_call_cc(cpu, !(cpu->f & FLAG_Z)); }
static void op_call_z_a16(CPU* cpu) { op_call_cc(cpu, cpu->f & FLAG_Z); }
static void op_call_nc_a16(CPU* cpu) { op_call_cc(cpu, !(cpu->f & FLAG_C)); }
static void op_call_c_a16(CPU* cpu) { op_call_cc(cpu, cpu->f & FLAG_C); }

static void op_pop_bc(CPU* cpu) { cpu_set_bc(cpu, pop_u16(cpu)); cpu->cycles += 12; }
static void op_pop_de(CPU* cpu) { cpu_set_de(cpu, pop_u16(cpu)); cpu->cycles += 12; }
static void op_pop_hl(CPU* cpu) { cpu_set_hl(cpu, pop_u16(cpu)); cpu->cycles += 12; }
static void op_pop_af(CPU* cpu) {
    uint16_t af = pop_u16(cpu);
    cpu->a = (af >> 8) & 0xFF;
    cpu->f = af & 0xF0;
    cpu->cycles += 12;
}

static void op_push_bc(CPU* cpu) { push_u16(cpu, cpu_get_bc(cpu)); cpu->cycles += 16; }
static void op_push_de(CPU* cpu) { push_u16(cpu, cpu_get_de(cpu)); cpu->cycles += 16; }
static void op_push_hl(CPU* cpu) { push_u16(cpu, cpu_get_hl(cpu)); cpu->cycles += 16; }
static void op_push_af(CPU* cpu) {
    push_u16(cpu, (cpu->a << 8) | (cpu->f & 0xF0));
    cpu->cycles += 16;
}

static void op_rst(CPU* cpu, uint16_t vector) {
    push_u16(cpu, cpu->pc);
    cpu->pc = vector;
    cpu->cycles += 16;
}

static void op_rst_00(CPU* cpu) { op_rst(cpu, 0x00); }
static void op_rst_08(CPU* cpu) { op_rst(cpu, 0x08); }
static void op_rst_10(CPU* cpu) { op_rst(cpu, 0x10); }
static void op_rst_18(CPU* cpu) { op_rst(cpu, 0x18); }
static void op_rst_20(CPU* cpu) { op_rst(cpu, 0x20); }
static void op_rst_28(CPU* cpu) { op_rst(cpu, 0x28); }
static void op_rst_30(CPU* cpu) { op_rst(cpu, 0x30); }
static void op_rst_38(CPU* cpu) { op_rst(cpu, 0x38); }

static void op_reti(CPU* cpu) {
    cpu->pc = pop_u16(cpu);
    cpu->ime = 1;
    cpu->cycles += 16;
}

static void op_ldh_a8_a(CPU* cpu) {
    uint16_t address = 0xFF00 | mem_read(cpu->pc++);
    mem_write(address, cpu->a);
    cpu->cycles += 12;
}

static void op_ldh_c_a(CPU* cpu) {
    mem_write(0xFF00 | cpu->c, cpu->a);
    cpu->cycles += 8;
}

static void op_ld_a16_a(CPU* cpu) {
    uint16_t address = read_u16(cpu);
    mem_write(address, cpu->a);
    cpu->cycles += 16;
}

static void op_ldh_a_a8(CPU* cpu) {
    uint16_t address = 0xFF00 | mem_read(cpu->pc++);
    cpu->a = mem_read(address);
    cpu->cycles += 12;
}

static void op_ldh_a_c(CPU* cpu) {
    cpu->a = mem_read(0xFF00 | cpu->c);
    cpu->cycles += 8;
}

static void op_ld_a_a16(CPU* cpu) {
    uint16_t address = read_u16(cpu);
    cpu->a = mem_read(address);
    cpu->cycles += 16;
}

static void op_di(CPU* cpu) {
    cpu->ime = 0;
    cpu->cycles += 4;
}

static void op_ei(CPU* cpu) {
    cpu->ime = 1;
    cpu->cycles += 4;
}

static void op_add_sp_r8(CPU* cpu) {
    int8_t value = (int8_t)mem_read(cpu->pc++);
    uint16_t sp = cpu->sp;
    uint16_t result = sp + value;

    cpu->f = 0;
    if (((sp & 0x0F) + (value & 0x0F)) > 0x0F) cpu->f |= FLAG_H;
    if (((sp & 0xFF) + (value & 0xFF)) > 0xFF) cpu->f |= FLAG_C;

    cpu->sp = result;
    cpu->cycles += 16;
}

static void op_ld_hl_sp_r8(CPU* cpu) {
    int8_t value = (int8_t)mem_read(cpu->pc++);
    uint16_t sp = cpu->sp;
    uint16_t result = sp + value;

    cpu->f = 0;
    if (((sp & 0x0F) + (value & 0x0F)) > 0x0F) cpu->f |= FLAG_H;
    if (((sp & 0xFF) + (value & 0xFF)) > 0xFF) cpu->f |= FLAG_C;

    cpu_set_hl(cpu, result);
    cpu->cycles += 12;
}

static void op_ld_sp_hl(CPU* cpu) {
    cpu->sp = cpu_get_hl(cpu);
    cpu->cycles += 8;
}

static void op_jp_hl(CPU* cpu) {
    cpu->pc = cpu_get_hl(cpu);
    cpu->cycles += 4;
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
    uint16_t address = read_u16(cpu);
    push_u16(cpu, cpu->pc);
    cpu->pc = address; // Jump to address
    cpu->cycles += 24;
}

static void op_ret(CPU* cpu) {
    cpu->pc = pop_u16(cpu); // Load PC from stack
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

static void op_adc_a_d8(CPU* cpu) {
    alu_adc(cpu, mem_read(cpu->pc++));
    cpu->cycles += 8;
}

static void op_sub_d8(CPU* cpu) {
    alu_sub(cpu, mem_read(cpu->pc++));
    cpu->cycles += 8;
}

static void op_sbc_a_d8(CPU* cpu) {
    alu_sbc(cpu, mem_read(cpu->pc++));
    cpu->cycles += 8;
}

static void op_and_d8(CPU* cpu) {
    alu_and(cpu, mem_read(cpu->pc++));
    cpu->cycles += 8;
}

static void op_xor_d8(CPU* cpu) {
    alu_xor(cpu, mem_read(cpu->pc++));
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
    cpu->b = mem_read(cpu_get_hl(cpu));
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
    cpu->c = mem_read(cpu_get_hl(cpu));
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
    cpu->d = mem_read(cpu_get_hl(cpu));
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
    cpu->e = mem_read(cpu_get_hl(cpu));
    cpu->cycles += 8;
}
static void op_load_e_a(CPU* cpu) { cpu->e = cpu->a; cpu->cycles += 4; }

static void op_load_h_b(CPU* cpu) { cpu->h = cpu->b; cpu->cycles += 4; }
static void op_load_h_c(CPU* cpu) { cpu->h = cpu->c; cpu->cycles += 4; }
static void op_load_h_d(CPU* cpu) { cpu->h = cpu->d; cpu->cycles += 4; }
static void op_load_h_e(CPU* cpu) { cpu->h = cpu->e; cpu->cycles += 4; }
static void op_load_h_h(CPU* cpu) { cpu->h = cpu->h; cpu->cycles += 4; }
static void op_load_h_l(CPU* cpu) { cpu->h = cpu->l; cpu->cycles += 4; }
static void op_load_h_hlp(CPU* cpu) {
    cpu->h = mem_read(cpu_get_hl(cpu));
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
    cpu->l = mem_read(cpu_get_hl(cpu));
    cpu->cycles += 8;
}
static void op_load_l_a(CPU* cpu) { cpu->l = cpu->a; cpu->cycles += 4; }

static void op_load_hlp_b(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->b);
    cpu->cycles += 8;
}

static void op_load_hlp_c(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->c);
    cpu->cycles += 8;
}

static void op_load_hlp_d(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->d);
    cpu->cycles += 8;
}

static void op_load_hlp_e(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->e);
    cpu->cycles += 8;
}

static void op_load_hlp_h(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->h);
    cpu->cycles += 8;
}

static void op_load_hlp_l(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->l);
    cpu->cycles += 8;
}

static void op_load_hlp_d8(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), mem_read(cpu->pc++));
    cpu->cycles += 12;
}

static void op_halt(CPU* cpu) {
    cpu->halted = 1;
    cpu->cycles += 4;
}

static void op_load_hlp_a(CPU* cpu) {
    mem_write(cpu_get_hl(cpu), cpu->a);
    cpu->cycles += 8;
}

static void op_load_a_b(CPU* cpu) { cpu->a = cpu->b; cpu->cycles += 4; }
static void op_load_a_c(CPU* cpu) { cpu->a = cpu->c; cpu->cycles += 4; }
static void op_load_a_d(CPU* cpu) { cpu->a = cpu->d; cpu->cycles += 4; }
static void op_load_a_e(CPU* cpu) { cpu->a = cpu->e; cpu->cycles += 4; }
static void op_load_a_h(CPU* cpu) { cpu->a = cpu->h; cpu->cycles += 4; }
static void op_load_a_l(CPU* cpu) { cpu->a = cpu->l; cpu->cycles += 4; }
static void op_load_a_hlp(CPU* cpu) {
    cpu->a = mem_read(cpu_get_hl(cpu));
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
    alu_add(cpu, mem_read(cpu_get_hl(cpu)));
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
    alu_adc(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_adc_a_a(CPU* cpu) {alu_adc(cpu, cpu->a); cpu->cycles += 4; }

static void alu_sub(CPU* cpu, uint8_t value) {
    uint16_t result = cpu->a - value;

    cpu->f = FLAG_N;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) < (value & 0x0F)) cpu->f |= FLAG_H;
    if (cpu->a < value) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
}

static void op_sub_b(CPU* cpu) { alu_sub(cpu, cpu->b); cpu->cycles += 4; }
static void op_sub_c(CPU* cpu) { alu_sub(cpu, cpu->c); cpu->cycles += 4; }
static void op_sub_d(CPU* cpu) { alu_sub(cpu, cpu->d); cpu->cycles += 4; }
static void op_sub_e(CPU* cpu) { alu_sub(cpu, cpu->e); cpu->cycles += 4; }
static void op_sub_h(CPU* cpu) { alu_sub(cpu, cpu->h); cpu->cycles += 4; }
static void op_sub_l(CPU* cpu) { alu_sub(cpu, cpu->l); cpu->cycles += 4; }
static void op_sub_hlp(CPU* cpu) {
    alu_sub(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_sub_a(CPU* cpu) { alu_sub(cpu, cpu->a); cpu->cycles += 4; }

static void alu_sbc(CPU* cpu, uint8_t value) {
    uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
    int result = cpu->a - value - carry;

    cpu->f = FLAG_N;
    if ((result & 0xFF) == 0) cpu->f |= FLAG_Z;
    if (((cpu->a & 0x0F) - (value & 0x0F) - carry) < 0) cpu->f |= FLAG_H;
    if (result < 0) cpu->f |= FLAG_C;

    cpu->a = result & 0xFF;
}

static void op_sbc_a_b(CPU* cpu) { alu_sbc(cpu, cpu->b); cpu->cycles += 4; }
static void op_sbc_a_c(CPU* cpu) { alu_sbc(cpu, cpu->c); cpu->cycles += 4; }
static void op_sbc_a_d(CPU* cpu) { alu_sbc(cpu, cpu->d); cpu->cycles += 4; }
static void op_sbc_a_e(CPU* cpu) { alu_sbc(cpu, cpu->e); cpu->cycles += 4; }
static void op_sbc_a_h(CPU* cpu) { alu_sbc(cpu, cpu->h); cpu->cycles += 4; }
static void op_sbc_a_l(CPU* cpu) { alu_sbc(cpu, cpu->l); cpu->cycles += 4; }
static void op_sbc_a_hlp(CPU* cpu) {
    alu_sbc(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_sbc_a_a(CPU* cpu) { alu_sbc(cpu, cpu->a); cpu->cycles += 4; }

static void alu_and(CPU* cpu, uint8_t value) {
    cpu->a &= value;

    cpu->f = 0;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
    cpu->f |= FLAG_H;
}

static void op_and_b(CPU* cpu) { alu_and(cpu, cpu->b); cpu->cycles += 4; }
static void op_and_c(CPU* cpu) {alu_and(cpu, cpu->c); cpu->cycles += 4; }
static void op_and_d(CPU* cpu) { alu_and(cpu, cpu->d); cpu->cycles += 4; }
static void op_and_e(CPU* cpu) { alu_and(cpu, cpu->e); cpu->cycles += 4; }
static void op_and_h(CPU* cpu) { alu_and(cpu, cpu->h); cpu->cycles += 4; }
static void op_and_l(CPU* cpu) { alu_and(cpu, cpu->l); cpu->cycles += 4; }
static void op_and_hlp(CPU* cpu) {
    alu_and(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_and_a(CPU* cpu) { alu_and(cpu, cpu->a); cpu->cycles += 4; }

static void alu_or(CPU* cpu, uint8_t value) {
    cpu->a |= value;

    cpu->f = 0;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
}

static void op_or_b(CPU* cpu) { alu_or(cpu, cpu->b); cpu->cycles += 4; }
static void op_or_c(CPU* cpu) { alu_or(cpu, cpu->c); cpu->cycles += 4; }
static void op_or_d(CPU* cpu) { alu_or(cpu, cpu->d); cpu->cycles += 4; }
static void op_or_e(CPU* cpu) { alu_or(cpu, cpu->e); cpu->cycles += 4; }
static void op_or_h(CPU* cpu) { alu_or(cpu, cpu->h); cpu->cycles += 4; }
static void op_or_l(CPU* cpu) { alu_or(cpu, cpu->l); cpu->cycles += 4; }
static void op_or_hlp(CPU* cpu) {
    alu_or(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_or_a(CPU* cpu) { alu_or(cpu, cpu->a); cpu->cycles += 4; }

static void op_or_d8(CPU* cpu) {
    alu_or(cpu, mem_read(cpu->pc++));
    cpu->cycles += 8;
}

static void alu_xor(CPU* cpu, uint8_t value) {
    cpu->a ^= value;

    cpu->f = 0;
    if (cpu->a == 0) cpu->f |= FLAG_Z;
}

static void op_xor_b(CPU* cpu) { alu_xor(cpu, cpu->b); cpu->cycles += 4; }
static void op_xor_c(CPU* cpu) { alu_xor(cpu, cpu->c); cpu->cycles += 4; }
static void op_xor_d(CPU* cpu) { alu_xor(cpu, cpu->d); cpu->cycles += 4; }
static void op_xor_e(CPU* cpu) { alu_xor(cpu, cpu->e); cpu->cycles += 4; }
static void op_xor_h(CPU* cpu) { alu_xor(cpu, cpu->h); cpu->cycles += 4; }
static void op_xor_l(CPU* cpu) { alu_xor(cpu, cpu->l); cpu->cycles += 4; }
static void op_xor_hlp(CPU* cpu) {
    alu_xor(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
// xor_a already implemented

static void alu_cp(CPU* cpu, uint8_t value) {
    uint8_t result = cpu->a - value;

    cpu->f = FLAG_N;
    if (result == 0) cpu->f |= FLAG_Z;
    if ((cpu->a & 0x0F) < (value & 0x0F)) cpu->f |= FLAG_H;
    if (cpu->a < value) cpu->f |= FLAG_C;
}

static void op_cp_b(CPU* cpu) { alu_cp(cpu, cpu->b); cpu->cycles += 4; }
static void op_cp_c(CPU* cpu) { alu_cp(cpu, cpu->c); cpu->cycles += 4; }
static void op_cp_d(CPU* cpu) { alu_cp(cpu, cpu->d); cpu->cycles += 4; }
static void op_cp_e(CPU* cpu) { alu_cp(cpu, cpu->e); cpu->cycles += 4; }
static void op_cp_h(CPU* cpu) { alu_cp(cpu, cpu->h); cpu->cycles += 4; }
static void op_cp_l(CPU* cpu) { alu_cp(cpu, cpu->l); cpu->cycles += 4; }
static void op_cp_hlp(CPU* cpu) {
    alu_cp(cpu, mem_read(cpu_get_hl(cpu)));
    cpu->cycles += 8;
}
static void op_cp_a(CPU* cpu) { alu_cp(cpu, cpu->a); cpu->cycles += 4; }


static void init_opcode_table() {
    memset(opcode_table, 0, sizeof(opcode_table));

    REGISTER_OPCODE(0x00, op_nop);
    REGISTER_OPCODE(0x01, op_ld_bc_d16);
    REGISTER_OPCODE(0x02, op_ld_bcp_a);
    REGISTER_OPCODE(0x03, op_inc_bc);
    REGISTER_OPCODE(0x04, op_inc_b);
    REGISTER_OPCODE(0x05, op_dec_b);
    REGISTER_OPCODE(0x3E, op_ld_a_d8);
    REGISTER_OPCODE(0x06, op_ld_b_d8);
    REGISTER_OPCODE(0x07, op_rlca);
    REGISTER_OPCODE(0x08, op_ld_a16_sp);
    REGISTER_OPCODE(0x09, op_add_hl_bc);
    REGISTER_OPCODE(0x0A, op_ld_a_bcp);
    REGISTER_OPCODE(0x0B, op_dec_bc);
    REGISTER_OPCODE(0x0C, op_inc_c);
    REGISTER_OPCODE(0x0D, op_dec_c);
    REGISTER_OPCODE(0x0E, op_ld_c_d8);
    REGISTER_OPCODE(0x0F, op_rrca);
    REGISTER_OPCODE(0x10, op_stop);
    REGISTER_OPCODE(0x11, op_ld_de_d16);
    REGISTER_OPCODE(0x12, op_ld_dep_a);
    REGISTER_OPCODE(0x13, op_inc_de);
    REGISTER_OPCODE(0x14, op_inc_d);
    REGISTER_OPCODE(0x15, op_dec_d);
    REGISTER_OPCODE(0x16, op_ld_d_d8);
    REGISTER_OPCODE(0x17, op_rla);
    REGISTER_OPCODE(0x18, op_jr_r8);
    REGISTER_OPCODE(0x19, op_add_hl_de);
    REGISTER_OPCODE(0x1A, op_ld_a_dep);
    REGISTER_OPCODE(0x1B, op_dec_de);
    REGISTER_OPCODE(0x1C, op_inc_e);
    REGISTER_OPCODE(0x1D, op_dec_e);
    REGISTER_OPCODE(0x1E, op_ld_e_d8);
    REGISTER_OPCODE(0x1F, op_rra);
    REGISTER_OPCODE(0x26, op_ld_h_d8);
    REGISTER_OPCODE(0x20, op_jr_nz_r8);
    REGISTER_OPCODE(0x21, op_ld_hl_d16);
    REGISTER_OPCODE(0x22, op_ld_hli_a);
    REGISTER_OPCODE(0x23, op_inc_hl);
    REGISTER_OPCODE(0x24, op_inc_h);
    REGISTER_OPCODE(0x25, op_dec_h);
    REGISTER_OPCODE(0x27, op_daa);
    REGISTER_OPCODE(0x28, op_jr_z_r8);
    REGISTER_OPCODE(0x29, op_add_hl_hl);
    REGISTER_OPCODE(0x2A, op_ld_a_hli);
    REGISTER_OPCODE(0x2B, op_dec_hl);
    REGISTER_OPCODE(0x2C, op_inc_l);
    REGISTER_OPCODE(0x2D, op_dec_l);
    REGISTER_OPCODE(0x2E, op_ld_l_d8);
    REGISTER_OPCODE(0x2F, op_cpl);
    REGISTER_OPCODE(0x30, op_jr_nc_r8);
    REGISTER_OPCODE(0x31, op_ld_sp_d16);
    REGISTER_OPCODE(0x32, op_ld_hld_a);
    REGISTER_OPCODE(0x33, op_inc_sp);
    REGISTER_OPCODE(0x34, op_inc_hlp);
    REGISTER_OPCODE(0x35, op_dec_hlp);
    REGISTER_OPCODE(0x36, op_load_hlp_d8);
    REGISTER_OPCODE(0x37, op_scf);
    REGISTER_OPCODE(0x38, op_jr_c_r8);
    REGISTER_OPCODE(0x39, op_add_hl_sp);
    REGISTER_OPCODE(0x3A, op_ld_a_hld);
    REGISTER_OPCODE(0x3B, op_dec_sp);
    REGISTER_OPCODE(0x3C, op_inc_a);
    REGISTER_OPCODE(0x3D, op_dec_a);
    REGISTER_OPCODE(0xC3, op_jp_a16);
    REGISTER_OPCODE(0xC2, op_jp_nz_a16);
    REGISTER_OPCODE(0xCA, op_jp_z_a16);
    REGISTER_OPCODE(0xD2, op_jp_nc_a16);
    REGISTER_OPCODE(0xDA, op_jp_c_a16);
    REGISTER_OPCODE(0xCD, op_call_a16);
    REGISTER_OPCODE(0xC4, op_call_nz_a16);
    REGISTER_OPCODE(0xCC, op_call_z_a16);
    REGISTER_OPCODE(0xD4, op_call_nc_a16);
    REGISTER_OPCODE(0xDC, op_call_c_a16);
    REGISTER_OPCODE(0xC9, op_ret);
    REGISTER_OPCODE(0xC0, op_ret_nz);
    REGISTER_OPCODE(0xC8, op_ret_z);
    REGISTER_OPCODE(0xD0, op_ret_nc);
    REGISTER_OPCODE(0xD8, op_ret_c);
    REGISTER_OPCODE(0xD9, op_reti);
    REGISTER_OPCODE(0xC1, op_pop_bc);
    REGISTER_OPCODE(0xD1, op_pop_de);
    REGISTER_OPCODE(0xE1, op_pop_hl);
    REGISTER_OPCODE(0xF1, op_pop_af);
    REGISTER_OPCODE(0xC5, op_push_bc);
    REGISTER_OPCODE(0xD5, op_push_de);
    REGISTER_OPCODE(0xE5, op_push_hl);
    REGISTER_OPCODE(0xF5, op_push_af);
    REGISTER_OPCODE(0xC7, op_rst_00);
    REGISTER_OPCODE(0xCF, op_rst_08);
    REGISTER_OPCODE(0xD7, op_rst_10);
    REGISTER_OPCODE(0xDF, op_rst_18);
    REGISTER_OPCODE(0xE7, op_rst_20);
    REGISTER_OPCODE(0xEF, op_rst_28);
    REGISTER_OPCODE(0xF7, op_rst_30);
    REGISTER_OPCODE(0xFF, op_rst_38);
    REGISTER_OPCODE(0xFE, op_cp_d8);
    REGISTER_OPCODE(0xC6, op_add_a_d8);
    REGISTER_OPCODE(0x87, op_add_a_a);
    REGISTER_OPCODE(0x3F, op_ccf);
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
    REGISTER_OPCODE(0xCE, op_adc_a_d8);
    REGISTER_OPCODE(0x90, op_sub_b);
    REGISTER_OPCODE(0x91, op_sub_c);
    REGISTER_OPCODE(0x92, op_sub_d);
    REGISTER_OPCODE(0x93, op_sub_e);
    REGISTER_OPCODE(0x94, op_sub_h);
    REGISTER_OPCODE(0x95, op_sub_l);
    REGISTER_OPCODE(0x96, op_sub_hlp);
    REGISTER_OPCODE(0x97, op_sub_a);
    REGISTER_OPCODE(0xD6, op_sub_d8);
    REGISTER_OPCODE(0x98, op_sbc_a_b);
    REGISTER_OPCODE(0x99, op_sbc_a_c);
    REGISTER_OPCODE(0x9A, op_sbc_a_d);
    REGISTER_OPCODE(0x9B, op_sbc_a_e);
    REGISTER_OPCODE(0x9C, op_sbc_a_h);
    REGISTER_OPCODE(0x9D, op_sbc_a_l);
    REGISTER_OPCODE(0x9E, op_sbc_a_hlp);
    REGISTER_OPCODE(0x9F, op_sbc_a_a);
    REGISTER_OPCODE(0xDE, op_sbc_a_d8);
    REGISTER_OPCODE(0xA0, op_and_b);
    REGISTER_OPCODE(0xA1, op_and_c);
    REGISTER_OPCODE(0xA2, op_and_d);
    REGISTER_OPCODE(0xA3, op_and_e);
    REGISTER_OPCODE(0xA4, op_and_h);
    REGISTER_OPCODE(0xA5, op_and_l);
    REGISTER_OPCODE(0xA6, op_and_hlp);
    REGISTER_OPCODE(0xA7, op_and_a);
    REGISTER_OPCODE(0xE6, op_and_d8);
    REGISTER_OPCODE(0xA8, op_xor_b);
    REGISTER_OPCODE(0xA9, op_xor_c);
    REGISTER_OPCODE(0xAA, op_xor_d);
    REGISTER_OPCODE(0xAB, op_xor_e);
    REGISTER_OPCODE(0xAC, op_xor_h);
    REGISTER_OPCODE(0xAD, op_xor_l);
    REGISTER_OPCODE(0xAE, op_xor_hlp);
    REGISTER_OPCODE(0xAF, op_xor_a);
    REGISTER_OPCODE(0xEE, op_xor_d8);
    REGISTER_OPCODE(0xB0, op_or_b);
    REGISTER_OPCODE(0xB1, op_or_c);
    REGISTER_OPCODE(0xB2, op_or_d);
    REGISTER_OPCODE(0xB3, op_or_e);
    REGISTER_OPCODE(0xB4, op_or_h);
    REGISTER_OPCODE(0xB5, op_or_l);
    REGISTER_OPCODE(0xB6, op_or_hlp);
    REGISTER_OPCODE(0xB7, op_or_a);
    REGISTER_OPCODE(0xF6, op_or_d8);
    REGISTER_OPCODE(0xE0, op_ldh_a8_a);
    REGISTER_OPCODE(0xE2, op_ldh_c_a);
    REGISTER_OPCODE(0xEA, op_ld_a16_a);
    REGISTER_OPCODE(0xF0, op_ldh_a_a8);
    REGISTER_OPCODE(0xF2, op_ldh_a_c);
    REGISTER_OPCODE(0xFA, op_ld_a_a16);
    REGISTER_OPCODE(0xE8, op_add_sp_r8);
    REGISTER_OPCODE(0xF8, op_ld_hl_sp_r8);
    REGISTER_OPCODE(0xF9, op_ld_sp_hl);
    REGISTER_OPCODE(0xE9, op_jp_hl);
    REGISTER_OPCODE(0xF3, op_di);
    REGISTER_OPCODE(0xFB, op_ei);
    REGISTER_OPCODE(0xB8, op_cp_b);
    REGISTER_OPCODE(0xB9, op_cp_c);
    REGISTER_OPCODE(0xBA, op_cp_d);
    REGISTER_OPCODE(0xBB, op_cp_e);
    REGISTER_OPCODE(0xBC, op_cp_h);
    REGISTER_OPCODE(0xBD, op_cp_l);
    REGISTER_OPCODE(0xBE, op_cp_hlp);
    REGISTER_OPCODE(0xBF, op_cp_a);
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
        printf("Unknown opcode: 0x%02X at PC: 0x%04X\n", opcode, cpu->pc - 1);
        exit(1);
    }
}

void cpu_print_state(const CPU* cpu) {
    printf("PC=%04X SP=%04X AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X\n",
        cpu->pc, cpu->sp,
        cpu->a, cpu->f, cpu->b, cpu->c,
        cpu->d, cpu->e, cpu->h, cpu->l);
}
