#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "hidden.h"
#include "logo.h"
#include "term.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MF_HIDDEN_FPS 60.0
#define MF_HIDDEN_SCALE 0.01
#define MF_HIDDEN_SPEED 0.02
#define MF_HIDDEN_GRADIENT " .+100"
#define MF_HIDDEN_COLUMN_GAP 3
#define MF_HIDDEN_SIZE_REFRESH_SEC 0.5
#define MF_HIDDEN_BUFFER_CAP (1u << 20)
#define MF_HIDDEN_FRAMETIME_S (1.0 / MF_HIDDEN_FPS)

struct mf_hidden_cell {
    unsigned char len;
    char bytes[4];
};

static volatile sig_atomic_t g_hidden_running = 1;
static struct termios g_hidden_orig_tio;
static int g_hidden_raw_enabled = 0;
static int g_hidden_scroll_region_set = 0;

static void mf_hidden_cell_set_char(struct mf_hidden_cell *cell, char c)
{
    if (cell == NULL) {
        return;
    }
    cell->len = 1;
    cell->bytes[0] = c;
}

static void mf_hidden_cell_set_utf8(struct mf_hidden_cell *cell, const char *bytes, size_t len)
{
    size_t copy_len;

    if (cell == NULL || bytes == NULL || len == 0) {
        mf_hidden_cell_set_char(cell, ' ');
        return;
    }

    copy_len = len;
    if (copy_len > sizeof(cell->bytes)) {
        copy_len = sizeof(cell->bytes);
    }
    memcpy(cell->bytes, bytes, copy_len);
    cell->len = (unsigned char)copy_len;
}

static void mf_hidden_on_signal(int sig)
{
    (void)sig;
    g_hidden_running = 0;
}

static void mf_hidden_show_cursor(void)
{
    fputs("\x1b[?25h\x1b[0m", stdout);
}

static void mf_hidden_hide_cursor(void)
{
    fputs("\x1b[?25l", stdout);
}

static void mf_hidden_disable_raw(void)
{
    if (g_hidden_raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_hidden_orig_tio);
        g_hidden_raw_enabled = 0;
    }
}

static void mf_hidden_enable_raw(void)
{
    struct termios tio;

    if (!isatty(STDIN_FILENO)) {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &g_hidden_orig_tio) == -1) {
        return;
    }

    tio = g_hidden_orig_tio;
    tio.c_lflag &= (unsigned int)~(ICANON | ECHO);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &tio) == 0) {
        g_hidden_raw_enabled = 1;
    }
}

static void mf_hidden_reset_scroll_region(void)
{
    if (g_hidden_scroll_region_set) {
        fputs("\x1b[r", stdout);
        g_hidden_scroll_region_set = 0;
    }
}

static void mf_hidden_cleanup(void)
{
    mf_hidden_disable_raw();
    mf_hidden_reset_scroll_region();
    mf_hidden_show_cursor();
    fflush(stdout);
}

