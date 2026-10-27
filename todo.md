# micro-os 구현 TODO

## Phase 1 — 부팅 & 화면 출력 ✅

- [x] `linker.ld` 작성 (메모리 레이아웃 정의)
- [x] `boot/boot.s` 작성 (GRUB multiboot2 헤더, 32bit → 64bit 전환, 페이지 테이블)
- [x] `Makefile` 작성 (x86_64-elf-gcc 크로스 컴파일러, ISO 빌드, QEMU 실행)
- [x] `drivers/vga.c` 작성 (텍스트 모드 화면 출력)
- [x] `drivers/serial.c` 작성 (COM1 시리얼 포트 출력 — QEMU 디버깅용)
- [x] `lib/string.c` 작성 (memset, memcpy, strlen)
- [x] `lib/printf.c` 작성 (커널용 printf — %d, %x, %s, %p)
- [x] `kernel/main.c` 작성 (kernel_main 진입, "Hello, Kernel!" 출력 확인)
- [x] QEMU 실행 후 화면/시리얼에 문자 출력 확인 ✓

## Phase 2 — 인터럽트 & 예외 처리 ✅

- [x] `kernel/idt.c` 작성 (IDT 구조체, `lidt` 로드)
- [x] `kernel/idt_asm.s` 작성 (예외/인터럽트 스텁 — 레지스터 저장/복원)
- [x] CPU 예외 핸들러 등록 (0~31번: divide-by-zero, page fault 등)
- [x] `kernel/panic.c` 작성 (레지스터 덤프 후 hlt)
- [x] QEMU에서 의도적으로 divide-by-zero 발생시켜 예외 핸들러 확인 ✓

### 구현 노트

- GDT는 `boot/boot.s`에 인라인으로 정의 (null / 64-bit code 0x08 / data 0x10)
- `ISR_NOERR` / `ISR_ERR` 매크로로 32개 스텁 생성, 공통 경로 `isr_common`에서 레지스터 저장 후 `exception_handler` 호출
- `registers_t` 구조체가 `isr_common`의 push 순서와 1:1 매핑 (r15→rax→int_no→err_code→rip/cs/rflags)
- **주의**: Makefile에 `-mno-sse -mno-sse2 -mno-mmx -mno-avx -mno-red-zone` 필수
  - `-O2`에서 GCC가 SSE 명령어 생성 → CR4.OSFXSR 미설정으로 `#UD` → triple fault
- **주의**: C 코드로 divide-by-zero 테스트 시 `-O2`가 UB로 판단해 나눗셈 코드 제거
  - 인라인 어셈블리 `__asm__ volatile("xorl %%eax, %%eax\n\tdivl %%eax" ::: "eax", "edx")` 사용

## Phase 3 — 하드웨어 인터럽트 ✅

- [x] `kernel/pic.c` 작성 (8259A PIC 초기화, IRQ 리매핑 0x20~0x2F)
- [x] `kernel/timer.c` 작성 (PIT IRQ0, 100Hz 타이머 인터럽트)
- [x] `drivers/keyboard.c` 작성 (IRQ1 키보드, 스캔코드 → ASCII 변환)
- [x] 타이머 tick 카운터 증가 확인, 키 입력 시리얼 출력 확인 ✓

### 구현 목적

Phase 2의 예외(Exception)는 CPU가 스스로 발생시키는 신호였다면,
Phase 3의 인터럽트(IRQ)는 **외부 하드웨어가 CPU에 비동기적으로 보내는 신호**다.

- **타이머 인터럽트**: Phase 7 스케줄러의 핵심 동력. "매 N ms마다 CPU를 강제로 빼앗아 다음 프로세스로 넘긴다"는 선점형 멀티태스킹의 본질.
- **키보드 인터럽트**: 키 입력을 폴링(무한루프로 포트 계속 읽기) 없이 처리. CPU가 다른 일을 하다가 입력이 오면 그때만 반응.

인터럽트가 없으면 OS가 CPU 제어권을 자발적으로 양보하지 않는 프로세스를 멈출 방법이 없다.

### 구현 노트

