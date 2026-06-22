#include "pci.h"
#include "ports.h"
#include "string.h"
#include "heap.h"

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_dev_count = 0;

static const char *pci_class_names[] = {
    "Unclassified",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device",
    "Docking Station",
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent Controller",
    "Satellite Communication Controller",
    "Encryption Controller",
    "Signal Processing Controller"
};

static const struct { uint16_t id; const char *name; } pci_vendors[] = {
    { 0x8086, "Intel Corporation" },
    { 0x1022, "AMD" },
    { 0x10DE, "NVIDIA" },
    { 0x1002, "ATI/AMD" },
    { 0x1AF4, "Red Hat (QEMU)" },
    { 0x1234, "Bochs/QEMU" },
    { 0x15AD, "VMware" },
    { 0x80EE, "Oracle (VirtualBox)" },
    { 0x10EC, "Realtek Semiconductor" },
    { 0x8087, "Intel" },
    { 0x1033, "NEC Corporation" },
    { 0x1044, "Adaptec" },
    { 0x9005, "Adaptec" },
    { 0x1106, "VIA Technologies" },
    { 0x10B7, "3Com Corporation" },
    { 0x14E4, "Broadcom" },
    { 0x168C, "Qualcomm Atheros" },
    { 0x1186, "D-Link System" },
    { 0x10EC, "Realtek" },
    { 0x0E11, "Compaq" },
    { 0x0E0F, "VMware" },
    { 0, 0 }
};

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_read_device_info(uint8_t bus, uint8_t slot, uint8_t func, pci_device_info_t *info) {
    uint32_t tmp;

    tmp = pci_read_config(bus, slot, func, PCI_VENDOR_ID);
    info->vendor_id = tmp & 0xFFFF;
    info->device_id = (tmp >> 16) & 0xFFFF;

    tmp = pci_read_config(bus, slot, func, PCI_COMMAND);
    info->command = tmp & 0xFFFF;
    info->status = (tmp >> 16) & 0xFFFF;

    tmp = pci_read_config(bus, slot, func, PCI_REVISION_ID);
    info->revision_id = tmp & 0xFF;
    info->prog_if = (tmp >> 8) & 0xFF;
    info->subclass = (tmp >> 16) & 0xFF;
    info->class_code = (tmp >> 24) & 0xFF;

    tmp = pci_read_config(bus, slot, func, PCI_CACHE_LINE);
    info->cache_line = tmp & 0xFF;
    info->latency_timer = (tmp >> 8) & 0xFF;
    info->header_type = (tmp >> 16) & 0xFF;
    info->bist = (tmp >> 24) & 0xFF;

    for (int i = 0; i < 6; i++) {
        info->bar[i] = pci_read_config(bus, slot, func, PCI_BAR0 + i * 4);
    }

    info->cardbus_cis = pci_read_config(bus, slot, func, PCI_CARDBUS_CIS);

    tmp = pci_read_config(bus, slot, func, PCI_SUBSYS_VENDOR);
    info->subsys_vendor = tmp & 0xFFFF;
    info->subsys_id = (tmp >> 16) & 0xFFFF;

    info->exp_rom = pci_read_config(bus, slot, func, PCI_EXP_ROM);
    info->capabilities = pci_read_config(bus, slot, func, PCI_CAPABILITIES) & 0xFF;

    tmp = pci_read_config(bus, slot, func, PCI_INTERRUPT_LINE);
    info->interrupt_line = tmp & 0xFF;
    info->interrupt_pin = (tmp >> 8) & 0xFF;
    info->min_gnt = (tmp >> 16) & 0xFF;
    info->max_lat = (tmp >> 24) & 0xFF;
}

static int pci_device_has_valid_vendor(uint16_t vendor_id) {
    return vendor_id != 0xFFFF && vendor_id != 0x0000;
}

int pci_init(void) {
    pci_dev_count = 0;
    memset(pci_devices, 0, sizeof(pci_devices));

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                if (pci_dev_count >= MAX_PCI_DEVICES) return pci_dev_count;

                uint32_t vendor_data = pci_read_config(bus, slot, func, PCI_VENDOR_ID);
                uint16_t vendor_id = vendor_data & 0xFFFF;

                if (!pci_device_has_valid_vendor(vendor_id)) {
                    if (func == 0) break;
                    continue;
                }

                pci_device_t *dev = &pci_devices[pci_dev_count];
                dev->bus = bus;
                dev->slot = slot;
                dev->func = func;
                dev->present = 1;
                pci_read_device_info(bus, slot, func, &dev->info);
                pci_dev_count++;

                uint8_t header_type = (pci_read_config(bus, slot, func, PCI_HEADER_TYPE) >> 16) & 0xFF;

                if (!(header_type & 0x80)) break;
            }
        }
    }

    return pci_dev_count;
}

int pci_device_count(void) {
    return pci_dev_count;
}

pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= pci_dev_count) return 0;
    return &pci_devices[index];
}

const char *pci_class_name(uint8_t class_code) {
    if (class_code <= PCI_CLASS_SIGNAL_PROC) {
        return pci_class_names[class_code];
    }
    return "Unknown";
}

const char *pci_vendor_name(uint16_t vendor_id) {
    for (int i = 0; pci_vendors[i].id != 0; i++) {
        if (pci_vendors[i].id == vendor_id) return pci_vendors[i].name;
    }
    return "Unknown Vendor";
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    if (!dev || !dev->present) return;
    uint16_t cmd = dev->info.command;
    cmd |= (1 << 2) | (1 << 0);
    pci_write_config(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
    dev->info.command = pci_read_config(dev->bus, dev->slot, dev->func, PCI_COMMAND) & 0xFFFF;
}
