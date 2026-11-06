/* Pinned Finder 2.8.4 logging adapter. No instruction or stock ELF changes.
 * All retained Qt messages go to two capped 64 KiB files on tmpfs. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef void (*handler)(int, const char *);
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int (*real_open)(const char *, int, ...);
static int log_fd = -1;
static unsigned used;
#define LIMIT 65536U
#define LOG "/run/revival-logs/ui.log"

static int target(void) {
    char exe[160];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe)-1);
    if (n < 0) return 0;
    exe[n] = 0;
    return !strcmp(exe, "/PROGRAM/software/2.8.4/finder-plus-arm");
}

static int stock_log_path(const char *path) {
    if (strncmp(path,"/data/log/",10)) return 0;
    path+=10;
    while (*path=='/') path++;
    return !strcmp(path,"finder-plus-arm.log");
}

/* Only exact known routine messages: never suppress an entire source file. */
static int routine(int type, const char *s) {
    static const char *const noise[] = {
        "wifiDevice/myserver.cpp updateStatus 105",
        "wifiDevice/myserver.cpp updateStatus 156",
        "wifiDevice/myserver.cpp updateStatus 176 isConnected == false",
        "UI/mainwindow.cpp slot_disconnected 2471",
        "UI/mainwindow.cpp dialogNoButtonClose 1604",
        "Remote: IP : \"127.0.0.1\""
    };
    if (type != 2 || !s) return 0; /* Fatal and warning messages always retained. */
    size_t n = strlen(s);
    while (n && (s[n-1]==' ' || s[n-1]=='\n' || s[n-1]=='\r')) n--;
    for (unsigned i=0; i<sizeof(noise)/sizeof(noise[0]); i++)
        if (strlen(noise[i])==n && !memcmp(s,noise[i],n)) return 1;
    return 0;
}

static void ram_message(int type, const char *message) {
    int saved = errno;
    if (routine(type,message)) { errno=saved; return; }
    char line[2304];
    int len = snprintf(line,sizeof(line),"[Qt %d] %.2200s\n",type,message?message:"<null>");
    if (len<0) { errno=saved; return; }
    pthread_mutex_lock(&lock);
    if (!real_open) *(void **)(&real_open)=dlsym(RTLD_NEXT,"open");
    if (real_open) {
        if (log_fd>=0 && used+(unsigned)len>LIMIT) {
            close(log_fd); log_fd=-1; used=0;
            rename(LOG,LOG ".1");
        }
        if (log_fd<0) {
            /* Starting a fresh process intentionally starts a fresh RAM log. */
            log_fd=real_open(LOG,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
            used=0;
        }
        if (log_fd>=0) {
            size_t at=0;
            while (at<(size_t)len) {
                ssize_t n=write(log_fd,line+at,(size_t)len-at);
                if (n<0 && errno==EINTR) continue;
                if (n<=0) break;
                at+=(size_t)n;
            }
            used+=(unsigned)at;
        }
    }
    pthread_mutex_unlock(&lock);
    errno=saved;
}

/* Install our handler at the stock registration point. Qt retains its own
 * fatal-message abort semantics; we change only the message destination. */
handler revival_install(handler requested) __asm__("_Z18qInstallMsgHandlerPFv9QtMsgTypePKcE");
handler revival_install(handler requested) {
    handler (*original)(handler)=dlsym(RTLD_NEXT,"_Z18qInstallMsgHandlerPFv9QtMsgTypePKcE");
    static const unsigned char prologue[]={0x94,0x31,0x9f,0xe5,0xf0,0x40,0x2d,0xe9,
                                         0x00,0x30,0x93,0xe5,0x14,0xd0,0x4d,0xe2};
    if (!original) return NULL;
    if (target() && requested==(handler)0x1198fc &&
        !memcmp((void *)requested,prologue,sizeof(prologue))) {
        ram_message(0,"Revival logging active: bounded RAM only; routine connection/menu chatter filtered.");
        return original(ram_message);
    }
    return original(requested);
}

/* The stock main opens this file before registering its Qt handler. Redirect
 * that exact open too, so startup and any direct saveLog calls cannot hit eMMC.
 * /dev/null's size stays zero, so stock log rotation is never entered. */
int revival_open(const char *,int,...) __asm__("open");
int revival_open(const char *path,int flags,...) {
    mode_t mode=0;
    if (flags&O_CREAT) {
        va_list ap; va_start(ap,flags); mode=(mode_t)va_arg(ap,int); va_end(ap);
    }
    if (!real_open) *(void **)(&real_open)=dlsym(RTLD_NEXT,"open");
    if (!real_open) { errno=ENOSYS; return -1; }
    if (stock_log_path(path) && target()) path="/dev/null";
    return real_open(path,flags,mode);
}

#ifdef LOG_TEST
#include <assert.h>
int main(void) {
    assert(routine(2,"Remote: IP : \"127.0.0.1\" \n"));
    assert(routine(2,"wifiDevice/myserver.cpp updateStatus 176 isConnected == false "));
    assert(!routine(3,"UI/mainwindow.cpp dialogNoButtonClose 1604"));
    assert(!routine(1,"UI/mainwindow.cpp slot_disconnected 2471"));
    assert(!routine(2,"wifiDevice/myserver.cpp updateStatus 176 actual failure"));
    assert(!routine(2,"Remote: IP : \"192.168.0.2\""));
    assert(!routine(2,NULL));
    assert(stock_log_path("/data/log/finder-plus-arm.log"));
    assert(stock_log_path("/data/log//finder-plus-arm.log"));
    assert(!stock_log_path("/data/log/finder-plus-arm.log.1"));
    assert(!stock_log_path("/data/log-other/finder-plus-arm.log"));
    assert(!mkdir("/run/revival-logs",0700) || errno==EEXIST);
    ram_message(3,"Retained fatal diagnostic (test calls handler only).");
    struct stat st;
    assert(!stat(LOG,&st) && st.st_size>0);
    char payload[2201];memset(payload,'x',2200);payload[2200]=0;
    for (int i=0;i<300;i++) ram_message(1,payload);
    assert(!stat(LOG,&st) && st.st_size<=LIMIT && st.st_size>0);
    assert(!stat(LOG ".1",&st) && st.st_size<=LIMIT && st.st_size>0);
    puts("Exact noise filter checks passed.");
    return 0;
}
#endif
