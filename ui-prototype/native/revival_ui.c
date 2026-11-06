/* Temporary Qt 4/QWS home-screen adapter for the verified Finder 2.8.4 ELF.
 * Loaded only by an explicit LD_PRELOAD launch from RAM. The theme is data.
 * ABI facts come from this ELF's symbols, decompilation, and Qt 4.8.6 headers.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "native_capacity.h"
#include "flow_skin.h"

/* The host toolchain's glibc headers redirect to C23/time64 entry points absent
 * from the printer's glibc 2.18. Bind this small adapter to the legacy ABI. */
struct legacy_timespec { long tv_sec, tv_nsec; };
extern FILE *legacy_fopen(const char *, const char *) __asm__("fopen");
extern int legacy_sscanf(const char *, const char *, ...) __asm__("sscanf");
extern long legacy_strtol(const char *, char **, int) __asm__("strtol");
extern int legacy_clock_gettime(int, struct legacy_timespec *) __asm__("clock_gettime");
extern int legacy_fcntl(int,int,...) __asm__("fcntl");

static int socket_owner_is_prototype(void) {
    static int matched=-1;
    if(matched<0) {
        char path[256];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);
        if(n>=0)path[n]=0;
        matched=n>=0 && getenv("REVIVAL_UI_CONFIG") && !strcmp(path,"/PROGRAM/software/2.8.4/finder-plus-arm");
    }
    return matched;
}
static int socket_no_inherit(int fd) {
    if(fd>=0 && socket_owner_is_prototype()) {
        int saved=errno,flags=legacy_fcntl(fd,F_GETFD);
        if(flags>=0)legacy_fcntl(fd,F_SETFD,flags|FD_CLOEXEC);
        errno=saved;
    }
    return fd;
}
int revival_socket(int,int,int) __asm__("socket");
int revival_socket(int domain,int type,int protocol) {
    static int (*original)(int,int,int);
    if(!original)original=dlsym(RTLD_NEXT,"socket");
    if(!original)return -1;
    return socket_no_inherit(original(domain,type,protocol));
}
int revival_accept(int,void *,void *) __asm__("accept");
int revival_accept(int fd,void *address,void *length) {
    static int (*original)(int,void *,void *);
    if(!original)original=dlsym(RTLD_NEXT,"accept");
    if(!original)return -1;
    return socket_no_inherit(original(fd,address,length));
}

typedef struct { void *d; } QtString;
typedef struct { int left, top, right, bottom; } QtRect;
typedef struct { int width, height; } QtSize;
typedef struct { char name[80]; void *object; int label; } Binding;
static Binding bindings[48];
static int binding_count, profiling, auto_bench, coalesce_home, bench_capacity_sync, skip_capacity_sync;
static int bench_native_capacity, native_capacity;
static struct legacy_timespec bench_started;
static void *(*string_ascii)(const char *, int);
static void (*object_name)(QtString *, const void *);
static void (*set_object_name)(void *, const QtString *);
static void (*set_geometry)(void *, const QtRect *);
static void (*set_style)(void *, const QtString *);
static void (*button_text)(void *, const QtString *);
static void (*label_text)(void *, const QtString *);
static void (*label_ctor)(void *, void *, const int *);
static void (*set_visible)(void *, unsigned char);
static void (*set_attribute)(void *, int, unsigned char);
static void (*set_tool_style)(void *, int);
static void (*set_icon_size)(void *, const QtSize *);
static int (*add_font)(const QtString *);
static void (*pixmap_ctor)(void *, const QtString *, const char *, const int *);
static void (*pixmap_copy)(void *, const void *, const QtRect *);
static void (*pixmap_scaled)(void *, const void *, const QtSize *, int, int);
static void (*pixmap_destroy)(void *);
static void (*label_pixmap)(void *, const void *);
static void (*single_shot)(int, const void *, const char *);
static void (*destroy_string)(QtString *) = (void *)0x000386a8;

static const char *class_of(void *object) {
    if (!object) return "<null>";
    const void *(*meta_fn)(const void *) = (*(void ***)object)[0];
    const void *const *meta = meta_fn(object);
    const char *names = meta[1];
    const unsigned int *data = meta[2];
    return names && data ? names + data[1] : "<unknown>";
}

static long elapsed_us(const struct legacy_timespec *before, const struct legacy_timespec *after) {
    return (after->tv_sec-before->tv_sec)*1000000L + (after->tv_nsec-before->tv_nsec)/1000L;
}

