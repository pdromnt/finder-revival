/* Read-only equivalents of the five capacity-display shell queries.
 * No filesystem repair, deletion, file-data write, or cache of free space.
 * Hooking is restricted to the pinned stock ELF and the main GUI thread.
 */
#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include "native_capacity.h"

struct old_timespec { long sec, nsec; };
extern int old_clock(int, struct old_timespec *) __asm__("clock_gettime");
extern int old_statvfs(const char *, void *) __asm__("statvfs");
extern int old_scan(const char *, const char *, ...) __asm__("sscanf");
extern FILE *old_open_file(const char *, const char *) __asm__("fopen");
extern void *old_mmap(void *, size_t, int, int, int, long) __asm__("mmap");
extern void *opendir(const char *);
extern int closedir(void *);
struct old_dirent64 { uint64_t ino; int64_t off; unsigned short reclen; unsigned char type; char name[256]; };
extern struct old_dirent64 *readdir64(void *);

/* Linux v3.10 arch/arm/include/uapi/asm/stat.h, EABI alignment. */
struct arm_stat64 {
    uint64_t dev; unsigned char pad0[4]; uint32_t old_ino, mode, nlink, uid, gid;
    uint64_t rdev; unsigned char pad3[4]; int64_t size; uint32_t blksize;
    uint64_t blocks; uint32_t atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns;
    uint64_t ino;
};
_Static_assert(sizeof(void *) == 4, "ARM32 only");
_Static_assert(offsetof(struct arm_stat64, blocks) == 64, "ARM stat64 ABI");
_Static_assert(offsetof(struct arm_stat64, ino) == 96, "ARM stat64 ABI");
_Static_assert(offsetof(struct old_dirent64, name) == 19, "dirent64 ABI");

static const char *queries[] = {
    "df /data | grep /dev | grep /data",
    "du -s /data/log | tr -s ' '| cut -d '/' -f0",
    "du -s /data/pics | tr -s ' '| cut -d '/' -f0",
    "du -s /data/update | tr -s ' '| cut -d '/' -f0",
    "df /media | grep /dev | grep /media"
};
static const char *du_paths[] = {"/data/log", "/data/pics", "/data/update"};
static FILE *(*real_open)(const char *, const char *);
static int (*real_close)(FILE *);
static int gui_tid;
struct stream_slot { FILE *file; int query, native; struct old_timespec start; char text[256]; };
static struct stream_slot streams[8];

struct seen_inode { uint64_t dev, ino; };
struct walk_state { uint64_t blocks; unsigned int seen_count; int partial; struct seen_inode seen[256]; };

static long since(const struct old_timespec *start) {
    struct old_timespec now;
    old_clock(1, &now);
    return (now.sec-start->sec)*1000000L + (now.nsec-start->nsec)/1000L;
}

static int walk(const char *path, int depth, struct walk_state *state) {
    struct arm_stat64 st;
    if (depth > 16) return 0;
    if (syscall(196, path, &st) < 0) { /* ARM __NR_lstat64 */
        if (errno == ENOENT && depth) { state->partial = 1; return 1; }
        return 0;
    }
    int directory = (st.mode & 0170000) == 0040000;
    if (!directory && st.nlink > 1) {
        for (unsigned int i = 0; i < state->seen_count; i++)
            if (state->seen[i].dev == st.dev && state->seen[i].ino == st.ino) return 1;
        if (state->seen_count == 256) return 0;
        state->seen[state->seen_count++] = (struct seen_inode){st.dev, st.ino};
    }
    state->blocks += st.blocks;
    if (!directory) return 1; /* lstat: never follow symlinks. */
    void *dir = opendir(path);
    if (!dir) return 0;
    struct old_dirent64 *entry;
    int ok = 1;
    for (;;) {
        errno = 0;
        entry = readdir64(dir);
        if (!entry) { if (errno) ok = 0; break; }
        if (!strcmp(entry->name, ".") || !strcmp(entry->name, "..")) continue;
        char child[1024];
        int size = snprintf(child, sizeof(child), "%s/%s", path, entry->name);
        if (size < 0 || size >= (int)sizeof(child) || !walk(child, depth+1, state)) { ok = 0; break; }
    }
    closedir(dir);
    return ok;
}

