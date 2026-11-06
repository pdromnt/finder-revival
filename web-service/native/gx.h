#ifndef REVIVAL_GX_H
#define REVIVAL_GX_H
#include <stdint.h>
#include <sys/types.h>
struct gx_meta {
    uint32_t seconds, filament;
    uint16_t layer, shells, speed, bed, nozzle;
};
int gx_convert(int input, off_t start, size_t length, int output, char *error, size_t error_size);
int gx_header(int fd, struct gx_meta *meta, uint32_t *bmp_start, uint32_t *gcode_start);
#endif