- **PIC 리매핑 필수**: 8259A PIC 기본값은 IRQ0-7이 벡터 0x08-0x0F → CPU 예외(0-31번)와 충돌. ICW1-4 초기화로 IRQ0-7 → 0x20-0x27, IRQ8-15 → 0x28-0x2F 로 이동.
- **EOI(End of Interrupt) 필수**: IRQ 핸들러가 끝날 때 `outb(0x20, 0x20)` 으로 PIC에 처리 완료를 알려야 다음 IRQ를 받을 수 있음. 슬레이브 IRQ(8-15)는 마스터(0x20)와 슬레이브(0xA0) 양쪽에 전송.
- **`sti` 타이밍**: `pic_init()` + `irq_init()` 이후에 호출. 그 전에 `sti`하면 리매핑되지 않은 PIC가 예외 벡터로 IRQ를 보내 triple fault 가능.
- **`hlt` 메인루프**: `for(;;) hlt` — 인터럽트가 올 때까지 CPU를 절전 상태로 대기. 바쁜 루프(`for(;;){}`) 대신 사용.
- **스캔코드 세트 1**: PS/2 키보드는 make code(키 누름)와 break code(키 뗌, 0x80 OR) 를 보냄. `sc & 0x80` 체크로 key release 무시.
- **PIT 주파수 계산**: PIT 기준 클럭 1,193,182 Hz / divisor = 원하는 Hz. 100Hz → divisor = 11932 (약 10ms마다 IRQ0 발생).

## Phase 4 — 물리 메모리 관리 ✅

- [x] multiboot2 메모리 맵 파싱 (사용 가능한 물리 메모리 영역 추출)
- [x] `mm/pmm.c` 작성 (페이지 프레임 할당자 — 비트맵 방식)
- [x] `pmm_alloc_page()` / `pmm_free_page()` 구현
- [x] 할당/해제 테스트 출력 확인 ✓

### 구현 목적
지금까지 커널은 메모리를 전혀 관리하지 않았다. 스택은 boot.s에서 고정 16KB, 그 외 동적 할당은 불가능한 상태.

앞으로 만들 모든 것이 메모리를 필요로 한다:
- **Phase 5 (페이징)**: 새 페이지 테이블 만들 때 4KB 페이지 필요
- **Phase 6 (kmalloc)**: 힙으로 쓸 페이지 필요
- **Phase 7 (프로세스)**: 프로세스마다 스택/코드 페이지 필요

PMM은 이 모든 것의 기반. "어느 물리 주소가 비어있는가"를 추적하는 가장 낮은 레벨의 메모리 관리자.

### 구현 노트
- **multiboot2 파싱**: `kernel_main(magic, info)`의 `info`는 GRUB이 넘긴 구조체 물리 주소. 8바이트 헤더 다음에 태그들이 붙고, 각 태그는 8바이트 정렬. type=6 태그가 메모리 맵.
- **비트맵 128KB**: 4GB / 4KB = 1M 페이지 → 1M bits = 128KB. BSS에 정적 배열로 선언 (런타임 전 0으로 초기화됨 = 전부 "사용 중" 상태에서 시작).
- **커널 페이지 보호**: `_kernel_end` 심볼(linker.ld에서 정의)보다 낮은 페이지는 free 표시 안 함. 커널 코드/데이터/BSS(비트맵 포함)를 덮어쓰는 것 방지.
- **`_kernel_end` 추가**: linker.ld `.bss` 끝에 `. = ALIGN(4K); _kernel_end = .;` 추가 필요.
- **Makefile**: `find` 경로에 `mm` 디렉토리 추가 필요.

## Phase 5 — 가상 메모리 & 페이징 ✅

- [x] `mm/vmm.c` 작성 (PML4 → PDPT → PD → PT 4단계 페이지 테이블)
- [x] `vmm_map_page()` / `vmm_unmap_page()` 구현
- [x] Page Fault 핸들러 연결 (CR2 주소 + 오류 유형 출력)
- [ ] 커널 고위 주소 매핑 (Higher Half Kernel — 0xFFFFFFFF80000000) ← Phase 7 전 선택
- [x] 매핑 후 페이지 접근 테스트 확인 ✓

### 구현 목적
Phase 4가 "어느 물리 메모리가 비어있나"를 관리한다면, Phase 5는 "그 물리 메모리를 어떤 가상 주소로 보여줄 것인가"를 관리한다.

- **프로세스 격리**: 프로세스마다 독립된 PML4를 가지면, 같은 가상 주소 `0x5000`이 프로세스별로 다른 물리 메모리를 가리킨다. CR3 레지스터 교체 한 줄로 CPU의 "현재 보고 있는 세계"가 바뀐다.
- **Phase 7 선행 조건**: 프로세스 생성 시 새 주소 공간(PML4)을 만들고, 스택/코드 페이지를 매핑하는 데 VMM이 사용된다.

