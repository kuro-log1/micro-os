# micro-os 학습 가이드

x86-64 커널을 **23단계**로 나눠 처음부터 쌓아 올리는 과정을 정리한 문서입니다.
각 단계는 `learn` 브랜치의 태그(`step01-boot` … `step23-shell-spawn`) 하나와 1:1로 대응하고,
**코드처럼 문서도 단계마다 하나씩 덧붙습니다.** `step05-pmm` 에는 Step 01~05 문서가, `step23` 에는 전부 들어 있습니다.

> 각 문서 끝의 "다음 단계로" 링크는 다음 태그에서 생깁니다. 미리 보고 싶으면 `git show learn:docs/step06-vmm.md`

## 공부하는 법

1. 문서를 먼저 읽는다 — "이 단계에서 무엇을 만들고, 어떤 틀을 깔아 두는지"
2. 그 단계로 이동해서 코드를 읽는다 — 문서의 **만들 파일** 표 순서대로
3. 빌드하고 실행해서 **확인** 항목이 화면에 나오는지 본다
4. 여유가 있으면 이전 단계 태그에서 **직접 만든다면** 순서대로 직접 구현해 보고, 다음 태그와 비교한다

```sh
os-learn                                   # 메뉴로 단계 고르기 · 빌드 · QEMU 실행 (dotfiles 의 bin/os-learn)

# 손으로 할 때
git checkout step05-pmm
git diff step04-keyboard step05-pmm        # 이번 단계에서 바뀐 코드만
make CC=gcc LD=ld AS=as MKRSC=grub-mkrescue
qemu-system-x86_64 -cdrom micro.iso -m 512M -serial stdio
git checkout learn                         # 최신으로 돌아오기
```

> Makefile은 macOS용(`x86_64-elf-gcc`, `i686-elf-grub-mkrescue`, `-display cocoa`)이라 Linux에서는 위처럼 도구 이름을 바꿔서 부릅니다.
> Step 16부터는 디스크 이미지, Step 18부터는 유저 프로그램 빌드도 필요해서 `os-learn` 을 쓰는 게 편합니다.

## 문서 구성 (모든 단계 공통)

| 절 | 내용 |
|---|---|
| **목표** | 이 단계가 끝나면 무엇이 되는가 |
| **이 단계가 만드는 틀** | 뒤 단계들이 기대게 되는 뼈대 · 약속 (가장 중요) |
| **만들 파일** | 새로 생기거나 바뀌는 파일과 역할, 읽는 순서 |
| **핵심 개념** | 코드를 이해하는 데 필요한 하드웨어 · OS 지식 |
| **직접 만든다면** | 이전 단계에서 출발해 이 단계를 구현하는 순서 |
| **확인** | QEMU에서 눈으로 볼 결과 |
| **자주 막히는 곳** | 증상 → 원인 |
| **다음 단계로** | 이 단계의 한계, 다음 단계가 왜 필요한지 |

## 전체 지도

```
Part 1. 커널 기초 — "부팅해서 여러 일을 번갈아 할 수 있는 커널"
  01 부팅 & 화면 출력 ─ 02 IDT & 예외 ─ 03 PIC & 타이머 ─ 04 키보드 & 마우스
  05 물리 메모리(PMM) ─ 06 페이징(VMM) ─ 07 힙(kmalloc)
  08 프로세스 & 컨텍스트 스위칭 ─ 09 셸 & 인메모리 FS

Part 2. 유저 모드 — "커널과 분리된 ring 3 코드 실행"
  10 GDT 유저 세그먼트 & TSS ─ 11 int 0x80 게이트 ─ 12 유저 메모리 매핑
  13 부팅 로그 강화 ─ 14 ring 3 진입 & write/exit

Part 3. 디스크와 프로그램 — "디스크의 실행 파일을 읽어 실행"
  15 PCI 스캔 ─ 16 ATA 디스크 ─ 17 ext2 파일시스템 ─ 18 ELF 로더 & /init

Part 4. 유닉스 프로세스 — "fork · exec · wait 로 프로그램을 띄우는 셸"
  19 파일 디스크립터 ─ 20 fork ─ 21 exec ─ 22 wait/exit ─ 23 셸에서 실행
```

