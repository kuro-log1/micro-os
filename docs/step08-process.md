# Step 08 — 프로세스 & 컨텍스트 스위칭 (선점형 라운드 로빈)

> 프로세스마다 커널 스택을 주고, 저장된 스택 포인터(rsp)만 바꿔 끼우는 `swtch` 로 실행 흐름을 전환한다. 타이머가 100ms마다 강제로 넘긴다.

## 목표

두 커널 스레드가 `AAAA…BBBB…AAAA…` 처럼 번갈아 출력한다. 둘 다 무한 루프인데도.

## 이 단계가 만드는 틀

- **PCB `process_t`** — 프로세스의 모든 것을 담는 구조체. 이후 단계마다 필드가 늘어난다:
  `user_rip/user_rsp`(14), `fd_table`(19), `saved_regs`(20), `exit_code`(22)
- **`swtch(&prev->rsp, next->rsp)`** — 모든 전환의 유일한 통로. 이후 바뀌지 않는다
- **"가짜 초기 스택 프레임" 기법** — 아직 한 번도 실행 안 된 프로세스를, 이미 `swtch` 하다 멈춘 것처럼 꾸민다.
  Step 14(ring 3 진입), Step 20(fork 자식 복귀)이 **같은 기법**으로 진입점만 바꿔 쓴다
- **원형 큐 스케줄러** — `scheduler_add` / `schedule` / `current_process`. Step 14에서 DEAD 건너뛰기, Step 22에서 CR3 교체가 추가된다
- **idle 프로세스(pid 0)** = 부팅한 그 흐름 자체

## 만들 파일

| 파일 | 역할 |
|---|---|
| `proc/process.h` | `process_t` (rsp, pml4, stack, state, pid, next, entry) |
| `proc/swtch.s` | callee-saved 6개 push → rsp 저장/교체 → pop → `ret` |
| `proc/process.c` | `process_create`(8KB 스택 + 초기 프레임), `process_create_idle`, `proc_trampoline` |
| `proc/scheduler.c` | 원형 큐, `schedule()` |
| `kernel/timer.c` | 10틱(100ms)마다 `schedule()` |
| `kernel/main.c` | thread_a / thread_b 생성 후 `schedule()` |

읽는 순서: `proc/process.h` → `proc/swtch.s` → `proc/process.c` → `proc/scheduler.c` → `kernel/timer.c`

## 핵심 개념

- **왜 rsp만 바꾸면 되나?** C 함수 호출 규약상 함수를 부르는 쪽은 rbx, rbp, r12~r15만 "보존된다"고 기대한다(callee-saved).
  `swtch` 는 함수처럼 불리므로 이 6개 + rsp만 저장하면, 나머지는 호출한 쪽이 이미 알아서 저장해 둔 상태다
- **swtch의 흐름**
  ```
  push rbp,rbx,r12..r15   (현재 프로세스 스택에)
  prev->rsp = rsp
  rsp = next->rsp         ← 여기서 세상이 바뀜
  pop r15..r12,rbx,rbp    (다음 프로세스 스택에서)
  ret                     ← 다음 프로세스가 swtch를 불렀던 곳으로 복귀
  ```
- **새 프로세스의 초기 스택**: `[proc_trampoline 주소][0 ×6]` 을 쌓아 두면, 첫 `swtch` 의 pop 6번 + `ret` 이 trampoline으로 들어간다
- **trampoline의 `sti`**: 전환은 타이머 인터럽트 안에서 일어난다(IF=0). 원래 흐름은 `iretq` 로 돌아가며 IF가 복원되지만,
  새 프로세스는 `ret` 으로 시작하므로 직접 인터럽트를 켜야 다음 타이머가 온다
- **EOI를 먼저 보내는 이유** (Step 03): 타이머 핸들러 안에서 `schedule()` 로 다른 프로세스로 가 버리면 핸들러가 한참 뒤에야 끝난다. EOI가 그 뒤에 있으면 그 동안 타이머가 멈춘다

## 직접 만든다면

1. PCB + `swtch.s` 작성
2. 협력형부터: 스레드가 직접 `schedule()` 을 부르는 버전으로 A/B 교대 확인
3. `proc_trampoline` + 초기 프레임 만들기 — 여기가 핵심, 그림을 그려 가며
4. idle 프로세스 등록, 원형 큐 연결
5. 타이머에서 `schedule()` 호출 → 선점형으로 전환, `sti` 없이 돌려서 멈추는 것도 관찰

## 확인

- A와 B가 덩어리로 번갈아 출력
- 생각해 볼 것: `proc_trampoline` 의 `sti` 를 지우면? (첫 전환 후 타이머가 안 와서 한 스레드만 계속 돈다)

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 첫 전환에서 General Protection / 이상한 rip | 초기 프레임의 push 순서가 swtch의 pop 순서와 안 맞음 |
| A만 계속 출력 | trampoline의 `sti` 누락, 또는 EOI가 schedule 뒤에 있음 |
| 몇 번 돌다 랜덤하게 죽음 | 스택 8KB 넘게 씀 (큰 지역 배열) → 다른 힙 블록 덮어씀 |

## 다음 단계로

여러 흐름을 돌릴 수 있게 됐으니, 그중 하나를 **사람과 대화하는 프로그램**(셸)으로 만든다 → [Step 09](step09-shell.md)