static void mf_hidden_setup_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = mf_hidden_on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static unsigned int mf_hidden_hash_u32(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static double mf_hidden_hash_unit(unsigned int x)
{
    return mf_hidden_hash_u32(x) / 4294967295.0;
}

static size_t mf_hidden_utf8_glyph_len(const char *s)
{
    unsigned char c;

    if (s == NULL) {
        return 0;
    }

    c = (unsigned char)s[0];
    if (c == 0) {
        return 0;
    }
    if ((c & 0x80U) == 0) {
        return 1;
    }
    if ((c & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((c & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((c & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

static double mf_hidden_smooth(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static double mf_hidden_value_noise3(double x, double y, double z)
{
    int x0 = (int)floor(x);
    int y0 = (int)floor(y);
    int z0 = (int)floor(z);
    double tx = x - (double)x0;
    double ty = y - (double)y0;
    double tz = z - (double)z0;
    double sx = mf_hidden_smooth(tx);
    double sy = mf_hidden_smooth(ty);
    double sz = mf_hidden_smooth(tz);
    unsigned int hx0y0z0 = (unsigned int)(x0 * 73856093 ^ y0 * 19349663 ^ z0 * 83492791);
    unsigned int hx1y0z0 = (unsigned int)((x0 + 1) * 73856093 ^ y0 * 19349663 ^ z0 * 83492791);
    unsigned int hx0y1z0 = (unsigned int)(x0 * 73856093 ^ (y0 + 1) * 19349663 ^ z0 * 83492791);
    unsigned int hx1y1z0 = (unsigned int)((x0 + 1) * 73856093 ^ (y0 + 1) * 19349663 ^ z0 * 83492791);
    unsigned int hx0y0z1 = (unsigned int)(x0 * 73856093 ^ y0 * 19349663 ^ (z0 + 1) * 83492791);
    unsigned int hx1y0z1 = (unsigned int)((x0 + 1) * 73856093 ^ y0 * 19349663 ^ (z0 + 1) * 83492791);
    unsigned int hx0y1z1 = (unsigned int)(x0 * 73856093 ^ (y0 + 1) * 19349663 ^ (z0 + 1) * 83492791);
    unsigned int hx1y1z1 = (unsigned int)((x0 + 1) * 73856093 ^ (y0 + 1) * 19349663 ^ (z0 + 1) * 83492791);
    double c000 = mf_hidden_hash_unit(hx0y0z0);
    double c100 = mf_hidden_hash_unit(hx1y0z0);
    double c010 = mf_hidden_hash_unit(hx0y1z0);
    double c110 = mf_hidden_hash_unit(hx1y1z0);
    double c001 = mf_hidden_hash_unit(hx0y0z1);
    double c101 = mf_hidden_hash_unit(hx1y0z1);
    double c011 = mf_hidden_hash_unit(hx0y1z1);
    double c111 = mf_hidden_hash_unit(hx1y1z1);
    double x00 = c000 + sx * (c100 - c000);
    double x10 = c010 + sx * (c110 - c010);
    double x01 = c001 + sx * (c101 - c001);
    double x11 = c011 + sx * (c111 - c011);
    double y0v = x00 + sy * (x10 - x00);
    double y1v = x01 + sy * (x11 - x01);
    return y0v + sz * (y1v - y0v);
}

static double mf_hidden_fbm3(double x, double y, double z, int octaves, double lacunarity, double gain)
{
    int i;
    double amp = 0.5;
    double freq = 1.0;
    double sum = 0.0;
    double denom;

    for (i = 0; i < octaves; ++i) {
        sum += amp * mf_hidden_value_noise3(x * freq, y * freq, z * freq);
        freq *= lacunarity;
        amp *= gain;
    }

    denom = 1.0 - pow(gain, (double)octaves);
    if (denom == 0.0) {
        denom = 1.0;
    }
    return sum / denom;
}

static double mf_hidden_now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void mf_hidden_get_term_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *rows = ws.ws_row > 0 ? ws.ws_row : 24;
        *cols = ws.ws_col > 0 ? ws.ws_col : 80;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

static void mf_hidden_build_gradient(char lut[256])
{
    size_t i;
    size_t len;
    const char *gradient = MF_HIDDEN_GRADIENT;

    len = strlen(gradient);
    if (len < 2) {
        gradient = " .-+#@";
        len = strlen(gradient);
    }

    for (i = 0; i < 256; ++i) {
        double t = (double)i / 255.0;
        size_t idx = (size_t)(t * (double)(len - 1) + 0.5);
        if (idx >= len) {
            idx = len - 1;
        }
        lut[i] = gradient[idx];
    }
}

static int mf_hidden_ensure_buffers(int rows, int cols, struct mf_hidden_cell **curr, struct mf_hidden_cell **prev, double **fx, double **fy, int *buf_rows, int *buf_cols)
{
    size_t total;
    struct mf_hidden_cell *new_curr;
    struct mf_hidden_cell *new_prev;
    double *new_fx;
    double *new_fy;

    if (rows <= 0 || cols <= 0) {
        return -1;
    }

    if (*buf_rows == rows && *buf_cols == cols && *curr && *prev && *fx && *fy) {
        return 0;
    }

    total = (size_t)rows * (size_t)cols;
    new_curr = (struct mf_hidden_cell *)malloc(total * sizeof(struct mf_hidden_cell));
    new_prev = (struct mf_hidden_cell *)malloc(total * sizeof(struct mf_hidden_cell));
    new_fx = (double *)malloc((size_t)cols * sizeof(double));
    new_fy = (double *)malloc((size_t)rows * sizeof(double));

    if (!new_curr || !new_prev || !new_fx || !new_fy) {
        free(new_curr);
        free(new_prev);
        free(new_fx);
        free(new_fy);
        return -1;
    }

    free(*curr);
    free(*prev);
    free(*fx);
    free(*fy);

    *curr = new_curr;
    *prev = new_prev;
    *fx = new_fx;
    *fy = new_fy;
    *buf_rows = rows;
    *buf_cols = cols;

    {
        size_t idx;
        for (idx = 0; idx < total; ++idx) {
            mf_hidden_cell_set_char(&(*prev)[idx], ' ');
            mf_hidden_cell_set_char(&(*curr)[idx], ' ');
        }
    }

    {
        int x;
        for (x = 0; x < cols; ++x) {
            (*fx)[x] = (double)x * MF_HIDDEN_SCALE;
        }
        for (x = 0; x < rows; ++x) {
            (*fy)[x] = (double)x * MF_HIDDEN_SCALE * 0.5;
        }
    }

    return 0;
}

static void mf_hidden_overlay_buffer(struct mf_hidden_cell *buf, int rows, int cols, char formatted[][MF_FORMATTED_LINE_MAX], const size_t widths[], size_t count)
{
    size_t overlay_rows = count;
    size_t info_display_width = 0;
    size_t logo_display_width = 0;
    size_t inner_width;
    size_t inner_height;
    size_t total_width;
    size_t total_height;
    int start_row = 0;
    int start_col = 0;
    size_t i;

    for (i = 0; i < count; ++i) {
        if (widths[i] > info_display_width) {
            info_display_width = widths[i];
        }
    }

    for (i = 0; i < g_logo_line_count; ++i) {
        size_t disp = mf_utf8_display_width(g_logo_lines[i]);
        if (disp > logo_display_width) {
            logo_display_width = disp;
        }
    }

    if (g_logo_line_count > overlay_rows) {
        overlay_rows = g_logo_line_count;
    }

    inner_width = logo_display_width;
    if (info_display_width > 0) {
        if (inner_width > 0) {
            inner_width += MF_HIDDEN_COLUMN_GAP;
        }
        inner_width += info_display_width;
    }
    inner_height = overlay_rows;

    total_width = inner_width + 2U;
    total_height = inner_height + 2U;

    if (rows > (int)total_height) {
        start_row = (rows - (int)total_height) / 2;
    }
    if (cols > (int)total_width) {
        start_col = (cols - (int)total_width) / 2;
    }

    for (i = 0; i < total_height; ++i) {
        int row = start_row + (int)i;
        int col;
        if (row < 0 || row >= rows) {
            continue;
        }
        for (col = 0; col < (int)total_width; ++col) {
            int c = start_col + col;
            if (c < 0 || c >= cols) {
                continue;
            }
            mf_hidden_cell_set_char(&buf[row * cols + c], ' ');
        }
    }

    for (i = 0; i < overlay_rows; ++i) {
        int row = start_row + 1 + (int)i;
        size_t col_disp = 0;

        if (row < 0 || row >= rows) {
            continue;
        }

        if (logo_display_width > 0) {
            size_t written = 0;
            if (i < g_logo_line_count) {
                const char *p = g_logo_lines[i];
                while (*p != '\0' && written < logo_display_width) {
                    size_t glyph_len = mf_hidden_utf8_glyph_len(p);
                    int col_index;
                    size_t cell_index;
                    if (glyph_len == 0) {
                        break;
                    }
                    if (col_disp >= inner_width) {
                        break;
                    }
                    col_index = start_col + 1 + (int)col_disp;
                    if (col_index >= 0 && col_index < cols) {
                        cell_index = (size_t)row * (size_t)cols + (size_t)col_index;
                        mf_hidden_cell_set_utf8(&buf[cell_index], p, glyph_len);
                    }
                    p += glyph_len;
                    ++col_disp;
                    ++written;
                }
            }
            while (written < logo_display_width && col_disp < inner_width) {
                int col_index = start_col + 1 + (int)col_disp;
                size_t cell_index;
                if (col_index >= 0 && col_index < cols) {
                    cell_index = (size_t)row * (size_t)cols + (size_t)col_index;
                    mf_hidden_cell_set_char(&buf[cell_index], ' ');
                }
                ++col_disp;
                ++written;
            }
        }

        if (logo_display_width > 0 && info_display_width > 0) {
            size_t gap = MF_HIDDEN_COLUMN_GAP;
            while (gap > 0 && col_disp < inner_width) {
                int col_index = start_col + 1 + (int)col_disp;
                size_t cell_index;
                if (col_index >= 0 && col_index < cols) {
                    cell_index = (size_t)row * (size_t)cols + (size_t)col_index;
                    mf_hidden_cell_set_char(&buf[cell_index], ' ');
                }
                ++col_disp;
                --gap;
            }
        }

        if (info_display_width > 0) {
            size_t written = 0;
            if (i < count) {
                const char *p = formatted[i];
                while (*p != '\0' && written < info_display_width) {
                    size_t glyph_len = mf_hidden_utf8_glyph_len(p);
                    int col_index;
                    size_t cell_index;
                    if (glyph_len == 0) {
                        break;
                    }
                    if (col_disp >= inner_width) {
                        break;
                    }
                    col_index = start_col + 1 + (int)col_disp;
                    if (col_index >= 0 && col_index < cols) {
                        cell_index = (size_t)row * (size_t)cols + (size_t)col_index;
                        mf_hidden_cell_set_utf8(&buf[cell_index], p, glyph_len);
                    }
                    p += glyph_len;
                    ++col_disp;
                    ++written;
                }
            }
            while (written < info_display_width && col_disp < inner_width) {
                int col_index = start_col + 1 + (int)col_disp;
                size_t cell_index;
                if (col_index >= 0 && col_index < cols) {
                    cell_index = (size_t)row * (size_t)cols + (size_t)col_index;
                    mf_hidden_cell_set_char(&buf[cell_index], ' ');
                }
                ++col_disp;
                ++written;
            }
        }
    }
}

static void mf_hidden_write_hud(struct mf_hidden_cell *buf, int rows, int cols)
{
    const char *hud = "press q to exit hidden mode";
    size_t len = strlen(hud);
    int row = rows - 1;
    int col;
    size_t k;

    if (row < 0) {
        return;
    }

    for (col = 0; col < cols; ++col) {
        mf_hidden_cell_set_char(&buf[row * cols + col], ' ');
    }

    for (k = 0; k < len && (int)k < cols; ++k) {
        mf_hidden_cell_set_char(&buf[row * cols + (int)k], hud[k]);
    }
}

static void mf_hidden_present_diff(const struct mf_hidden_cell *curr, struct mf_hidden_cell *prev, int rows, int cols)
{
    size_t total = (size_t)rows * (size_t)cols;
    size_t pos = 0;

    while (pos < total) {
        if (curr[pos].len != prev[pos].len || memcmp(curr[pos].bytes, prev[pos].bytes, curr[pos].len) != 0) {
            size_t row = pos / (size_t)cols;
            size_t col = pos % (size_t)cols;
            size_t idx = pos + 1;

            while (idx < total) {
                size_t next_row = idx / (size_t)cols;
                if (next_row != row) {
                    break;
                }
                if (curr[idx].len == prev[idx].len && memcmp(curr[idx].bytes, prev[idx].bytes, curr[idx].len) == 0) {
                    break;
                }
                ++idx;
            }

            fprintf(stdout, "\x1b[%lu;%luH", (unsigned long)(row + 1), (unsigned long)(col + 1));
            while (pos < idx) {
                fwrite(curr[pos].bytes, 1, curr[pos].len, stdout);
                prev[pos] = curr[pos];
                ++pos;
            }
        } else {
            prev[pos] = curr[pos];
            ++pos;
        }
    }

    fflush(stdout);
}

int mf_run_hidden_mode(char formatted[][MF_FORMATTED_LINE_MAX], const size_t widths[], size_t count, int quiet_mode)
{
    char gradient_lut[256];
    int rows;
    int cols;
    struct mf_hidden_cell *curr_buf = NULL;
    struct mf_hidden_cell *prev_buf = NULL;
    double *fx = NULL;
    double *fy = NULL;
    int buf_rows = 0;
    int buf_cols = 0;
    double start_time;
    double next_deadline;
    double last_size_check;

    (void)quiet_mode;

    if (!isatty(STDOUT_FILENO)) {
        size_t i;
        for (i = 0; i < count; ++i) {
            fputs(formatted[i], stdout);
            fputc('\n', stdout);
        }
        return 0;
    }

    setvbuf(stdout, NULL, _IOFBF, MF_HIDDEN_BUFFER_CAP);

    mf_hidden_setup_signals();
    mf_hidden_hide_cursor();
    mf_hidden_enable_raw();

    mf_hidden_get_term_size(&rows, &cols);
    mf_hidden_build_gradient(gradient_lut);
    if (mf_hidden_ensure_buffers(rows, cols, &curr_buf, &prev_buf, &fx, &fy, &buf_rows, &buf_cols) != 0) {
        mf_hidden_cleanup();
        free(curr_buf);
        free(prev_buf);
        free(fx);
        free(fy);
        return -1;
    }

    fprintf(stdout, "\x1b[2J");
    if (rows > 1) {
        fprintf(stdout, "\x1b[1;%dr", rows - 1);
        g_hidden_scroll_region_set = 1;
    }
    fprintf(stdout, "\x1b[H");

    atexit(mf_hidden_cleanup);

    start_time = mf_hidden_now_sec();
    next_deadline = start_time + MF_HIDDEN_FRAMETIME_S;
    last_size_check = start_time;

    while (g_hidden_running) {
        double now = mf_hidden_now_sec();
        double z;
        int y;

        if (now - last_size_check > MF_HIDDEN_SIZE_REFRESH_SEC) {
            int new_rows;
            int new_cols;
            mf_hidden_get_term_size(&new_rows, &new_cols);
            if (new_rows != rows || new_cols != cols) {
                rows = new_rows;
                cols = new_cols;
            if (mf_hidden_ensure_buffers(rows, cols, &curr_buf, &prev_buf, &fx, &fy, &buf_rows, &buf_cols) != 0) {
                break;
            }
                fprintf(stdout, "\x1b[r\x1b[2J");
                if (rows > 1) {
                    fprintf(stdout, "\x1b[1;%dr", rows - 1);
                    g_hidden_scroll_region_set = 1;
                }
                fprintf(stdout, "\x1b[H");
            {
                size_t total = (size_t)rows * (size_t)cols;
                size_t idx;
                for (idx = 0; idx < total; ++idx) {
                    prev_buf[idx] = curr_buf[idx];
                }
            }
        }
            last_size_check = now;
        }

        z = (now - start_time) * MF_HIDDEN_SPEED;
        if (z < 0.0) {
            z = 0.0;
        }

        for (y = 0; y < rows; ++y) {
            int x;
            double fyv = fy[y];
            size_t row_base = (size_t)y * (size_t)cols;
            for (x = 0; x < cols; ++x) {
                double fxv = fx[x];
                double n = mf_hidden_fbm3(fxv, fyv, z, 4, 2.0, 0.5);
                double bands = 0.5 * (sin(n * 10.0 * M_PI) + 1.0);
                double v = 0.65 * n + 0.35 * bands;
                int lut_idx = (int)(v * 255.0 + 0.5);
                if (lut_idx < 0) {
                    lut_idx = 0;
                } else if (lut_idx > 255) {
                    lut_idx = 255;
                }
                mf_hidden_cell_set_char(&curr_buf[row_base + (size_t)x], gradient_lut[lut_idx]);
            }
        }

        mf_hidden_overlay_buffer(curr_buf, rows, cols, formatted, widths, count);
        mf_hidden_write_hud(curr_buf, rows, cols);
        mf_hidden_present_diff(curr_buf, prev_buf, rows, cols);

        {
            fd_set set;
            struct timeval tv;
            FD_ZERO(&set);
            FD_SET(STDIN_FILENO, &set);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0) {
                char ch;
                if (read(STDIN_FILENO, &ch, 1) > 0) {
                    if (ch == 'q' || ch == 'Q') {
                        break;
                    }
                }
            }
        }

        next_deadline += MF_HIDDEN_FRAMETIME_S;
        now = mf_hidden_now_sec();
        if (next_deadline <= now) {
            next_deadline = now + MF_HIDDEN_FRAMETIME_S;
        } else {
            double sleep_s = next_deadline - now;
            struct timespec ts;
            if (sleep_s < 0.0) {
                sleep_s = 0.0;
            }
            ts.tv_sec = (time_t)sleep_s;
            ts.tv_nsec = (long)((sleep_s - (double)ts.tv_sec) * 1e9);
            if (ts.tv_nsec < 0) {
                ts.tv_nsec = 0;
            }
            nanosleep(&ts, NULL);
        }
    }

    mf_hidden_cleanup();
    free(curr_buf);
    free(prev_buf);
    free(fx);
    free(fy);
    return 0;
}