### 구현 노트
- **4단계 탐색**: 가상 주소 비트를 `[47:39] PML4 → [38:30] PDPT → [29:21] PD → [20:12] PT → [11:0] offset` 으로 분해. 각 레벨이 없으면 `pmm_alloc_page()`로 새 페이지 테이블 페이지 할당.
- **identity mapping 활용**: PMM이 반환하는 물리 주소는 모두 1GB 이내 → boot.s identity map 안에 있어 포인터로 직접 접근 가능. `물리 주소 = 가상 주소`이므로 별도 변환 불필요.
- **2MB 대형 페이지 충돌**: boot.s가 0~1GB를 2MB 페이지로 매핑. 그 범위 안에 4KB 매핑 시도 시 PS 비트 감지해서 거절. 테스트는 1GB 밖 `0x200000000` (8GB) 사용.
- **TLB 무효화**: `invlpg (virt)` 로 매핑/해제 후 CPU 캐시된 주소 변환 제거 필수. 없으면 unmap 후에도 이전 매핑을 계속 사용.
- **#PF 핸들러**: CR2 레지스터에 폴트 발생 가상 주소 저장됨. err_code 비트로 "not-present/protection", "read/write", "user/kernel" 구분 가능.

## Phase 6 — 힙 메모리 (kmalloc) ✅

- [x] `mm/heap.c` 작성 (free list 방식 kmalloc/kfree)
- [x] 힙 영역 초기화 (vmm으로 페이지 매핑 후 관리)
- [x] kmalloc/kfree 할당-해제 반복 테스트 확인 ✓

### 구현 목적
Phase 5까지는 메모리를 4KB 단위(페이지)로만 할당할 수 있었다. 커널 내부에서 구조체, 문자열, 버퍼 등 임의 크기의 메모리가 필요할 때마다 페이지 전체를 낭비하는 건 비효율적이다. kmalloc은 페이지 위에 올라타는 세밀한 할당자로, Phase 7 프로세스 구조체 등 모든 동적 할당의 기반이 된다.

### 구현 노트
- **힙 레이아웃**: 가상 주소 `0x300000000` (12GB)에 VMM으로 256 페이지(1MB) 매핑. 한 덩어리의 큰 free 블록으로 시작.
- **블록 헤더 (24바이트)**: `size(8) | used(8) | next*(8)`. 헤더 바로 뒤가 데이터 영역. `kfree(ptr)`은 `(block_t*)ptr - 1`로 헤더를 역산.
- **first-fit + 블록 분할**: 요청 크기보다 큰 free 블록 발견 시, 남은 공간이 `sizeof(header) + 8` 이상이면 두 블록으로 분할.
- **순방향 병합(forward coalescing)**: `kfree` 시 바로 다음 블록이 free면 병합. 역방향 병합은 미구현 → 전부 해제해도 블록 3개 남음(앞 블록이 free일 때 이미 사용 중인 블록이 사이에 있었기 때문).
- **8바이트 정렬**: `ALIGN8(size)` 매크로로 모든 할당 크기를 8의 배수로 올림 → 데이터 포인터 항상 8바이트 정렬 보장.

## Phase 7 — 프로세스 & 컨텍스트 스위칭 ✅

- [x] `proc/process.c` 작성 (PCB 구조체, 프로세스 테이블, 스택 할당)
- [x] `proc/swtch.s` 작성 (레지스터 저장/복원 — 컨텍스트 스위칭 어셈블리)
- [x] `proc/scheduler.c` 작성 (라운드로빈 스케줄러)
- [x] 타이머 인터럽트에서 `schedule()` 호출 (선점형 스케줄링)
- [x] 프로세스 2개 생성 후 번갈아 실행 확인 ✓

### 구현 목적

Phase 1~6까지는 커널이 하나의 실행 흐름(kernel_main)만 가지고 있었다. Phase 7은 CPU가 여러 프로세스를 빠르게 번갈아 실행해 "동시에 돌아가는 것처럼" 보이게 하는 **선점형 멀티태스킹**의 핵심이다.

- **PCB (Process Control Block)**: 프로세스마다 자신의 실행 상태(rsp, 스택, pid 등)를 저장하는 구조체. CPU는 레지스터 세트가 하나뿐이므로, 프로세스를 멈출 때 이 구조체에 상태를 저장하고, 재개할 때 꺼내 쓴다.
- **swtch**: callee-saved 레지스터(rbp, rbx, r12-r15)와 rsp만 교환하는 어셈블리 루틴. caller-saved 레지스터는 C 호출 규약상 호출자가 이미 보존해 두므로 swtch가 신경 쓸 필요 없다.
- **라운드로빈 스케줄러**: 원형 연결 리스트로 프로세스를 관리. 타이머가 10틱(100ms)마다 `schedule()`을 호출해 다음 프로세스로 CPU를 강제로 넘긴다.

