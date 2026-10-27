#include "drivers/pci.h"
#include "kernel/io.h"
#include "lib/printf.h"
#include <stdint.h>

#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

/* PCI 설정 공간 32비트 읽기 */
uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)dev  << 11)
                  | ((uint32_t)func <<  8)
                  | (off & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static const char *class_name(uint8_t base, uint8_t sub) {
    if (base == 0x01 && sub == 0x01) return "IDE controller";
    if (base == 0x01)                return "Mass storage";
    if (base == 0x02 && sub == 0x00) return "Ethernet controller";
    if (base == 0x02)                return "Network controller";
    if (base == 0x03 && sub == 0x00) return "VGA controller";
    if (base == 0x03)                return "Display controller";
    if (base == 0x06 && sub == 0x00) return "Host bridge";
    if (base == 0x06 && sub == 0x01) return "ISA bridge";
    if (base == 0x06 && sub == 0x04) return "PCI bridge";
    if (base == 0x06)                return "Bridge";
    if (base == 0x0C && sub == 0x03) return "USB controller";
    return "Unknown";
}

void pci_scan(void) {
    klog("[pci] scanning bus 0...\n");

    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read(0, dev, 0, 0x00);
        if ((id & 0xFFFF) == 0xFFFF) continue;  /* 슬롯 비어있음 */

        uint16_t vendor  = (uint16_t)(id & 0xFFFF);
        uint16_t device  = (uint16_t)(id >> 16);
        uint32_t cc      = pci_read(0, dev, 0, 0x08);
        uint8_t  base    = (uint8_t)((cc >> 24) & 0xFF);
        uint8_t  sub     = (uint8_t)((cc >> 16) & 0xFF);

        klog("[pci]   00:%02x.0  %s  [%x:%x]\n",
             dev, class_name(base, sub), vendor, device);
    }
}
