# Step 02 — IDT & CPU 예외 처리

> CPU 예외(0~31번)가 나면 재부팅 대신 레지스터를 화면에 덤프하고 멈춘다.

## 목표

0으로 나누면 `*** KERNEL PANIC: Divide by Zero ***` 와 레지스터 값이 나온다.

## 이 단계가 만드는 틀

- **`registers_t`** — 인터럽트 순간의 레지스터를 담는 구조체. 어셈블리 스텁이 push한 순서와 **필드 순서가 1:1** 이어야 한다.
  이 구조체가 이후 IRQ(03), 시스템 콜(11), fork(20)까지 **모든 커널 진입의 공통 형식**이 된다
- **"어셈블리 스텁 → 공통 루틴 → C 핸들러" 패턴** — 03의 IRQ, 11의 시스템 콜이 똑같은 모양으로 붙는다
- **`idt_set_gate(n, fn)`** — IDT에 칸을 채우는 함수. 뒤에서 IRQ와 `0x80` 게이트도 이걸로 등록
- **`panic()`** — "여기 오면 끝" 지점. 이후 단계의 디버깅 출발점

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/idt.h` | `registers_t` 레이아웃, `idt_init` 선언 |
| `kernel/idt_asm.s` | `isr0`~`isr31` 스텁 (매크로 `ISR_NOERR` / `ISR_ERR`), `isr_common` |
| `kernel/idt.c` | 16바이트 게이트 구조체, `idt_set_gate`, `lidt`, `exception_handler` |
| `kernel/panic.c` | 레지스터 덤프 후 `cli; hlt` |
| `kernel/main.c` | `idt_init()` 호출, 0으로 나누기 실습 (주석) |

읽는 순서: `kernel/idt.h` → `kernel/idt_asm.s` → `kernel/idt.c` → `kernel/panic.c`

## 핵심 개념

- **IDT** = 인터럽트 번호(0~255)마다 "어디로 점프할지" 적은 표. 64비트에서 한 칸은 16바이트(핸들러 주소 64비트를 셋으로 쪼개 저장 + 세그먼트 `0x08` + 타입 `0x8E`)
- `0x8E` = Present | DPL 0 | 64비트 인터럽트 게이트 (진입 시 IF가 꺼져서 중첩 인터럽트 방지)
- **에러 코드가 있는 예외와 없는 예외**: 8, 10~14, 17번은 CPU가 에러 코드를 하나 더 push한다.
  스택 모양을 통일하려고 없는 예외는 스텁에서 가짜 `0` 을 push → 모든 예외가 같은 `registers_t` 로 읽힌다
- **스택 모양** (위가 높은 주소):
  ```
  ss, rsp      ← (ring 3에서 왔을 때만 — Step 14부터)
  rflags, cs, rip   ← CPU가 자동 push
  err_code          ← CPU 또는 가짜 0
  int_no            ← 스텁이 push
  rax … r15         ← isr_common이 push
  ```
  마지막에 `mov %rsp, %rdi` 로 이 위치를 `registers_t*` 로 C 함수에 넘긴다
- **복귀는 `iretq`**: 레지스터를 pop하고 `int_no` · `err_code` 16바이트를 건너뛴 뒤 `iretq` (rip/cs/rflags 복원)

## 직접 만든다면

1. 게이트 구조체 · `idtr` · `lidt` 만 만들고 빈 IDT를 로드해 본다
2. 스텁 하나(`isr0`)와 `isr_common` 을 작성, C의 `exception_handler` 에서 번호만 출력
3. 레지스터 push 순서에 맞춰 `registers_t` 정의 — 일부러 순서를 틀리게 해서 값이 이상하게 나오는 걸 보면 이해가 빠르다
4. 매크로로 0~31번 전부 생성, 에러 코드 유무 구분
5. 예외 이름 표 + `panic()`

## 확인

- `kernel/main.c` 의 `/* volatile int zero = 0; ... */` 주석을 풀고 실행 → `KERNEL PANIC: Divide by Zero`, `int=0`, `rip=...`
- 주석을 되돌리면 정상 부팅

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 예외 대신 재부팅 | IDT 로드 실패, 게이트 셀렉터가 `0x08` 이 아님, 주소 쪼개기 틀림 |
| 레지스터 값이 한 칸씩 밀림 | push 순서와 `registers_t` 필드 순서 불일치, 에러 코드 유무 혼동 |
| `1 / zero` 가 예외 없이 지나감 | 컴파일러가 상수 계산으로 없앰 → `volatile` 필요 |

## 다음 단계로

CPU 내부 사건(예외)은 받을 수 있지만, **바깥 장치**(타이머, 키보드)가 보내는 신호는 아직 못 받는다.
같은 스텁 패턴으로 하드웨어 인터럽트를 받는다 → [Step 03](step03-pic-timer.md)
