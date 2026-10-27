#include "drivers/mouse.h"
#include "drivers/vga.h"
#include "kernel/idt.h"
#include "kernel/io.h"
#include <stdint.h>

#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define CURSOR_COLOR 0x70  /* black on light-gray */

static int     cx = 40;
static int     cy = 12;
static uint8_t saved_color;

static int     packet_idx = 0;
static uint8_t packet[3];

static void cursor_draw(void) {
    saved_color = vga_get_color_at(cx, cy);
    vga_set_color_at(cx, cy, CURSOR_COLOR);
}

static void cursor_erase(void) {
    vga_set_color_at(cx, cy, saved_color);
}

static void mouse_handler(registers_t *r) {
    (void)r;
    uint8_t data = inb(0x60);

    /* 첫 바이트: bit3이 항상 1 — 패킷 경계 동기화 */
    if (packet_idx == 0 && !(data & 0x08)) return;

    packet[packet_idx++] = data;
    if (packet_idx < 3) return;
    packet_idx = 0;

    int dx =  (int)(int8_t)packet[1];
    int dy = -(int)(int8_t)packet[2];  /* PS/2 Y축은 위가 양수 → 반전 */

    cursor_erase();

    cx += dx;
    cy += dy;
    if (cx < 0)          cx = 0;
    if (cx >= VGA_WIDTH)  cx = VGA_WIDTH  - 1;
    if (cy < 0)          cy = 0;
    if (cy >= VGA_HEIGHT) cy = VGA_HEIGHT - 1;

    cursor_draw();
}

/* 키보드 컨트롤러 입력 버퍼가 비워질 때까지 대기 */
static void kbc_wait_in(void) {
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++);
}

/* 키보드 컨트롤러 출력 버퍼에 데이터가 올 때까지 대기 */
static void kbc_wait_out(void) {
    for (int i = 0; i < 100000 && !(inb(0x64) & 0x01); i++);
}

static void mouse_write(uint8_t cmd) {
    kbc_wait_in();
    outb(0x64, 0xD4);  /* 다음 바이트를 마우스로 전달 */
    kbc_wait_in();
    outb(0x60, cmd);
}

static uint8_t mouse_read(void) {
    kbc_wait_out();
    return inb(0x60);
}

void mouse_init(void) {
    /* aux 포트(마우스) 활성화 */
    kbc_wait_in();
    outb(0x64, 0xA8);

    /* IRQ12 활성화 + aux 클럭 활성화 */
    kbc_wait_in();
    outb(0x64, 0x20);
    kbc_wait_out();
    uint8_t cfg = inb(0x60);
    cfg |=  0x02;  /* IRQ12 enable */
    cfg &= ~0x20;  /* aux clock enable */
    kbc_wait_in();
    outb(0x64, 0x60);
    kbc_wait_in();
    outb(0x60, cfg);

    /* 마우스에 "데이터 보고 시작" 명령 */
    mouse_write(0xF4);
    mouse_read();  /* ACK(0xFA) 수신 */

    irq_install_handler(12, mouse_handler);
    cursor_draw();
}
