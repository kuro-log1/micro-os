# Step 18 — ELF 로더 & /init 실행

> 디스크의 `/init` 을 읽어 ELF64 헤더를 검사하고, 세그먼트를 유저 페이지에 복사한 뒤 ring 3에서 실행한다.

## 목표

부팅 로그에 `[elf] seg[0] vaddr=400000000 ...`, 이어서 `Hello from /init (ring 3 ELF)!` — **C로 작성해 따로 빌드한 프로그램**이 실행된다.

## 이 단계가 만드는 틀

- **`elf_load(name, &entry, &rsp)`** — "프로그램 이름 → 실행 준비 완료(진입점, 스택)". 이후 exec(21)와 셸 실행(23)이 그대로 부른다
- **유저 프로그램 빌드 규칙** — 표준 라이브러리 없이(`-nostdlib -ffreestanding`), `user/linker.ld` 로 `0x400000000` 에 배치, 결과물은 `diskfiles/` → 디스크 이미지
- **유저 쪽 시스템 콜 래퍼** — `user/*.c` 마다 인라인 어셈블리로 `int 0x80` 을 감싼 작은 함수들 (아직 공용 libc는 없음)
- **유저 스택 위치** — `0x7FF00000` 부터 16KB, rsp = 그 꼭대기
- **Part 3 완성** — 트리가 원래 커밋 `03c5032` 와 같다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/elf.h` | `Elf64_Ehdr`, `Elf64_Phdr`, 상수 (`PT_LOAD`, `PF_W` …) |
| `kernel/elf.c` | 파일 읽기 → 검증 → `PT_LOAD` 마다 페이지 할당 · 매핑 · 복사 · BSS 0 채우기 → 스택 매핑 |
| `user/linker.ld` | 진입 `_start`, `.text` 를 `0x400000000` 에, 불필요한 섹션 버림 |
| `user/init.c` | 첫 유저 프로그램: write + exit |
| `Makefile` | `user` 타깃 (`diskfiles/init` 빌드), `disk` 가 그걸 포함 |
| `kernel/main.c` | `usermode_setup` 대신 `elf_load("init")` → `process_create_user` |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `kernel/elf.h` → `kernel/elf.c` → `user/linker.ld` → `user/init.c` → `Makefile`

## 핵심 개념

- **ELF 파일 구조**
  ```
  [ELF 헤더: 매직 7F 'E' 'L' 'F', 64비트, x86-64, 실행 파일, 진입점 e_entry, 프로그램 헤더 위치]
  [프로그램 헤더 × N: 타입, 파일 오프셋, 가상 주소, 파일 크기, 메모리 크기, 권한]
  [섹션 데이터 ...]
  ```
  로더는 **프로그램 헤더만** 본다 (섹션 헤더는 링커 · 디버거용)
- **PT_LOAD 하나 = "파일의 [offset, +filesz) 를 메모리 vaddr에 올려라"**. `memsz > filesz` 인 나머지는 BSS(0으로 초기화된 전역 변수)
- **권한**: `PF_W` 가 있으면 WRITABLE. 모든 유저 페이지는 USER
- **왜 `0x400000000`?** 커널의 1GB 아이덴티티 영역(2MB 대형 페이지)과 겹치면 4KB 매핑을 못 한다 (Step 06)
- **왜 `-nostdlib`?** 이 OS에는 glibc가 없다. `main` 대신 `_start` 가 진입점이고, 끝날 때 반드시 `exit` 를 불러야 한다 (리턴할 곳이 없음)
- **아직 주소 공간은 하나** — 지금은 모든 프로세스가 커널의 PML4를 공유한다. 유저 프로그램 둘을 동시에 올리면 같은 주소에서 충돌 → Step 20에서 프로세스별 주소 공간

## 직접 만든다면

1. 유저 프로그램을 빌드해 `readelf -h -l diskfiles/init` 로 구조 확인
2. `elf_load`: 파일 읽기 + 매직 검사만
3. PT_LOAD 세그먼트 매핑 · 복사 · BSS
4. 스택 매핑, 진입점 반환
5. `process_create_user(entry, rsp)` 로 실행

## 확인

- `[elf]   seg[0] vaddr=400000000 filesz=... R-` 로그와 `Hello from /init (ring 3 ELF)!`
- 생각해 볼 것: 유저 프로그램을 왜 `-nostdlib` + 전용 링커 스크립트로 빌드해야 할까?

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `invalid ELF` | PIE로 빌드됨(`ET_DYN`) — `-fno-pic -no-pie -static` 필요 (Linux host gcc는 기본이 PIE) |
| 진입하자마자 #PF | 세그먼트 vaddr이 페이지 정렬이 아닌데 정렬 없이 매핑 / USER 비트 누락 |
| 전역 변수 값이 이상함 | BSS 0 채우기 누락 |
| `_start` 에서 리턴 후 크래시 | exit 호출 없음 |

## 다음 단계로

유저 프로그램은 돌지만 할 수 있는 건 "화면에 쓰기"와 "끝내기"뿐이다.
파일을 열고 읽으려면 유닉스식 **파일 디스크립터**가 필요하다 → [Step 19](step19-fd.md)