static unsigned int divide32(unsigned int n, unsigned int d) {
    unsigned int q = 0;
    if (!d) return 0;
    for (int bit = 31; bit >= 0; bit--)
        if ((n >> bit) >= d) { n -= d << bit; q |= 1u << bit; }
    return q;
}

static int query(int index, char *out, size_t length, int *partial) {
    *partial = 0;
    if (index >= 1 && index <= 3) {
        struct walk_state state = {0};
        if (!walk(du_paths[index-1], 0, &state)) return 0;
        *partial = state.partial;
        int n = snprintf(out, length, "%llu\t\n", (unsigned long long)((state.blocks+1) >> 1));
        return n > 0 && (size_t)n < length;
    }
    const char *path = index == 0 ? "/data" : "/media";
    char device[128] = "", mount[128], line[512];
    FILE *file = old_open_file("/proc/mounts", "r");
    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        if (old_scan(line, "%127s %127s", device, mount) == 2 && !strcmp(mount, path)) break;
        device[0] = 0;
    }
    fclose(file);
    if (!device[0]) return 0;
    /* The unversioned glibc 2.18 statvfs entry writes 32-bit count fields. */
    unsigned long data[32] = {0};
    if (old_statvfs(path, data)) return 0;
    unsigned long unit = data[1] ? data[1] : data[0];
    if (unit < 512 || unit > 65536 || data[3] > data[2] || data[4] > data[2]) return 0;
    uint64_t total = (uint64_t)data[2]*unit >> 10;
    uint64_t used = (uint64_t)(data[2]-data[3])*unit >> 10;
    uint64_t avail = (uint64_t)data[4]*unit >> 10;
    if (used > 0xffffffffu/100u || used+avail > 0xffffffffu) return 0;
    unsigned int percentage = divide32((unsigned int)used*100u, (unsigned int)(used+avail));
    int n = snprintf(out, length, "%s %llu %llu %llu %u%% %s\n", device,
                    (unsigned long long)total, (unsigned long long)used,
                    (unsigned long long)avail, percentage, path);
    return n > 0 && (size_t)n < length;
}

static FILE *capacity_open(const char *command, const char *mode) {
    int index = -1;
    if (command && mode && !strcmp(mode, "r") && syscall(224) == gui_tid) /* ARM gettid */
        for (int i = 0; i < 5; i++) if (!strcmp(command, queries[i])) { index = i; break; }
    if (index < 0) return real_open(command, mode);
    struct stream_slot *slot = NULL;
    for (int i = 0; i < 8; i++) if (!streams[i].file) { slot = &streams[i]; break; }
    if (!slot) return real_open(command, mode);
    old_clock(1, &slot->start);
    slot->query = index;
    slot->native = 0;
    int partial = 0;
    if (ffinder_native_capacity_active() && query(index, slot->text, sizeof(slot->text), &partial)) {
        slot->file = fmemopen(slot->text, strlen(slot->text), "r");
        slot->native = slot->file != NULL;
        if (slot->native && partial)
            fprintf(stderr, "REVIVAL_UI_QUERY query=%d partial=1 (same missing-entry behavior as du)\n", index);
    }
    if (!slot->file) slot->file = real_open(command, mode);
    return slot->file;
}

static int capacity_close(FILE *file) {
    if (syscall(224) != gui_tid) return real_close(file);
    for (int i = 0; i < 8; i++) if (streams[i].file == file) {
        int result = streams[i].native ? fclose(file) : real_close(file);
        fprintf(stderr, "REVIVAL_UI_QUERY query=%d native=%d duration_us=%ld\n", streams[i].query, streams[i].native, since(&streams[i].start));
        streams[i].file = NULL;
        return result;
    }
    return real_close(file);
}

static int cache_flush(void *first, size_t length) {
    return syscall(0x0f0002, first, (char *)first+length, 0); /* ARM cacheflush */
}

static uint32_t branch_to(uintptr_t from, uintptr_t to) {
    intptr_t delta = (intptr_t)to-(intptr_t)from-8;
    return 0xea000000u | (((uint32_t)(delta >> 2)) & 0x00ffffffu);
}

