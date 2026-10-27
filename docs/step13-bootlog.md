# Step 13 — 부팅 로그 강화 (klog, TSC 타임스탬프, CPUID, 메모리 맵)

> printf를 "출력 대상만 바꿔 끼우는" 구조로 다시 짜고, 시리얼 로그에 마이크로초 타임스탬프를 붙인다.

## 목표

시리얼(터미널) 부팅 로그가 `[   0.012345] [pmm] total: ...` 처럼 시각과 함께 나오고, CPU 이름과 메모리 맵이 출력된다.

## 이 단계가 만드는 틀

- **출력 함수 3종 — 이후 코드는 용도에 따라 골라 쓴다**
  | 함수 | 화면 | 시리얼 | 타임스탬프 | 쓰는 곳 |
  |---|:-:|:-:|:-:|---|
  | `printf` | ✔ | ✔ | | 셸 출력, 사용자에게 보이는 것 |
  | `klog` | ✔ | ✔ | 시리얼에만 | 부팅 단계 로그 (`[pmm]`, `[ata]` …) |
  | `kprintf` | | ✔ | ✔ | 디버그 — 화면을 어지럽히면 안 되는 것 (`[exec]`, `[wait]` …) |
- **`do_printf(emit_fn, fmt, ap)`** — 포맷 해석은 한 곳, 출력 함수만 갈아 끼우는 구조. `%02x` 같은 0 채우기 폭도 지원
- **`tsc_elapsed_us()`** — 부팅 후 경과 시간(μs). 성능 측정이나 타임아웃에 쓸 수 있는 시계

## 만들 파일

| 파일 | 역할 |
|---|---|
| `lib/printf.c` | `emit_fn` 기반 재작성, `klog` · `kprintf`, 타임스탬프 출력 |
| `kernel/timer.c` | `rdtsc`, `tsc_calibrate`(PIT 1틱 = 10ms 동안 TSC 증가량 측정), `tsc_elapsed_us` |
| `kernel/cpuid.c` | 벤더 문자열(`GenuineIntel`/`AuthenticAMD`)과 브랜드 문자열 |
| `kernel/mmap.c` | multiboot2 메모리 맵을 사람이 읽기 좋게 출력 |
| `drivers/vga.c` | 하드웨어 커서(깜빡이는 밑줄)를 글자 위치에 맞춤 (포트 `0x3D4/0x3D5`) |
| 여러 파일 | 기존 `printf` 로그를 `klog` / `kprintf` 로 교체 |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `lib/printf.c` → `kernel/timer.c` → `kernel/cpuid.c` → `kernel/mmap.c`

## 핵심 개념

- **TSC (Time Stamp Counter)**: CPU 클럭마다 1씩 오르는 64비트 카운터, `rdtsc` 로 읽는다. 매우 정밀하지만 "1초에 몇인지"는 CPU마다 다르다
- **보정(calibration)**: 주기를 아는 PIT 틱 경계에 맞춰 `rdtsc` 두 번 → 10ms 동안의 증가량 → `tsc_per_us`. 틱 0 시점의 TSC 값도 역산해서 "부팅 기준 시각"을 만든다
- **CPUID**: `eax=0` → 벤더 12글자(ebx, edx, ecx 순), `eax=0x80000002~4` → 브랜드 48글자
- **함수 포인터로 출력 분리**: 같은 포맷 엔진을 VGA+시리얼(`emit`)과 시리얼 전용(`semit`)에 재사용

## 직접 만든다면

1. 기존 printf를 `do_printf(emit_fn …)` 로 바꾸고 동작이 같은지 확인
2. `kprintf` (시리얼 전용) 추가
3. `rdtsc` 와 보정 — `sti` 이후에 불러야 틱이 오른다
4. 타임스탬프 출력, `klog`
5. CPUID · 메모리 맵 로그는 덤

## 확인

- 터미널(`-serial stdio`)에 `[    0.xxxxxx] [boot] micro-os starting...`, CPU 이름, 메모리 영역 목록
- 화면에는 타임스탬프 없이 같은 로그

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `tsc_calibrate` 에서 멈춤 | `sti` 전에 불러서 틱이 안 오름 |
| 타임스탬프가 전부 0 | 보정 전 로그 — fallback(`ticks × 10000`)이 아직 0 |

## 다음 단계로

로그 도구가 갖춰졌으니, 드디어 **ring 3으로 내려가 첫 유저 코드**를 실행한다 → [Step 14](step14-ring3.md)
