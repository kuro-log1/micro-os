#pragma once
#include <stdint.h>

#define USER_CS 0x1B  /* GDT 0x18(유저코드) | RPL=3 */
#define USER_DS 0x23  /* GDT 0x20(유저데이터) | RPL=3 */

/* identity map(1GB) 밖 주소 → vmm_map_page로 4KB 매핑 가능 */
#define USER_CODE_VIRT  0x400000000UL   /* 16GB */
#define USER_STACK_VIRT 0x500000000UL   /* 20GB */

void usermode_setup(void);
