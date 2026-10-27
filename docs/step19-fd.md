# Step 19 — 파일 디스크립터 & 시스템 콜 정리

> 전역 `struct file` 풀과 프로세스별 `fd_table[16]` 을 만들어 open/read/write/close/lseek을 정수 fd로 다룬다.

## 목표

`/init` 이 `open("/hello.txt")` → `read` → `write(1, …)` → `close` 로 디스크 파일 내용을 출력한다.

## 이 단계가 만드는 틀

- **2층 구조: fd → `file_t*` → inode**
  ```
  프로세스 A fd_table: [0]─┐ [1]─┐ [2]─┐ [3]──────┐
                          ▼     ▼     ▼          ▼
  전역 file 풀:        STDIN  STDOUT STDOUT   EXT2(ino=12, offset=5, refcount=1)
  ```
  fd_table은 **포인터**만 든다 → Step 20 fork가 테이블을 복사하면 부모와 자식이 **같은 열린 파일(같은 offset)** 을 공유한다. 이게 유닉스 의미론
- **refcount** — 공유되는 `file_t` 를 언제 해제할지. close/exit(22)가 줄이고 fork(20)가 늘린다
- **시스템 콜 번호 = Linux x86-64** — read 0, write 1, open 2, close 3, lseek 8, (fork 57, exec 59, exit 60, wait 61)
- **반환값을 `r->rax` 로** — 이제 모든 시스템 콜이 값을 돌려준다 (Step 11에서 깔아 둔 틀)

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/file.h` / `file.c` | `file_t` (type, inode_no, offset, refcount), 64칸 풀, `file_alloc` / `file_free` |
| `proc/process.h` / `process.c` | PCB에 `fd_table[FD_MAX]`, `proc_init_fds` 로 0/1/2 연결 |
| `kernel/syscall.c` | `fd_get` · `fd_alloc`, `sys_open` · `sys_read` · `sys_write` · `sys_close` · `sys_lseek` |
| `kernel/syscall.h` | 번호 상수 |
| `fs/ext2.c` | `ext2_read_file_at` — offset부터 n바이트 |
| `Makefile` | 유저 빌드에 `-mcmodel=large` |
| `user/init.c` | open → read → write → close 테스트 |

원본 커밋: `0e8ae6d` · 읽는 순서: `kernel/file.h` → `kernel/file.c` → `proc/process.c` → `kernel/syscall.c` → `fs/ext2.c` → `user/init.c`

## 핵심 개념

- **fd는 그냥 배열 인덱스** — `open` 은 비어 있는 가장 작은 번호를 준다 (0~2가 차 있으니 첫 파일은 3)
- **offset은 `file_t` 에** — 같은 파일을 두 번 `open` 하면 `file_t` 가 둘 → offset 따로. fork로 공유하면 하나 → offset 같이 움직임
- **read의 흐름**: fd → `file_t` → 타입이 EXT2면 `ext2_read_file_at(ino, buf, offset, n)` → offset += 읽은 양 → 반환. 파일 끝이면 0
- **stdin/stdout도 같은 틀** — 타입만 다르다. write는 STDOUT이면 화면+시리얼, EXT2면 (이 구현에선) 미지원
- **`-mcmodel=large`** — 기본 small 모델은 코드 · 데이터가 2GB 아래라고 가정한 32비트 주소를 쓴다. 16GB에 링크하면 재배치가 넘쳐서, 64비트 절대 주소를 쓰는 large 모델이 필요

## 직접 만든다면

1. `file_t` 풀과 alloc/free
2. PCB에 fd_table, 프로세스 생성 시 0/1/2 채우기
3. `sys_write` 를 fd 기반으로 다시 작성 (동작은 그대로여야 함)
4. `sys_open` (경로 → inode → `file_t` → fd), `sys_close`
5. `ext2_read_file_at` + `sys_read`, 마지막으로 `sys_lseek`

## 확인

- `/init` 출력에 `hello.txt` 내용 (`Hello from ext2!`)
- 생각해 볼 것: `file_t` 를 fd_table에 직접 넣지 않고 포인터로 공유하는 이유는? (Step 20에서 답이 나온다)

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 유저 빌드에서 `relocation truncated to fit` | `-mcmodel=large` 누락 |
| read가 계속 같은 내용만 | offset 갱신 누락 |
| 유저 포인터를 그대로 믿어서 커널 크래시 | 이 구현은 검증하지 않는다 — 진짜 OS는 유저 주소 범위 검사 필수 |

## 다음 단계로

프로세스는 커널이 하나씩 만들어 줄 뿐, 유저가 **스스로 새 프로세스를 만들** 방법이 없다 → [Step 20](step20-fork.md)
