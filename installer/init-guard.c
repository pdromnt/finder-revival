/* Exact-profile, volatile PID 1 guard for the NUC970 USB handoff.
 * Never edit /bin/busybox. The two private memory changes disappear on exec.
 * Build without libc; only open/read/write/lseek/close/exit syscalls are used.
 */
typedef unsigned int u32;
typedef int (*io_fn)(u32 address, unsigned char *bytes, unsigned int count);
static const u32 addresses[2] = {0x7dae4, 0x7db0c};
static const u32 expected[2][4] = {
    {0xe3a0100f, 0xe3e00000, 0xebfe39d4, 0xe1a01004},
    {0xe3a01009, 0xe3e00000, 0xebfe39ca, 0xe1a01004}
};
static const u32 replacement = 0xe3a00000; /* mov r0, #0; skip kill(-1, sig) */

static int same(const unsigned char *a, const unsigned char *b, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

/* mode 0 probes original instructions, 1 applies, 2 restores known variants.
 * Every context is read and validated before any write. On an apply failure,
 * attempt both original words and verify them, including a partially written word.
 * Return 2 means refusal, 3 means a failed change restored, 4 means restore failed.
 */
static int change(io_fn get, io_fn put, int mode)
{
    u32 words[2][4], value;
    unsigned int i, j;
    int failed = 0;
    for (i = 0; i < 2; ++i) {
        if (!get(addresses[i], (unsigned char *)words[i], 16)) return 2;
        for (j = 0; j < 4; ++j) {
            if (words[i][j] == expected[i][j]) continue;
            if (mode == 2 && j == 2 && words[i][j] == replacement) continue;
            return 2;
        }
    }
    if (!mode) return 0;
    for (i = 0; i < 2; ++i) {
        value = mode == 1 ? replacement : expected[i][2];
        if (!put(addresses[i] + 8, (unsigned char *)&value, 4)) { failed = 1; break; }
    }
    for (i = 0; i < 2 && !failed; ++i) {
        value = mode == 1 ? replacement : expected[i][2];
        if (!get(addresses[i] + 8, (unsigned char *)&words[i][2], 4)
            || words[i][2] != value) failed = 1;
    }
    if (!failed) return 0;
    /* A short write must also be repaired, without relying on its return value. */
    for (i = 0; i < 2; ++i)
        if (!put(addresses[i] + 8, (unsigned char *)&expected[i][2], 4)) failed = 2;
    for (i = 0; i < 2; ++i)
        if (!get(addresses[i], (unsigned char *)words[i], 16)
            || !same((unsigned char *)words[i], (const unsigned char *)expected[i], 16)) failed = 2;
    return failed == 1 ? 3 : 4;
}

#ifdef INIT_GUARD_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char memory[2][16];
static int reads, writes, fail_read, fail_write, partial_write, fail_forever;
static int fixture_get(u32 address, unsigned char *bytes, unsigned int n)
{
    unsigned int i;
    ++reads;
    if (reads == fail_read) return 0;
    for (i = 0; i < 2; ++i) if (address >= addresses[i] && address + n <= addresses[i] + 16) {
        memcpy(bytes, memory[i] + address - addresses[i], n); return 1;
    }
    return 0;
}
static int fixture_put(u32 address, unsigned char *bytes, unsigned int n)
{
    unsigned int i;
    ++writes;
    if (writes == fail_write || (fail_forever && writes >= fail_forever)) {
        if (partial_write) for (i = 0; i < 2; ++i)
            if (address == addresses[i] + 8) memcpy(memory[i] + 8, bytes, 2);
        return 0;
    }
    for (i = 0; i < 2; ++i) if (address == addresses[i] + 8 && n == 4) {
        memcpy(memory[i] + 8, bytes, n); return 1;
    }
    return 0;
}
static void reset(void)
{
    memcpy(memory, expected, sizeof(memory)); reads = writes = 0;
    fail_read = fail_write = partial_write = fail_forever = 0;
}
int main(void)
{
    int i, j;
    reset(); assert(change(fixture_get, fixture_put, 0) == 0 && writes == 0);
    for (i = 0; i < 2; ++i) for (j = 0; j < 16; ++j) {
        reset(); memory[i][j] ^= 1;
        assert(change(fixture_get, fixture_put, 1) == 2 && writes == 0);
    }
    for (i = 1; i <= 2; ++i) {
        reset(); fail_read = i;
        assert(change(fixture_get, fixture_put, 1) == 2 && writes == 0);
    }
    reset(); assert(change(fixture_get, fixture_put, 1) == 0 && writes == 2);
    for (i = 0; i < 2; ++i) assert(same(memory[i] + 8, (const unsigned char *)&replacement, 4));
    assert(change(fixture_get, fixture_put, 1) == 2 && writes == 2);
    assert(change(fixture_get, fixture_put, 2) == 0);
    assert(same((unsigned char *)memory, (const unsigned char *)expected, sizeof(memory)));
    for (i = 1; i <= 2; ++i) {
        reset(); fail_write = i; partial_write = 1;
        assert(change(fixture_get, fixture_put, 1) == 3);
        assert(same((unsigned char *)memory, (const unsigned char *)expected, sizeof(memory)));
    }
    for (i = 3; i <= 4; ++i) {
        reset(); fail_read = i;
        assert(change(fixture_get, fixture_put, 1) == 3);
        assert(same((unsigned char *)memory, (const unsigned char *)expected, sizeof(memory)));
    }
    reset(); fail_forever = 2; partial_write = 1;
    assert(change(fixture_get, fixture_put, 1) == 4);
    reset(); memcpy(memory[0] + 8, &replacement, 4);
    assert(change(fixture_get, fixture_put, 2) == 0);
    assert(same((unsigned char *)memory, (const unsigned char *)expected, sizeof(memory)));
    puts("init guard: context refusals, probe, apply, restore and I/O failures passed");
    return 0;
}
#else
#if !defined(__arm__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error This helper supports only the pinned little-endian ARM baseline.
#endif
static int descriptor;
static int equal_string(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static long syscall3(long number, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = number;
    __asm__ volatile("svc 0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory", "cc");
    return r0;
}
static int memory_get(u32 address, unsigned char *bytes, unsigned int count)
{
    if (syscall3(19, descriptor, address, 0) != (long)address) return 0;
    return syscall3(3, descriptor, (long)bytes, count) == (long)count;
}
static int memory_put(u32 address, unsigned char *bytes, unsigned int count)
{
    if (syscall3(19, descriptor, address, 0) != (long)address) return 0;
    return syscall3(4, descriptor, (long)bytes, count) == (long)count;
}
int guard_main(int argc, char **argv)
{
    int mode, result;
    if (argc != 2) return 2;
    if (equal_string(argv[1], "--probe")) mode = 0;
    else if (equal_string(argv[1], "--apply")) mode = 1;
    else if (equal_string(argv[1], "--restore")) mode = 2;
    else return 2;
    descriptor = syscall3(5, (long)"/proc/1/mem", mode ? 2 : 0, 0);
    if (descriptor < 0) return 2;
    result = change(memory_get, memory_put, mode);
    syscall3(6, descriptor, 0, 0);
    return result;
}
__asm__(".global _start\n_start:\nldr r0, [sp]\nadd r1, sp, #4\nbl guard_main\nmov r7, #1\nsvc 0\nb .\n");
#endif
