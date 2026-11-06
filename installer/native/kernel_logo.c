/* Local, hash-pinned Finder logo conversion. No block-device or flash writes.
 * Freestanding ARM build: fixed allocation arena; scratch streams stay on USB.
 * Original code: Unlicense. zlib's unchanged sources retain their own license.
 */
#include "../vendor/zlib/zlib.h"
#include <stddef.h>
#include <stdint.h>

#define IMAGE_BYTES 3199416U
#define GZIP_OFFSET 0x462cU
#define GZIP_BYTES 3181400U
#define PAYLOAD_BYTES 6673472U
#define ART_OFFSET 6249820U
#define ART_BYTES 154260U
#define CHUNK 16384U
#define ARENA_BYTES (512U * 1024U)
static const char stock_hash[] = "ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87";
static const char revival_hash[] =
    "651ded763ab91ea7b8584ba8d7f9cfb80b53a1c57835d8c62b855b893efdbdb0";
static const char art_hash[] = "089cb2f0aca9843f942ef1a5ccf75d096a9a7f78e3fb1a0d100026a6acf13997";

#ifdef __arm__
static long call3(long n, long a, long b, long c) {
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return r0;
}
static long read_file(int f, void *p, unsigned n) { return call3(3, f, (long)p, n); }
static long write_file(int f, const void *p, unsigned n) { return call3(4, f, (long)p, n); }
static int open_file(const char *p, int flags) { return (int)call3(5, (long)p, flags, 0600); }
static int close_file(int f) { return (int)call3(6, f, 0, 0); }
static long seek_file(int f, long at, int whence) { return call3(19, f, at, whence); }
static int remove_file(const char *p) { return (int)call3(10, (long)p, 0, 0); }
/* ARMv5 lacks hardware division. These EABI helpers keep libc/libgcc out. */
__asm__(".global __aeabi_uidiv\n.global __aeabi_uidivmod\n"
        "__aeabi_uidiv:\n__aeabi_uidivmod:\n"
        "cmp r1, #0\nbeq 2f\nmov r2, #0\nmov r3, #0\nmov ip, #32\n"
        "1: movs r0, r0, lsl #1\nadcs r3, r3, r3\ncmpcc r3, r1\n"
        "subcs r3, r3, r1\nadc r2, r2, r2\nsubs ip, ip, #1\nbne 1b\n"
        "mov r0, r2\nmov r1, r3\nbx lr\n2: mov r0, #0\nbx lr\n");
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
static long read_file(int f, void *p, unsigned n) {
    long r = read(f, p, n);
    return r < 0 ? -errno : r;
}
static long write_file(int f, const void *p, unsigned n) {
    long r = write(f, p, n);
    return r < 0 ? -errno : r;
}
static int open_file(const char *p, int flags) {
    int r = open(p, flags, 0600);
    return r < 0 ? -errno : r;
}
static int close_file(int f) { return close(f); }
static long seek_file(int f, long at, int whence) {
    long r = lseek(f, at, whence);
    return r < 0 ? -errno : r;
}
static int remove_file(const char *p) { return unlink(p); }
#endif

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *out = d;
    const unsigned char *in = s;
    for (size_t i = 0; i < n; i++)
        out[i] = in[i];
    return d;
}
void *memset(void *d, int c, size_t n) {
    unsigned char *out = d;
    for (size_t i = 0; i < n; i++)
        out[i] = (unsigned char)c;
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    return 0;
}
static unsigned length(const char *s) {
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}
static int equal(const char *a, const char *b) {
    return length(a) == length(b) && !memcmp(a, b, length(a));
}
static int stop(const char *s) {
    write_file(2, s, length(s));
    write_file(2, "\n", 1);
    return 1;
}
static unsigned char inbuf[CHUNK], outbuf[CHUNK], artwork[ART_BYTES];
static union {
    uint64_t align;
    unsigned char bytes[ARENA_BYTES];
} arena;
static unsigned arena_used, arena_peak;
static voidpf allocate(voidpf unused, uInt items, uInt size) {
    (void)unused;
    if (items && size > ARENA_BYTES / items)
        return 0;
    unsigned n = items * size;
    if (n > ARENA_BYTES - 15)
        return 0;
    n = (n + 15U) & ~15U;
    if (n > ARENA_BYTES - arena_used)
        return 0;
    void *p = arena.bytes + arena_used;
    arena_used += n;
    if (arena_used > arena_peak)
        arena_peak = arena_used;
    memset(p, 0, n);
    return p;
}
static void release(voidpf unused, voidpf address) {
    (void)unused;
    (void)address;
}
static void stream_init(z_stream *s) {
    arena_used = 0;
    memset(s, 0, sizeof(*s));
    s->zalloc = allocate;
    s->zfree = release;
}
static int write_all(int f, const void *bytes, unsigned n) {
    const unsigned char *p = bytes;
    while (n) {
        long r = write_file(f, p, n);
        if (r == -4)
            continue;
        if (r <= 0)
            return 0;
        p += r;
        n -= (unsigned)r;
    }
    return 1;
}
static int read_all(int f, void *bytes, unsigned n) {
    unsigned char *p = bytes;
    while (n) {
        long r = read_file(f, p, n);
        if (r == -4)
            continue;
        if (r <= 0)
            return 0;
        p += r;
        n -= (unsigned)r;
    }
    return 1;
}
static int copy_bytes(int from, int to, unsigned n) {
    while (n) {
        unsigned step = n > CHUNK ? CHUNK : n;
        if (!read_all(from, inbuf, step) || !write_all(to, inbuf, step))
            return 0;
        n -= step;
    }
    return 1;
}

