#ifndef TVN_PCI_H
#define TVN_PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

#define PCI_VENDOR_ID      0
#define PCI_DEVICE_ID      2
#define PCI_COMMAND        4
#define PCI_STATUS         6
#define PCI_REVISION_ID    8
#define PCI_PROG_IF        9
#define PCI_SUBCLASS       0xA
#define PCI_CLASS          0xB
#define PCI_CACHE_LINE     0xC
#define PCI_LATENCY_TIMER  0xD
#define PCI_HEADER_TYPE    0xE
#define PCI_BIST           0xF
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14
#define PCI_BAR2           0x18
#define PCI_BAR3           0x1C
#define PCI_BAR4           0x20
#define PCI_BAR5           0x24
#define PCI_CARDBUS_CIS    0x28
#define PCI_SUBSYS_VENDOR  0x2C
#define PCI_SUBSYS_ID      0x2E
#define PCI_EXP_ROM        0x30
#define PCI_CAPABILITIES   0x34
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D
#define PCI_MIN_GNT        0x3E
#define PCI_MAX_LAT        0x3F

#define PCI_HEADER_NORMAL  0x00
#define PCI_HEADER_BRIDGE  0x01
#define PCI_HEADER_CARDBUS 0x02

#define PCI_CLASS_MASS_STORAGE   0x01
#define PCI_CLASS_NETWORK        0x02
#define PCI_CLASS_DISPLAY        0x03
#define PCI_CLASS_MULTIMEDIA     0x04
#define PCI_CLASS_MEMORY         0x05
#define PCI_CLASS_BRIDGE         0x06
#define PCI_CLASS_SIMPLE_COMM    0x07
#define PCI_CLASS_SYSTEM_PERIPH  0x08
#define PCI_CLASS_INPUT_DEV      0x09
#define PCI_CLASS_DOCKING        0x0A
#define PCI_CLASS_PROCESSOR      0x0B
#define PCI_CLASS_SERIAL_BUS     0x0C
#define PCI_CLASS_WIRELESS       0x0D
#define PCI_CLASS_INTELLIGENT    0x0E
#define PCI_CLASS_SATELLITE      0x0F
#define PCI_CLASS_ENCRYPTION     0x10
#define PCI_CLASS_SIGNAL_PROC    0x11
#define PCI_CLASS_UNDEFINED      0xFF

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  header_type;
    uint8_t  bist;
    uint8_t  cache_line;
    uint8_t  latency_timer;
    uint32_t bar[6];
    uint32_t cardbus_cis;
    uint16_t subsys_vendor;
    uint16_t subsys_id;
    uint32_t exp_rom;
    uint8_t  capabilities;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_gnt;
    uint8_t  max_lat;
} pci_device_info_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    pci_device_info_t info;
    int present;
} pci_device_t;

#define MAX_PCI_DEVICES 64

int  pci_init(void);
int  pci_device_count(void);
pci_device_t *pci_get_device(int index);
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
const char *pci_class_name(uint8_t class_code);
const char *pci_vendor_name(uint16_t vendor_id);
void pci_enable_bus_mastering(pci_device_t *dev);

#endif
