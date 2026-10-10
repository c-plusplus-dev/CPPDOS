#ifndef OS_KERNEL_MAIN_C
#define OS_KERNEL_MAIN_C

#include <stdint.h>
#include <stddef.h>

/*
 * Keep this structure identical to the BootInfo definition
 * in boot/main.c. The framebuffer is a UEFI GOP framebuffer
 * using 32-bit RGBX or BGRX pixels.
 */
typedef struct {
    void *memory_map;
    uint64_t memory_map_size;
    uint64_t descriptor_size;
    uint32_t descriptor_version;

    uint64_t framebuffer_base;
    uint64_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t pixels_per_scanline;
    uint32_t framebuffer_pixel_format;
} BootInfo;

#define GOP_PIXEL_RGBX 0U
#define GOP_PIXEL_BGRX 1U

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

static const Color COLOR_BACKGROUND = { 13, 13, 15 };
static const Color COLOR_PANEL      = { 21, 21, 25 };
static const Color COLOR_LIME       = { 184, 255, 92 };
static const Color COLOR_WHITE      = { 241, 240, 235 };
static const Color COLOR_MUTED      = { 145, 147, 140 };
static const Color COLOR_LINE       = { 49, 53, 43 };

/* =========================================================
   COM1 serial output
   ========================================================= */

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x03);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);
}

static void serial_putc(char c) {
    while ((inb(0x3FD) & 0x20) == 0) { }
    outb(0x3F8, (uint8_t)c);
}

static void serial_write(const char *text) {
    while (*text) serial_putc(*text++);
}

