#pragma once
#include <stdint.h>

#define EXT2_MAGIC      0xEF53
#define EXT2_ROOT_INO   2       /* root 디렉토리는 항상 inode #2 */

/* 파일 타입 (inode i_mode 상위 4비트) */
#define EXT2_S_IFREG    0x8000  /* 일반 파일 */
#define EXT2_S_IFDIR    0x4000  /* 디렉토리 */

/* dirent file_type 필드 */
#define EXT2_FT_REG     1
#define EXT2_FT_DIR     2

/* ── Superblock (디스크 오프셋 1024, 크기 1024바이트) ── */
typedef struct {
    uint32_t s_inodes_count;        /* 전체 inode 수 */
    uint32_t s_blocks_count;        /* 전체 블록 수 */
    uint32_t s_r_blocks_count;      /* 예약 블록 수 */
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;    /* 첫 데이터 블록 (1k블록=1, 4k블록=0) */
    uint32_t s_log_block_size;      /* 블록 크기 = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;               /* 0xEF53 */
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* rev 1 이상 추가 필드 */
    uint32_t s_first_ino;
    uint16_t s_inode_size;          /* inode 구조체 크기 */
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    /* 나머지 필드 생략 */
} __attribute__((packed)) ext2_superblock_t;

/* ── Block Group Descriptor (슈퍼블록 바로 다음 블록) ── */
typedef struct {
    uint32_t bg_block_bitmap;       /* block bitmap 블록 번호 */
    uint32_t bg_inode_bitmap;       /* inode bitmap 블록 번호 */
    uint32_t bg_inode_table;        /* inode table 시작 블록 번호 */
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed)) ext2_bgd_t;

/* ── Inode (128바이트 or s_inode_size바이트) ── */
typedef struct {
    uint16_t i_mode;                /* 파일 타입 + 권한 */
    uint16_t i_uid;
    uint32_t i_size;                /* 파일 크기 (바이트) */
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;             /* 512바이트 단위 블록 수 */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];          /* [0..11] 직접, [12] 단일간접, ... */
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed)) ext2_inode_t;

/* ── Directory Entry ── */
typedef struct {
    uint32_t inode;                 /* inode 번호 (0 = 사용 안 함) */
    uint16_t rec_len;               /* 이 엔트리의 전체 길이 */
    uint8_t  name_len;              /* 파일명 길이 */
    uint8_t  file_type;
    char     name[255];             /* NULL 종료 아님 */
} __attribute__((packed)) ext2_dirent_t;

/* ── API ── */
int      ext2_init(void);
int      ext2_read_inode(uint32_t ino, ext2_inode_t *out);
int      ext2_read_block(uint32_t block_no, void *buf);
uint32_t ext2_dir_lookup(uint32_t dir_ino, const char *name);
void     ext2_dir_ls(uint32_t dir_ino);
int      ext2_read_file(uint32_t ino, void *buf, uint32_t max_len);
int      ext2_write_block(uint32_t block_no, const void *buf);
int      ext2_write_file(uint32_t parent_ino, const char *name,
                         const void *buf, uint32_t len);
