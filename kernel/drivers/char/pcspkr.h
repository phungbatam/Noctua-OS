#ifndef TVN_PCSPKR_H
#define TVN_PCSPKR_H

#include <stdint.h>

#define PCSPKR_PORT_DATA  0x42
#define PCSPKR_PORT_CMD   0x43
#define PCSPKR_PORT_GATE  0x61

#define PCSPKR_CMD_SEL2   0xB6

void pcspkr_init(void);
void pcspkr_on(uint32_t frequency);
void pcspkr_off(void);
void pcspkr_beep(uint32_t frequency, uint32_t duration_ms);
void pcspkr_play_note(int note, int octave, uint32_t duration_ms);
void pcspkr_silence(void);

#define NOTE_REST 0
#define NOTE_C    261
#define NOTE_CS  277
#define NOTE_D   294
#define NOTE_DS  311
#define NOTE_E   329
#define NOTE_F   349
#define NOTE_FS  370
#define NOTE_G   392
#define NOTE_GS  415
#define NOTE_A   440
#define NOTE_AS  466
#define NOTE_B   494

#endif
