# Step 20 — fork()

> 부모의 주소 공간을 복제한 새 PML4를 만들고, 부모의 시스템 콜 레지스터 프레임을 자식 커널 스택에 복사해 자식이 "fork에서 0을 받으며" ring 3으로 돌아가게 한다.

## 목표

`/init` 에서 `fork()` 한 번으로 `[parent] forked child!` 와 `[child]  hello from child process!` 가 **둘 다** 출력된다.

## 이 단계가 만드는 틀

- **프로세스별 주소 공간** — PCB의 `pml4` 가 처음으로 프로세스마다 달라진다. (CR3 전환은 Step 22에서 스케줄러에 들어감)
- **`vmm_fork(pml4)` 의 복제 규칙** — 이후 메모리 관리의 기준
  | 페이지 | 처리 |
  |---|---|
  | USER 4KB 페이지 | **물리 복사** (자식만의 사본) |
  | 커널 4KB 페이지 (힙 등) | 엔트리 공유 |
  | 2MB 대형 페이지 (커널 1GB) | 엔트리 공유 |
- **`saved_regs` + `fork_return`** — "인터럽트 프레임을 통째로 복사해서 다른 프로세스로 복귀시키는" 기법. Step 08의 초기 프레임 기법의 확장판
- **fd 공유** — 포인터 복사 + `refcount++` (Step 19의 설계가 여기서 쓰인다)

## 만들 파일

| 파일 | 역할 |
|---|---|
| `mm/vmm.c` | `vmm_fork` — 4단계를 전부 순회하며 새 테이블 생성, USER 페이지 복사 |
| `proc/process.h` | `saved_regs`, `exit_code` 필드 |
| `proc/process.c` | `process_fork(r)` — PCB 복사, fd refcount, 커널 스택에 ring 3 프레임 복사, `rax = 0`, swtch 프레임 |
| `proc/swtch.s` | `fork_return` — 복사된 프레임에서 레지스터 pop → `iretq` |
| `kernel/syscall.c` / `.h` | `SYS_FORK(57)` → 부모에게 자식 pid 반환 |
| `user/init.c` | fork 테스트 |

원본 커밋: `24e1c3c` · 읽는 순서: `mm/vmm.c`(vmm_fork) → `proc/process.c`(process_fork) → `proc/swtch.s`(fork_return) → `kernel/syscall.c`

## 핵심 개념

- **fork는 "한 번 불러 두 번 돌아오는" 함수** — 부모에게는 자식 pid, 자식에게는 0
- **자식이 0을 받는 원리**
  ```
  부모: int 0x80 → 커널 스택에 [r15..rax, int_no, err, rip, cs, rflags, rsp, ss] (22워드)
  process_fork: 이 22워드를 자식 커널 스택 꼭대기에 복사 → 복사본의 rax = 0
              그 아래에 [fork_return][0 ×6] — swtch용 가짜 프레임
  나중에 스케줄러가 자식을 고르면: swtch → ret → fork_return → pop 15개 → iretq
              → 부모와 같은 rip(int 0x80 다음 줄)에서, rax=0 으로 ring 3 재개
  ```
- **rsp/ss도 복사** — ring 3에서 들어온 인터럽트는 CPU가 유저 rsp, ss까지 push하므로 `registers_t`(20워드) + 2 = 22워드
- **왜 커널 페이지는 공유?** 커널 코드 · 힙 · 다른 프로세스의 PCB는 모든 주소 공간에서 같은 곳에 보여야 한다. 공유하지 않으면 CR3를 바꾸는 순간 커널이 사라진다
- **Copy-on-Write는 아님** — 실제 OS는 쓰기 직전까지 복사를 미룬다. 여기서는 단순하게 즉시 전체 복사

## 직접 만든다면

1. `vmm_fork`: 테이블 구조만 복제하고 USER 페이지 복사 — 커널에서 부모 pml4를 넘겨 테스트
2. `process_fork`: PCB 복사, 새 pid, 새 커널 스택
3. ring 3 프레임 복사 + `rax = 0` + swtch 프레임 + `fork_return`
4. `sys_fork` 에서 부모 쪽 `r->rax = child->pid`
5. 유저에서 `pid = fork(); if (pid == 0) ... else ...`

## 확인

- `[parent] forked child!` 와 `[child]  hello from child process!` 가 둘 다 나온다 (순서는 스케줄링에 따라 다를 수 있음)
- 생각해 볼 것: 자식은 fork()의 반환값 0을 어떻게 받게 될까? (위 그림의 `rax = 0`)

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 자식이 실행되자마자 #GP | 복사한 프레임 크기 틀림(ss/rsp 누락) → iretq가 쓰레기를 꺼냄 |
| 자식이 부모의 변수를 바꿈 | USER 페이지를 복사하지 않고 공유 |
| 자식에서 커널이 Page Fault | 커널 영역 엔트리를 공유하지 않음 |
| 자식도 pid를 받음 | 복사본이 아니라 부모 프레임의 rax를 0으로 바꿈 |

## 다음 단계로

자식은 부모와 똑같은 프로그램을 돈다. 쓸모 있으려면 자식이 **다른 프로그램으로 변신**해야 한다 → [Step 21](step21-exec.md)
