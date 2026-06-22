#ifndef NOCTUA_AHCI_H
#define NOCTUA_AHCI_H

#include <stdint.h>

#define AHCI_MAX_PORTS     32
#define AHCI_CMD_SLOTS     32
#define AHCI_SECTOR_SIZE   512

/* HBA memory register offsets */
#define AHCI_CAP           0x00
#define AHCI_GHC           0x04
#define AHCI_IS            0x08
#define AHCI_PI            0x0C
#define AHCI_VS            0x10
#define AHCI_CCC_CTL       0x14
#define AHCI_CCC_PORTS     0x18
#define AHCI_EM_LOC        0x1C
#define AHCI_EM_CTL        0x20
#define AHCI_CAP2          0x24
#define AHCI_BOHC          0x28

/* GHC bits */
#define AHCI_GHC_AE        0x80000000
#define AHCI_GHC_IE        0x00000002
#define AHCI_GHC_HR        0x00000001

/* Port register offsets */
#define AHCI_PORT_CLB      0x00
#define AHCI_PORT_CLBU     0x04
#define AHCI_PORT_FB       0x08
#define AHCI_PORT_FBU      0x0C
#define AHCI_PORT_IS        0x10
#define AHCI_PORT_IE        0x14
#define AHCI_PORT_CMD       0x18
#define AHCI_PORT_TFD       0x20
#define AHCI_PORT_SIG       0x24
#define AHCI_PORT_SSTS      0x28
#define AHCI_PORT_SCTL      0x2C
#define AHCI_PORT_SERR      0x30
#define AHCI_PORT_SACT      0x34
#define AHCI_PORT_CI        0x38
#define AHCI_PORT_ERR       0x3C

/* CMD bits */
#define AHCI_CMD_ST         0x0001
#define AHCI_CMD_FRE        0x0010
#define AHCI_CMD_FR         0x4000
#define AHCI_CMD_CR         0x8000

/* SSTS bits (SATA status) */
#define AHCI_SSTS_DET_PRESENT 0x03
#define AHCI_SSTS_IPM_ACTIVE  0x0100

/* ATA commands */
#define ATA_CMD_READ_DMA  0xC8
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_IDENTIFY  0xEC

/* Port types */
#define AHCI_PORT_NOTHING  0
#define AHCI_PORT_SATA     1
#define AHCI_PORT_SATAPI   2
#define AHCI_PORT_SEMB     3
#define AHCI_PORT_PM       4

/* Received FIS structure */
typedef struct {
    uint8_t  fis_type;
    uint8_t  flags;
    uint8_t  status;
    uint8_t  error;
    uint32_t lba;
    uint32_t lba_hi;
    uint16_t count;
    uint16_t reserved;
    uint32_t control;
    uint32_t reserved2[4];
} __attribute__((packed)) fis_reg_d2h_t;

typedef struct {
    uint8_t  fis_type;
    uint8_t  pm_port;
    uint8_t  command;
    uint8_t  features;
    uint32_t lba;
    uint32_t lba_hi;
    uint16_t count;
    uint8_t  icc;
    uint8_t  control;
    uint32_t reserved;
} __attribute__((packed)) fis_reg_h2d_t;

/* Command header */
typedef struct {
    uint16_t cfis_length : 5;
    uint16_t atapi : 1;
    uint16_t write : 1;
    uint16_t prefetchable : 1;
    uint16_t reset : 1;
    uint16_t bist : 1;
    uint16_t clear_busy : 1;
    uint16_t reserved : 1;
    uint16_t cfl : 4;     /* command FIS length in dwords */
    uint16_t reserved2 : 7;
    uint16_t prdt_length;  /* number of PRD entries */
    uint32_t prdbc;        /* bytes transferred */
    uint32_t ctba;         /* command table base address (low) */
    uint32_t ctba_hi;      /* command table base address (high) */
    uint32_t reserved3[4];
} __attribute__((packed)) ahci_cmd_hdr_t;

/* Physical Region Descriptor */
typedef struct {
    uint32_t dba;     /* data base address */
    uint32_t dba_hi;  /* data base address high */
    uint32_t reserved;
    uint32_t dbc;     /* byte count (bits 0-21), 22: interrupt on completion */
} __attribute__((packed)) ahci_prdt_t;

/* Command table */
typedef struct {
    uint8_t       cfis[64];
    uint8_t       acmd[16];
    uint8_t       reserved[48];
    ahci_prdt_t   prdt[1];
} __attribute__((packed)) ahci_cmd_table_t;

/* Port structure */
typedef struct {
    volatile uint32_t *port_base;
    uint32_t   clb;
    uint32_t   fb;
    ahci_cmd_hdr_t   *cmd_hdrs;
    fis_reg_d2h_t    *recv_fis;
    int        present;
    int        port_type;
    uint64_t   num_sectors;
    int        is_atapi;
} ahci_port_t;

/* AHCI controller */
typedef struct {
    volatile uint32_t *mmio;
    ahci_port_t ports[AHCI_MAX_PORTS];
    int        num_ports;
    int        present;
} ahci_ctrl_t;

/* Functions */
int ahci_init(void);
int ahci_read(ahci_port_t *port, uint64_t lba, uint32_t count, void *buffer);
int ahci_write(ahci_port_t *port, uint64_t lba, uint32_t count, const void *buffer);
void ahci_probe_port(ahci_ctrl_t *ctrl, int port_num);

#endif
