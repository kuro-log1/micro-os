# Step 15 — PCI 버스 스캔

> 포트 `0xCF8`/`0xCFC` 로 PCI 설정 공간을 읽어 bus 0에 붙은 장치를 나열한다.

## 목표

부팅 로그에 `[pci]   00:01.1  IDE controller  [8086:7010]` 같은 장치 목록이 나온다.

## 이 단계가 만드는 틀

- **`pci_read(bus, dev, func, off)`** — 장치를 찾고 설정을 읽는 기본 도구. 지금은 목록 출력에만 쓰지만,
  네트워크 카드나 AHCI 같은 드라이버를 붙일 때 "장치 찾기 → BAR(레지스터 주소) 읽기"의 출발점
- **32비트 포트 I/O (`outl` / `inl`)** — `kernel/io.h` 에 추가

## 만들 파일

| 파일 | 역할 |
|---|---|
| `kernel/io.h` | `outl`, `inl` |
| `drivers/pci.c` | `pci_read`, 클래스 코드 → 이름 표, `pci_scan` |
| `kernel/main.c` | `pci_scan()` |

원본 커밋: `03c5032` 의 일부 · 읽는 순서: `kernel/io.h` → `drivers/pci.c`

## 핵심 개념

- **PCI 설정 공간**: 장치마다 256바이트 레지스터 묶음. 오프셋 `0x00` = vendor/device ID, `0x08` = 클래스 코드(상위 바이트가 base class, 그다음 subclass)
- **주소 조립** — 포트 `0xCF8` 에 아래 값을 쓰고 `0xCFC` 에서 32비트를 읽는다
  ```
  bit 31: enable | [23:16] bus | [15:11] device | [10:8] function | [7:2] 레지스터 (4바이트 정렬)
  ```
- **빈 슬롯** — vendor ID가 `0xFFFF` 면 장치 없음
- 이 코드는 **bus 0, function 0만** 본다 (QEMU 기본 구성에는 충분). 브리지 너머 버스나 다기능 장치는 생략
- QEMU i440FX 머신에서 IDE 컨트롤러가 보인다 → 다음 단계 ATA 드라이버가 다룰 장치

## 직접 만든다면

1. `outl`/`inl` 인라인 어셈블리
2. `pci_read` 로 dev 0의 ID 하나 출력 (Host bridge `8086:1237`)
3. 0~31 루프, 빈 슬롯 건너뛰기
4. 클래스 코드 이름표

## 확인

- Host bridge, ISA bridge, IDE controller, VGA controller, Ethernet controller 등

## 자주 막히는 곳

| 증상 | 원인 |
|---|---|
| 모든 슬롯이 `ffff` | enable 비트(31) 누락 |
| 값이 엉뚱함 | 오프셋 하위 2비트를 안 지움 (`& 0xFC`) |

## 다음 단계로

IDE 컨트롤러가 있다는 걸 확인했으니, 그 뒤의 **디스크**에 명령을 보내 섹터를 읽는다 → [Step 16](step16-ata.md)