| # | 문서 | 한 줄 요약 |
|---|---|---|
| 01 | [step01-boot](step01-boot.md) | GRUB → 32비트 → 64비트 롱 모드 → `kernel_main`, 화면/시리얼 출력 |
| 02 | [step02-idt](step02-idt.md) | CPU 예외가 나면 레지스터를 덤프하고 멈추는 틀 |
| 03 | [step03-pic-timer](step03-pic-timer.md) | 하드웨어 인터럽트(IRQ)를 C 함수로 받는 틀 + 100Hz 타이머 |
| 04 | [step04-keyboard](step04-keyboard.md) | 키보드 링 버퍼 + `keyboard_getchar()`, 마우스 커서 |
| 05 | [step05-pmm](step05-pmm.md) | 4KB 물리 프레임을 빌려 주고 돌려받는 할당기 |
| 06 | [step06-vmm](step06-vmm.md) | 가상 주소 ↔ 물리 프레임 매핑 (`vmm_map_page`) |
| 07 | [step07-heap](step07-heap.md) | 크기가 제각각인 메모리를 주는 `kmalloc` / `kfree` |
| 08 | [step08-process](step08-process.md) | PCB + `swtch` 로 커널 스레드를 번갈아 실행 |
| 09 | [step09-shell](step09-shell.md) | 명령을 입력받는 셸, 메모리 위의 디렉터리 트리 |
| 10 | [step10-gdt-tss](step10-gdt-tss.md) | ring 3 세그먼트, ring 3 → 0 전환 때 쓸 커널 스택(TSS) |
| 11 | [step11-syscall-gate](step11-syscall-gate.md) | 유저가 커널을 부르는 문 `int 0x80` |
| 12 | [step12-user-mapping](step12-user-mapping.md) | 유저가 접근할 수 있는 페이지 (U/S 비트) |
| 13 | [step13-bootlog](step13-bootlog.md) | 타임스탬프 로그(`klog`) · 시리얼 디버그(`kprintf`) |
| 14 | [step14-ring3](step14-ring3.md) | `iretq` 로 ring 3 진입, 첫 시스템 콜 write/exit |
| 15 | [step15-pci](step15-pci.md) | PCI 설정 공간 읽기, 장치 목록 |
| 16 | [step16-ata](step16-ata.md) | 디스크 섹터 읽기/쓰기 (ATA PIO) |
| 17 | [step17-ext2](step17-ext2.md) | 섹터 위에 파일 · 디렉터리 해석 (ext2) |
| 18 | [step18-elf-init](step18-elf-init.md) | 디스크의 C 프로그램(ELF)을 메모리에 올려 실행 |
| 19 | [step19-fd](step19-fd.md) | open/read/write/close — 파일 디스크립터 |
| 20 | [step20-fork](step20-fork.md) | 주소 공간을 복제해 자식 프로세스 만들기 |
| 21 | [step21-exec](step21-exec.md) | 내 주소 공간을 새 프로그램으로 갈아 끼우기 |
| 22 | [step22-wait-exit](step22-wait-exit.md) | 종료 코드 전달, 좀비 회수, 프로세스별 CR3 |
| 23 | [step23-shell-spawn](step23-shell-spawn.md) | 셸에서 디스크의 프로그램 실행 |

## 미리 알면 좋은 것

- **C 포인터와 비트 연산** — 페이지 테이블 · 디스크립터는 전부 비트 필드
- **x86-64 어셈블리 읽기 (AT&T 문법)** — `mov %rax, %rbx` 는 rax → rbx. 직접 짤 일은 적고 읽을 수 있으면 충분
- **System V AMD64 호출 규약** — 인자 `rdi, rsi, rdx, rcx, r8, r9`, 반환 `rax`, callee-saved `rbx, rbp, r12–r15`
- 참고 자료: [OSDev Wiki](https://wiki.osdev.org), Intel SDM Vol. 3, xv6 (MIT 6.S081) 교재
