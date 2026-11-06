#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include "gx.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 1024
/* PNG dimensions alone do not bound an oversized DEFLATE stream. Keep the
 * decoder's complete allocation budget below the Finder's RAM headroom. */
typedef union {
    max_align_t alignment;
    size_t size;
} ImageAllocation;
static size_t image_bytes;
#define IMAGE_BUDGET (12u * 1024 * 1024)
static void *image_alloc(size_t n) {
    if (n > IMAGE_BUDGET || image_bytes > IMAGE_BUDGET - n)
        return NULL;
    ImageAllocation *p = malloc(sizeof(*p) + n);
    if (!p)
        return NULL;
    p->size = n;
    image_bytes += n;
    return p + 1;
}
static void image_free(void *value) {
    if (!value)
        return;
    ImageAllocation *p = (ImageAllocation *)value - 1;
    image_bytes -= p->size;
    free(p);
}
static void *image_realloc(void *value, size_t n) {
    if (!value)
        return image_alloc(n);
    if (!n) {
        image_free(value);
        return NULL;
    }
    ImageAllocation *p = (ImageAllocation *)value - 1;
    size_t old = p->size;
    if (n > IMAGE_BUDGET || image_bytes - old > IMAGE_BUDGET - n)
        return NULL;
    p = realloc(p, sizeof(*p) + n);
    if (!p)
        return NULL;
    p->size = n;
    image_bytes = image_bytes - old + n;
    return p + 1;
}
#define STBI_MALLOC(n) image_alloc(n)
#define STBI_FREE(p) image_free(p)
#define STBI_REALLOC_SIZED(p, old, n) image_realloc(p, n)
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
#define THUMB_MAX (2 * 1024 * 1024)
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(unsigned char *p, uint32_t v) {
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}
static void put16(unsigned char *p, unsigned v) {
    p[0] = v;
    p[1] = v >> 8;
}
static int write_all(int fd, const void *data, size_t n) {
    const char *p = data;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0)
            return -1;
        p += w;
        n -= w;
    }
    return 0;
}
int gx_header(int fd, struct gx_meta *m, uint32_t *bmp, uint32_t *code) {
    unsigned char h[58];
    struct stat s;
    if (fstat(fd, &s) || pread(fd, h, 58, 0) != 58 || memcmp(h, "xgcode 1.0\n\0", 12))
        return -1;
    *bmp = le32(h + 16);
    *code = le32(h + 20);
    if (*bmp != 58 || *code < 58 || *code >= (uint64_t)s.st_size || *code != le32(h + 24) ||
        *code - 58 > 1024 * 1024)
        return -1;
    if (*code > *bmp) {
        unsigned char b[14];
        if (*code - *bmp < 54 || pread(fd, b, 14, *bmp) != 14 || memcmp(b, "BM", 2) ||
            le32(b + 2) > *code - *bmp || le32(b + 10) < 14 || le32(b + 10) >= *code - *bmp)
            return -1;
    }
    memset(m, 0, sizeof(*m));
    m->seconds = le32(h + 28);
    m->filament = le32(h + 32);
    m->layer = h[42] | h[43] << 8;
    m->shells = h[46] | h[47] << 8;
    m->speed = h[48] | h[49] << 8;
    m->bed = h[50] | h[51] << 8;
    m->nozzle = h[52] | h[53] << 8;
    return 0;
}
static unsigned number(const char *s, unsigned max, double scale) {
    char *end;
    double v = strtod(s, &end) * scale;
    if (end == s || !isfinite(v) || v < 0 || v > max)
        return 0;
    return (unsigned)(v + (scale == 1000 ? 0.5 : 0));
}
static int b64value(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}
static unsigned char *thumbnail(const char *encoded, size_t size, size_t *bmp_size) {
    unsigned char *bin = malloc(size * 3 / 4 + 4);
    if (!bin)
        return NULL;
    size_t n = 0;
    unsigned bits = 0, count = 0;
    for (size_t i = 0; i < size; i++) {
        int c = (unsigned char)encoded[i];
        if (c == '=')
            break;
        int v = b64value(c);
        if (v < 0) {
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
                continue;
            free(bin);
            return NULL;
        }
        bits = (bits << 6) | (unsigned)v;
        count += 6;
        if (count >= 8) {
            count -= 8;
            bin[n++] = (unsigned char)(bits >> count);
        }
    }
    int w, h, channels;
    if (!stbi_info_from_memory(bin, (int)n, &w, &h, &channels) || w < 1 || h < 1 || w > 1024 ||
        h > 1024) {
        free(bin);
        return NULL;
    }
    unsigned char *pixels = stbi_load_from_memory(bin, (int)n, &w, &h, &channels, 4);
    free(bin);
    if (!pixels)
        return NULL;
    const int width = 80, height = 60;
    *bmp_size = 54 + width * height * 3;
    unsigned char *bmp = calloc(1, *bmp_size);
    if (!bmp) {
        stbi_image_free(pixels);
        return NULL;
    }
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(bmp + 2, *bmp_size);
    put32(bmp + 10, 54);
    put32(bmp + 14, 40);
    put32(bmp + 18, width);
    put32(bmp + 22, height);
    put16(bmp + 26, 1);
    put16(bmp + 28, 24);
    put32(bmp + 34, width * height * 3);
    put32(bmp + 38, 2835);
    put32(bmp + 42, 2835);
    memset(bmp + 54, 255, *bmp_size - 54);
    double scale = fmin((double)width / w, (double)height / h);
    int dw = (int)(w * scale + 0.5), dh = (int)(h * scale + 0.5);
    if (dw < 1)
        dw = 1;
    if (dh < 1)
        dh = 1;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            int sx = x * w / dw, sy = y * h / dh;
            unsigned char *src = pixels + 4 * (sy * w + sx);
            int dx = x + (width - dw) / 2, dy = y + (height - dh) / 2;
            unsigned char *dst = bmp + 54 + ((height - 1 - dy) * width + dx) * 3;
            for (int c = 0; c < 3; c++)
                dst[2 - c] = (src[c] * src[3] + 255 * (255 - src[3]) + 127) / 255;
        }
    stbi_image_free(pixels);
    return bmp;
}
int gx_convert(int input, off_t start, size_t length, int output, char *error, size_t cap) {
    struct gx_meta m = {0};
    char line[16384];
    char *encoded = malloc(THUMB_MAX + 1);
    size_t used = 0;
    int block = 0, found = 0, bad = 0;
    unsigned char *bmp = NULL;
    size_t bmp_size = 0;
    if (!encoded) {
        snprintf(error, cap, "Not enough memory");
        return -1;
    }
    FILE *f = fdopen(dup(input), "rb");
    if (!f) {
        free(encoded);
        return -1;
    }
    if (fseeko(f, start, SEEK_SET)) {
        fclose(f);
        free(encoded);
        return -1;
    }
    size_t remaining = length;
    int any = 0;
    while (remaining) {
        size_t n = 0;
        int c;
        while (n < sizeof(line) - 1 && remaining) {
            c = fgetc(f);
            if (c == EOF) {
                bad = 1;
                break;
            }
            remaining--;
            if (c == 0) {
                bad = 1;
                break;
            }
            line[n++] = (char)c;
            if (c == '\n')
                break;
        }
        if (bad)
            break;
        line[n] = 0;
        for (size_t j = 0; j < n; j++)
            if (line[j] != ' ' && line[j] != '\r' && line[j] != '\n' && line[j] != '\t')
                any = 1;
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;
        char *equal = strchr(s, '=');
        const char *v = equal ? equal + 1 : "";
        if (!strncmp(s, "; estimated printing time (normal mode) =", 41)) {
            unsigned total = 0;
            const char *p = v;
            while (*p) {
                char *e;
                unsigned long a = strtoul(p, &e, 10);
                if (e == p) {
                    p++;
                    continue;
                }
                if (*e == 'h')
                    total += a * 3600;
                else if (*e == 'm')
                    total += a * 60;
                else if (*e == 's')
                    total += a;
                p = e + (*e != 0);
            }
            m.seconds = total;
        } else if (!strncmp(s, "; filament used [mm] =", 22))
            m.filament = number(v, UINT32_MAX, 1);
        else if (!strncmp(s, "; layer_height =", 16))
            m.layer = number(v, 32767, 1000);
        else if (!strncmp(s, "; wall_loops =", 14))
            m.shells = number(v, 32767, 1);
        else if (!strncmp(s, "; outer_wall_speed =", 20))
            m.speed = number(v, 32767, 1);
        else if (!strncmp(s, "; first_layer_bed_temperature =", 31))
            m.bed = number(v, 32767, 1);
        else if (!strncmp(s, "; nozzle_temperature =", 22))
            m.nozzle = number(v, 32767, 1);
        if (!strncmp(s, "; thumbnail begin", 17) || !strncmp(s, "; thumbnail_JPG begin", 21) ||
            !strncmp(s, "; thumbnail_PNG begin", 21)) {
            if (!found) {
                block = 1;
                used = 0;
            }
            continue;
        }
        if (block &&
            (!strncmp(s, "; thumbnail end", 15) || !strncmp(s, "; thumbnail_JPG end", 19) ||
             !strncmp(s, "; thumbnail_PNG end", 19))) {
            block = 0;
            bmp = thumbnail(encoded, used, &bmp_size);
            if (!bmp) {
                bad = 1;
                break;
            }
            found = 1;
            continue;
        }
        if (block) {
            if (*s != ';') {
                bad = 1;
                break;
            }
            s++;
            while (*s == ' ' || *s == '\t')
                s++;
            size_t a = strcspn(s, "\r\n");
            if (used + a > THUMB_MAX) {
                bad = 1;
                break;
            }
            memcpy(encoded + used, s, a);
            used += a;
        }
    }
    fclose(f);
    free(encoded);
    if (bad || block || !any) {
        free(bmp);
        snprintf(error, cap, "G-code or thumbnail is incomplete or invalid");
        return -1;
    }
    unsigned char h[58] = {0};
    memcpy(h, "xgcode 1.0\n\0", 12);
    put32(h + 16, 58);
    put32(h + 20, 58 + bmp_size);
    put32(h + 24, 58 + bmp_size);
    put32(h + 28, m.seconds);
    put32(h + 32, m.filament);
    put16(h + 42, m.layer);
    put16(h + 46, m.shells);
    put16(h + 48, m.speed);
    put16(h + 50, m.bed);
    put16(h + 52, m.nozzle);
    put16(h + 56, 1);
    if (write_all(output, h, 58) || write_all(output, bmp, bmp_size)) {
        free(bmp);
        return -1;
    }
    free(bmp);
    unsigned char buf[32768];
    remaining = length;
    off_t at = start;
    while (remaining) {
        size_t n = remaining < sizeof(buf) ? remaining : sizeof(buf);
        ssize_t r = pread(input, buf, n, at);
        if (r <= 0 || write_all(output, buf, r))
            return -1;
        at += r;
        remaining -= r;
    }
    return 0;
}
