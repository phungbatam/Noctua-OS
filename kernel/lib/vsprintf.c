#include "vsprintf.h"
#include "string.h"

enum {
    ZEROPAD  = 1<<0,
    SIGN     = 1<<1,
    PLUS     = 1<<2,
    SPACE    = 1<<3,
    LEFT     = 1<<4,
    SPECIAL  = 1<<5,
    SMALL    = 1<<6,
};

struct fmt_spec {
    unsigned int flags;
    int field_width;
    int precision;
    int base;
};

static const char digits_small[] = "0123456789abcdef";
static const char digits_large[] = "0123456789ABCDEF";

static char *number(char *buf, char *end, unsigned long num, struct fmt_spec spec) {
    char sign_char = 0;
    unsigned long val = num;
    char tmp[34];
    int i = 0;

    (void)val;

    if (spec.flags & SIGN) {
        int snum = (int)num;
        if (snum < 0) {
            sign_char = '-';
            num = (unsigned long)(-snum);
        } else if (spec.flags & PLUS) {
            sign_char = '+';
        } else if (spec.flags & SPACE) {
            sign_char = ' ';
        }
    }

    const char *digits = (spec.flags & SMALL) ? digits_small : digits_large;

    if (spec.precision < 0) spec.precision = 1;

    do {
        tmp[i++] = digits[num % spec.base];
        num /= spec.base;
    } while (num);

    int len = i;
    int zeros = 0;
    if (spec.precision > len)
        zeros = spec.precision - len;

    int pfx_len = sign_char ? 1 : 0;
    if (spec.flags & SPECIAL) {
        if (spec.base == 16) pfx_len += 2;
    }
    int width = spec.field_width - pfx_len - zeros - len;
    if (width < 0) width = 0;

    if (!(spec.flags & LEFT)) {
        char pad = (spec.flags & ZEROPAD) ? '0' : ' ';
        if (pad == ' ') {
            while (width-- > 0 && buf < end) *buf++ = ' ';
        }
    }

    if (sign_char && buf < end) *buf++ = sign_char;

    if (spec.flags & SPECIAL) {
        if (spec.base == 16) {
            if (buf < end) *buf++ = '0';
            if (buf < end) *buf++ = 'x';
        }
    }

    while (zeros-- > 0 && buf < end) *buf++ = '0';
    while (--i >= 0 && buf < end) *buf++ = tmp[i];
    while (width-- > 0 && buf < end) *buf++ = ' ';

    return buf;
}

static char *string(char *buf, char *end, const char *s, struct fmt_spec spec) {
    if (!s) s = "(null)";

    int len = 0;
    const char *p = s;
    while (*p) { len++; p++; }

    if (spec.precision >= 0 && spec.precision < len)
        len = spec.precision;

    int width = spec.field_width - len;
    if (width < 0) width = 0;

    if (!(spec.flags & LEFT)) {
        while (width-- > 0 && buf < end) *buf++ = ' ';
    }

    for (int i = 0; i < len && buf < end; i++) {
        *buf++ = s[i];
    }

    while (width-- > 0 && buf < end) *buf++ = ' ';
    return buf;
}

int vsnprintf(char *buf, uint32_t size, const char *fmt, va_list args) {
    char *str = buf;
    char *end = buf + size;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (str < end) *str++ = *p;
            continue;
        }

        p++;
        struct fmt_spec spec;
        spec.flags = 0;
        spec.field_width = -1;
        spec.precision = -1;
        spec.base = 10;

        int done = 0;
        while (!done && *p) {
            switch (*p) {
                case '-': spec.flags |= LEFT; p++; break;
                case '+': spec.flags |= PLUS; p++; break;
                case ' ': spec.flags |= SPACE; p++; break;
                case '#': spec.flags |= SPECIAL; p++; break;
                case '0': spec.flags |= ZEROPAD; p++; break;
                default: done = 1; break;
            }
        }

        if (*p == '*') {
            spec.field_width = va_arg(args, int);
            if (spec.field_width < 0) {
                spec.flags |= LEFT;
                spec.field_width = -spec.field_width;
            }
            p++;
        } else {
            while (*p >= '0' && *p <= '9') {
                spec.field_width = (spec.field_width < 0) ? 0 : spec.field_width;
                spec.field_width = spec.field_width * 10 + (*p - '0');
                p++;
            }
        }

        if (*p == '.') {
            p++;
            spec.precision = 0;
            while (*p >= '0' && *p <= '9') {
                spec.precision = spec.precision * 10 + (*p - '0');
                p++;
            }
        }

        if (*p == 'l') { p++; if (*p == 'l') p++; }

        switch (*p) {
            case 'd':
            case 'i':
                spec.flags |= SIGN;
                str = number(str, end, (unsigned long)va_arg(args, int), spec);
                break;
            case 'u':
                str = number(str, end, va_arg(args, unsigned int), spec);
                break;
            case 'x':
                spec.flags |= SMALL;
                spec.base = 16;
                str = number(str, end, va_arg(args, unsigned int), spec);
                break;
            case 'X':
                spec.base = 16;
                str = number(str, end, va_arg(args, unsigned int), spec);
                break;
            case 's':
                str = string(str, end, va_arg(args, const char *), spec);
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                if (str < end) *str++ = c;
                break;
            }
            case 'p':
                spec.flags |= SMALL | SPECIAL;
                spec.base = 16;
                str = number(str, end, (unsigned long)va_arg(args, void *), spec);
                break;
            case '%':
                if (str < end) *str++ = '%';
                break;
            default:
                if (str < end) *str++ = '%';
                if (str < end && *p) *str++ = *p;
                break;
        }
    }

    if (str < end)
        *str = '\0';
    else if (size > 0)
        end[-1] = '\0';

    return str - buf;
}

int snprintf(char *buf, uint32_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, 0x7FFFFFFF, fmt, args);
    va_end(args);
    return ret;
}