static void name_of(void *object, char *out, size_t length) {
    QtString value = {0};
    out[0] = 0;
    if (!object_name || !object) return;
    object_name(&value, object); /* Non-trivial C++ return uses the hidden sret argument. */
    if (value.d) {
        int size = ((int *)value.d)[2];
        const unsigned short *text = ((const unsigned short **)value.d)[3];
        if (text && size >= 0 && size < 256) {
            size_t n = (size_t)size < length - 1 ? (size_t)size : length - 1;
            for (size_t i = 0; i < n; i++) out[i] = text[i] < 128 ? (char)text[i] : '?';
            out[n] = 0;
        }
        destroy_string(&value);
    }
}

static Binding *binding(const char *name) {
    for (int i = 0; i < binding_count; i++)
        if (!strcmp(bindings[i].name, name)) return &bindings[i];
    return NULL;
}

static void bind(const char *name, void *object, int label) {
    if (binding_count == 48) return;
    snprintf(bindings[binding_count].name, sizeof(bindings[0].name), "%s", name);
    bindings[binding_count].object = object;
    bindings[binding_count++].label = label;
}

static void string_call(void (*fn)(void *, const QtString *), void *object, const char *text) {
    QtString value = { string_ascii(text, -1) };
    fn(object, &value);
    destroy_string(&value);
}

static int load_api(void) {
    char executable[256];
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable)-1);
    if (n < 0) return 0;
    executable[n] = 0;
    if (strcmp(executable, "/PROGRAM/software/2.8.4/finder-plus-arm")) return 0;
    static const unsigned char expected[] = {0xf8,0x40,0x2d,0xe9,0x00,0x50,0x90,0xe5,0x34,0x60,0x9f,0xe5,0x00,0x70,0xa0,0xe1};
    if (memcmp((void *)destroy_string, expected, sizeof(expected))) {
        fprintf(stderr, "REVIVAL_UI: target ELF guard failed\n");
        return 0;
    }
#define LOAD(var, symbol) do { *(void **)(&var) = dlsym(RTLD_DEFAULT, symbol); if (!var) { fprintf(stderr, "REVIVAL_UI: missing %s\n", symbol); return 0; } } while (0)
    LOAD(string_ascii, "_ZN7QString16fromAscii_helperEPKci");
    LOAD(object_name, "_ZNK7QObject10objectNameEv");
    LOAD(set_object_name, "_ZN7QObject13setObjectNameERK7QString");
    LOAD(set_geometry, "_ZN7QWidget11setGeometryERK5QRect");
    LOAD(set_style, "_ZN7QWidget13setStyleSheetERK7QString");
    LOAD(button_text, "_ZN15QAbstractButton7setTextERK7QString");
    LOAD(label_text, "_ZN6QLabel7setTextERK7QString");
    LOAD(label_ctor, "_ZN6QLabelC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE");
    LOAD(set_visible, "_ZN7QWidget10setVisibleEb");
    LOAD(set_attribute, "_ZN7QWidget12setAttributeEN2Qt15WidgetAttributeEb");
    LOAD(set_tool_style, "_ZN11QToolButton18setToolButtonStyleEN2Qt15ToolButtonStyleE");
    LOAD(set_icon_size, "_ZN15QAbstractButton11setIconSizeERK5QSize");
    LOAD(add_font, "_ZN13QFontDatabase18addApplicationFontERK7QString");
    LOAD(pixmap_ctor, "_ZN7QPixmapC1ERK7QStringPKc6QFlagsIN2Qt19ImageConversionFlagEE");
    LOAD(pixmap_copy, "_ZNK7QPixmap4copyERK5QRect");
    LOAD(pixmap_scaled, "_ZNK7QPixmap6scaledERK5QSizeN2Qt15AspectRatioModeENS3_18TransformationModeE");
    LOAD(pixmap_destroy, "_ZN7QPixmapD1Ev");
    LOAD(label_pixmap, "_ZN6QLabel9setPixmapERK7QPixmap");
    LOAD(single_shot, "_ZN6QTimer10singleShotEiP7QObjectPKc");
#undef LOAD
    return 1;
}