### 구현 노트

- **스택 레이아웃**: 프로세스당 8KB 힙 할당. 초기 스택 상단에 `proc_trampoline` 주소를 ret 주소로, 그 아래 callee-saved 레지스터 6개(0)를 배치. swtch의 pop+ret이 곧바로 trampoline으로 점프한다.
- **원형 큐 삽입**: `scheduler_add(p)`는 항상 `current` 바로 뒤에 삽입. `scheduler_add(task_a)` → `scheduler_add(task_b)` 순서면 실행 순서는 idle→task_b→task_a→idle.
- **EOI 타이밍**: `irq_handler`에서 핸들러 호출 *전*에 EOI를 보낸다(`pic_send_eoi` → `timer_handler`). 덕분에 schedule() 안에서 컨텍스트 스위치가 일어나도 PIC가 막히지 않고 다음 타이머 인터럽트를 정상적으로 받는다.
- **idle 프로세스**: `process_create_idle()`로 현재 kernel_main 컨텍스트 자체를 프로세스 0으로 등록. 별도 스택 없이 커널 부트 스택을 그대로 사용. 모든 프로세스가 실행 중이 아닐 때 `for (;;) sti; hlt`로 대기.

### 트러블슈팅

**버그 1 — 첫 번째 진입 프로세스만 실행되고 이후 스케줄링 안 됨**

- **증상**: B가 0~20 출력 후 A가 영원히 실행 (스케줄 전환 없음)
- **원인**: 타이머 인터럽트에서 최초로 새 프로세스(task_a)로 스위칭할 때, CPU가 인터럽트 진입 시 IF=0으로 만든 상태가 그대로 유지됨. 기존 프로세스가 재개될 때는 IRET이 RFLAGS(IF=1 포함)를 복원하지만, **새 프로세스의 첫 진입은 IRET 없이 `ret`으로 점프**하기 때문에 IF=0인 채로 실행 → 타이머 인터럽트 불가 → 스케줄링 멈춤
- **수정**: `proc_trampoline` 도입. 모든 새 프로세스의 초기 ret 주소를 entry 함수 직접 대신 trampoline으로 설정. trampoline이 `sti` 후 실제 entry 호출.

```c
static void proc_trampoline(void) {
    __asm__ volatile("sti");
    current_process()->entry();
}
```

**버그 2 — 두 번째 프로세스까지만 실행 후 완전 중단**

- **증상**: B 0~19, A 0~19 출력 후 완전 중단 (A 19에서 멈춤)
- **원인**: task_a의 타이머 인터럽트 컨텍스트에서 idle로 스위칭될 때, `swtch`의 `ret`으로 kernel_main에 복귀. **IRET이 아닌 일반 ret이므로 RFLAGS가 복원되지 않아 IF=0 유지**. idle의 `hlt`는 IF=0 상태에서 일반 인터럽트를 받지 못함 → 타이머 인터럽트 불가 → 스케줄러 영원히 멈춤
- **수정**: idle 대기 루프를 `hlt` → `sti; hlt`로 변경. x86에서 `sti; hlt`는 인터럽트를 활성화하고 즉시 대기하는 표준 관용구.

```c
for (;;) __asm__ volatile("sti; hlt");
```

- **공통 교훈**: swtch로 복귀하는 모든 진입점(새 프로세스 첫 실행, idle 복귀)은 IRET 경로가 아니므로 **명시적으로 `sti`를 호출해야** 한다.

## Phase 8 — 시스템 콜 (선택)

### Step 1 — GDT 확장 + TSS 설정 ✅
- [x] `boot/boot.s` GDT에 유저 코드(0x18, DPL=3), 유저 데이터(0x20, DPL=3), TSS 슬롯(0x28/0x30) 추가
- [x] `kernel/tss.c` 작성 (TSS 구조체, rsp0 설정, GDT 디스크립터 패치, `ltr` 로드)
- [x] `kernel/main.c`에서 `tss_init()` 호출
- [x] 빌드 + QEMU: 셸 정상 동작 확인 ✓