static void serial_write_u64(uint64_t value) {
    char buffer[20];
    unsigned int count = 0;

    if (value == 0) {
        serial_putc('0');
        return;
    }

    while (value > 0) {
        buffer[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (count > 0) serial_putc(buffer[--count]);
}

/* =========================================================
   Direct framebuffer drawing
   ========================================================= */

typedef struct {
    volatile uint8_t *bytes;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_pixels;
    uint32_t pixel_format;
    uint64_t size;
} Framebuffer;

static Framebuffer g_framebuffer;

static int framebuffer_init(const BootInfo *boot_info) {
    if (boot_info == 0 ||
        boot_info->framebuffer_base == 0 ||
        boot_info->framebuffer_width == 0 ||
        boot_info->framebuffer_height == 0 ||
        boot_info->pixels_per_scanline < boot_info->framebuffer_width ||
        (boot_info->framebuffer_pixel_format != GOP_PIXEL_RGBX &&
         boot_info->framebuffer_pixel_format != GOP_PIXEL_BGRX)) {
        return 0;
    }

    uint64_t required =
        (uint64_t)boot_info->pixels_per_scanline *
        (uint64_t)boot_info->framebuffer_height * 4ULL;

    if (required > boot_info->framebuffer_size) return 0;

    g_framebuffer.bytes = (volatile uint8_t *)(uintptr_t)boot_info->framebuffer_base;
    g_framebuffer.width = boot_info->framebuffer_width;
    g_framebuffer.height = boot_info->framebuffer_height;
    g_framebuffer.pitch_pixels = boot_info->pixels_per_scanline;
    g_framebuffer.pixel_format = boot_info->framebuffer_pixel_format;
    g_framebuffer.size = boot_info->framebuffer_size;
    return 1;
}

static void put_pixel(int32_t x, int32_t y, Color color) {
    if (g_framebuffer.bytes == 0 ||
        x < 0 || y < 0 ||
        (uint32_t)x >= g_framebuffer.width ||
        (uint32_t)y >= g_framebuffer.height) {
        return;
    }

    uint64_t offset =
        ((uint64_t)(uint32_t)y * g_framebuffer.pitch_pixels +
         (uint32_t)x) * 4ULL;

    if (offset + 3 >= g_framebuffer.size) return;

    volatile uint8_t *pixel = g_framebuffer.bytes + offset;

    if (g_framebuffer.pixel_format == GOP_PIXEL_RGBX) {
        pixel[0] = color.r;
        pixel[1] = color.g;
        pixel[2] = color.b;
    } else {
        pixel[0] = color.b;
        pixel[1] = color.g;
        pixel[2] = color.r;
    }
    pixel[3] = 0;
}

static void fill_rect(
    int32_t x, int32_t y, int32_t width, int32_t height, Color color
) {
    if (width <= 0 || height <= 0) return;

    int64_t x_end = (int64_t)x + width;
    int64_t y_end = (int64_t)y + height;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x_end > g_framebuffer.width) x_end = g_framebuffer.width;
    if (y_end > g_framebuffer.height) y_end = g_framebuffer.height;

    for (int32_t py = y; py < y_end; ++py) {
        for (int32_t px = x; px < x_end; ++px) {
            put_pixel(px, py, color);
        }
    }
}

/*
 * Tiny 5x7 bitmap font. Each byte represents one vertical
 * column; bit 0 is the top pixel. Text is uppercase so the
 * font stays compact and requires no external assets.
 */
static const uint8_t *glyph_for(char c) {
    static const uint8_t blank[5] = { 0, 0, 0, 0, 0 };
    static const uint8_t A[5] = { 0x7E, 0x11, 0x11, 0x11, 0x7E };
    static const uint8_t B[5] = { 0x7F, 0x49, 0x49, 0x49, 0x36 };
    static const uint8_t C[5] = { 0x3E, 0x41, 0x41, 0x41, 0x22 };
    static const uint8_t D[5] = { 0x7F, 0x41, 0x41, 0x22, 0x1C };
    static const uint8_t E[5] = { 0x7F, 0x49, 0x49, 0x49, 0x41 };
    static const uint8_t F[5] = { 0x7F, 0x09, 0x09, 0x09, 0x01 };
    static const uint8_t G[5] = { 0x3E, 0x41, 0x49, 0x49, 0x7A };
    static const uint8_t H[5] = { 0x7F, 0x08, 0x08, 0x08, 0x7F };
    static const uint8_t I[5] = { 0x00, 0x41, 0x7F, 0x41, 0x00 };
    static const uint8_t J[5] = { 0x20, 0x40, 0x41, 0x3F, 0x01 };
    static const uint8_t K[5] = { 0x7F, 0x08, 0x14, 0x22, 0x41 };
    static const uint8_t L[5] = { 0x7F, 0x40, 0x40, 0x40, 0x40 };
    static const uint8_t M[5] = { 0x7F, 0x02, 0x0C, 0x02, 0x7F };
    static const uint8_t N[5] = { 0x7F, 0x04, 0x08, 0x10, 0x7F };
    static const uint8_t O[5] = { 0x3E, 0x41, 0x41, 0x41, 0x3E };
    static const uint8_t P[5] = { 0x7F, 0x09, 0x09, 0x09, 0x06 };
    static const uint8_t Q[5] = { 0x3E, 0x41, 0x51, 0x21, 0x5E };
    static const uint8_t R[5] = { 0x7F, 0x09, 0x19, 0x29, 0x46 };
    static const uint8_t S[5] = { 0x46, 0x49, 0x49, 0x49, 0x31 };
    static const uint8_t T[5] = { 0x01, 0x01, 0x7F, 0x01, 0x01 };
    static const uint8_t U[5] = { 0x3F, 0x40, 0x40, 0x40, 0x3F };
    static const uint8_t V[5] = { 0x1F, 0x20, 0x40, 0x20, 0x1F };
    static const uint8_t W[5] = { 0x3F, 0x40, 0x38, 0x40, 0x3F };
    static const uint8_t X[5] = { 0x63, 0x14, 0x08, 0x14, 0x63 };
    static const uint8_t Y[5] = { 0x07, 0x08, 0x70, 0x08, 0x07 };
    static const uint8_t Z[5] = { 0x61, 0x51, 0x49, 0x45, 0x43 };
    static const uint8_t D0[5] = { 0x3E, 0x51, 0x49, 0x45, 0x3E };
    static const uint8_t D1[5] = { 0x00, 0x42, 0x7F, 0x40, 0x00 };
    static const uint8_t D2[5] = { 0x42, 0x61, 0x51, 0x49, 0x46 };
    static const uint8_t D3[5] = { 0x21, 0x41, 0x45, 0x4B, 0x31 };
    static const uint8_t D4[5] = { 0x18, 0x14, 0x12, 0x7F, 0x10 };
    static const uint8_t D5[5] = { 0x27, 0x45, 0x45, 0x45, 0x39 };
    static const uint8_t D6[5] = { 0x3C, 0x4A, 0x49, 0x49, 0x30 };
    static const uint8_t D7[5] = { 0x01, 0x71, 0x09, 0x05, 0x03 };
    static const uint8_t D8[5] = { 0x36, 0x49, 0x49, 0x49, 0x36 };
    static const uint8_t D9[5] = { 0x06, 0x49, 0x49, 0x29, 0x1E };
    static const uint8_t DASH[5] = { 0x08, 0x08, 0x08, 0x08, 0x08 };
    static const uint8_t COLON[5] = { 0x00, 0x36, 0x36, 0x00, 0x00 };
    static const uint8_t PERIOD[5] = { 0x00, 0x60, 0x60, 0x00, 0x00 };
    static const uint8_t COMMA[5] = { 0x00, 0x40, 0x20, 0x00, 0x00 };
    static const uint8_t EXCLAMATION[5] = { 0x00, 0x00, 0x5F, 0x00, 0x00 };
    static const uint8_t SLASH[5] = { 0x20, 0x10, 0x08, 0x04, 0x02 };
    static const uint8_t QUESTION[5] = { 0x02, 0x01, 0x51, 0x09, 0x06 };

    switch (c) {
        case 'A': return A; case 'B': return B; case 'C': return C;
        case 'D': return D; case 'E': return E; case 'F': return F;
        case 'G': return G; case 'H': return H; case 'I': return I;
        case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O;
        case 'P': return P; case 'Q': return Q; case 'R': return R;
        case 'S': return S; case 'T': return T; case 'U': return U;
        case 'V': return V; case 'W': return W; case 'X': return X;
        case 'Y': return Y; case 'Z': return Z;
        case '0': return D0; case '1': return D1; case '2': return D2;
        case '3': return D3; case '4': return D4; case '5': return D5;
        case '6': return D6; case '7': return D7; case '8': return D8;
        case '9': return D9; case '-': return DASH; case ':': return COLON;
        case '.': return PERIOD; case ',': return COMMA;
        case '!': return EXCLAMATION; case '/': return SLASH;
        case '?': return QUESTION; default: return blank;
    }
}

static void draw_char(
    int32_t x, int32_t y, char c, uint32_t scale, Color color
) {
    const uint8_t *glyph = glyph_for(c);

    for (uint32_t column = 0; column < 5; ++column) {
        for (uint32_t row = 0; row < 7; ++row) {
            if ((glyph[column] & (1U << row)) == 0) continue;
            fill_rect(x + (int32_t)(column * scale),
                      y + (int32_t)(row * scale),
                      (int32_t)scale, (int32_t)scale, color);
        }
    }
}

static void draw_text(
    int32_t x, int32_t y, const char *text, uint32_t scale, Color color
) {
    int32_t origin_x = x;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            x = origin_x;
            y += (int32_t)(9 * scale);
            continue;
        }

        draw_char(x, y, c, scale, color);
        x += (int32_t)(6 * scale);
    }
}

