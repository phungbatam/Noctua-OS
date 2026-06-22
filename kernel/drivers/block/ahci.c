#include "ahci.h"
#include "drivers/bus/pci.h"
#include "mm/page.h"
#include "mm/heap.h"
#include "screen.h"
#include "string.h"
#include "string.h"

static ahci_ctrl_t ahci_ctrl;

static uint32_t ahci_port_read(ahci_port_t *port, uint32_t reg) {
    return *(port->port_base + reg / 4);
}

static void ahci_port_write(ahci_port_t *port, uint32_t reg, uint32_t val) {
    *(port->port_base + reg / 4) = val;
}

static int ahci_find_ctrl(pci_device_t **out_dev, uint32_t *out_mmio) {
    int nd = pci_device_count();
    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;
        if (dev->info.class_code == 0x01 && dev->info.subclass == 0x06) {
            uint32_t bar5 = dev->info.bar[5];
            if (bar5 & 1) {
                bar5 &= ~3;
            }
            uint32_t mmio = dev->info.bar[5] & ~0xF;
            if (mmio) {
                *out_dev = dev;
                *out_mmio = mmio;
                return 0;
            }
        }
    }
    return -1;
}

static void ahci_start_cmd(ahci_port_t *port) {
    while (ahci_port_read(port, AHCI_PORT_CMD) & AHCI_CMD_CR);

    uint32_t cmd = ahci_port_read(port, AHCI_PORT_CMD);
    cmd |= AHCI_CMD_FRE;
    ahci_port_write(port, AHCI_PORT_CMD, cmd);

    cmd |= AHCI_CMD_ST;
    ahci_port_write(port, AHCI_PORT_CMD, cmd);
}

static void ahci_stop_cmd(ahci_port_t *port) {
    uint32_t cmd = ahci_port_read(port, AHCI_PORT_CMD);
    cmd &= ~AHCI_CMD_ST;
    ahci_port_write(port, AHCI_PORT_CMD, cmd);

    while (ahci_port_read(port, AHCI_PORT_CMD) & AHCI_CMD_CR);

    cmd &= ~AHCI_CMD_FRE;
    ahci_port_write(port, AHCI_PORT_CMD, cmd);

    while (ahci_port_read(port, AHCI_PORT_CMD) & AHCI_CMD_FR);
}

static int ahci_find_free_slot(ahci_port_t *port) {
    uint32_t sact = ahci_port_read(port, AHCI_PORT_SACT);
    uint32_t ci = ahci_port_read(port, AHCI_PORT_CI);
    uint32_t slots = sact | ci;

    for (int i = 0; i < AHCI_CMD_SLOTS; i++) {
        if (!(slots & (1 << i))) return i;
    }
    return -1;
}

int ahci_read(ahci_port_t *port, uint64_t lba, uint32_t count, void *buffer) {
    if (!port->present) return -1;

    uint32_t sector_size = AHCI_SECTOR_SIZE;

    int slot = ahci_find_free_slot(port);
    if (slot < 0) return -1;

    ahci_cmd_hdr_t *cmd_hdr = &port->cmd_hdrs[slot];
    memset(cmd_hdr, 0, sizeof(ahci_cmd_hdr_t));
    cmd_hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    cmd_hdr->write = 0;
    cmd_hdr->prdt_length = 1;

    ahci_cmd_table_t *cmd_table = (ahci_cmd_table_t *)((uint32_t)cmd_hdr->ctba);
    if (!cmd_table) {
        cmd_table = (ahci_cmd_table_t *)kmalloc(sizeof(ahci_cmd_table_t));
        memset(cmd_table, 0, sizeof(ahci_cmd_table_t));
        uint32_t phys = (uint32_t)cmd_table;
        if (phys >= 0xC0000000) phys -= 0xC0000000;
        cmd_hdr->ctba = phys;
        cmd_hdr->ctba_hi = 0;
    }
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)cmd_table->cfis;
    fis->fis_type = 0x27;
    fis->pm_port = 0;
    fis->command = ATA_CMD_READ_DMA;
    fis->lba = lba & 0xFFFFFF;
    fis->lba_hi = (lba >> 24) & 0xFFFFFF;
    fis->count = count & 0xFFFF;

    uint32_t phys_buf = (uint32_t)buffer;
    if (phys_buf >= 0xC0000000) phys_buf -= 0xC0000000;

    cmd_table->prdt[0].dba = phys_buf;
    cmd_table->prdt[0].dbc = (count * sector_size - 1) | (1 << 31);

    ahci_port_write(port, AHCI_PORT_CI, 1 << slot);

    while (ahci_port_read(port, AHCI_PORT_CI) & (1 << slot));

    fis_reg_d2h_t *rfis = (fis_reg_d2h_t *)((uint8_t *)port->recv_fis + 0x20);
    (void)rfis;

    return count * sector_size;
}

