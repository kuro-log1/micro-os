#pragma once
#include <stdint.h>

void    vga_init(void);
void    vga_putchar(char c);
void    vga_clear(void);
uint8_t vga_get_color_at(int col, int row);
void    vga_set_color_at(int col, int row, uint8_t color);
