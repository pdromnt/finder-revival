/* Transport e2image's sparse output to the PC without storing an image in RAM.
 * The source device remains opened by e2image's ordinary read-only IO manager.
 * Only the synthetic output path below is replaced. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
static int output=-1;
static int transport=-1;
static int64_t position;
static ssize_t (*real_write)(int,const void *,size_t);
static int send_all(const void *data,size_t size) {
    if(!real_write) *(void **)(&real_write)=dlsym(RTLD_NEXT,"write");
    while(size) {
        ssize_t n=real_write(transport,data,size);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return -1;
        data=(const char *)data+n;size-=n;
    }
    return 0;
}
static int frame(uint32_t size,uint32_t kind) {
    struct {uint64_t offset;uint32_t length,kind;} header={(uint64_t)position,size,kind};
    return send_all(&header,sizeof(header));
}
int ext2fs_open_file(const char *path,int flags,mode_t mode) {
    if(!strcmp(path,"/run/stream-image")) {
        /* Old e2image flushes buffered "Scanning inodes..." to stdout at exit.
         * Preserve a separate transport FD and move all normal stdout to stderr. */
        transport=dup(1);
        if(transport<0 || dup2(2,1)<0)return -1;
        output=open("/dev/null",O_RDWR|O_CLOEXEC);position=0;return output;
    }
    int (*original)(const char *,int,mode_t)=dlsym(RTLD_NEXT,"ext2fs_open_file");
    return original(path,flags,mode);
}
int64_t ext2fs_llseek(int fd,int64_t offset,int whence) {
    if(fd==output) {
        if(whence==SEEK_SET && offset>=0)position=offset;
        else if(whence==SEEK_CUR && position+offset>=0)position+=offset;
        else {errno=EINVAL;return -1;}
        return position;
    }
    int64_t (*original)(int,int64_t,int)=dlsym(RTLD_NEXT,"ext2fs_llseek");
    return original(fd,offset,whence);
}
ssize_t write(int fd,const void *data,size_t size) {
    if(fd==output) {
        if(size>65536){errno=E2BIG;return -1;}
        if(frame((uint32_t)size,0)<0 || send_all(data,size)<0)return -1;
        position+=(int64_t)size;return (ssize_t)size;
    }
    if(!real_write) *(void **)(&real_write)=dlsym(RTLD_NEXT,"write");
    return real_write(fd,data,size);
}
int stream_truncate(int,int64_t) __asm__("ftruncate64");
int stream_truncate(int fd,int64_t length) {
    if(fd==output){position=length;return frame(0,1);}
    int (*original)(int,int64_t)=dlsym(RTLD_NEXT,"ftruncate64");
    return original(fd,length);
}