/* SHA-256, FIPS 180-4. File identity is checked independently again by the shell. */
typedef struct {
    uint32_t h[8];
    uint64_t count;
    unsigned used;
    unsigned char block[64];
} SHA;
static uint32_t rotate(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static uint32_t big(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void put_big(unsigned char *p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
static void put_little(unsigned char *p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}
static void sha_block(SHA *s) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    uint32_t w[64];
    for (unsigned i = 0; i < 16; i++)
        w[i] = big(s->block + 4 * i);
    for (unsigned i = 16; i < 64; i++) {
        uint32_t x = w[i - 15], y = w[i - 2];
        w[i] = w[i - 16] + (rotate(x, 7) ^ rotate(x, 18) ^ (x >> 3)) + w[i - 7] +
               (rotate(y, 17) ^ rotate(y, 19) ^ (y >> 10));
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5],
             g = s->h[6], h = s->h[7];
    for (unsigned i = 0; i < 64; i++) {
        uint32_t t =
            h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t u = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t;
        d = c;
        c = b;
        b = a;
        a = t + u;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}
static void sha_init(SHA *s) {
    static const uint32_t initial[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memset(s, 0, sizeof(*s));
    memcpy(s->h, initial, sizeof(initial));
}
static void sha_add(SHA *s, const unsigned char *p, unsigned n) {
    s->count += n;
    while (n) {
        unsigned take = 64 - s->used;
        if (take > n)
            take = n;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) {
            sha_block(s);
            s->used = 0;
        }
    }
}
static void sha_finish(SHA *s, char hex[65]) {
    static const char digits[] = "0123456789abcdef";
    uint64_t bits = s->count * 8;
    unsigned char marker = 128, zero = 0;
    sha_add(s, &marker, 1);
    while (s->used != 56)
        sha_add(s, &zero, 1);
    unsigned char size[8];
    for (unsigned i = 0; i < 8; i++)
        size[7 - i] = (unsigned char)(bits >> (i * 8));
    sha_add(s, size, 8);
    for (unsigned i = 0; i < 32; i++) {
        unsigned v = (s->h[i / 4] >> (24 - 8 * (i % 4))) & 255;
        hex[i * 2] = digits[v >> 4];
        hex[i * 2 + 1] = digits[v & 15];
    }
    hex[64] = 0;
}
static int hash_file(int f, unsigned n, char hex[65]) {
    SHA s;
    sha_init(&s);
    if (seek_file(f, 0, 0) != 0)
        return 0;
    while (n) {
        unsigned take = n > CHUNK ? CHUNK : n;
        if (!read_all(f, inbuf, take))
            return 0;
        sha_add(&s, inbuf, take);
        n -= take;
    }
    sha_finish(&s, hex);
    return seek_file(f, 0, 0) == 0;
}
static int file_size(int f, unsigned expected) {
    return seek_file(f, 0, 2) == (long)expected && seek_file(f, 0, 0) == 0;
}
static int scratch_name(char *out, const char *prefix, const char *suffix) {
    unsigned n = length(prefix), m = length(suffix);
    if (n + m >= 512)
        return 0;
    memcpy(out, prefix, n);
    memcpy(out + n, suffix, m + 1);
    return 1;
}

static int inflate_logo(int input, int raw, uint32_t *payload_crc) {
    z_stream s;
    stream_init(&s);
    if (inflateInit2(&s, 31) != Z_OK)
        return 0;
    unsigned remaining = GZIP_BYTES, produced = 0;
    int result = Z_OK;
    *payload_crc = 0;
    if (seek_file(input, GZIP_OFFSET, 0) != GZIP_OFFSET)
        return 0;
    while (result != Z_STREAM_END) {
        if (!s.avail_in) {
            unsigned n = remaining > CHUNK ? CHUNK : remaining;
            if (!n || !read_all(input, inbuf, n))
                return 0;
            remaining -= n;
            s.next_in = inbuf;
            s.avail_in = n;
        }
        s.next_out = outbuf;
        s.avail_out = CHUNK;
        unsigned before = s.avail_in;
        result = inflate(&s, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END)
            return 0;
        unsigned n = CHUNK - s.avail_out;
        if (n > PAYLOAD_BYTES - produced)
            return 0;
        unsigned from = produced > ART_OFFSET ? produced : ART_OFFSET;
        unsigned end =
            produced + n < ART_OFFSET + ART_BYTES ? produced + n : ART_OFFSET + ART_BYTES;
        if (end > from)
            memcpy(outbuf + from - produced, artwork + from - ART_OFFSET, end - from);
        *payload_crc = (uint32_t)crc32(*payload_crc, outbuf, n);
        if (!write_all(raw, outbuf, n))
            return 0;
        produced += n;
        if (!n && s.avail_in == before)
            return 0;
    }
    int ok = produced == PAYLOAD_BYTES && !remaining && !s.avail_in;
    return inflateEnd(&s) == Z_OK && ok;
}
static int deflate_logo(int raw, int compressed, unsigned *bytes) {
    z_stream s;
    stream_init(&s);
    if (deflateInit2(&s, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return 0;
    if (seek_file(raw, 0, 0) != 0)
        return 0;
    *bytes = 0;
    int ended = 0;
    while (!ended) {
        long got = read_file(raw, inbuf, CHUNK);
        if (got == -4)
            continue;
        if (got < 0)
            return 0;
        s.next_in = inbuf;
        s.avail_in = (unsigned)got;
        do {
            s.next_out = outbuf;
            s.avail_out = CHUNK;
            int r = deflate(&s, got ? Z_NO_FLUSH : Z_FINISH);
            if (r != Z_OK && r != Z_STREAM_END)
                return 0;
            unsigned n = CHUNK - s.avail_out;
            if (n > GZIP_BYTES - 18 - *bytes)
                return 0;
            if (!write_all(compressed, outbuf, n))
                return 0;
            *bytes += n;
            ended = r == Z_STREAM_END;
        } while (!s.avail_out || (!got && !ended));
    }
    return deflateEnd(&s) == Z_OK;
}
static int write_image(int input, int compressed, int output, unsigned bytes,
                       uint32_t payload_crc) {
    if (seek_file(input, 0, 0) != 0 || !copy_bytes(input, output, GZIP_OFFSET))
        return 0;
    unsigned padding = GZIP_BYTES - 18 - bytes;
    unsigned char head[10] = {31, 139, 8, padding ? 8 : 0, 0, 0, 0, 0, 2, 255};
    if (!write_all(output, head, 10))
        return 0;
    if (padding) {
        memset(outbuf, 'R', CHUNK);
        unsigned left = padding - 1;
        while (left) {
            unsigned n = left > CHUNK ? CHUNK : left;
            if (!write_all(output, outbuf, n))
                return 0;
            left -= n;
        }
        unsigned char zero = 0;
        if (!write_all(output, &zero, 1))
            return 0;
    }
    if (seek_file(compressed, 0, 0) != 0 || !copy_bytes(compressed, output, bytes))
        return 0;
    unsigned char tail[8];
    put_little(tail, payload_crc);
    put_little(tail + 4, PAYLOAD_BYTES);
    if (!write_all(output, tail, 8) ||
        seek_file(input, GZIP_OFFSET + GZIP_BYTES, 0) != GZIP_OFFSET + GZIP_BYTES)
        return 0;
    if (!copy_bytes(input, output, IMAGE_BYTES - GZIP_OFFSET - GZIP_BYTES))
        return 0;
    if (!file_size(output, IMAGE_BYTES) || seek_file(output, 64, 0) != 64)
        return 0;
    unsigned remaining = IMAGE_BYTES - 64;
    uint32_t crc = 0;
    while (remaining) {
        unsigned n = remaining > CHUNK ? CHUNK : remaining;
        if (!read_all(output, inbuf, n))
            return 0;
        crc = (uint32_t)crc32(crc, inbuf, n);
        remaining -= n;
    }
    unsigned char header[64];
    if (seek_file(input, 0, 0) != 0 || !read_all(input, header, 64))
        return 0;
    put_big(header + 24, crc);
    put_big(header + 4, 0);
    put_big(header + 4, (uint32_t)crc32(0, header, 64));
    return seek_file(output, 0, 0) == 0 && write_all(output, header, 64);
}
static int self_test(void) {
    SHA s;
    char value[65];
    sha_init(&s);
    sha_add(&s, (const unsigned char *)"abc", 3);
    sha_finish(&s, value);
    if (!equal(value, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"))
        return 1;
    if (crc32(0, (const unsigned char *)"123456789", 9) != 0xcbf43926U)
        return 1;
    volatile uint32_t a = 0xfefdfcfbU, b = 0xabcdeU;
    if (a / b != 6079U || a % b != 205145U)
        return 1;
    const char *ok = "kernel-logo self-test passed\n";
    write_file(1, ok, length(ok));
    return 0;
}
int kernel_main(int argc, char **argv) {
    if (argc == 2 && equal(argv[1], "--self-test"))
        return self_test();
    if (argc != 5)
        return stop("Usage: kernel-logo INPUT ARTWORK OUTPUT SCRATCH-PREFIX");
    int input = open_file(argv[1], 0), art = open_file(argv[2], 0);
    char hash[65];
    if (input < 0 || art < 0)
        return stop("Cannot open input files");
    if (!file_size(input, IMAGE_BYTES) || !hash_file(input, IMAGE_BYTES, hash))
        return stop("Unsupported kernel length/read");
    int ready = equal(hash, revival_hash);
    if (!ready && !equal(hash, stock_hash))
        return stop("Unsupported kernel SHA256");
    if (!file_size(art, ART_BYTES) || !hash_file(art, ART_BYTES, hash) || !equal(hash, art_hash) ||
        !read_all(art, artwork, ART_BYTES))
        return stop("Unsupported artwork SHA256/length");
    close_file(art);
    int output = open_file(argv[3], 2 | 64 | 128);
    if (output < 0)
        return stop("Output exists or cannot be created");
    if (ready) {
        if (!copy_bytes(input, output, IMAGE_BYTES))
            return stop("Approved kernel copy failed");
    } else {
        char raw_name[512], packed_name[512];
        if (!scratch_name(raw_name, argv[4], ".inflated") ||
            !scratch_name(packed_name, argv[4], ".deflate"))
            return stop("Scratch path too long");
        int raw = open_file(raw_name, 2 | 64 | 128), packed = open_file(packed_name, 2 | 64 | 128);
        uint32_t payload_crc;
        unsigned bytes;
        if (raw < 0 || packed < 0)
            return stop("Scratch file exists or cannot be created");
        if (!inflate_logo(input, raw, &payload_crc) || !deflate_logo(raw, packed, &bytes) ||
            !write_image(input, packed, output, bytes, payload_crc))
            return stop("Kernel conversion refused");
        close_file(raw);
        close_file(packed);
        if (!hash_file(output, IMAGE_BYTES, hash) || !equal(hash, revival_hash))
            return stop("Converted image SHA256 mismatch; no flash writes performed");
        if (remove_file(raw_name) || remove_file(packed_name))
            return stop("Cannot remove owned scratch files");
    }
    if (!file_size(output, IMAGE_BYTES) || !hash_file(output, IMAGE_BYTES, hash) ||
        !equal(hash, revival_hash))
        return stop("Output verification failed");
    close_file(input);
    if (close_file(output))
        return stop("Cannot close output");
    const char *ok = "Approved logo kernel prepared; no flash writes performed.\n";
    write_file(1, ok, length(ok));
    return 0;
}
#ifdef __arm__
__asm__(
    ".global _start\n_start:\nldr r0,[sp]\nadd r1,sp,#4\nbl kernel_main\nmov r7,#1\nsvc 0\nb .\n");
#else
int main(int argc, char **argv) { return kernel_main(argc, argv); }
#endif