int ahci_write(ahci_port_t *port, uint64_t lba, uint32_t count, const void *buffer) {
    if (!port->present) return -1;

    uint32_t sector_size = AHCI_SECTOR_SIZE;

    int slot = ahci_find_free_slot(port);
    if (slot < 0) return -1;

    ahci_cmd_hdr_t *cmd_hdr = &port->cmd_hdrs[slot];
    memset(cmd_hdr, 0, sizeof(ahci_cmd_hdr_t));
    cmd_hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    cmd_hdr->write = 1;
    cmd_hdr->prdt_length = 1;

    ahci_cmd_table_t *cmd_table = (ahci_cmd_table_t *)((uint32_t)cmd_hdr->ctba);
    if (!cmd_table) {
        cmd_table = (ahci_cmd_table_t *)kmalloc(sizeof(ahci_cmd_table_t));
        memset(cmd_table, 0, sizeof(ahci_cmd_table_t));
        uint32_t phys = (uint32_t)cmd_table;
        if (phys >= 0xC0000000) phys -= 0xC0000000;
        cmd_hdr->ctba = phys;
        cmd_hdr->ctba_hi = 0;
    }
    memset(cmd_table, 0, sizeof(ahci_cmd_table_t));

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)cmd_table->cfis;
    fis->fis_type = 0x27;
    fis->pm_port = 0;
    fis->command = ATA_CMD_WRITE_DMA;
    fis->lba = lba & 0xFFFFFF;
    fis->lba_hi = (lba >> 24) & 0xFFFFFF;
    fis->count = count & 0xFFFF;

    uint32_t phys_buf = (uint32_t)buffer;
    if (phys_buf >= 0xC0000000) phys_buf -= 0xC0000000;

    memcpy((void *)(cmd_hdr->ctba + sizeof(ahci_cmd_table_t) - sizeof(ahci_prdt_t)), buffer, count * sector_size);

    cmd_table->prdt[0].dba = phys_buf;
    cmd_table->prdt[0].dbc = (count * sector_size - 1) | (1 << 31);

    ahci_port_write(port, AHCI_PORT_CI, 1 << slot);

    while (ahci_port_read(port, AHCI_PORT_CI) & (1 << slot));

    return count * sector_size;
}

