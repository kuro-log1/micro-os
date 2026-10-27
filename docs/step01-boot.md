# Step 01 — 부팅 & 화면 출력

> GRUB이 커널을 메모리에 올려 주면, 32비트에서 64비트 롱 모드로 넘어가 C 함수 `kernel_main` 을 부르고 글자를 찍는다.

## 목표

QEMU를 켜면 GRUB 메뉴 → 화면과 시리얼(터미널)에 `micro-os booted!` 가 나온다.

## 이 단계가 만드는 틀

- **메모리 배치 약속** — 커널은 물리 1MB(`0x100000`)부터 놓이고, 끝 주소는 `_kernel_end` 심볼로 알 수 있다 (Step 05 PMM이 사용)
- **0~1GB 아이덴티티 매핑** — 가상 주소 = 물리 주소. 그래서 앞으로 물리 주소를 그대로 포인터로 써도 된다 (Step 05~06의 전제)
- **64비트 GDT** — 커널 코드 세그먼트 `0x08`, 데이터 `0x10`. Step 02의 IDT 게이트가 `0x08` 을 쓰고, Step 10에서 유저 세그먼트를 덧붙인다
- **`kernel_main(magic, info)`** — 모든 초기화가 이 함수에 한 줄씩 늘어난다. `info` 는 Step 05에서 메모리 맵을 읽을 때 쓴다
- **`printf`** — 이후 모든 디버깅의 기반. 출력은 VGA + 시리얼 두 곳

## 만들 파일

| 파일 | 역할 |
|---|---|
| `linker.ld` | 섹션을 1MB부터 4KB 정렬로 배치, `_kernel_end` 정의. multiboot2 헤더를 `.text` 맨 앞에 |
| `boot/boot.s` | multiboot2 헤더, 페이지 테이블, 롱 모드 전환, `kernel_main` 호출 |
| `grub.cfg` | GRUB 메뉴: `multiboot2 /boot/kernel.elf` |
| `Makefile` | 크로스 컴파일 → 링크 → `grub-mkrescue` 로 ISO |
| `kernel/main.c` | `kernel_main`: 출력 장치 초기화 후 메시지 |
| `drivers/vga.c` | `0xB8000` 텍스트 버퍼에 글자 쓰기, 줄바꿈 · 백스페이스 · 스크롤 |
| `drivers/serial.c` | COM1(`0x3F8`) 115200bps 초기화, 한 글자씩 송신 |
| `kernel/io.h` | `outb` / `inb` 포트 I/O 인라인 함수 |
| `lib/string.c` | `memset` `memcpy` `strlen` `strcmp` — 표준 라이브러리가 없으니 직접 |
| `lib/printf.c` | `%d %u %x %s %c %p` (+ `l` 접두사) 만 지원하는 작은 printf |

읽는 순서: `linker.ld` → `boot/boot.s` → `kernel/main.c` → `drivers/vga.c` · `serial.c` → `lib/printf.c`

## 핵심 개념

**부팅 흐름**
```
BIOS → GRUB (multiboot2 헤더 찾음, 커널 ELF 로드)
     → _start (32비트 보호 모드, 페이징 꺼짐, eax=magic, ebx=정보 구조체 주소)
     → 페이지 테이블 만들기 → PAE 켜기 → CR3 → EFER.LME → CR0.PG
     → lgdt (64비트 GDT) → ljmp 0x08:start64
     → start64 (64비트) → 세그먼트 레지스터 0x10 → 스택 → call kernel_main
```

- **multiboot2 헤더**: 매직 `0xE85250D6`, 아키텍처, 길이, 체크섬(넷의 합이 0), 종료 태그. GRUB은 이걸 보고 "부팅할 수 있는 커널"로 인식
- **롱 모드는 페이징이 필수**: 64비트로 가려면 페이지 테이블이 먼저 있어야 한다. 그래서 어셈블리에서 PML4 → PDPT → PD 3단을 만들고, PD 512칸에 **2MB 대형 페이지**(`0x83` = Present|Writable|PS)를 깔아 1GB를 한 번에 매핑
- **EFER MSR(`0xC0000080`)의 LME 비트(8번)** 를 켜고 페이징을 켜는 순간 롱 모드(호환 모드)가 되고, 64비트 코드 세그먼트로 far jump 해야 진짜 64비트
- **GRUB이 준 값 보존**: `eax`/`ebx` 는 곧 덮어쓰니 맨 처음에 메모리에 저장, 64비트에서 `rdi`/`rsi` 로 넘김 (호출 규약)
- **VGA 텍스트 모드**: 80×25 칸, 칸마다 2바이트 = `(색 << 8) | 글자`
- **시리얼**: QEMU `-serial stdio` 로 터미널에 그대로 나온다. 화면이 지워지거나 커널이 죽어도 로그가 남아 디버깅에 필수
- **컴파일 옵션**: `-ffreestanding`(표준 라이브러리 없음), `-mno-red-zone`(인터럽트가 스택 아래 128바이트를 덮어쓸 수 있어서), `-mno-sse` 등(커널이 FPU/SSE 상태를 저장하지 않으니 컴파일러가 쓰지 않게)

## 직접 만든다면

1. `linker.ld` 와 multiboot2 헤더만 있는 `boot.s` 를 만들고, `_start` 에서 `hlt` 무한 루프 → GRUB이 커널을 인식하는지 확인
2. 32비트에서 VGA `0xB8000` 에 글자 하나 직접 써 보기 (여기까지 되면 부팅 경로는 OK)
3. 페이지 테이블 3단 + 1GB 아이덴티티 매핑
4. PAE → CR3 → EFER.LME → CR0.PG → GDT → far jump 순서로 64비트 진입
5. 64비트에서 스택을 잡고 `call kernel_main` (C 코드 시작)
6. `vga_putchar` → `serial_putchar` → `printf` 순으로 출력 함수 쌓기

## 확인

- QEMU 화면과 터미널에 `micro-os booted!`
- 터미널에도 나온다면 시리얼까지 성공

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| GRUB이 "no multiboot header found" | 헤더가 파일 앞 32KB 안에 없음 / 8바이트 정렬 안 됨 / 체크섬 틀림 |
| 화면이 계속 재부팅됨 (트리플 폴트) | 페이지 테이블 비트, GDT 디스크립터, far jump 대상 중 하나가 틀림. `qemu ... -d int -no-reboot` 로 확인 |
| C 함수에서 이상한 값 | 64비트 스택을 안 잡음 / 인자를 `rdi`, `rsi` 로 안 넘김 |
| 링크 에러 `undefined reference to memset` | 컴파일러가 구조체 초기화에 `memset` 을 부름 → `lib/string.c` 필요 |

## 다음 단계로

지금은 CPU가 예외(0으로 나누기, 잘못된 메모리 접근)를 만나면 처리할 곳이 없어 **그냥 재부팅**된다.
무슨 일이 났는지 보려면 예외를 받는 틀(IDT)이 먼저 있어야 한다 → [Step 02](step02-idt.md)
