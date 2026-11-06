/* Disable only the verified GUI's cloud/update trigger entry points in RAM.
 * Existing local TCP, file handling and motion/controller routines are untouched. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
struct patch { uintptr_t address; uint32_t expected[4]; };
static const struct patch patches[]={
    {0x12b750,{0xe92d40f0,0xe24dd01c,0xe3a01002,0xe1a07000}},
    {0x12cfa8,{0xe92d4070,0xe24dd028,0xe1a06000,0xe1a04001}}
};
__attribute__((constructor)) static void local_only(void) {
    char executable[192];ssize_t length=readlink("/proc/self/exe",executable,sizeof(executable)-1);
    if(length<0)return;
    executable[length]=0;
    if(strcmp(executable,"/PROGRAM/software/2.8.4/finder-plus-arm"))return;
    for(unsigned i=0;i<sizeof(patches)/sizeof(patches[0]);i++)
        if(memcmp((void *)patches[i].address,patches[i].expected,sizeof(patches[i].expected))) {
            fprintf(stderr,"REVIVAL_OFFLINE: target guard failed; refusing an unprotected GUI\n");_exit(126);
        }
    for(unsigned i=0;i<sizeof(patches)/sizeof(patches[0]);i++) {
        uintptr_t address=patches[i].address;
        void *page=(void *)(address&~(uintptr_t)4095);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE|PROT_EXEC))_exit(126);
        *(volatile uint32_t *)address=0xe12fff1e; /* bx lr: void method returns immediately */
        if(syscall(0x0f0002,address,address+4,0) || mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(126);
    }
    fprintf(stderr,"REVIVAL_OFFLINE: guarded cloud/update triggers disabled in RAM\n");
}
