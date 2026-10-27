# Step 22 — wait / exit & 좀비 회수

> exit은 fd를 닫고 종료 코드를 남긴 채 좀비가 되고, 부모의 wait이 그 코드를 받은 뒤 자식의 모든 자원을 해제한다. 스케줄러는 이제 CR3와 TSS.rsp0도 바꿔 끼운다.

## 목표

`[parent] waiting for child pid=N` → 자식의 hello 출력 → `[parent] child exited, done!`. 시리얼에 `[wait] zombie pid=N reaped`.

## 이 단계가 만드는 틀

- **프로세스 생애 주기 완성**
  ```
  생성(fork/create) → READY ⇄ RUNNING → exit → DEAD(좀비) → 부모 wait → 해제
  ```
- **스케줄러의 전환 책임 3가지** — 이후 바뀌지 않는 최종 형태
  1. `swtch` (레지스터 · 스택)
  2. **CR3** — 다음 프로세스의 pml4가 다르면 교체
  3. **TSS.rsp0** — 다음 프로세스의 커널 스택 꼭대기로
- **`scheduler_find(pid)` / `scheduler_remove(p)`** — 큐에서 프로세스 찾기 · 빼기. 셸(23)도 사용
- **자원 정리 순서** — 큐에서 제거 → 유저 페이지 → PML4 프레임 → 커널 스택 → PCB

## 만들 파일

| 파일 | 역할 |
|---|---|
| `proc/scheduler.c` / `.h` | `scheduler_find`, `scheduler_remove`, `schedule()` 에 CR3 · rsp0 교체 |
| `proc/process.h` | `STACK_SIZE` 를 헤더로 (스케줄러가 스택 꼭대기 계산) |
| `kernel/syscall.c` / `.h` | `SYS_WAIT(61)`, `sys_wait`, `sys_exit` 에 fd 닫기 · `exit_code` 저장 |
| `user/init.c` | fork → exec → wait 전체 흐름 |

원본 커밋: `9e34ae8` · 읽는 순서: `proc/scheduler.c` → `kernel/syscall.c`(sys_exit, sys_wait) → `user/init.c`

## 핵심 개념

- **좀비가 필요한 이유** — exit 순간 자식은 아직 **자기 커널 스택 위에서** 돌고 있다. 스스로 스택을 해제하면 발 밑을 지우는 셈.
  그래서 DEAD 표시만 하고 떠나고, 다른 프로세스(부모)가 나중에 치운다. 종료 코드를 전달할 곳도 그 PCB다
- **wait = 양보하며 기다리기** — `while (child->state != PROC_DEAD) schedule();` 단순한 방식(바쁜 대기). 실제 OS는 부모를 재워 두고 자식이 깨운다
- **왜 이제 rsp0를 바꿔야 하나?** 유저 프로세스가 여럿이면, 고정된 rsp0 하나를 같이 쓰다가 A가 커널 안에서 잠든 사이 B가 같은 스택에 인터럽트 프레임을 쌓아 A의 스택을 부순다. 프로세스마다 자기 커널 스택을 rsp0로
- **왜 이제 CR3를?** fork(20)로 pml4가 갈라졌으니, 자식으로 전환할 때 자식의 주소 공간이 보여야 한다. 커널 영역은 공유하므로 CR3를 바꿔도 커널 코드는 그대로 돈다
- **exit에서 fd 닫기** — `refcount--`, 0이 되면 `file_t` 반환 (Step 19의 설계)

## 직접 만든다면

1. `schedule()` 에 rsp0 · CR3 교체 (fork만 있는 상태에서도 이게 있어야 안정적)
2. `sys_exit`: fd 닫기, `exit_code`, DEAD, `schedule()`
3. `scheduler_find` / `scheduler_remove`
4. `sys_wait`: 찾기 → DEAD까지 양보 → 코드 꺼내기 → 정리 → 반환
5. init에서 fork → 자식 exec → 부모 wait

## 확인

- 부모 메시지가 자식 출력 **뒤에** 나온다
- 생각해 볼 것: 자식이 스스로 메모리를 해제하지 않고 부모가 wait에서 해제하는 이유는? (위 "좀비가 필요한 이유")

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 유저 프로세스 두 개부터 랜덤 크래시 | rsp0 교체 누락 — 커널 스택 공유 |
| 자식으로 전환하자 Page Fault | CR3 교체 누락 / 자식 pml4에 커널 영역 없음 |
| wait 후 다음 fork에서 이상 | PML4를 해제했는데 그게 커널 PML4였음 (`!= vmm_get_pml4()` 검사) |
| wait이 영원히 안 끝남 | exit에서 DEAD 표시 누락 / scheduler가 DEAD를 건너뛰어 부모도 못 고름 |

## 다음 단계로

유닉스 프로세스 모델이 갖춰졌다. 마지막으로 이걸 **셸**에 연결해 사용자가 직접 디스크의 프로그램을 실행하게 한다 → [Step 23](step23-shell-spawn.md)
