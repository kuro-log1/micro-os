# Step 03 — PIC 리매핑 & PIT 타이머 (IRQ0)

> 하드웨어 인터럽트(IRQ 0~15)를 C 함수로 받는 틀을 만들고, 1초에 100번 울리는 타이머를 단다.

## 목표

1초마다 `[timer] N s` 가 출력된다.

## 이 단계가 만드는 틀

- **`irq_install_handler(irq, fn)`** — 드라이버가 "IRQ n이 오면 이 함수를 불러 줘"라고 등록하는 API.
  Step 04 키보드(IRQ1) · 마우스(IRQ12)가 이걸로 붙는다
- **벡터 배치 약속** — 0~31: CPU 예외, **32~47: IRQ 0~15**, 0x80: 시스템 콜(Step 11)
- **타이머 = 커널의 심장 박동** — 지금은 초를 세지만, Step 08에서 이 자리가 **스케줄러 호출**로 바뀐다 (선점형 멀티태스킹의 근거)
- **EOI를 핸들러 호출 *전에* 보내는 구조** — Step 08에서 타이머 핸들러가 다른 프로세스로 넘어가 버려도 PIC가 막히지 않게

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/pic.c` | 8259 PIC 초기화(ICW1~4)와 리매핑, `pic_send_eoi` |
| `kernel/idt_asm.s` | `IRQ_STUB` 매크로로 `irq0`~`irq15` (벡터 32~47), `irq_common` |
| `kernel/idt.c` | `irq_init` (게이트 등록), `irq_handler` (EOI → 등록된 함수 호출), `irq_install_handler` |
| `kernel/timer.c` | PIT 채널 0을 100Hz로, 틱 카운트 |
| `kernel/main.c` | `pic_init` → `irq_init` → `timer_init(100)` → `sti` |

읽는 순서: `kernel/pic.c` → `kernel/idt_asm.s`(IRQ_STUB) → `kernel/idt.c`(irq_handler) → `kernel/timer.c`

## 핵심 개념

- **왜 리매핑?** PIC의 기본 벡터는 IRQ0~7 → 8~15번인데, 이건 CPU 예외(8번 Double Fault 등)와 겹친다.
  ICW2로 마스터를 `0x20`(32), 슬레이브를 `0x28`(40)부터 쓰게 옮긴다
- **마스터/슬레이브**: PIC 두 개가 연결(cascade)돼 있다. 슬레이브는 마스터의 IRQ2에 물림. 그래서 IRQ 8~15의 EOI는 **둘 다**에 보낸다
- **EOI (End Of Interrupt)**: 처리했다고 알려야 PIC가 다음 인터럽트를 보낸다. 안 보내면 그 IRQ는 다시 오지 않는다
- **PIT**: 1,193,182Hz로 도는 카운터. `divisor = 1193182 / hz` 를 포트 `0x40` 에, 모드(`0x36`)를 `0x43` 에 쓴다
- **`sti`**: 여기까지 준비가 끝난 뒤에 인터럽트를 켠다. IDT/PIC 준비 전에 켜면 바로 터진다

## 직접 만든다면

1. `pic_init` 작성 (ICW1~4, 마스크 0 = 전부 허용)
2. IRQ 스텁을 예외 스텁과 같은 모양으로 — `int_no` 에 32~47을 넣는다
3. `irq_handler`: `irq = int_no - 32`, EOI, 등록 함수 호출
4. 타이머 핸들러에서 `ticks++`, 100번마다 출력
5. `main.c` 에서 순서대로 초기화 후 `sti`, 메인 루프는 `hlt`

## 확인

- `[timer] 1 s`, `[timer] 2 s` … 가 실제 시간과 비슷한 속도로 나온다

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `sti` 직후 Double Fault/패닉 | 리매핑 안 함 → 타이머가 8번(Double Fault)으로 들어옴 |
| 첫 틱만 오고 멈춤 | EOI 안 보냄 |
| 키보드 등 다른 IRQ에서 General Protection | 등록 안 된 IRQ인데 게이트가 비어 있음 → `irq_init` 이 16개 전부 등록하는지 확인 |

## 다음 단계로

인터럽트 틀이 생겼으니 입력 장치를 붙인다. 키보드 IRQ 핸들러와, 프로그램이 글자를 꺼내 가는 버퍼 → [Step 04](step04-keyboard.md)
