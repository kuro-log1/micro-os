# Step 21 — exec()

> 현재 프로세스의 유저 페이지를 전부 해제하고, 같은 주소 공간에 새 ELF를 로드해 그 진입점으로 ring 3에 들어간다.

## 목표

fork한 자식이 `exec("/hello")` 를 부르면 `Hello from exec'd /hello!` 가 출력된다.

## 이 단계가 만드는 틀

- **"fork + exec = 새 프로그램 실행"** — 유닉스에서 프로그램을 띄우는 표준 방법이 완성된다
- **`vmm_free_user(pml4)`** — 유저 4KB 페이지만 골라 해제(PMM 반환) + TLB 전체 플러시. exec(21), wait의 좀비 정리(22), 셸 실행(23)이 공통으로 쓴다
- **유저 프로그램 여러 개** — `user/hello.c` 추가, Makefile이 `diskfiles/hello` 도 빌드

## 만들 파일

| 파일 | 역할 |
|---|---|
| `mm/vmm.c` / `.h` | `vmm_free_user` |
| `kernel/syscall.c` / `.h` | `SYS_EXEC(59)`, `sys_exec` |
| `user/hello.c` | exec 대상 프로그램 |
| `user/init.c` | fork → 자식은 exec, 부모는 exit |
| `Makefile` | `diskfiles/hello` 빌드 |

원본 커밋: `ffe74a4` · 읽는 순서: `mm/vmm.c`(vmm_free_user) → `kernel/syscall.c`(sys_exec) → `user/hello.c` → `user/init.c`

## 핵심 개념

- **exec는 성공하면 돌아오지 않는다** — 돌아갈 코드(옛 프로그램)를 지웠기 때문. 실패하면(파일 없음) 이 구현은 프로세스를 DEAD로 만든다
- **경로 문자열을 먼저 커널로 복사** — 인자 `path` 는 유저 메모리에 있다. `vmm_free_user` 후에는 그 페이지가 없으니, 지우기 **전에** 커널 스택(`char name[256]`)으로 옮겨 둔다
- **같은 PCB, 같은 pid, 같은 fd** — exec는 프로세스를 새로 만들지 않는다. 주소 공간 내용만 갈아 끼운다. 그래서 열린 fd는 그대로 이어진다 (셸의 리다이렉션이 이 성질로 동작)
- **TLB 전체 플러시** — 많은 페이지를 지웠으니 `invlpg` 를 하나씩 하는 대신 CR3를 다시 써서 한 번에
- **진입**: `elf_load` 가 준 entry/rsp로 `jump_to_usermode` (Step 14의 함수). 시스템 콜 핸들러 안이라 IF가 꺼져 있으니 먼저 `sti`

## 직접 만든다면

1. `vmm_free_user`: vmm_fork와 같은 순회, USER 4KB 페이지면 `pmm_free_page` + 엔트리 0
2. `sys_exec`: 경로 복사 → free → `elf_load` → `jump_to_usermode`
3. `hello.c` 작성 · 빌드 · 디스크에 추가
4. init에서 fork 후 자식만 exec

## 확인

- 자식의 `Hello from exec'd /hello!`, 시리얼에 `[exec] pid=N exec(/hello): freeing user space` 등

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| exec 직후 Page Fault (커널 모드, 유저 주소) | 경로를 복사하지 않고 해제된 유저 메모리를 읽음 |
| 새 프로그램이 옛 코드를 실행 | TLB 플러시 누락 |
| `/hello not found` | Makefile/디스크에 hello가 안 들어감 |

## 다음 단계로

부모는 자식이 끝났는지 알 수 없고, 죽은 자식의 메모리(PCB, 커널 스택, 페이지 테이블)는 영원히 남는다 → [Step 22](step22-wait-exit.md)
