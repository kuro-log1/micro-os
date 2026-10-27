#include "kernel/usermode.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

/*
 * 셀프 컨테인드 유저 프로그램 (위치독립 기계어, Step 3 플레이스홀더)
 * Step 4에서 write + exit 시스템콜 코드로 교체한다.
 */
static const uint8_t user_program[] = {
    0xf4  /* hlt — 일단 멈춤 */
};

void usermode_setup(void) {
    uint64_t *pml4 = vmm_get_pml4();

    /* 유저 코드 페이지: VMM_USER 플래그로 매핑 → 유저 모드 접근 가능 */
    void *code_phys = pmm_alloc_page();
    vmm_map_page(pml4, USER_CODE_VIRT, (uint64_t)code_phys,
                 VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    memcpy((void *)USER_CODE_VIRT, user_program, sizeof(user_program));

    /* 유저 스택 페이지 */
    void *stack_phys = pmm_alloc_page();
    vmm_map_page(pml4, USER_STACK_VIRT, (uint64_t)stack_phys,
                 VMM_PRESENT | VMM_WRITABLE | VMM_USER);

    printf("[user] code  mapped: virt=%p phys=%p\n",
           (void *)USER_CODE_VIRT, code_phys);
    printf("[user] stack mapped: virt=%p phys=%p\n",
           (void *)USER_STACK_VIRT, stack_phys);
}
