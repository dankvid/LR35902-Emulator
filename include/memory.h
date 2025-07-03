//
// Created by davidg on 03.07.25.
//

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

void memory_init(void);
uint8_t mem_read(uint16_t address);
void mem_write(uint16_t address, uint8_t value);
void load_rom(const char* filename);

#endif // MEMORY_H
