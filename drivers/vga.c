#include "drivers/vga.h"
#include "kernel/io.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   ((uint16_t *)0xB8000)

#define COLOR_WHITE_ON_BLACK 0x0F

static uint16_t *vga   = (uint16_t *)0xB8000;
static int       col   = 0;
static int       row   = 0;
static uint8_t   color = COLOR_WHITE_ON_BLACK;

static void hw_cursor_sync(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void scroll(void) {
    for (int r = 0; r < VGA_HEIGHT - 1; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            vga[r * VGA_WIDTH + c] = vga[(r + 1) * VGA_WIDTH + c];
    for (int c = 0; c < VGA_WIDTH; c++)
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = ((uint16_t)color << 8) | ' ';
}

void vga_init(void) {
    col = 0;
    row = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = ((uint16_t)color << 8) | ' ';
    hw_cursor_sync();
}

void vga_putchar(char c) {
    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = VGA_WIDTH - 1;
        }
        vga[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | ' ';
        return;
    } else {
        vga[row * VGA_WIDTH + col] = ((uint16_t)color << 8) | (uint8_t)c;
        if (++col >= VGA_WIDTH) {
            col = 0;
            row++;
        }
    }
    if (row >= VGA_HEIGHT) {
        scroll();
        row = VGA_HEIGHT - 1;
    }
    hw_cursor_sync();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = ((uint16_t)color << 8) | ' ';
    col = 0;
    row = 0;
    hw_cursor_sync();
}

uint8_t vga_get_color_at(int c, int r) {
    return (uint8_t)(vga[r * VGA_WIDTH + c] >> 8);
}

void vga_set_color_at(int c, int r, uint8_t col) {
    uint16_t ch = vga[r * VGA_WIDTH + c] & 0x00FF;
    vga[r * VGA_WIDTH + c] = ((uint16_t)col << 8) | ch;
}
