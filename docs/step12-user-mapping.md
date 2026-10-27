# Step 12 — 유저 메모리 매핑 (U/S 비트)

> 유저 코드(16GB)와 유저 스택(20GB) 페이지를 `VMM_USER` 로 매핑한다. 중간 페이지 테이블에도 U/S 비트를 전파하도록 `vmm_map_page` 를 고친다.

## 목표

`[user] code mapped: virt=0x400000000 ...`, `[user] stack mapped: virt=0x500000000 ...` 로그. (아직 ring 3으로 내려가지는 않음 — 다음 단계 준비)

## 이 단계가 만드는 틀

- **유저 주소 배치 약속**
  | 주소 | 용도 |
  |---|---|
  | `0x0` ~ `0x40000000` (1GB) | 커널 (아이덴티티, 유저 접근 불가) |
  | `0x300000000` (12GB) | 커널 힙 |
  | `0x400000000` (16GB) | **유저 코드** — Step 18부터 ELF도 여기 링크 (`user/linker.ld`) |
  | `0x500000000` (20GB) | 유저 스택 (Step 14 데모) |
- **"유저 페이지 = U/S=1인 4KB 페이지"** 라는 구분 — Step 20 `vmm_fork` 는 이 비트로 "복사할 페이지"를, Step 21 `vmm_free_user` 는 "지울 페이지"를 고른다
- **`usermode.h`** — 유저 관련 상수의 집합소. Step 14에서 `USER_CS` · `USER_DS` 가 추가된다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `mm/vmm.c` | 새로 만드는 중간 테이블 엔트리에 `flags & VMM_USER` 전파 (`parent_flags`) |
| `kernel/usermode.c` | `usermode_setup` — 코드 · 스택 페이지 할당 후 USER로 매핑, 코드 자리에 `hlt` 한 바이트 |
| `kernel/usermode.h` | `USER_CODE_VIRT`, `USER_STACK_VIRT` |
| `kernel/main.c` | `usermode_setup()` (int 0x80 테스트는 제거) |

원본 커밋: `d93c252` · 읽는 순서: `mm/vmm.c`(parent_flags) → `kernel/usermode.c`

## 핵심 개념

- **U/S 비트(bit 2)**: 1이면 ring 3도 접근 가능, 0이면 커널만. 커널은 어디든 접근 가능
- **모든 레벨이 허락해야 한다** — CPU는 PML4 → PDPT → PD → PT 엔트리의 U/S를 **전부 AND** 한다. 마지막 PT만 USER여도 위 단계 하나가 0이면 유저 접근은 #PF
- **이 단계의 한계** (Step 14에서 드러남): 새로 만드는 중간 테이블은 USER가 되지만, **이미 있던** 엔트리는 그대로다.
  16GB 주소의 PML4 인덱스는 0 — boot.s가 U/S 없이 만든 바로 그 엔트리를 공유한다

## 직접 만든다면

1. `usermode_setup` 에서 페이지 두 장을 `VMM_USER` 로 매핑
2. `vmm_map_page` 의 중간 엔트리 플래그를 `parent_flags` 로
3. 매핑 결과를 페이지 테이블을 따라가며 출력해 각 레벨의 bit 2 확인

## 확인

- 두 줄의 `[user] ... mapped` 로그, 셸 정상 동작

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 커널이 유저 페이지에 쓸 때 Page Fault | WRITABLE 누락 (U/S와 별개) |
| ring 3에서 접근 시 `err=0x5`/`0x7` (protection, user) | 어느 레벨의 U/S가 0 → Step 14의 보완 참고 |

## 다음 단계로

ring 3로 내려가기 전에, 앞으로 늘어날 부팅 로그와 디버그 출력을 정리해 둔다 → [Step 13](step13-bootlog.md)
