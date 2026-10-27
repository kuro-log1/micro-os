#include "drivers/ata.h"
#include "kernel/io.h"
#include "lib/printf.h"
#include <stdint.h>

/* Primary ATA 채널 포트 */
#define ATA_DATA        0x1F0   /* 데이터 (16-bit) */
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE       0x1F6   /* 드라이브 선택 + LBA[27:24] */
#define ATA_STATUS      0x1F7   /* 읽기: 상태 */
#define ATA_CMD         0x1F7   /* 쓰기: 명령 */

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30
#define ATA_CMD_FLUSH    0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY      0x80    /* Busy */
#define ATA_SR_DRQ      0x08    /* Data Request */
#define ATA_SR_ERR      0x01    /* Error */

static void wait_ready(void) {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

static int wait_drq(void) {
    uint8_t st;
    do { st = inb(ATA_STATUS); } while (st & ATA_SR_BSY);
    return (st & ATA_SR_ERR) ? -1 : 0;
}

/* IDENTIFY — 디스크 모델명과 용량을 klog로 출력 */
int ata_identify(void) {
    wait_ready();
    outb(ATA_DRIVE, 0xA0);  /* Master 선택 */
    outb(ATA_SECTOR_CNT, 0);
    outb(ATA_LBA_LO,     0);
    outb(ATA_LBA_MID,    0);
    outb(ATA_LBA_HI,     0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0 || status == 0xFF) {
        klog("[ata] no drive detected\n");
        return -1;
    }

    if (wait_drq() < 0) {
        klog("[ata] IDENTIFY failed\n");
        return -1;
    }

    uint16_t buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = inw(ATA_DATA);

    /* 모델명: words 27-46, 각 word의 상위/하위 바이트가 바뀌어 있음 */
    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2]     = (char)(buf[27 + i] >> 8);
        model[i * 2 + 1] = (char)(buf[27 + i] & 0xFF);
    }
    model[40] = '\0';
    /* 뒤 공백 제거 */
    for (int i = 39; i >= 0 && model[i] == ' '; i--) model[i] = '\0';

    /* 용량: words 60-61 (LBA28 sector count) */
    uint32_t sectors = (uint32_t)buf[60] | ((uint32_t)buf[61] << 16);
    uint32_t mb      = sectors / 2048;  /* sectors × 512B / 1MB */

    klog("[ata] ata0: %s  (%u MB)\n", model, mb);
    return 0;
}

/* LBA28 모드로 섹터 하나(512바이트) 읽기 */
int ata_read_sector(uint32_t lba, void *buf) {
    wait_ready();

    outb(ATA_DRIVE,      0xE0 | ((lba >> 24) & 0x0F)); /* Master, LBA */
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,     (uint8_t)(lba));
    outb(ATA_LBA_MID,    (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
    outb(ATA_CMD,        ATA_CMD_READ);

    if (wait_drq() < 0) return -1;

    uint16_t *dst = buf;
    for (int i = 0; i < 256; i++)
        dst[i] = inw(ATA_DATA);

    return 0;
}

/* LBA28 모드로 섹터 하나(512바이트) 쓰기 */
int ata_write_sector(uint32_t lba, const void *buf) {
    wait_ready();

    outb(ATA_DRIVE,      0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,     (uint8_t)(lba));
    outb(ATA_LBA_MID,    (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
    outb(ATA_CMD,        ATA_CMD_WRITE);

    if (wait_drq() < 0) return -1;

    const uint16_t *src = buf;
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, src[i]);

    outb(ATA_CMD, ATA_CMD_FLUSH); /* 디스크에 실제로 반영 */
    wait_ready();
    return 0;
}
