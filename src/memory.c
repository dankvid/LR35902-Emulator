//
// Created by davidg on 03.07.25.
//

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE 0x10000 // 64 KB

static uint8_t memory[MEMORY_SIZE];

void memory_init(void) {
    memset(memory, 0, sizeof(memory));
}

uint8_t mem_read(uint16_t address) {
    return memory[address];
}

void mem_write(uint16_t address, uint8_t value) {
    // Für den Anfang einfach direkt schreiben
    memory[address] = value;
}

void load_rom(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Fehler beim Öffnen der ROM-Datei");
        exit(1);
    }

    fread(memory, 1, 0x8000, file); // Lade bis zu 32 KB ROM in 0x0000–0x7FFF
    fclose(file);
}

