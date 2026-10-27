#include "fs/ext2.h"
#include "drivers/ata.h"
#include "lib/printf.h"
#include "lib/string.h"
#include <stdint.h>
#include <stddef.h>

/* 슈퍼블록에서 읽어온 핵심 값들 */
static uint32_t block_size;
static uint32_t inodes_per_group;
static uint32_t inode_size;
static uint32_t bgdt_block;
static uint32_t blocks_per_group;
static uint32_t first_data_block;   /* 1024B 블록=1, 그 외=0 */

/* 블록 번호 → LBA 변환 (블록 크기가 512의 배수이므로 나누기) */
static uint32_t block_to_lba(uint32_t block_no) {
    return block_no * (block_size / 512);
}

/* 블록 하나(최대 4KB = 8섹터) 읽기 */
int ext2_read_block(uint32_t block_no, void *buf) {
    uint32_t sectors = block_size / 512;
    uint32_t lba     = block_to_lba(block_no);
    uint8_t *dst     = buf;

    for (uint32_t i = 0; i < sectors; i++) {
        if (ata_read_sector(lba + i, dst) < 0) return -1;
        dst += 512;
    }
    return 0;
}

int ext2_init(void) {
    /* 슈퍼블록: 디스크 오프셋 1024바이트 = LBA 2 */
    uint8_t buf[1024];
    if (ata_read_sector(2, buf)       < 0) return -1;
    if (ata_read_sector(3, buf + 512) < 0) return -1;

    ext2_superblock_t *sb = (ext2_superblock_t *)buf;

    if (sb->s_magic != EXT2_MAGIC) {
        kprintf("[ext2] bad magic: 0x%x (expected 0xef53)\n", sb->s_magic);
        return -1;
    }

    block_size       = 1024u << sb->s_log_block_size;
    inodes_per_group = sb->s_inodes_per_group;
    inode_size       = (sb->s_rev_level >= 1) ? sb->s_inode_size : 128;
    blocks_per_group = sb->s_blocks_per_group;
    first_data_block = sb->s_first_data_block;

    /* 볼륨 이름 (null 종료 보장) */
    char volname[17];
    for (int i = 0; i < 16; i++) volname[i] = sb->s_volume_name[i];
    volname[16] = '\0';

    klog("[ext2] mounting \"%s\"\n", volname[0] ? volname : "(no label)");
    klog("[ext2] block size: %u B  total: %u blocks  free: %u blocks\n",
         block_size, sb->s_blocks_count, sb->s_free_blocks_count);
    klog("[ext2] inodes: %u total  %u free  (size %u B)\n",
         sb->s_inodes_count, sb->s_free_inodes_count, inode_size);

    /* Block Group Descriptor Table: 슈퍼블록 바로 다음 블록 */
    bgdt_block = (block_size == 1024) ? 2 : 1;

    /* 그룹 0 디스크립터만 읽어서 로그 출력 */
    ext2_bgd_t bgd0;
    uint8_t  bgdt_buf[512];
    if (ata_read_sector(block_to_lba(bgdt_block), bgdt_buf) < 0) return -1;
    memcpy(&bgd0, bgdt_buf, sizeof(ext2_bgd_t));

    klog("[ext2] group 0 — inode_table block=%u  free_blocks=%u  free_inodes=%u\n",
         bgd0.bg_inode_table,
         bgd0.bg_free_blocks_count,
         bgd0.bg_free_inodes_count);

    return 0;
}

/* 디렉토리 inode의 데이터 블록을 순회하며 name 찾기 → inode 번호 반환 (없으면 0) */
uint32_t ext2_dir_lookup(uint32_t dir_ino, const char *name) {
    ext2_inode_t inode;
    if (ext2_read_inode(dir_ino, &inode) < 0) return 0;

    static uint8_t blk[1024];
    if (ext2_read_block(inode.i_block[0], blk) < 0) return 0;

    uint32_t offset = 0;
    while (offset < block_size) {
        ext2_dirent_t *de = (ext2_dirent_t *)(blk + offset);
        if (de->inode == 0) { offset += de->rec_len; continue; }

        if (de->name_len == strlen(name) &&
            memcmp(de->name, name, de->name_len) == 0)
            return de->inode;

        offset += de->rec_len;
    }
    return 0;
}

