#ifndef TVN_FLOPPY_H
#define TVN_FLOPPY_H

#include <stdint.h>

#define FLOPPY_DOR  0x3F2
#define FLOPPY_MSR  0x3F4
#define FLOPPY_FIFO 0x3F5
#define FLOPPY_CTRL 0x3F7

#define FLOPPY_CMD_SPECIFY       0x03
#define FLOPPY_CMD_WRITE         0xC5
#define FLOPPY_CMD_READ          0xE6
#define FLOPPY_CMD_RECALIBRATE   0x07
#define FLOPPY_CMD_SENSE_INT     0x08
#define FLOPPY_CMD_SEEK          0x0F
#define FLOPPY_CMD_VERSION       0x10
#define FLOPPY_CMD_CONFIGURE     0x13
#define FLOPPY_CMD_LOCK          0x94

#define FLOPPY_MSR_RQM           0x80
#define FLOPPY_MSR_DIO           0x40
#define FLOPPY_MSR_CB            0x10
#define FLOPPY_MSR_ACTIVE0       0x01
#define FLOPPY_MSR_ACTIVE1       0x02
#define FLOPPY_MSR_ACTIVE2       0x04
#define FLOPPY_MSR_ACTIVE3       0x08

#define FLOPPY_DOR_DRIVE0        0x00
#define FLOPPY_DOR_DRIVE1        0x01
#define FLOPPY_DOR_DRIVE2        0x02
#define FLOPPY_DOR_DRIVE3        0x03
#define FLOPPY_DOR_RESET         0x04
#define FLOPPY_DOR_DMA           0x08
#define FLOPPY_DOR_MOTOR0        0x10
#define FLOPPY_DOR_MOTOR1        0x20
#define FLOPPY_DOR_MOTOR2        0x40
#define FLOPPY_DOR_MOTOR3        0x80

#define FLOPPY_DRIVE_NONE    0
#define FLOPPY_DRIVE_360K    1
#define FLOPPY_DRIVE_720K    2
#define FLOPPY_DRIVE_1_2M    3
#define FLOPPY_DRIVE_1_44M   4
#define FLOPPY_DRIVE_2_88M   5

typedef struct {
    int present;
    int drive_number;
    int type;
    const char *type_name;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
} floppy_device_t;

#define MAX_FLOPPY_DRIVES 2

int floppy_init(void);
int floppy_device_count(void);
floppy_device_t *floppy_get_device(int index);
int floppy_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buffer);
int floppy_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buffer);
void floppy_motor_on(uint8_t drive);
void floppy_motor_off(uint8_t drive);

#endif
