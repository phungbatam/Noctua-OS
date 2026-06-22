#include "keyboard.h"
#include "mouse.h"
#include "ports.h"

static int shift = 0;
static int ctrl = 0;

static const char normal[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char shifted[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

void keyboard_init(void) {
    while (inb(0x64) & 0x01) inb(0x60);
    shift = 0;
    ctrl = 0;
}

int keyboard_ctrl(void) { return ctrl; }

int keyboard_getchar(void) {
    static int extended = 0;

    while (1) {
        while (!(inb(0x64) & 0x01));

        unsigned char status = inb(0x64);
        unsigned char sc = inb(0x60);

        if (status & 0x20) {
            mouse_handle_byte(sc);
            if (mouse_has_click()) return K_MOUSE_L;
            continue;
        }

        if (sc == 0xE0) { extended = 1; continue; }

        if (extended) {
            extended = 0;
            if (sc & 0x80) continue;
            switch (sc) {
                case 0x48: return K_UP;
                case 0x50: return K_DOWN;
                case 0x4B: return K_LEFT;
                case 0x4D: return K_RIGHT;
                case 0x47: return K_HOME;
                case 0x4F: return K_END;
                case 0x49: return K_PGUP;
                case 0x51: return K_PGDN;
                case 0x53: return K_DEL;
                case 0x52: return K_INS;
                default: continue;
            }
        }

        if (sc == 0x2A || sc == 0x36) { shift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift = 0; continue; }

        if (sc == 0x1D) { ctrl = 1; continue; }
        if (sc == 0x9D) { ctrl = 0; continue; }

        if (sc == 0x3B) return K_F1;
        if (sc == 0x3C) return K_F2;

        if (sc & 0x80) continue;
        if (sc >= sizeof(normal)) continue;

        char c = shift ? shifted[sc] : normal[sc];

        /* Ctrl+C */
        if (ctrl && c == 'c') return K_CTRL_C;

        if (c) return c;
    }
}

// Non-blocking version - handles input without waiting
int keyboard_getchar_nb(void) {
    static int extended = 0;

    // Check if data is available (bit 0 of port 0x64)
    if (!(inb(0x64) & 0x01)) {
        return 0;  // No data available
    }

    unsigned char status = inb(0x64);
    unsigned char sc = inb(0x60);

    // Handle mouse data
    if (status & 0x20) {
        mouse_handle_byte(sc);
        if (mouse_has_click()) return K_MOUSE_L;
        return 0;
    }

    // Handle extended key prefix
    if (sc == 0xE0) {
        extended = 1;
        return 0;
    }

    // Handle extended keys
    if (extended) {
        extended = 0;
        if (sc & 0x80) return 0;
        switch (sc) {
            case 0x48: return K_UP;
            case 0x50: return K_DOWN;
            case 0x4B: return K_LEFT;
            case 0x4D: return K_RIGHT;
            case 0x47: return K_HOME;
            case 0x4F: return K_END;
            case 0x49: return K_PGUP;
            case 0x51: return K_PGDN;
            case 0x53: return K_DEL;
            case 0x52: return K_INS;
            default: return 0;
        }
    }

    // Handle shift
    if (sc == 0x2A || sc == 0x36) { shift = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return 0; }

    // Handle ctrl
    if (sc == 0x1D) { ctrl = 1; return 0; }
    if (sc == 0x9D) { ctrl = 0; return 0; }

    // Handle function keys
    if (sc == 0x3B) return K_F1;
    if (sc == 0x3C) return K_F2;

    // Ignore key releases
    if (sc & 0x80) return 0;
    if (sc >= sizeof(normal)) return 0;

    // Return character
    char c = shift ? shifted[sc] : normal[sc];

    // Ctrl+C
    if (ctrl && c == 'c') return K_CTRL_C;

    return c;
}
