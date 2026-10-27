# Step 11 — int 0x80 시스템 콜 게이트

> IDT 0x80번에 ring 3도 부를 수 있는 게이트를 등록하고, `syscall_handler(registers_t*)` 로 받는 틀을 만든다.

## 목표

커널 안에서 `rax=0x42; int $0x80` 을 실행하면 `[syscall] rax=66 (unimplemented)` 가 출력된다.

## 이 단계가 만드는 틀

- **시스템 콜 호출 규약** (Linux x86-64와 같은 레지스터 배치)
  | 레지스터 | 의미 |
  |---|---|
  | `rax` | 시스템 콜 번호 / 반환값 |
  | `rdi`, `rsi`, `rdx` | 인자 1, 2, 3 |
- **반환값 = `r->rax` 에 쓰기** — 핸들러가 저장된 프레임의 rax를 바꾸면, 스텁이 pop할 때 그 값이 유저의 rax가 된다 (Step 19부터 사용)
- **`syscall_handler` 의 switch문** — 이후 시스템 콜은 전부 여기에 case 하나씩: write/exit(14) → read/open/close/lseek(19) → fork(20) → exec(21) → wait(22)

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/idt_asm.s` | `isr128` 스텁 + `syscall_common` (예외/IRQ 스텁과 같은 모양) |
| `kernel/idt.c` | `idt_set_user_gate` — 타입 `0xEE` (DPL 3) |
| `kernel/syscall.c` | `syscall_init` (게이트 등록), `syscall_handler` |
| `kernel/main.c` | `syscall_init()` + 커널 내 `int $0x80` 테스트 |

원본 커밋: `394acdb` · 읽는 순서: `kernel/idt_asm.s`(isr128) → `kernel/idt.c` → `kernel/syscall.c`

## 핵심 개념

- **게이트의 DPL** — `int n` 명령은 "현재 링 ≤ 게이트 DPL" 일 때만 허용된다. 예외/IRQ 게이트는 DPL 0(`0x8E`)이라 ring 3이 `int 32` 를 하면 #GP.
  0x80만 DPL 3(`0xEE`)으로 열어 두면 **유저가 들어올 수 있는 문은 이것 하나**가 된다
- **IRQ와의 차이** — PIC가 아니라 프로그램이 일으킨 인터럽트라 EOI가 필요 없다
- **왜 `syscall` 명령이 아니라 `int 0x80`?** 기존 IDT 틀을 그대로 쓸 수 있어 단순하다. `syscall`/`sysret` 은 MSR 설정과 별도 진입 코드가 필요 (빠르지만 복잡)

## 직접 만든다면

1. `idt_set_gate` 를 복사해 타입만 `0xEE` 인 `idt_set_user_gate`
2. `isr128` 스텁 (`int_no = 128`, 가짜 err_code)
3. `syscall_handler` 에서 rax 출력
4. ring 0에서 `int $0x80` 테스트 (ring 3 테스트는 Step 14)

## 확인

- 부팅 로그에 `[syscall] int 0x80 gate registered (DPL=3)` 와 `[syscall] rax=66 (unimplemented)`

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `int $0x80` 에서 General Protection | 게이트 DPL이 0 (Step 14에서 ring 3일 때 드러남) |
| 반환 후 레지스터가 엉망 | 스텁의 push/pop 순서 불일치 |

## 다음 단계로

문은 열었지만, 유저 코드가 놓일 **메모리**도 유저가 접근 가능해야 한다. 페이지 테이블의 U/S 비트를 다룬다 → [Step 12](step12-user-mapping.md)