### Step 2 — int 0x80 게이트 등록 ✅
- [x] `kernel/idt_asm.s`에 `isr128` 스텁 + `syscall_common` 추가
- [x] `kernel/idt.c`에 `idt_set_user_gate()` (DPL=3 게이트) 추가
- [x] `kernel/syscall.c` 작성 (syscall_init: int 0x80 등록, 기본 핸들러)
- [x] `kernel/main.c`에서 `syscall_init()` 호출
- [x] 빌드 + QEMU: 커널에서 int 0x80 테스트 호출 확인 ✓

### Step 3 — 유저 메모리 매핑
- [ ] `mm/vmm.c` 중간 페이지 테이블에 U/S 비트 전파 수정
- [ ] `kernel/usermode.c` 작성 (`usermode_setup`: 코드/스택 페이지 할당·매핑, 유저 프로그램 복사)
- [ ] `kernel/main.c`에서 `usermode_setup()` 호출 후 매핑 성공 로그 확인
- [ ] 빌드 + QEMU: Page Fault 없이 매핑 완료 로그 확인 ✓

### Step 4 — Ring 3 진입 + 유저 프로그램 실행
- [ ] `kernel/usermode.c`에 `jump_to_usermode()` (iretq 방식) 추가
- [ ] `proc/process.c`에 `process_create_user()` 추가 (유저 트램폴린 → iretq)
- [ ] `proc/process.h`에 `PROC_DEAD` 추가, `proc/scheduler.c` 스킵 로직 추가
- [ ] `kernel/syscall.c`에 `sys_write`, `sys_exit` 구현
- [ ] `kernel/main.c`에서 유저 프로세스 등록
- [ ] 빌드 + QEMU: "Hello from ring 3!" 출력 + 셸 복귀 확인 ✓

## Phase 9 — 셸 ✅

- [x] 키보드 입력 버퍼 관리
- [x] 간단한 커맨드 파서
- [x] `help`, `clear`, `echo` 명령어 구현
- [x] `pwd`, `ls`, `mkdir`, `touch`, `cd` 명령어 구현 (인메모리 파일시스템)

### 구현 목적

Phase 7까지는 커널과 직접 대화할 방법이 없었다. 셸은 사용자가 키보드로 명령을 입력하고 커널 기능을 호출하는 **인터페이스 계층**이다.

- **키보드 버퍼**: 인터럽트 핸들러는 빠르게 끝나야 한다. 키 입력을 버퍼에 저장해두고 셸이 필요할 때 꺼내 쓰는 방식으로 인터럽트 처리와 입력 처리를 분리한다.
- **인메모리 파일시스템**: 디스크 드라이버 없이도 트리 구조를 메모리에 올려 디렉토리/파일 개념을 구현할 수 있다. 커널 힙(`kmalloc`)을 쓰므로 재부팅하면 사라진다.

### 구현 노트

- **키보드 원형 버퍼**: `kb_head`(쓰기), `kb_tail`(읽기) 인덱스로 관리. `(head+1) % BUF_SIZE == tail`이면 가득 찬 상태 → 새 입력 버림. 인터럽트 핸들러가 head를 움직이고, `keyboard_getchar()`가 tail을 움직인다.
- **`keyboard_getchar()` 대기**: `kb_head == kb_tail`이면 버퍼가 비어있음. `sti; hlt`로 대기해 CPU를 낭비하지 않고 다음 키보드/타이머 인터럽트를 기다린다.
- **백스페이스 처리**: VGA는 커서 위치를 직접 관리하므로 `\b` 수신 시 커서를 한 칸 뒤로 이동하고 공백으로 덮어쓰는 방식. `printf("\b \b")`는 ← 이동 → 공백 출력 → 다시 ← 이동의 3단계.
- **커맨드 파서 (`split`)**: 공백으로 토큰을 나누되 원본 문자열에 직접 `\0`을 삽입해 별도 복사 없이 `argv[]` 포인터 배열을 만든다.
- **파일시스템 트리**: 노드(`fs_node_t`)가 이름, 타입(DIR/FILE), 부모 포인터, 자식 배열을 가진다. root의 parent는 자기 자신을 가리켜 `cd ..`가 root에서 멈추게 한다.
- **`pwd` 구현**: 재귀로 root까지 올라갔다가 내려오면서 경로를 출력. root 자신이면 `/`만 출력하고 종료.
- **파일시스템 제한**: 디렉토리당 최대 16개 항목(`CHILDREN_MAX`), 이름 최대 32자(`NAME_MAX`), 파일 내용 저장 없음(빈 노드만 생성).
