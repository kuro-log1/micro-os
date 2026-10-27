# Step 09 — 셸 & 인메모리 파일시스템

> 키보드로 한 줄을 읽어 명령을 실행하는 셸을 커널 스레드로 돌리고, kmalloc으로 만든 디렉터리 트리를 다룬다.

## 목표

`> ` 프롬프트에서 `help`, `mkdir a`, `cd a`, `pwd`, `ls`, `echo hi`, `clear` 가 동작한다.

## 이 단계가 만드는 틀

- **셸 = 사용자 인터페이스의 자리** — 이후 기능은 셸 명령으로 노출된다: `cat` · `write`(17), 디스크 프로그램 실행(23)
- **`readline` → `split` → 명령 분기** 구조. Step 23에서 "모르는 명령이면 디스크에서 찾아 실행"이 마지막 분기로 붙는다
- **트리형 FS 인터페이스** (`fs_mkdir`, `fs_cd`, `fs_ls` …) — 파일시스템이 어떤 연산을 제공해야 하는지의 첫 모델. Step 17에서 진짜 디스크(ext2)로 바뀐다
- **Part 1 완성** — 여기까지가 "부팅해서 멀티태스킹하는 커널". 트리가 원래 커밋 `bfc5002` 와 같다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `fs/fs.c` | `fs_node` (이름, 타입, 부모, 자식 16개), root/cwd, mkdir/touch/ls/cd/pwd |
| `shell/shell.c` | `readline`(에코 · 백스페이스), `split`(공백으로 argv), 명령 분기 |
| `kernel/main.c` | `fs_init`, 셸을 `process_create(shell_run)` 으로 등록 |

읽는 순서: `fs/fs.c` → `shell/shell.c` → `kernel/main.c`

## 핵심 개념

- **셸도 그냥 프로세스** — idle(pid 0)은 `hlt` 루프, 셸(pid 1)은 `keyboard_getchar()` 에서 대부분의 시간을 잔다. 둘을 타이머가 번갈아 준다
- **readline**: 글자를 받을 때마다 화면에 직접 에코(커널이 "터미널" 역할), `\b` 이면 한 칸 지우고 `"\b \b"` 출력
- **split**: 공백을 `\0` 으로 바꿔 한 버퍼 안에서 argv 포인터만 모은다 (복사 없음)
- **FS 노드 트리**: 각 노드가 부모 포인터와 자식 배열을 가짐. `cd ..` 은 부모로, `pwd` 는 부모를 따라 올라가며 경로 조립
- 이 FS는 **메모리에만** 있어서 재부팅하면 사라진다

## 직접 만든다면

1. 셸 루프: 프롬프트 → readline → 그대로 에코만
2. split + `help`, `echo`, `clear`
3. FS 노드 구조체와 root, `mkdir` / `ls`
4. `cd`(`..` 포함), `pwd`
5. `main.c` 에서 셸 프로세스 등록

## 확인

- `mkdir docs` → `cd docs` → `pwd` 가 `/docs`
- 모르는 명령은 `unknown: xxx`

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 입력이 안 받아짐 | 셸이 스케줄되지 않음 (scheduler_add 누락) / 키보드 IRQ 막힘 |
| 백스페이스가 프롬프트까지 지움 | `i > 0` 검사 누락 |
| 루트에서 `cd ..` 하면 Page Fault | root의 부모가 NULL — 그래서 `fs_init` 은 `root->parent = root` 로 자기 자신을 가리키게 한다 |
| 같은 이름 디렉터리가 여러 개 | `fs_find` 로 중복 검사를 안 함 (지금 코드는 `already exists` 로 막는다) |

## 다음 단계로

지금은 셸도, 테스트 스레드도 전부 **커널 권한(ring 0)** 으로 돈다. 한 프로그램의 버그가 커널 전체를 망가뜨릴 수 있다.
커널과 분리된 "유저 모드(ring 3)"를 만들기 위해 CPU에 권한 구분을 알려 준다 → [Step 10](step10-gdt-tss.md)
