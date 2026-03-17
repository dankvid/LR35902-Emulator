#include <cpu.h>
#include <memory.h>

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_EQ_U8(actual, expected, msg) do { \
    if ((uint8_t)(actual) != (uint8_t)(expected)) { \
        fprintf(stderr, "FAIL: %s (actual=0x%02X expected=0x%02X)\n", (msg), (unsigned)(uint8_t)(actual), (unsigned)(uint8_t)(expected)); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ_U16(actual, expected, msg) do { \
    if ((uint16_t)(actual) != (uint16_t)(expected)) { \
        fprintf(stderr, "FAIL: %s (actual=0x%04X expected=0x%04X)\n", (msg), (unsigned)(uint16_t)(actual), (unsigned)(uint16_t)(expected)); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ_U32(actual, expected, msg) do { \
    if ((uint32_t)(actual) != (uint32_t)(expected)) { \
        fprintf(stderr, "FAIL: %s (actual=%u expected=%u)\n", (msg), (unsigned)(uint32_t)(actual), (unsigned)(uint32_t)(expected)); \
        exit(1); \
    } \
} while (0)

static void cpu_reset_for_test(CPU* cpu) {
    memory_init();
    cpu_init(cpu);
    cpu->pc = 0x0100;
    cpu->cycles = 0;
    cpu->halted = false;
    cpu->ime = false;
}

static void write_bytes(uint16_t start, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        mem_write((uint16_t)(start + i), data[i]);
    }
}

static void test_nop(void) {
    CPU cpu;
    cpu_reset_for_test(&cpu);

    const uint8_t rom[] = {0x00};
    write_bytes(0x0100, rom, sizeof(rom));

    cpu_step(&cpu);

    ASSERT_EQ_U16(cpu.pc, 0x0101, "NOP muss PC um 1 erhoehen");
    ASSERT_EQ_U32(cpu.cycles, 4, "NOP muss 4 Zyklen dauern");
}

static void test_ld_bc_d16(void) {
    CPU cpu;
    cpu_reset_for_test(&cpu);

    const uint8_t rom[] = {0x01, 0x34, 0x12};
    write_bytes(0x0100, rom, sizeof(rom));

    cpu_step(&cpu);

    ASSERT_EQ_U8(cpu.b, 0x12, "LD BC,d16 setzt B auf High-Byte");
    ASSERT_EQ_U8(cpu.c, 0x34, "LD BC,d16 setzt C auf Low-Byte");
    ASSERT_EQ_U16(cpu.pc, 0x0103, "LD BC,d16 muss PC um 3 erhoehen");
    ASSERT_EQ_U32(cpu.cycles, 12, "LD BC,d16 muss 12 Zyklen dauern");
}

static void test_jr_nz_taken_and_not_taken(void) {
    CPU cpu;

    cpu_reset_for_test(&cpu);
    cpu.f = 0x00;
    const uint8_t taken_rom[] = {0x20, 0x02};
    write_bytes(0x0100, taken_rom, sizeof(taken_rom));
    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.pc, 0x0104, "JR NZ (taken) muss korrekt springen");
    ASSERT_EQ_U32(cpu.cycles, 12, "JR NZ (taken) muss 12 Zyklen dauern");

    cpu_reset_for_test(&cpu);
    cpu.f = FLAG_Z;
    const uint8_t not_taken_rom[] = {0x20, 0x02};
    write_bytes(0x0100, not_taken_rom, sizeof(not_taken_rom));
    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.pc, 0x0102, "JR NZ (not taken) darf nicht springen");
    ASSERT_EQ_U32(cpu.cycles, 8, "JR NZ (not taken) muss 8 Zyklen dauern");
}

static void test_push_pop_bc_roundtrip(void) {
    CPU cpu;
    cpu_reset_for_test(&cpu);

    cpu.b = 0x12;
    cpu.c = 0x34;
    cpu.sp = 0xFFFE;

    const uint8_t rom[] = {0xC5, 0x01, 0x00, 0x00, 0xC1};
    write_bytes(0x0100, rom, sizeof(rom));

    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.sp, 0xFFFC, "PUSH BC muss SP um 2 reduzieren");
    ASSERT_EQ_U8(mem_read(0xFFFC), 0x34, "PUSH BC schreibt C an SP");
    ASSERT_EQ_U8(mem_read(0xFFFD), 0x12, "PUSH BC schreibt B an SP+1");

    cpu.b = 0x00;
    cpu.c = 0x00;

    cpu_step(&cpu);
    cpu_step(&cpu);
    cpu_step(&cpu);

    ASSERT_EQ_U8(cpu.b, 0x12, "POP BC stellt B wieder her");
    ASSERT_EQ_U8(cpu.c, 0x34, "POP BC stellt C wieder her");
    ASSERT_EQ_U16(cpu.sp, 0xFFFE, "POP BC muss SP wiederherstellen");
}

static void test_call_and_ret(void) {
    CPU cpu;
    cpu_reset_for_test(&cpu);

    const uint8_t rom[] = {
        0xCD, 0x0F, 0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC9
    };
    write_bytes(0x0100, rom, sizeof(rom));

    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.pc, 0x010F, "CALL a16 muss zum Ziel springen");
    ASSERT_EQ_U16(cpu.sp, 0xFFFC, "CALL a16 muss Ruecksprungadresse pushen");
    ASSERT_EQ_U8(mem_read(0xFFFC), 0x03, "CALL a16 pusht Low-Byte der Ruecksprungadresse");
    ASSERT_EQ_U8(mem_read(0xFFFD), 0x01, "CALL a16 pusht High-Byte der Ruecksprungadresse");

    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.pc, 0x0103, "RET muss zur Ruecksprungadresse zurueckkehren");
    ASSERT_EQ_U16(cpu.sp, 0xFFFE, "RET muss SP wiederherstellen");
}

static void test_add_sp_r8_flags(void) {
    CPU cpu;

    cpu_reset_for_test(&cpu);
    cpu.sp = 0x000F;
    const uint8_t rom_pos[] = {0xE8, 0x01};
    write_bytes(0x0100, rom_pos, sizeof(rom_pos));
    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.sp, 0x0010, "ADD SP,+1 muss SP erhoehen");
    ASSERT_EQ_U8(cpu.f, FLAG_H, "ADD SP,+1 setzt bei 0x0F->0x10 nur H");

    cpu_reset_for_test(&cpu);
    cpu.sp = 0x00FF;
    const uint8_t rom_neg[] = {0xE8, 0x01};
    write_bytes(0x0100, rom_neg, sizeof(rom_neg));
    cpu_step(&cpu);
    ASSERT_EQ_U16(cpu.sp, 0x0100, "ADD SP,+1 ueber Byte-Grenze");
    ASSERT_EQ_U8(cpu.f, (FLAG_H | FLAG_C), "ADD SP,+1 setzt H und C ueber 0x00FF");
}

int main(void) {
    test_nop();
    test_ld_bc_d16();
    test_jr_nz_taken_and_not_taken();
    test_push_pop_bc_roundtrip();
    test_call_and_ret();
    test_add_sp_r8_flags();

    printf("Alle Non-CB-Opcode-Tests erfolgreich.\n");
    return 0;
}

