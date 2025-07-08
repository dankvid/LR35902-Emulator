//
// Created by davidg on 08.07.25.
//

#include <stdio.h>
#include <stdint.h>

int main() {
    uint8_t rom[] = {
        [0x100] = 0x3E, 0x42,    // LD A, $42
        [0x102] = 0x06, 0x13,    // LD B, $13
        [0x104] = 0x0E, 0x37,    // LD C, $37
        [0x106] = 0xAF,          // XOR A
        [0x107] = 0x3E, 0x10,    // LD A, $10
        [0x109] = 0xC6, 0x20,    // ADD A, $20
        [0x10B] = 0xFE, 0x30,    // CP $30
        [0x10D] = 0x20, 0x02,    // JR NZ, +2
        [0x10F] = 0x16, 0x99,    // LD D, $99
        [0x111] = 0xC3, 0x17, 0x01, // JP $0117
        [0x114] = 0x1E, 0xFF,    // LD E, $FF
        [0x117] = 0x26, 0xAA,    // LD H, $AA
        [0x119] = 0x2E, 0xBB,    // LD L, $BB
        [0x11B] = 0x00,          // NOP
        [0x11C] = 0xC3, 0x17, 0x01, // JP $0117
    };

    FILE* f = fopen("test.bin", "wb");
    fwrite(rom, 1, sizeof(rom), f);
    fclose(f);

    printf("Test-ROM created: test.bin\n");
    return 0;
}