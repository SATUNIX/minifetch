// topo_ascii.c - dynamic topographical ASCII animation (C99)
// Build: cc -std=c99 -O2 -Wall -Wextra topo_ascii.c -lm -o topo_ascii
// Usage: ./topo_ascii [--fps 30] [--scale 0.12] [--speed 0.6] [--chars " .:-=+*#%@"] [--invert]
// Quit with 'q' or Ctrl+C.

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static volatile sig_atomic_t g_running = 1;

static struct termios g_orig_tio;
static bool g_raw_enabled = false;

static void on_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

static void show_cursor(void) {
    fputs("\x1b[?25h\x1b[0m", stdout);
    fflush(stdout);
}

static void hide_cursor(void) {
    fputs("\x1b[?25l", stdout);
    fflush(stdout);
}

static void disable_raw_mode(void) {
    if (g_raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_tio);
        g_raw_enabled = false;
    }
}

static void enable_raw_mode(void) {
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &g_orig_tio) == -1) return;

    struct termios tio = g_orig_tio;
    tio.c_lflag &= ~(ICANON | ECHO);
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    g_raw_enabled = true;
}

static void cleanup(void) {
    disable_raw_mode();
    show_cursor();
}

// ---------- RNG / hash helpers for value-noise ----------
static inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline float hash_to_unit(uint32_t x) {
    return (hash_u32(x) / 4294967295.0f);
}

static inline float smooth(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// 3D value noise
static float value_noise3(float x, float y, float z) {
    int32_t x0 = (int32_t)floorf(x);
    int32_t y0 = (int32_t)floorf(y);
    int32_t z0 = (int32_t)floorf(z);

    float tx = x - (float)x0;
    float ty = y - (float)y0;
    float tz = z - (float)z0;

    float sx = smooth(tx);
    float sy = smooth(ty);
    float sz = smooth(tz);

    float c000 = hash_to_unit((uint32_t)(x0*73856093 ^ y0*19349663 ^ z0*83492791));
    float c100 = hash_to_unit((uint32_t)((x0+1)*73856093 ^ y0*19349663 ^ z0*83492791));
    float c010 = hash_to_unit((uint32_t)(x0*73856093 ^ (y0+1)*19349663 ^ z0*83492791));
    float c110 = hash_to_unit((uint32_t)((x0+1)*73856093 ^ (y0+1)*19349663 ^ z0*83492791));
    float c001 = hash_to_unit((uint32_t)(x0*73856093 ^ y0*19349663 ^ (z0+1)*83492791));
    float c101 = hash_to_unit((uint32_t)((x0+1)*73856093 ^ y0*19349663 ^ (z0+1)*83492791));
    float c011 = hash_to_unit((uint32_t)(x0*73856093 ^ (y0+1)*19349663 ^ (z0+1)*83492791));
    float c111 = hash_to_unit((uint32_t)((x0+1)*73856093 ^ (y0+1)*19349663 ^ (z0+1)*83492791));

    float x00 = c000 + sx*(c100 - c000);
    float x10 = c010 + sx*(c110 - c010);
    float x01 = c001 + sx*(c101 - c001);
    float x11 = c011 + sx*(c111 - c011);

    float y0v = x00 + sy*(x10 - x00);
    float y1v = x01 + sy*(x11 - x01);

    float v = y0v + sz*(y1v - y0v);
    return v; // [0,1]
}

// fBm
static float fbm3(float x, float y, float z, int octaves, float lacunarity, float gain) {
    float amp = 0.5f;
    float freq = 1.0f;
    float sum = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * value_noise3(x * freq, y * freq, z * freq);
        freq *= lacunarity;
        amp  *= gain;
    }
    return sum / (1.0f - powf(gain, octaves)); // ~[0,1]
}

