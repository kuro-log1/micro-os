# Step 17 — ext2 파일시스템 (읽기/쓰기)

> ATA 섹터 위에서 ext2 구조(슈퍼블록 → 블록 그룹 → inode → 디렉터리 엔트리)를 해석해 파일을 찾고 읽고 쓴다.

## 목표

부팅 로그에 디스크 루트 목록(`hello.txt`), 셸에서 `cat hello.txt` → `Hello from ext2!`, `write a.txt hi` 후 `cat a.txt` → `hi`.

## 이 단계가 만드는 틀

- **파일 API — 이후 모든 파일 접근의 바닥**
  | 함수 | 쓰는 곳 |
  |---|---|
  | `ext2_dir_lookup(dir_ino, name)` → inode 번호 | ELF 로더(18), `open`(19) |
  | `ext2_read_inode(ino, &inode)` | 파일 크기 확인 (18) |
  | `ext2_read_file(ino, buf, max)` | ELF 로더(18) |
  | `ext2_read_file_at(ino, buf, off, n)` | `read` 시스템 콜 (19에서 추가) |
  | `ext2_write_file(dir, name, buf, len)` | 셸 `write` |
- **"파일 = inode 번호"** — 이름은 디렉터리 엔트리에만 있고, 실제 파일은 번호로 다룬다. Step 19의 `struct file` 이 `inode_no` 를 들고 다닌다
- **평평한 구조** — 루트 디렉터리(inode 2)만 쓴다. 경로의 `/` 는 앞에서 떼어 낸다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `fs/ext2.h` | 디스크 위 구조체: 슈퍼블록, 블록 그룹 디스크립터, inode(128바이트), dirent |
| `fs/ext2.c` | 블록 ↔ 섹터 변환, `ext2_init`, inode 읽기/쓰기, 디렉터리 순회, 파일 읽기, 비트맵 할당, 파일 쓰기 |
| `lib/string.c` | `memcmp` (이름 비교) |
| `shell/shell.c` | `ls` 가 디스크 루트를 보여 줌, `cat` · `write` 추가 |
| `kernel/main.c` | `ext2_init()` + 루트 목록 |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `fs/ext2.h` → `fs/ext2.c` (`ext2_init` → `ext2_read_inode` → `ext2_dir_lookup` → `ext2_read_file` → `ext2_write_file`) → `shell/shell.c`

## 핵심 개념

```
디스크:  [부트 1KB][슈퍼블록 1KB][블록 그룹 디스크립터][블록 비트맵][inode 비트맵][inode 테이블][데이터 블록 ...]
```
- **슈퍼블록** (1024바이트 지점): 매직 `0xEF53`, 블록 크기 = `1024 << s_log_block_size`, 그룹당 inode 수, inode 크기(rev 1부터 `s_inode_size`, rev 0은 128)
- **inode 찾기**: 번호는 1부터. `group = (ino-1) / inodes_per_group`, `index = (ino-1) % inodes_per_group` → 그 그룹의 inode 테이블 시작 블록 + `index × inode_size`
- **inode의 블록 포인터**: `i_block[0..11]` 직접 블록, `i_block[12]` 단일 간접(블록 번호들이 든 블록). 이중/삼중 간접은 생략 → 큰 파일은 못 읽음
- **디렉터리 = 엔트리가 이어진 파일**: `[inode][rec_len][name_len][file_type][name]` 가변 길이. `rec_len` 만큼 건너뛰며 순회. 루트는 항상 inode 2
- **쓰기 = 할당 + 기록 + 연결**: 비트맵에서 빈 inode · 블록 찾기 → 사용 표시 · 개수 갱신(그룹 디스크립터, 슈퍼블록) → 데이터 · inode 기록 → 디렉터리 마지막 엔트리의 `rec_len` 을 줄여 새 엔트리를 끼움.
  이 구현은 **한 블록(1KB) 이하 파일만** 쓴다
- **블록 ↔ 섹터**: 1KB 블록 = 섹터 2개. `lba = block × (block_size / 512)`

## 직접 만든다면

1. 슈퍼블록 읽어 매직 · 블록 크기 · inode 수 출력
2. 그룹 디스크립터에서 inode 테이블 위치 → inode 2(루트) 읽기
3. 루트 디렉터리 블록을 엔트리 단위로 순회하며 이름 출력 (`ls`)
4. 이름으로 inode 찾기 → 직접 블록 따라 읽기 (`cat`)
5. 단일 간접 블록 지원
6. 쓰기: 비트맵 할당 → inode/데이터 → 디렉터리 엔트리 추가

## 확인

- 부팅 로그: `[ext2]` 블록 크기 · inode 수, 루트 목록
- 셸: `ls`, `cat hello.txt`, `write memo.txt hello` → `cat memo.txt`
- 재부팅 후에도 `memo.txt` 가 남아 있는지 (`os-learn` 은 실행할 때마다 디스크를 새로 만드니, 손으로 QEMU를 다시 켜서 확인)

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| inode 내용이 엉뚱함 | inode 번호를 0부터 셈 (`ino - 1` 누락) / inode 크기를 128로 고정 (mkfs 기본은 256) |
| 디렉터리 순회가 무한 루프 | `rec_len` 이 0인 엔트리 — 블록 끝 검사 필요 |
| 쓴 파일이 `ls` 에 안 보임 | 마지막 엔트리 `rec_len` 을 안 줄여 새 엔트리가 덮임 / 블록을 디스크에 다시 안 씀 |
| fsck가 오류를 냄 | 비트맵만 바꾸고 free 개수 갱신을 빠뜨림 |

## 다음 단계로

디스크에서 파일을 읽을 수 있으니, 그 파일이 **실행 파일**이라면 메모리에 올려 실행할 수 있다 → [Step 18](step18-elf-init.md)
