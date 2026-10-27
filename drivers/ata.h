#pragma once
#include <stdint.h>

int  ata_read_sector (uint32_t lba, void *buf);
int  ata_write_sector(uint32_t lba, const void *buf);
int  ata_identify    (void);   /* 디스크 모델명/용량 출력, 실패 시 -1 */