static void get_term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *rows = ws.ws_row > 0 ? ws.ws_row : 24;
        *cols = ws.ws_col > 0 ? ws.ws_col : 80;
    } else {
        *rows = 24; *cols = 80;
    }
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void usage(const char *prog) {
    // escape % as %%
    fprintf(stderr,
        "Usage: %s [--fps N] [--scale S] [--speed V] [--chars STR] [--invert]\n"
        "  --fps N     : frames per second (default 30)\n"
        "  --scale S   : spatial scale; larger = coarser (default 0.12)\n"
        "  --speed V   : temporal speed (default 0.6)\n"
        "  --chars STR : gradient chars light->dark (default \" .:-=+*#%%@\")\n"
        "  --invert    : invert gradient mapping\n",
        prog
    );
}

int main(int argc, char **argv) {
    int fps = 30;
    float scale = 0.12f;
    float speed = 0.6f;
    const char *gradient = " .:-=+*#%@";
    bool invert = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--fps") && i+1 < argc) {
            fps = atoi(argv[++i]);
            if (fps < 1) fps = 1;
        } else if (!strcmp(argv[i], "--scale") && i+1 < argc) {
            scale = strtof(argv[++i], NULL);
            if (scale <= 0.0f) scale = 0.12f;
        } else if (!strcmp(argv[i], "--speed") && i+1 < argc) {
            speed = strtof(argv[++i], NULL);
            if (speed <= 0.0f) speed = 0.6f;
        } else if (!strcmp(argv[i], "--chars") && i+1 < argc) {
            gradient = argv[++i];
            if (strlen(gradient) < 2) gradient = " .:-=+*#%@";
        } else if (!strcmp(argv[i], "--invert")) {
            invert = true;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    atexit(cleanup);
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    hide_cursor();
    fputs("\x1b[2J", stdout);

    // Make keypress immediate
    enable_raw_mode();

    int rows, cols;
    get_term_size(&rows, &cols);

    const double frame_dt = 1.0 / (double)fps;
    double t0 = now_sec();
    double recheck_size_t = t0;
    int grad_len = (int)strlen(gradient);

    while (g_running) {
        double t = now_sec() - t0;
        if (now_sec() - recheck_size_t > 0.5) {
            int r2, c2; get_term_size(&r2, &c2);
            if (r2 != rows || c2 != cols) { rows = r2; cols = c2; fputs("\x1b[2J", stdout); }
            recheck_size_t = now_sec();
        }

        fputs("\x1b[H", stdout); // home

        int draw_rows = rows > 1 ? rows - 1 : rows;
        float z = (float)(t * speed);

        for (int y = 0; y < draw_rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                float fx = (float)x * scale;
                float fy = (float)y * scale * 0.5f;

                float n = fbm3(fx, fy, z, 4, 2.0f, 0.5f);
                float bands = 0.5f * (sinf(n * 10.0f * (float)M_PI) + 1.0f);
                float v = 0.65f * n + 0.35f * bands;

                int idx = (int)(v * (float)(grad_len - 1) + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= grad_len) idx = grad_len - 1;
                if (invert) idx = (grad_len - 1) - idx;

                char c = gradient[idx];
                if (c == '\0' || c == '\n' || c == '\r') c = ' ';
                fputc(c, stdout);
            }
            fputc('\n', stdout);
        }

        // bottom HUD
        fprintf(stdout, "\x1b[s\x1b[%d;1H\x1b[2K"
                        "topo_ascii  fps=%d  scale=%.2f  speed=%.2f  (q to quit)\x1b[u",
                rows, fps, scale, speed);
        fflush(stdout);

        // nonblocking single-char read
        fd_set set;
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);
        struct timeval tv;
        tv.tv_sec = 0; tv.tv_usec = 0;
        int rv = select(STDIN_FILENO + 1, &set, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(STDIN_FILENO, &set)) {
            char ch;
            ssize_t rd = read(STDIN_FILENO, &ch, 1);
            if (rd > 0 && (ch == 'q' || ch == 'Q')) break;
        }

        struct timespec ts;
        ts.tv_sec = 0;
        long ns = (long)(frame_dt * 1e9);
        if (ns < 0) ns = 0;
        ts.tv_nsec = ns;
        nanosleep(&ts, NULL);
    }

    fprintf(stdout, "\x1b[%d;1H\x1b[2K", rows);
    fflush(stdout);
    return 0;
}
 