static int apply_theme(const char *path) {
    FILE *file = legacy_fopen(path, "r");
    if (!file) { perror("REVIVAL_UI config"); return 0; }
    void *main = *(void **)0x00384974;
    char name[80];
    name_of(main, name, sizeof(name));
    if (strcmp(name, "MainWindow")) {
        fprintf(stderr, "REVIVAL_UI: unexpected main object: %s\n", name);
        fclose(file); return 0;
    }
    bind("MainWindow", main, 0);
    void **ui = *(void ***)(main + 0xac);
    for (int i = 0; i < 12; i++) {
        name_of(ui[i], name, sizeof(name));
        if (*name) bind(name, ui[i], i == 10);
    }
    fprintf(stderr, "REVIVAL_UI: resolved %d native widgets\n", binding_count);
    if (!binding("showBuild") || !binding("showPreheat") || !binding("showTools")) {
        fclose(file); return 0;
    }
    char line[8192];
    int applied = 0, line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        line[strcspn(line, "\r\n")] = 0;
        if (!*line || *line == '#') continue;
        char *state = NULL;
        char *op = strtok_r(line, "\t", &state);
        char *target = strtok_r(NULL, "\t", &state);
        if (!op || !target) goto invalid;
        if (!strcmp(op, "font")) {
            QtString value = { string_ascii(target, -1) };
            int id = add_font(&value);
            destroy_string(&value);
            fprintf(stderr, "REVIVAL_UI: application font %d\n", id);
            if (id < 0) goto invalid;
            applied++; continue;
        }
        Binding *b = binding(target);
        if (!strcmp(op, "label")) {
            if (b) goto invalid;
            int flags = 0;
            void *label = calloc(1, 0x14); /* QLabel has no extra instance fields beyond QWidget. */
            if (!label) goto invalid;
            label_ctor(label, main, &flags); /* Qt 4 QFlags has a non-trivial copy constructor. */
            string_call(set_object_name, label, target);
            set_attribute(label, 51, 1); /* WA_TransparentForMouseEvents */
            bind(target, label, 1);
            set_visible(label, 1);
            applied++; continue;
        }
        if (!b) goto invalid;
        if (!strcmp(op, "style") || !strcmp(op, "text")) {
            char *value = state;
            if (!value) goto invalid;
            /* Decode a literal backslash-n for deliberate multiline QLabel copy. */
            char *src = value, *dest = value;
            while (*src) {
                if (src[0] == '\\' && src[1] == 'n') { *dest++ = '\n'; src += 2; }
                else *dest++ = *src++;
            }
            *dest = 0;
            string_call(!strcmp(op, "style") ? set_style : b->label ? label_text : button_text, b->object, value);
        } else if (!strcmp(op, "pixmap-region")) {
            char *path = strtok_r(NULL, "\t", &state);
            QtRect crop; int x,y,w,h,tw=0,th=0,flags=0;
            int fields=state?legacy_sscanf(state,"%d\t%d\t%d\t%d\t%d\t%d",&x,&y,&w,&h,&tw,&th):0;
            if (!b->label || !path || (fields!=4 && fields!=6) || x<0 || y<0 || w<1 || h<1 || (fields==6 && (tw<1 || th<1))) goto invalid;
            crop=(QtRect){x,y,x+w-1,y+h-1};
            uint64_t source[8]={0}, region[8]={0}; QtString file={string_ascii(path,-1)};
            pixmap_ctor(source,&file,NULL,&flags); destroy_string(&file);
            pixmap_copy(region,source,&crop);
            if(fields==6) {
                uint64_t scaled[8]={0};QtSize size={tw,th};
                pixmap_scaled(scaled,region,&size,0,1);label_pixmap(b->object,scaled);pixmap_destroy(scaled);
            } else label_pixmap(b->object,region);
            pixmap_destroy(region); pixmap_destroy(source);
        } else if (!strcmp(op, "geometry")) {
            int x, y, w, h;
            if (!state || legacy_sscanf(state, "%d\t%d\t%d\t%d", &x, &y, &w, &h) != 4 || w < 1 || h < 1 || x < 0 || y < 0 || x+w > 480 || y+h > 320) goto invalid;
            QtRect rect = {x, y, x+w-1, y+h-1};
            set_geometry(b->object, &rect);
        } else if (!strcmp(op, "iconsize")) {
            QtSize size;
            if (!state || legacy_sscanf(state, "%d\t%d", &size.width, &size.height) != 2) goto invalid;
            set_icon_size(b->object, &size);
        } else if (!strcmp(op, "toolstyle")) {
            int style = state ? (int)legacy_strtol(state, NULL, 10) : -1;
            if (style < 0 || style > 4) goto invalid;
            set_tool_style(b->object, style);
        } else if (!strcmp(op, "visible")) {
            set_visible(b->object, state && legacy_strtol(state, NULL, 10) != 0);
        } else goto invalid;
        applied++; continue;
invalid:
        fprintf(stderr, "REVIVAL_UI: rejected config line %d\n", line_number);
        fclose(file); return 0;
    }
    fclose(file);
    fprintf(stderr, "REVIVAL_UI: applied %d operations; stock action connections retained\n", applied);
    return 1;
}

