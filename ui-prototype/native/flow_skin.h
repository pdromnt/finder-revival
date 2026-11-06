#ifndef FINDER_FLOW_SKIN_H
#define FINDER_FLOW_SKIN_H
int ffinder_flow_init(const char *path);
void ffinder_flow_show(void *widget, int visible);
void ffinder_flow_tick(void);
void ffinder_flow_event(void *receiver, unsigned short type);
int ffinder_flow_status_enter(void *widget);
void ffinder_flow_status_leave(int tracked);
int ffinder_flow_dispatch_safe(void);
int ffinder_flow_preview(void *root, const char *class_name);
void ffinder_flow_preview_close(void *root);
void ffinder_flow_preview_status(void *root, int show);
#endif
