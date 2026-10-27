# Step 23 — 셸에서 유저 프로그램 실행

> 셸이 빌트인이 아닌 명령을 받으면 ext2에서 같은 이름의 ELF를 찾아 실행하고, 끝날 때까지 기다린 뒤 정리한다.

## 목표

셸에서 `hello` → 실행 후 프롬프트 복귀, `count` → 1~5 출력 후 복귀.

## 이 단계가 만드는 틀

- **"명령 = 디스크의 프로그램"** — 새 기능을 커널을 고치지 않고 `user/*.c` 하나 추가해서 늘릴 수 있다. 진짜 OS의 사용 모델
- **`shell_spawn(name)`** = 로드 → 프로세스 생성 → 대기 → 정리. 커널 셸 버전의 fork+exec+wait
- **최종 형태** — 이 단계의 트리가 `main` 브랜치와 같다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `shell/shell.c` | `shell_spawn` (elf_load → process_create_user → scheduler_add → DEAD까지 양보 → remove · free), 마지막 else 분기 |
| `user/count.c` | 테스트 프로그램 |
| `Makefile` | `diskfiles/count` 빌드 |

원본 커밋: `c4ba06a` · 읽는 순서: `shell/shell.c`(shell_spawn) → `user/count.c`

## 핵심 개념

- **셸은 커널 스레드라서 fork를 안 쓴다** — ring 0 셸은 ring 3 프레임이 없어서 fork할 수 없다. 대신 `elf_load` + `process_create_user` 로 직접 만들고, `sys_wait` 과 같은 방식으로 기다리고 치운다
- **주소 공간 공유 주의** — `process_create_user` 는 현재(커널) PML4를 쓴다. 그래서 실행 중인 프로그램은 하나씩만(셸이 기다리는 동안) 안전하다. `/init` 이 아직 돌고 있으면 같은 `0x400000000` 을 덮어쓸 수 있다
- **유저 셸로 가는 길** — 진짜 OS처럼 하려면 셸 자체를 유저 프로그램으로 만들고 `fork` → `exec` → `wait` 를 쓰면 된다. 그 재료(19~22)는 이미 다 있다

## 직접 만든다면

1. `shell_spawn` 을 `sys_wait` 코드를 참고해 작성
2. 셸 분기의 마지막 `unknown:` 을 `shell_spawn(argv[0])` 으로
3. `count.c` 를 추가해 빌드 · 디스크 포함

## 확인

- `hello`, `count` 실행 후 프롬프트로 돌아온다
- 없는 이름은 `xxx: not found`

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 프로그램 실행 후 셸이 멈춤 | 자식이 exit을 안 부름 / DEAD 확인 루프에서 schedule 안 함 |
| 두 번째 실행부터 이상함 | 정리 누락 (유저 페이지 · PCB) |

## 여기서 더 나아가려면

- **유저 공간 셸** — 셸을 `user/sh.c` 로 옮기고 `fork/exec/wait` 사용
- **인자 전달** — `exec(path, argv)` 와 유저 스택에 argc/argv 쌓기
- **Copy-on-Write fork**, **sleep/wakeup 대기 큐** (바쁜 대기 제거)
- **SMP** — `todo.md` 의 멀티코어 로드맵 (LAPIC → AP 부팅 → 스핀락 → per-CPU → SMP 스케줄러)