void ahci_probe_port(ahci_ctrl_t *ctrl, int port_num) {
    ahci_port_t *port = &ctrl->ports[port_num];
    port->port_base = ctrl->mmio + 0x100 + port_num * 0x80;

    uint32_t ssts = ahci_port_read(port, AHCI_PORT_SSTS);
    uint32_t det = ssts & 0x0F;
    uint32_t ipm = (ssts >> 8) & 0x0F;
    uint32_t sig = ahci_port_read(port, AHCI_PORT_SIG);
    uint32_t cmd = ahci_port_read(port, AHCI_PORT_CMD);

    (void)cmd;

    if (det != 3 || ipm != 1) return;

    port->present = 1;

    switch (sig) {
        case 0xEB140101: port->port_type = AHCI_PORT_SATA;    break;
        case 0xEB140102: case 0x96690101: port->port_type = AHCI_PORT_SATAPI; break;
        default:         port->port_type = AHCI_PORT_SATA;    break;
    }

    ahci_stop_cmd(port);

    uint32_t clb = ahci_port_read(port, AHCI_PORT_CLB);
    if (!clb) {
        clb = (uint32_t)pmem_alloc_page();
        memset((void *)clb, 0, 4096);
        if (clb >= 0xC0000000) clb -= 0xC0000000;
        port->clb = clb;
        port->cmd_hdrs = (ahci_cmd_hdr_t *)(clb + 0xC0000000);
        ahci_port_write(port, AHCI_PORT_CLB, clb);
        ahci_port_write(port, AHCI_PORT_CLBU, 0);
    }

    uint32_t fb = ahci_port_read(port, AHCI_PORT_FB);
    if (!fb) {
        fb = (uint32_t)pmem_alloc_page();
        memset((void *)fb, 0, 4096);
        if (fb >= 0xC0000000) fb -= 0xC0000000;
        port->fb = fb;
        port->recv_fis = (fis_reg_d2h_t *)(fb + 0xC0000000);
        ahci_port_write(port, AHCI_PORT_FB, fb);
        ahci_port_write(port, AHCI_PORT_FBU, 0);
    }

    ahci_port_write(port, AHCI_PORT_ERR, 0xFFFFFFFF);
    ahci_port_write(port, AHCI_PORT_IE, 0x00000000);

    ahci_start_cmd(port);

    uint32_t tfd = ahci_port_read(port, AHCI_PORT_TFD);
    (void)tfd;

    /* Send IDENTIFY command to get size */
    uint16_t *ident_buf = (uint16_t *)pmem_alloc_page();
    uint32_t ident_phys = (uint32_t)ident_buf;
    if (ident_phys >= 0xC0000000) ident_phys -= 0xC0000000;
    memset(ident_buf, 0, 512);

    if (port->port_type == AHCI_PORT_SATA) {
        int slot = ahci_find_free_slot(port);
        if (slot >= 0) {
            ahci_cmd_hdr_t *cmd_hdr = &port->cmd_hdrs[slot];
            memset(cmd_hdr, 0, sizeof(ahci_cmd_hdr_t));
            cmd_hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
            cmd_hdr->prdt_length = 1;

            ahci_cmd_table_t *cmd_table = (ahci_cmd_table_t *)kmalloc(sizeof(ahci_cmd_table_t));
            memset(cmd_table, 0, sizeof(ahci_cmd_table_t));
            uint32_t ct_phys = (uint32_t)cmd_table;
            if (ct_phys >= 0xC0000000) ct_phys -= 0xC0000000;

            cmd_hdr->ctba = ct_phys;
            cmd_hdr->ctba_hi = 0;

            fis_reg_h2d_t *fis = (fis_reg_h2d_t *)cmd_table->cfis;
            fis->fis_type = 0x27;
            fis->command = ATA_CMD_IDENTIFY;

            cmd_table->prdt[0].dba = ident_phys;
            cmd_table->prdt[0].dbc = 511 | (1 << 31);

            ahci_port_write(port, AHCI_PORT_CI, 1 << slot);
            while (ahci_port_read(port, AHCI_PORT_CI) & (1 << slot));

            port->num_sectors = *(uint64_t *)&ident_buf[100];
            if (port->num_sectors == 0) {
                port->num_sectors = *(uint32_t *)&ident_buf[60];
            }

            kfree(cmd_table);
        }
    }

    pmem_free_page((void *)((uint32_t)ident_buf & ~0xFFF));
}

int ahci_init(void) {
    pci_device_t *dev = NULL;
    uint32_t mmio_base = 0;

    if (ahci_find_ctrl(&dev, &mmio_base) < 0) {
        screen_term_write("[AHCI] No AHCI controller found\n");
        return -1;
    }

    memset(&ahci_ctrl, 0, sizeof(ahci_ctrl));

    uint32_t mmio_phys = mmio_base & ~0xF;
    ahci_ctrl.mmio = (volatile uint32_t *)(mmio_phys + 0xC0000000);
    ahci_ctrl.present = 1;

    uint32_t ghc = ahci_ctrl.mmio[AHCI_GHC / 4];
    ghc |= AHCI_GHC_AE;
    ahci_ctrl.mmio[AHCI_GHC / 4] = ghc;

    uint32_t pi = ahci_ctrl.mmio[AHCI_PI / 4];
    ahci_ctrl.num_ports = 0;

    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (pi & (1 << i)) {
            ahci_probe_port(&ahci_ctrl, i);
            ahci_ctrl.num_ports++;
        }
    }

    screen_term_write("[AHCI] AHCI controller initialized, ");
    char buf[8];
    int2str(ahci_ctrl.num_ports, buf);
    screen_term_write(buf);
    screen_term_write(" ports\n");

    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (ahci_ctrl.ports[i].present) {
            screen_term_write("  Port ");
            int2str(i, buf);
            screen_term_write(buf);
            screen_term_write(": SATA, ");
            uint32_t sectors_low = (uint32_t)(ahci_ctrl.ports[i].num_sectors);
            int2str(sectors_low * 512 / 1024 / 1024, buf);
            screen_term_write(buf);
            screen_term_write(" MB\n");
        }
    }

    return 0;
}

ahci_port_t *ahci_get_port(int port_num) {
    if (port_num < 0 || port_num >= AHCI_MAX_PORTS) return NULL;
    if (!ahci_ctrl.ports[port_num].present) return NULL;
    return &ahci_ctrl.ports[port_num];
}
