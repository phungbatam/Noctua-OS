#include "pcspkr.h"
#include "ports.h"
#include "pit.h"

#define PIT_FREQ 1193182

static int pcspkr_initialized = 0;

void pcspkr_init(void) {
    pcspkr_initialized = 1;
    pcspkr_off();
}

void pcspkr_on(uint32_t frequency) {
    if (!pcspkr_initialized) pcspkr_init();
    if (frequency == 0) {
        pcspkr_off();
        return;
    }

    uint32_t divisor = PIT_FREQ / frequency;

    outb(PCSPKR_PORT_CMD, PCSPKR_CMD_SEL2);
    outb(PCSPKR_PORT_DATA, divisor & 0xFF);
    outb(PCSPKR_PORT_DATA, (divisor >> 8) & 0xFF);

    uint8_t tmp = inb(PCSPKR_PORT_GATE);
    outb(PCSPKR_PORT_GATE, tmp | 0x03);
}

void pcspkr_off(void) {
    uint8_t tmp = inb(PCSPKR_PORT_GATE);
    outb(PCSPKR_PORT_GATE, tmp & 0xFC);
}

void pcspkr_beep(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
        pit_sleep(duration_ms);
        return;
    }
    pcspkr_on(frequency);
    pit_sleep(duration_ms);
    pcspkr_off();
}

void pcspkr_play_note(int note, int octave, uint32_t duration_ms) {
    if (note == NOTE_REST || note == 0) {
        pit_sleep(duration_ms);
        return;
    }
    uint32_t freq = note;
    for (int i = 4; i < octave; i++) freq *= 2;
    for (int i = 4; i > octave; i--) freq /= 2;
    pcspkr_beep(freq, duration_ms);
}

void pcspkr_silence(void) {
    pcspkr_off();
}