int revival_exec(void) __asm__("_ZN12QApplication4execEv");
int revival_exec(void) {
    int (*original)(void) = dlsym(RTLD_NEXT, "_ZN12QApplication4execEv");
    if (!original) { fprintf(stderr, "REVIVAL_UI: original event loop missing\n"); return 1; }
    const char *path = getenv("REVIVAL_UI_CONFIG");
    if (path && load_api()) {
        int result = apply_theme(path);
        const char *flow_config = getenv("REVIVAL_UI_FLOW_CONFIG");
        int flow = result && flow_config && ffinder_flow_init(flow_config);
        profiling = result && getenv("REVIVAL_UI_PROFILE") != NULL;
        auto_bench = result && getenv("REVIVAL_UI_AUTOBENCH") != NULL;
        coalesce_home = result && getenv("REVIVAL_UI_COALESCE_HOME_REPAINT") != NULL;
        bench_capacity_sync = result && getenv("REVIVAL_UI_BENCH_CAPACITY_SYNC") != NULL;
        skip_capacity_sync = result && getenv("REVIVAL_UI_SKIP_CAPACITY_SYNC") != NULL;
        bench_native_capacity = result && getenv("REVIVAL_UI_BENCH_NATIVE_CAPACITY") != NULL;
        native_capacity = result && (flow || getenv("REVIVAL_UI_NATIVE_CAPACITY") != NULL);
        if ((bench_native_capacity || native_capacity) && !ffinder_capacity_install()) {
            bench_native_capacity = native_capacity = 0;
        }
        fprintf(stderr, "REVIVAL_UI: repaint mode=%s; automated navigation=%d\n", coalesce_home ? "coalesced-home" : "baseline", auto_bench);
        if (auto_bench) {
            void *main = *(void **)0x00384974;
            legacy_clock_gettime(CLOCK_MONOTONIC, &bench_started);
            single_shot(25000, main, "1on_showTools_pressed()");
            single_shot(30000, main, "1on_showBuild_pressed()");
            single_shot(35000, main, "1on_showTools_pressed()");
            single_shot(40000, main, "1on_showBuild_pressed()");
            single_shot(50000, main, "1on_showTools_pressed()");
            single_shot(55000, main, "1on_showBuild_pressed()");
            single_shot(60000, main, "1on_showTools_pressed()");
            single_shot(65000, main, "1on_showBuild_pressed()");
            single_shot(75000, main, "1on_showTools_pressed()");
            single_shot(80000, main, "1on_showBuild_pressed()");
            single_shot(85000, main, "1on_showTools_pressed()");
            single_shot(90000, main, "1on_showBuild_pressed()");
        }
    }
    return original();
}

static int home_repaint_mode(void) {
    if (auto_bench && !bench_capacity_sync && !bench_native_capacity) {
        struct legacy_timespec now;
        legacy_clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = now.tv_sec - bench_started.tv_sec;
        return elapsed >= 45 && elapsed < 70;
    }
    return coalesce_home;
}

int ffinder_native_capacity_active(void) {
    if (auto_bench && bench_native_capacity) {
        struct legacy_timespec now;
        legacy_clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = now.tv_sec - bench_started.tv_sec;
        return elapsed >= 45 && elapsed < 70;
    }
    return native_capacity;
}

static int capacity_sync_mode(void) {
    if (auto_bench && bench_capacity_sync) {
        struct legacy_timespec now;
        legacy_clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = now.tv_sec - bench_started.tv_sec;
        return elapsed >= 45 && elapsed < 70;
    }
    return skip_capacity_sync;
}

void revival_sync(void) __asm__("sync");
void revival_sync(void) {
    static void (*original)(void);
    if (!original) original = dlsym(RTLD_NEXT, "sync");
    if (!original) return;
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    /* These two pinned ELF methods only read df/du output and update widgets.
     * Every sync from all other code paths is passed through unchanged. */
    int capacity = caller >= 0xc7b14 && caller < 0xcb904;
    if (profiling && capacity && capacity_sync_mode()) {
        fprintf(stderr, "REVIVAL_UI_SYNC caller=%lx capacity=1 skipped_readonly=1\n", (unsigned long)caller);
        return;
    }
    if (!profiling) { original(); return; }
    struct legacy_timespec before, after;
    legacy_clock_gettime(CLOCK_MONOTONIC, &before);
    original();
    legacy_clock_gettime(CLOCK_MONOTONIC, &after);
    fprintf(stderr, "REVIVAL_UI_SYNC caller=%lx capacity=%d duration_us=%ld\n", (unsigned long)caller, capacity, elapsed_us(&before, &after));
}