int ffinder_capacity_install(void) {
    static int installed;
    if (installed) return 1;
    uintptr_t open_address = 0xbb05c, close_address = 0xbb234;
    if (*(uint32_t *)open_address != 0xe92d4070 || *(uint32_t *)close_address != 0xe92d4030) {
        fprintf(stderr, "REVIVAL_UI_QUERY: stock function guard failed\n"); return 0;
    }
    uint32_t *page = (void *)-1;
    const uintptr_t hints[] = {0x01000000, 0x00800000, 0x02000000};
    for (unsigned int i = 0; i < 3; i++) {
        page = old_mmap((void *)hints[i], 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (page == (void *)-1) continue;
        intptr_t delta = (intptr_t)page-(intptr_t)open_address-8;
        if (delta > -0x2000000 && delta < 0x2000000-64) break;
        munmap(page, 4096); page = (void *)-1;
    }
    if (page == (void *)-1) { fprintf(stderr, "REVIVAL_UI_QUERY: no nearby trampoline mapping\n"); return 0; }
    /* Entry stubs preserve the original caller LR; original-function stubs
     * replay only a verified PUSH, then return to the original code address. */
    page[0] = 0xe51ff004; page[1] = (uint32_t)(uintptr_t)capacity_open;
    page[4] = 0xe92d4070; page[5] = 0xe51ff004; page[6] = open_address+4;
    page[8] = 0xe51ff004; page[9] = (uint32_t)(uintptr_t)capacity_close;
    page[12] = 0xe92d4030; page[13] = 0xe51ff004; page[14] = close_address+4;
    if (cache_flush(page, 64) || mprotect(page, 4096, PROT_READ|PROT_EXEC)) { munmap(page, 4096); return 0; }
    real_open = (void *)(page+4);
    real_close = (void *)(page+12);
    gui_tid = (int)syscall(224);
    void *text_page = (void *)(open_address & ~4095u);
    if (mprotect(text_page, 4096, PROT_READ|PROT_WRITE|PROT_EXEC)) { munmap(page, 4096); return 0; }
    /* Install close first; aligned ARM instruction writes are single-word. */
    *(volatile uint32_t *)close_address = branch_to(close_address, (uintptr_t)(page+8));
    if (cache_flush((void *)close_address, 4)) goto restore;
    *(volatile uint32_t *)open_address = branch_to(open_address, (uintptr_t)page);
    if (cache_flush((void *)open_address, 4)) goto restore;
    if (mprotect(text_page, 4096, PROT_READ|PROT_EXEC)) goto restore;
    installed = 1;
    fprintf(stderr, "REVIVAL_UI_QUERY: guarded RAM hooks installed; only GUI df/du queries eligible\n");
    return 1;
restore:
    *(volatile uint32_t *)open_address = 0xe92d4070;
    *(volatile uint32_t *)close_address = 0xe92d4030;
    cache_flush(text_page, 4096);
    mprotect(text_page, 4096, PROT_READ|PROT_EXEC);
    /* Retain the page if another thread had already entered a trampoline. */
    fprintf(stderr, "REVIVAL_UI_QUERY: hook setup failed; stock instructions restored\n");
    return 0;
}

/* Explicit diagnostic mode works in /bin/true; it does not load the Qt UI. */
__attribute__((constructor)) static void capacity_probe(void) {
    if (!getenv("REVIVAL_UI_FS_PROBE")) return;
    for (int i = 0; i < 5; i++) {
        char text[256] = ""; int partial = 0;
        int ok = query(i, text, sizeof(text), &partial);
        fprintf(stdout, "NATIVE_QUERY index=%d ok=%d partial=%d output=%s", i, ok, partial, text);
        if (!ok) fprintf(stdout, "\n");
    }
    const char *fixture = getenv("REVIVAL_UI_FS_FIXTURE");
    if (fixture && !strncmp(fixture, "/run/revival-ui/", strlen("/run/revival-ui/"))) {
        struct walk_state state = {0};
        int ok = walk(fixture, 0, &state);
        fprintf(stdout, "NATIVE_FIXTURE ok=%d partial=%d kib=%llu\n", ok, state.partial, (unsigned long long)((state.blocks+1)>>1));
    }
}
