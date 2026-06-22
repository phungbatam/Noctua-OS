#include "floppy.h"
#include "ports.h"
#include "dma.h"
#include "cmos.h"
#include "string.h"
#include "pit.h"

static floppy_device_t floppy_drives[MAX_FLOPPY_DRIVES];
static int floppy_count = 0;
static uint8_t floppy_dor_value = 0;

static int floppy_calibrated = 0;

static const struct {
    int type;
    const char *name;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
} floppy_types[] = {
    { FLOPPY_DRIVE_NONE,  "None",     0,   0, 0  },
    { FLOPPY_DRIVE_360K,  "360 KB",   40,  2, 9  },
    { FLOPPY_DRIVE_720K,  "720 KB",   80,  2, 9  },
    { FLOPPY_DRIVE_1_2M,  "1.2 MB",   80,  2, 15 },
    { FLOPPY_DRIVE_1_44M, "1.44 MB",  80,  2, 18 },
    { FLOPPY_DRIVE_2_88M, "2.88 MB",  80,  2, 36 },
};

static void floppy_wait(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(FLOPPY_MSR) & FLOPPY_MSR_RQM) return;
    }
}

static uint8_t floppy_read_data(void) {
    floppy_wait();
    return inb(FLOPPY_FIFO);
}

static void floppy_write_data(uint8_t data) {
    floppy_wait();
    outb(FLOPPY_FIFO, data);
}

static void floppy_sense_interrupt(void) {
    outb(FLOPPY_FIFO, FLOPPY_CMD_SENSE_INT);
    uint8_t st0 = floppy_read_data();
    uint8_t cyl = floppy_read_data();
    (void)st0;
    (void)cyl;
}

void floppy_motor_on(uint8_t drive) {
    if (drive > 3) return;
    floppy_dor_value = (drive & 0x03) | FLOPPY_DOR_RESET | FLOPPY_DOR_DMA;
    if (drive == 0) floppy_dor_value |= FLOPPY_DOR_MOTOR0;
    else if (drive == 1) floppy_dor_value |= FLOPPY_DOR_MOTOR1;
    outb(FLOPPY_DOR, floppy_dor_value);
    pit_sleep(50);
}

void floppy_motor_off(uint8_t drive) {
    if (drive > 3) return;
    floppy_dor_value &= ~(FLOPPY_DOR_MOTOR0 << drive);
    outb(FLOPPY_DOR, floppy_dor_value);
}

static int floppy_recalibrate(uint8_t drive) {
    floppy_motor_on(drive);
    for (int retry = 0; retry < 3; retry++) {
        floppy_write_data(FLOPPY_CMD_RECALIBRATE);
        floppy_write_data(drive & 0x03);
        pit_sleep(100);
        floppy_sense_interrupt();
    }
    return 0;
}

static int floppy_seek(uint8_t drive, uint8_t cylinder, uint8_t head) {
    floppy_motor_on(drive);
    floppy_write_data(FLOPPY_CMD_SEEK);
    floppy_write_data((head << 2) | (drive & 0x03));
    floppy_write_data(cylinder);
    pit_sleep(50);
    floppy_sense_interrupt();
    return 0;
}

static int floppy_convert_lba(uint32_t lba, uint8_t *cylinder, uint8_t *head, uint8_t *sector, uint8_t drive) {
    floppy_device_t *dev = floppy_get_device(drive);
    if (!dev || !dev->present) return -1;

    uint8_t sectors = dev->sectors_per_track;
    uint8_t heads = dev->heads;

    *cylinder = lba / (sectors * heads);
    *head = (lba / sectors) % heads;
    *sector = (lba % sectors) + 1;
    return 0;
}