/* 디렉토리 내용 출력 */
void ext2_dir_ls(uint32_t dir_ino) {
    ext2_inode_t inode;
    if (ext2_read_inode(dir_ino, &inode) < 0) return;

    static uint8_t blk[1024];
    if (ext2_read_block(inode.i_block[0], blk) < 0) return;

    uint32_t offset = 0;
    while (offset < block_size) {
        ext2_dirent_t *de = (ext2_dirent_t *)(blk + offset);
        if (de->inode == 0) { offset += de->rec_len; continue; }

        /* "." ".." 숨김 */
        if (!(de->name_len == 1 && de->name[0] == '.') &&
            !(de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.')) {
            for (uint8_t i = 0; i < de->name_len; i++)
                printf("%c", de->name[i]);
            if (de->file_type == EXT2_FT_DIR)
                printf("/");
            printf("\n");
        }

        offset += de->rec_len;
    }
}

/* inode 번호(1-based) → ext2_inode_t 읽기 */
int ext2_read_inode(uint32_t ino, ext2_inode_t *out) {
    uint32_t group = (ino - 1) / inodes_per_group;   /* 속한 블록 그룹 */
    uint32_t idx   = (ino - 1) % inodes_per_group;   /* 그룹 내 인덱스 */

    /* 해당 그룹의 BGD 읽기 (BGD 하나 = 32바이트, 한 섹터에 여러 개 들어감) */
    ext2_bgd_t bgd;
    uint32_t bgd_byte   = group * sizeof(ext2_bgd_t);
    uint32_t bgd_sector = block_to_lba(bgdt_block) + bgd_byte / 512;
    uint32_t bgd_off    = bgd_byte % 512;
    uint8_t  bgd_buf[512];
    if (ata_read_sector(bgd_sector, bgd_buf) < 0) return -1;
    memcpy(&bgd, bgd_buf + bgd_off, sizeof(ext2_bgd_t));

    uint32_t byte_off  = idx * inode_size;
    uint32_t block_off = byte_off / block_size;
    uint32_t inner_off = byte_off % block_size;
    uint32_t block_no  = bgd.bg_inode_table + block_off;

    static uint8_t blk[4096];
    if (ext2_read_block(block_no, blk) < 0) return -1;

    memcpy(out, blk + inner_off, sizeof(ext2_inode_t));
    return 0;
}

/* 파일 inode의 데이터 블록들을 순서대로 읽어 buf에 복사. 실제 읽은 바이트 수 반환 */
int ext2_read_file(uint32_t ino, void *buf, uint32_t max_len) {
    ext2_inode_t inode;
    if (ext2_read_inode(ino, &inode) < 0) return -1;

    uint32_t remaining = inode.i_size < max_len ? inode.i_size : max_len;
    uint8_t *dst       = buf;
    uint32_t total     = 0;

    static uint8_t blk[1024];

    /* 직접 블록 포인터 [0..11] */
    for (int i = 0; i < 12 && remaining > 0; i++) {
        if (inode.i_block[i] == 0) break;
        if (ext2_read_block(inode.i_block[i], blk) < 0) return -1;

        uint32_t chunk = remaining < block_size ? remaining : block_size;
        memcpy(dst, blk, chunk);
        dst       += chunk;
        total     += chunk;
        remaining -= chunk;
    }

    /* 단일 간접 블록 [12]: 블록 번호 목록이 든 블록 */
    if (remaining > 0 && inode.i_block[12] != 0) {
        static uint32_t indirect[256]; /* 1024 / 4 */
        if (ext2_read_block(inode.i_block[12], indirect) < 0) return -1;

        uint32_t entries = block_size / 4;
        for (uint32_t i = 0; i < entries && remaining > 0; i++) {
            if (indirect[i] == 0) break;
            if (ext2_read_block(indirect[i], blk) < 0) return -1;

            uint32_t chunk = remaining < block_size ? remaining : block_size;
            memcpy(dst, blk, chunk);
            dst       += chunk;
            total     += chunk;
            remaining -= chunk;
        }
    }

    return (int)total;
}

/* offset부터 최대 n바이트 읽기 (sys_read의 fd offset 지원용) */
int ext2_read_file_at(uint32_t ino, void *buf, uint32_t offset, uint32_t n) {
    ext2_inode_t inode;
    if (ext2_read_inode(ino, &inode) < 0) return -1;

    if (offset >= inode.i_size) return 0;
    uint32_t remaining = inode.i_size - offset;
    if (n > remaining) n = remaining;
    if (n == 0) return 0;

    uint8_t *dst   = buf;
    uint32_t total = 0;

    static uint8_t blk[1024];

    /* 직접 블록 [0..11] */
    for (int i = 0; i < 12 && n > 0; i++) {
        uint32_t blk_start = (uint32_t)i * block_size;
        uint32_t blk_end   = blk_start + block_size;

        if (offset >= blk_end) continue;
        if (inode.i_block[i] == 0) break;
        if (ext2_read_block(inode.i_block[i], blk) < 0) return -1;

        uint32_t inner = (offset > blk_start) ? (offset - blk_start) : 0;
        uint32_t avail = block_size - inner;
        uint32_t chunk = (n < avail) ? n : avail;

        memcpy(dst, blk + inner, chunk);
        dst    += chunk;
        total  += chunk;
        n      -= chunk;
        offset += chunk;
    }

    /* 단일 간접 블록 [12] */
    if (n > 0 && inode.i_block[12] != 0) {
        static uint32_t indirect[256];
        if (ext2_read_block(inode.i_block[12], indirect) < 0) return -1;

        uint32_t entries = block_size / 4;
        for (uint32_t i = 0; i < entries && n > 0; i++) {
            uint32_t blk_start = (12 + i) * block_size;
            uint32_t blk_end   = blk_start + block_size;
            if (offset >= blk_end) continue;
            if (indirect[i] == 0) break;
            if (ext2_read_block(indirect[i], blk) < 0) return -1;

            uint32_t inner = (offset > blk_start) ? (offset - blk_start) : 0;
            uint32_t avail = block_size - inner;
            uint32_t chunk = (n < avail) ? n : avail;

            memcpy(dst, blk + inner, chunk);
            dst    += chunk;
            total  += chunk;
            n      -= chunk;
            offset += chunk;
        }
    }

    return (int)total;
}

/* ── 쓰기 헬퍼 ─────────────────────────────────────────────── */

int ext2_write_block(uint32_t block_no, const void *buf) {
    uint32_t sectors  = block_size / 512;
    uint32_t lba      = block_to_lba(block_no);
    const uint8_t *src = buf;
    for (uint32_t i = 0; i < sectors; i++) {
        if (ata_write_sector(lba + i, src) < 0) return -1;
        src += 512;
    }
    return 0;
}

/* BGD 읽기 / 쓰기 */
static ext2_bgd_t bgd_read(uint32_t group) {
    ext2_bgd_t bgd;
    uint32_t off    = group * sizeof(ext2_bgd_t);
    uint32_t sector = block_to_lba(bgdt_block) + off / 512;
    uint32_t inner  = off % 512;
    uint8_t  buf[512];
    ata_read_sector(sector, buf);
    memcpy(&bgd, buf + inner, sizeof(ext2_bgd_t));
    return bgd;
}

static void bgd_write(uint32_t group, const ext2_bgd_t *bgd) {
    uint32_t off    = group * sizeof(ext2_bgd_t);
    uint32_t sector = block_to_lba(bgdt_block) + off / 512;
    uint32_t inner  = off % 512;
    uint8_t  buf[512];
    ata_read_sector(sector, buf);
    memcpy(buf + inner, bgd, sizeof(ext2_bgd_t));
    ata_write_sector(sector, buf);
}

/* 슈퍼블록 free 카운트 업데이트 */
static void sb_dec_free_blocks(void) {
    uint8_t buf[512];
    ata_read_sector(2, buf);
    ext2_superblock_t *sb = (ext2_superblock_t *)buf;
    sb->s_free_blocks_count--;
    ata_write_sector(2, buf);
}

static void sb_dec_free_inodes(void) {
    uint8_t buf[512];
    ata_read_sector(2, buf);
    ext2_superblock_t *sb = (ext2_superblock_t *)buf;
    sb->s_free_inodes_count--;
    ata_write_sector(2, buf);
}

/* block bitmap에서 빈 블록 찾아 할당 → 절대 블록 번호 반환 (0=실패) */
static uint32_t ext2_alloc_block(void) {
    /* 모든 그룹 순서대로 탐색 */
    uint32_t total_groups = (blocks_per_group == 0) ? 1
        : (16384 + blocks_per_group - 1) / blocks_per_group; /* 대략 계산 */

    for (uint32_t g = 0; g < total_groups; g++) {
        ext2_bgd_t bgd = bgd_read(g);
        if (bgd.bg_free_blocks_count == 0) continue;

        static uint8_t bitmap[4096];
        ext2_read_block(bgd.bg_block_bitmap, bitmap);

        for (uint32_t i = 0; i < blocks_per_group; i++) {
            if (bitmap[i / 8] & (1 << (i % 8))) continue;  /* 사용 중 */

            /* 할당 */
            bitmap[i / 8] |= (1 << (i % 8));
            ext2_write_block(bgd.bg_block_bitmap, bitmap);

            bgd.bg_free_blocks_count--;
            bgd_write(g, &bgd);
            sb_dec_free_blocks();

            uint32_t abs_block = first_data_block + g * blocks_per_group + i;
            return abs_block;
        }
    }
    return 0;
}

/* inode bitmap에서 빈 inode 찾아 할당 → inode 번호 반환 (0=실패) */
static uint32_t ext2_alloc_inode(void) {
    uint32_t total_groups = (blocks_per_group == 0) ? 1
        : (16384 + blocks_per_group - 1) / blocks_per_group;

    for (uint32_t g = 0; g < total_groups; g++) {
        ext2_bgd_t bgd = bgd_read(g);
        if (bgd.bg_free_inodes_count == 0) continue;

        static uint8_t bitmap[4096];
        ext2_read_block(bgd.bg_inode_bitmap, bitmap);

        for (uint32_t i = 0; i < inodes_per_group; i++) {
            if (bitmap[i / 8] & (1 << (i % 8))) continue;

            bitmap[i / 8] |= (1 << (i % 8));
            ext2_write_block(bgd.bg_inode_bitmap, bitmap);

            bgd.bg_free_inodes_count--;
            bgd_write(g, &bgd);
            sb_dec_free_inodes();

            uint32_t ino = g * inodes_per_group + i + 1; /* 1-based */
            return ino;
        }
    }
    return 0;
}

/* inode 디스크에 쓰기 */
static int ext2_write_inode(uint32_t ino, const ext2_inode_t *in) {
    uint32_t group    = (ino - 1) / inodes_per_group;
    uint32_t idx      = (ino - 1) % inodes_per_group;
    ext2_bgd_t bgd    = bgd_read(group);

    uint32_t byte_off  = idx * inode_size;
    uint32_t block_off = byte_off / block_size;
    uint32_t inner_off = byte_off % block_size;
    uint32_t block_no  = bgd.bg_inode_table + block_off;

    static uint8_t blk[4096];
    if (ext2_read_block(block_no, blk) < 0) return -1;
    memcpy(blk + inner_off, in, sizeof(ext2_inode_t));
    return ext2_write_block(block_no, blk);
}

/* 디렉토리에 새 엔트리 추가 */
static int ext2_dir_add_entry(uint32_t dir_ino, uint32_t new_ino,
                               const char *name, uint8_t ftype) {
    ext2_inode_t dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) return -1;

    static uint8_t blk[1024];
    uint32_t dir_block = dir_inode.i_block[0];
    if (ext2_read_block(dir_block, blk) < 0) return -1;

    /* 마지막 dirent 찾기 — 그것의 rec_len을 줄여서 새 항목 공간 확보 */
    uint32_t offset = 0;
    ext2_dirent_t *last = NULL;
    while (offset < block_size) {
        ext2_dirent_t *de = (ext2_dirent_t *)(blk + offset);
        if (offset + de->rec_len >= block_size) { last = de; break; }
        offset += de->rec_len;
    }
    if (!last) return -1;

    uint8_t  nlen      = (uint8_t)strlen(name);
    uint16_t need      = (uint16_t)((8 + nlen + 3) & ~3u); /* 4바이트 정렬 */
    uint16_t last_real = (uint16_t)((8 + last->name_len + 3) & ~3u);
    uint16_t avail     = last->rec_len - last_real;

    if (avail < need) return -1; /* 공간 부족 — 다음 블록 필요 (미구현) */

    /* 마지막 엔트리 축소 후 새 엔트리 삽입 */
    last->rec_len = last_real;
    ext2_dirent_t *ne = (ext2_dirent_t *)(blk + offset + last_real);
    ne->inode     = new_ino;
    ne->rec_len   = avail;
    ne->name_len  = nlen;
    ne->file_type = ftype;
    memcpy(ne->name, name, nlen);

    return ext2_write_block(dir_block, blk);
}

