# Step 16 — ATA PIO 디스크 드라이버

> 프라이머리 IDE 채널(포트 `0x1F0`~`0x1F7`)에 명령을 보내고, 상태 비트를 폴링하며 512바이트 섹터를 16비트씩 주고받는다.

## 목표

`[ata] ata0: QEMU HARDDISK  (16 MB)` 로그, 그리고 디스크 1024바이트 지점에서 ext2 매직 넘버 `[test] ext2 magic=0xef53` 을 읽는다.

## 이 단계가 만드는 틀

- **블록 장치 인터페이스** — 파일시스템이 기대는 단 두 함수
  ```c
  int ata_read_sector (uint32_t lba, void *buf);        /* 512바이트 */
  int ata_write_sector(uint32_t lba, const void *buf);
  ```
  Step 17 ext2는 디스크가 ATA인지 몰라도 이 두 함수만 부른다 (나중에 AHCI/virtio로 바꿔도 위층은 그대로)
- **디스크 이미지 빌드 흐름** — `diskfiles/` 의 파일로 ext2 이미지(`disk.img`)를 만들어 QEMU에 IDE 드라이브로 연결.
  Step 18부터 유저 프로그램도 이 폴더에 들어간다

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/io.h` | 16비트 포트 I/O `outw` / `inw` |
| `drivers/ata.c` | `wait_drq`, `ata_identify`(모델명 · 용량), `ata_read_sector`, `ata_write_sector`(+ 캐시 플러시) |
| `diskfiles/hello.txt` | 디스크에 넣을 첫 파일 |
| `Makefile` | `disk` 타깃, `qemu` 에 `-drive file=disk.img,format=raw,if=ide` |
| `kernel/main.c` | `ata_identify()` + 섹터 2 읽기 실습 |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `kernel/io.h` → `drivers/ata.c`

## 핵심 개념

- **LBA28 읽기 순서**
  ```
  0x1F6 ← 0xE0 | LBA[27:24]   (마스터 드라이브, LBA 모드)
  0x1F2 ← 1                   (섹터 수)
  0x1F3/4/5 ← LBA[7:0] / [15:8] / [23:16]
  0x1F7 ← 0x20 (READ SECTORS)
  상태(0x1F7)의 BSY가 꺼지고 DRQ가 켜질 때까지 대기
  0x1F0 에서 16비트 × 256번 = 512바이트
  ```
- **PIO = CPU가 직접 한 워드씩 옮김** — 느리지만 단순. DMA나 인터럽트 없이 폴링
- **상태 비트**: BSY(`0x80`) 바쁨, DRQ(`0x08`) 데이터 준비됨, ERR(`0x01`) 실패
- **IDENTIFY(`0xEC`)**: 256워드 정보. 모델명은 27~46번 워드인데 **워드마다 두 바이트가 뒤집혀** 있다. 60~61번 = 전체 섹터 수
- **쓰기 후 FLUSH(`0xE7`)** — 디스크 캐시를 실제로 기록
- **왜 LBA 2?** ext2 슈퍼블록은 디스크 1024바이트 지점 = 512바이트 섹터 2번. 오프셋 56의 `0xEF53` 이 ext2라는 표시 — 다음 단계로 이어지는 다리

## 직접 만든다면

1. IDENTIFY만 구현해 모델명 출력 (바이트 뒤집기 확인)
2. `ata_read_sector` 로 섹터 0, 2를 16진수 덤프
3. 매직 넘버 확인
4. 쓰기 + 플러시 (빈 섹터에 써 보고 재부팅 후 읽기)

## 확인

- `[ata] ata0: QEMU HARDDISK  (16 MB)`, `[test] ext2 magic=0xef53`
- 디스크 이미지는 `os-learn` 이 `mkfs.ext2 -d diskfiles` 로 만든다 (원래 Makefile은 `genext2fs`)

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| `no drive detected` | QEMU에 `-drive` 를 안 붙임 / 이미지가 없음 |
| 읽은 데이터가 전부 0 또는 `ff` | DRQ 대기 없이 바로 읽음 |
| 모델명 글자가 짝으로 뒤바뀜 | 워드 내 바이트 순서 안 뒤집음 |

## 다음 단계로

섹터는 읽히지만 섹터는 그냥 숫자 덩어리다. "hello.txt가 몇 번 섹터에 있는지" 알려면 파일시스템 구조를 해석해야 한다 → [Step 17](step17-ext2.md)
