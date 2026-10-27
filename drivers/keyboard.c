#include "drivers/keyboard.h"
#include "kernel/idt.h"
#include "kernel/io.h"
#include <stdint.h>

/* PS/2 스캔코드 세트 1 → ASCII (make code만, break code는 0x80 비트 세트) */
static const char sc_ascii[128] = {
    /* 0x00-0x0F */
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    /* 0x10-0x1F */
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    /* 0x20-0x2F */
    'd','f','g','h','j','k','l',';','\'','`', 0, '\\','z','x','c','v',
    /* 0x30-0x3F */
    'b','n','m',',','.','/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    /* 0x40-0x4F */
    0, 0, 0, 0, 0, 0, 0, '7','8','9','-','4','5','6','+','1',
    /* 0x50-0x5F */
    '2','3','0','.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x60-0x6F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x70-0x7F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define KB_BUF_SIZE 256
static volatile char kb_buf[KB_BUF_SIZE];
static volatile int  kb_head = 0;
static volatile int  kb_tail = 0;

static void keyboard_handler(registers_t *r) {
    (void)r;
    uint8_t sc = inb(0x60);
    if (sc & 0x80) return;
    char c = sc_ascii[sc];
    if (!c) return;
    int next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

char keyboard_getchar(void) {
    while (kb_head == kb_tail)
        __asm__ volatile("sti; hlt");
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_handler);
}