unsigned char revival_notify(void *, void *, void *) __asm__("_ZN12QApplication6notifyEP7QObjectP6QEvent");
unsigned char revival_notify(void *app, void *receiver, void *event) {
    static unsigned char (*original)(void *, void *, void *);
    if (!original) original = dlsym(RTLD_NEXT, "_ZN12QApplication6notifyEP7QObjectP6QEvent");
    if (!original) return 0;
    ffinder_flow_event(receiver, *(unsigned short *)(event + 8));
    if (!profiling) {
        unsigned char result = original(app, receiver, event);
        ffinder_flow_tick();
        return result;
    }
    /* Qt 4 QEvent: vptr, private pointer, ushort type (offset 8 on this ARM ABI). */
    unsigned short type = *(unsigned short *)(event + 8);
    char name[80] = "";
    char class_name[80];
    snprintf(class_name, sizeof(class_name), "%s", class_of(receiver));
    if (type == 1 || type == 2 || type == 3 || type == 12 || type == 43 || type == 77) name_of(receiver, name, sizeof(name));
    struct legacy_timespec before, after;
    legacy_clock_gettime(CLOCK_MONOTONIC, &before);
    unsigned char result = original(app, receiver, event);
    legacy_clock_gettime(CLOCK_MONOTONIC, &after);
    long us = elapsed_us(&before, &after);
    if (type == 2 || type == 3 || us > 40000)
        fprintf(stderr, "REVIVAL_UI_EVENT type=%u receiver=%s class=%s dispatch_us=%ld\n", type, name, class_name, us);
    /* The flow adapter suppresses refresh inside stock status visibility/
     * geometry changes, while allowing legitimate nested processEvents loops. */
    ffinder_flow_tick();
    return result;
}

void revival_repaint(void *) __asm__("_ZN7QWidget7repaintEv");
void revival_repaint(void *widget) {
    static void (*original)(void *);
    static void (*update)(void *);
    if (!original) original = dlsym(RTLD_NEXT, "_ZN7QWidget7repaintEv");
    if (!original) return;
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    if (home_repaint_mode() && caller >= 0x129ed0 && caller < 0x12a26c) {
        if (!update) update = dlsym(RTLD_NEXT, "_ZN7QWidget6updateEv");
        if (update) {
            update(widget);
            fprintf(stderr, "REVIVAL_UI_REPAINT caller=%lx coalesced=1\n", (unsigned long)caller);
            return;
        }
    }
    if (!profiling) { original(widget); return; }
    char name[80];
    name_of(widget, name, sizeof(name));
    struct legacy_timespec before, after;
    legacy_clock_gettime(CLOCK_MONOTONIC, &before);
    original(widget);
    legacy_clock_gettime(CLOCK_MONOTONIC, &after);
    fprintf(stderr, "REVIVAL_UI_REPAINT caller=%lx receiver=%s duration_us=%ld\n", (unsigned long)caller, name, elapsed_us(&before, &after));
}

void revival_visible(void *, unsigned char) __asm__("_ZN7QWidget10setVisibleEb");
void revival_visible(void *widget, unsigned char visible) {
    static void (*original)(void *, unsigned char);
    if (!original) original = dlsym(RTLD_NEXT, "_ZN7QWidget10setVisibleEb");
    if (!original) return;
    int status_tracked=ffinder_flow_status_enter(widget);
    ffinder_flow_show(widget, visible);
    int measured = profiling && visible;
    char class_name[80] = "";
    struct legacy_timespec before, after;
    if (measured) {
        snprintf(class_name, sizeof(class_name), "%s", class_of(widget));
        legacy_clock_gettime(CLOCK_MONOTONIC, &before);
    }
    original(widget, visible);
    ffinder_flow_status_leave(status_tracked);
    if (measured && (!strcmp(class_name, "Tools") || !strcmp(class_name, "Build"))) {
        legacy_clock_gettime(CLOCK_MONOTONIC, &after);
        fprintf(stderr, "REVIVAL_UI_SHOW class=%s duration_us=%ld\n", class_name, elapsed_us(&before, &after));
        if (auto_bench) single_shot(1000, widget, "1on_back_pressed()");
    }
}