static void draw_boot_screen(void) {
    uint32_t width = g_framebuffer.width;
    uint32_t height = g_framebuffer.height;

    fill_rect(0, 0, (int32_t)width, (int32_t)height, COLOR_BACKGROUND);

    /* Left accent stripe and restrained panel layout. */
    fill_rect(0, 0, 8, (int32_t)height, COLOR_LIME);

    int32_t margin = width >= 900 ? 72 : 24;
    int32_t top = height >= 700 ? 76 : 32;
    int32_t scale = width >= 900 ? 5 : 3;

    fill_rect(margin, top, (int32_t)width - margin * 2, 2, COLOR_LINE);

    draw_text(margin, top + 36, "CPPDOS", (uint32_t)scale, COLOR_LIME);
    draw_text(margin, top + 36 + 7 * scale + 24,
              "A SMALL OS, BUILT FROM SCRATCH",
              width >= 900 ? 2U : 1U, COLOR_WHITE);

    int32_t panel_y = top + 170;
    int32_t panel_h = height > (uint32_t)(panel_y + 150) ? 132 : 90;
    int32_t panel_w = (int32_t)width - margin * 2;

    fill_rect(margin, panel_y, panel_w, panel_h, COLOR_PANEL);
    fill_rect(margin, panel_y, 4, panel_h, COLOR_LIME);

    uint32_t body_scale = width >= 900 ? 2U : 1U;
    draw_text(margin + 24, panel_y + 20,
              "UEFI BOOT SUCCESSFUL", body_scale, COLOR_WHITE);
    draw_text(margin + 24, panel_y + 20 + 18 * (int32_t)body_scale,
              "KERNEL RUNNING", body_scale, COLOR_LIME);
    draw_text(margin + 24, panel_y + 20 + 36 * (int32_t)body_scale,
              "SERIAL: COM1", body_scale, COLOR_MUTED);

    int32_t footer_y = (int32_t)height - 36;
    if (footer_y > panel_y + panel_h + 20) {
        draw_text(margin, footer_y, "CPPDOS / EARLY KERNEL BUILD",
                  1, COLOR_MUTED);
    }
}

/* =========================================================
   Kernel entry point
   ========================================================= */

extern "C" [[noreturn]]
void kernel_main(BootInfo *boot_info) {
    serial_init();

    serial_write("\r\n");
    serial_write("================================\r\n");
    serial_write(" CPPDOS - C kernel\r\n");
    serial_write("================================\r\n");
    serial_write("Hello from the actual kernel!\r\n");

    if (boot_info != 0 && boot_info->descriptor_size != 0) {
        uint64_t descriptor_count =
            boot_info->memory_map_size / boot_info->descriptor_size;
        serial_write("UEFI memory descriptors: ");
        serial_write_u64(descriptor_count);
        serial_write("\r\n");
    }

    if (framebuffer_init(boot_info)) {
        serial_write("GOP framebuffer: ");
        serial_write_u64(g_framebuffer.width);
        serial_write(" x ");
        serial_write_u64(g_framebuffer.height);
        serial_write("\r\n");

        draw_boot_screen();
        serial_write("Graphical boot screen rendered.\r\n");
    } else {
        serial_write("GOP framebuffer unavailable or unsupported.\r\n");
    }

    serial_write("Kernel is alive. Halting CPU when idle.\r\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

#endif
