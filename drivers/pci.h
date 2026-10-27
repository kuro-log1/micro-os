#pragma once
#include <stdint.h>

uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
void     pci_scan(void);
