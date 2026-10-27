#include "kernel/usermode.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

/*
 * 셀프 컨테인드 유저 프로그램 (위치독립 기계어)
 *
 * 레이아웃 (base = USER_CODE_VIRT):
 *   base+ 0 : mov rax, 1          (SYS_WRITE)
 *   base+ 7 : mov rdi, 1          (fd = stdout)
 *   base+14 : lea rsi, [rip+21]   → base+42 (문자열)
 *   base+21 : mov rdx, 19         (len = 19)
 *   base+28 : int 0x80
 *   base+30 : mov rax, 60         (SYS_EXIT)
 *   base+37 : xor rdi, rdi        (code = 0)
 *   base+40 : int 0x80
 *   base+42 : "Hello from ring 3!\n"
 */
static const uint8_t user_program[] = {
    /* +0  */ 0x48,0xc7,0xc0, 0x01,0x00,0x00,0x00, /* mov rax, 1  */
    /* +7  */ 0x48,0xc7,0xc7, 0x01,0x00,0x00,0x00, /* mov rdi, 1  */
    /* +14 */ 0x48,0x8d,0x35, 0x15,0x00,0x00,0x00, /* lea rsi,[rip+21] */
    /* +21 */ 0x48,0xc7,0xc2, 0x13,0x00,0x00,0x00, /* mov rdx, 19 */
    /* +28 */ 0xcd,0x80,                             /* int 0x80    */
    /* +30 */ 0x48,0xc7,0xc0, 0x3c,0x00,0x00,0x00, /* mov rax, 60 */
    /* +37 */ 0x48,0x31,0xff,                        /* xor rdi,rdi */
    /* +40 */ 0xcd,0x80,                             /* int 0x80    */
    /* +42 */ 'H','e','l','l','o',' ','f','r','o','m',
              ' ','r','i','n','g',' ','3','!','\n'
};

void usermode_setup(void) {
    uint64_t *pml4 = vmm_get_pml4();

    void *code_phys = pmm_alloc_page();
    vmm_map_page(pml4, USER_CODE_VIRT, (uint64_t)code_phys,
                 VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    memcpy((void *)USER_CODE_VIRT, user_program, sizeof(user_program));

    void *stack_phys = pmm_alloc_page();
    vmm_map_page(pml4, USER_STACK_VIRT, (uint64_t)stack_phys,
                 VMM_PRESENT | VMM_WRITABLE | VMM_USER);

    klog("[user] code  mapped: virt=%p phys=%p\n",
         (void *)USER_CODE_VIRT, code_phys);
    klog("[user] stack mapped: virt=%p phys=%p\n",
         (void *)USER_STACK_VIRT, stack_phys);
}

/*
 * iretq로 ring 3(유저 모드)에 진입한다.
 *
 * iretq 스택 프레임 (push 역순으로 소비):
 *   rip    ← 유저 진입점
 *   cs     ← USER_CS (0x1B)
 *   rflags ← IF=1
 *   rsp    ← 유저 스택
 *   ss     ← USER_DS (0x23)
 */
void jump_to_usermode(uint64_t rip, uint64_t rsp) {
    uint64_t cs = USER_CS;
    uint64_t ss = USER_DS;
    __asm__ volatile(
        "push %0\n"              /* ss     */
        "push %1\n"              /* rsp    */
        "pushfq\n"
        "orq  $0x200, (%%rsp)\n" /* IF=1  */
        "push %2\n"              /* cs     */
        "push %3\n"              /* rip    */
        "iretq\n"
        :
        : "r"(ss), "r"(rsp), "r"(cs), "r"(rip)
        : "memory"
    );
}
