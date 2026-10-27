# Step 14 — ring 3 진입 & write/exit 시스템 콜

> `iretq` 스택 프레임을 직접 쌓아 ring 3으로 내려가고, 손으로 짠 기계어가 `int 0x80` 으로 write와 exit를 부른다.

## 목표

`Hello from ring 3!` 출력 후 (시리얼에) `[syscall] pid N: exit(0)`, 셸은 계속 동작.

## 이 단계가 만드는 틀

- **`jump_to_usermode(rip, rsp)`** — 커널에서 유저로 가는 유일한 길. Step 18(ELF), 21(exec)도 이 함수로 진입
- **유저 프로세스 = 커널 스택 + 유저 진입점** — `process_create_user(rip, rsp)` 는 Step 08의 초기 프레임 기법을 그대로 쓰되, 진입점이 `user_proc_trampoline` 이다
- **첫 시스템 콜 2개** — `SYS_WRITE(1)`: fd 1/2면 화면+시리얼, `SYS_EXIT(60)`: `PROC_DEAD` 로 표시 후 양보
- **`PROC_DEAD` 상태** — 스케줄러가 건너뛴다. Step 22에서 "좀비"로 발전 (부모가 회수할 때까지 남아 있음)
- **U/S 비트 보완** — 이미 있는 상위 엔트리에도 `|= VMM_USER`

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/usermode.c` | 42바이트 기계어 `user_program` (write → exit), `jump_to_usermode` |
| `kernel/usermode.h` | `USER_CS = 0x1B`, `USER_DS = 0x23` |
| `proc/process.c` | `user_proc_trampoline`, `process_create_user` |
| `proc/process.h` | `user_rip` · `user_rsp` 필드, `PROC_DEAD` |
| `proc/scheduler.c` | DEAD 프로세스 건너뛰기 |
| `kernel/syscall.c` | `sys_write`, `sys_exit`, switch 분기 |
| `mm/vmm.c` | 기존 PML4/PDPT/PD 엔트리에도 USER 비트 OR |
| `kernel/main.c` | `process_create_user(USER_CODE_VIRT, USER_STACK_VIRT + 4096)` |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `kernel/usermode.c` → `proc/process.c` → `kernel/syscall.c` → `proc/scheduler.c`

## 핵심 개념

- **ring 0 → 3은 "인터럽트에서 돌아가는 척"** — `iretq` 는 스택에서 `rip, cs, rflags, rsp, ss` 를 꺼낸다. 이 다섯 개를 직접 push하고 `iretq` 하면, CPU는 cs의 RPL=3을 보고 ring 3으로 전환한다
  ```
  push ss (0x23) → push rsp(유저 스택 꼭대기) → pushfq + IF=1 → push cs (0x1B) → push rip(진입점) → iretq
  ```
- **ring 3 → 0은 `int 0x80`** — CPU가 TSS.rsp0으로 스택을 바꾸고 `ss, rsp, rflags, cs, rip` 를 push (Step 10에서 준비한 것)
- **스택은 아래로 자란다** — 그래서 스택 rsp는 페이지의 **끝** `USER_STACK_VIRT + 4096`
- **기계어를 손으로?** 아직 디스크도 ELF 로더도 없어서, 위치에 상관없이 도는 짧은 코드를 바이트 배열로 넣었다. `lea rsi, [rip+21]` 로 바로 뒤 문자열 주소를 구한다. (Step 18에서 진짜 C 프로그램으로 교체)
- **exit는 돌아오지 않는다** — DEAD로 표시하고 `schedule()`. 스케줄러가 다시는 고르지 않으니 그 다음 줄은 실행되지 않는다
- **왜 vmm 보완이 필요했나**: `0x400000000` 의 PML4 인덱스는 0. boot.s가 만든 PML4[0] 엔트리에 U/S가 없어 Step 12 코드만으로는 ring 3 접근이 #PF였다

## 직접 만든다면

1. `jump_to_usermode` 와 기계어 `hlt` 하나 → ring 3에서 `hlt` 는 특권 명령이라 **#GP가 나면 성공** (ring 3에 도착했다는 증거)
2. 기계어를 `int 0x80` 하나로 바꾸고 `syscall_handler` 에서 출력 확인
3. write/exit 구현, 기계어 완성
4. `process_create_user` 로 프로세스화 (셸과 동시에 돌게)

## 확인

- 화면/시리얼에 `Hello from ring 3!`, 시리얼에 exit 로그
- `info registers` (QEMU 모니터) 로 cs=`0x1b` 를 잡아 보는 것도 좋다

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `iretq` 에서 #GP | 셀렉터 RPL이 3이 아님(`0x18` 대신 `0x1B`), 유저 세그먼트 DPL 틀림 |
| 첫 명령에서 #PF `err=0x5` (user, read, protection) | 상위 엔트리 U/S 누락 — 이 단계의 vmm 보완 |
| `int 0x80` 에서 #GP | 0x80 게이트 DPL이 0 |
| 유저에서 인터럽트가 걸리자 커널이 죽음 | TSS.rsp0 미설정 / TSS 로드 안 됨 |

## 다음 단계로

ring 3은 되지만, 프로그램이 커널 소스 안의 바이트 배열이다. 진짜 프로그램은 **디스크**에서 읽어 와야 한다.
디스크에 가려면 먼저 어떤 장치가 붙어 있는지부터 알아본다 → [Step 15](step15-pci.md)