/* ── 공개 API ────────────────────────────────────────────────── */

int ext2_write_file(uint32_t parent_ino, const char *name,
                    const void *buf, uint32_t len) {
    if (len > block_size) { kprintf("[ext2] write: file too large\n"); return -1; }

    /* 1. inode + 데이터 블록 할당 */
    uint32_t ino       = ext2_alloc_inode();
    uint32_t data_blk  = ext2_alloc_block();
    if (!ino || !data_blk) { kprintf("[ext2] alloc failed\n"); return -1; }

    /* 2. 데이터 블록에 내용 쓰기 */
    static uint8_t zero[4096];
    memcpy(zero, buf, len);
    if (len < block_size) memset(zero + len, 0, block_size - len);
    if (ext2_write_block(data_blk, zero) < 0) return -1;

    /* 3. inode 초기화 및 쓰기 */
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode        = 0x81A4;   /* regular file, 0644 */
    inode.i_size        = len;
    inode.i_links_count = 1;
    inode.i_blocks      = block_size / 512; /* 512바이트 단위 */
    inode.i_block[0]    = data_blk;
    if (ext2_write_inode(ino, &inode) < 0) return -1;

    /* 4. 부모 디렉토리에 엔트리 추가 */
    if (ext2_dir_add_entry(parent_ino, ino, name, EXT2_FT_REG) < 0) return -1;

    klog("[ext2] wrote '%s' ino=%u block=%u len=%u\n", name, ino, data_blk, len);
    return 0;
}