int floppy_init(void) {
    memset(floppy_drives, 0, sizeof(floppy_drives));

    floppy_dor_value = 0;
    outb(FLOPPY_DOR, 0x00);
    pit_sleep(10);
    outb(FLOPPY_DOR, FLOPPY_DOR_RESET);
    pit_sleep(10);

    floppy_dor_value = FLOPPY_DOR_RESET | FLOPPY_DOR_DMA;
    outb(FLOPPY_DOR, floppy_dor_value);

    floppy_sense_interrupt();

    floppy_write_data(FLOPPY_CMD_SPECIFY);
    floppy_write_data(0xDF);
    floppy_write_data(0x02);

    floppy_write_data(FLOPPY_CMD_CONFIGURE);
    floppy_write_data(0x00);
    floppy_write_data(0x00);

    uint8_t version = 0;
    floppy_write_data(FLOPPY_CMD_VERSION);
    floppy_write_data(0x00);
    version = floppy_read_data();

    if (version == 0x80 || version == 0x90) {
        uint8_t drive_types = cmos_read(CMOS_REG_DISK_DRIVE);
        
        for (int d = 0; d < 2; d++) {
            int type_num;
            if (d == 0) type_num = (drive_types >> 4) & 0x0F;
            else type_num = drive_types & 0x0F;

            if (type_num > 0 && type_num <= 5 && floppy_count < MAX_FLOPPY_DRIVES) {
                floppy_device_t *dev = &floppy_drives[floppy_count];
                dev->present = 1;
                dev->drive_number = d;
                dev->type = floppy_types[type_num].type;
                dev->type_name = floppy_types[type_num].name;
                dev->cylinders = floppy_types[type_num].cylinders;
                dev->heads = floppy_types[type_num].heads;
                dev->sectors_per_track = floppy_types[type_num].sectors;
                floppy_count++;
            }
        }

        if (floppy_count > 0) {
            floppy_recalibrate(0);
            floppy_calibrated = 1;
        }
    }

    return floppy_count;
}

int floppy_device_count(void) {
    return floppy_count;
}

floppy_device_t *floppy_get_device(int index) {
    if (index < 0 || index >= floppy_count) return 0;
    return &floppy_drives[index];
}

int floppy_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buffer) {
    if (!buffer || !floppy_calibrated) return -1;

    floppy_device_t *dev = floppy_get_device(drive);
    if (!dev || !dev->present) return -1;

    uint8_t cylinder, head, sector;
    if (floppy_convert_lba(lba, &cylinder, &head, &sector, drive) < 0) return -1;

    dma_prepare_floppy((uint8_t *)buffer, count);

    floppy_motor_on(drive);
    floppy_seek(drive, cylinder, head);

    floppy_write_data(FLOPPY_CMD_READ);
    floppy_write_data((head << 2) | (drive & 0x03));
    floppy_write_data(cylinder);
    floppy_write_data(head);
    floppy_write_data(sector);
    floppy_write_data(dev->sectors_per_track);
    floppy_write_data(0x1B);
    floppy_write_data(0xFF);

    pit_sleep(200);

    floppy_sense_interrupt();
    floppy_motor_off(drive);

    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();

    return 0;
}

int floppy_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buffer) {
    if (!buffer || !floppy_calibrated) return -1;

    floppy_device_t *dev = floppy_get_device(drive);
    if (!dev || !dev->present) return -1;

    uint8_t cylinder, head, sector;
    if (floppy_convert_lba(lba, &cylinder, &head, &sector, drive) < 0) return -1;

    dma_prepare_floppy((uint8_t *)buffer, count);

    floppy_motor_on(drive);
    floppy_seek(drive, cylinder, head);

    floppy_write_data(FLOPPY_CMD_WRITE);
    floppy_write_data((head << 2) | (drive & 0x03));
    floppy_write_data(cylinder);
    floppy_write_data(head);
    floppy_write_data(sector);
    floppy_write_data(dev->sectors_per_track);
    floppy_write_data(0x1B);
    floppy_write_data(0xFF);

    pit_sleep(200);

    floppy_sense_interrupt();
    floppy_motor_off(drive);

    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();
    (void)floppy_read_data();

    return 0;
}
