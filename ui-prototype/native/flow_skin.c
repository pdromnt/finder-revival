/* Native, data-driven presentation adapter. The stock handlers, sensor values,
 * job state and controller command ordering remain owned by the stock ELF. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "flow_skin.h"

typedef struct { void *d; } String;
typedef struct { int x1,y1,x2,y2; } Rect;
typedef struct { int w,h; } Size;
typedef struct { uint64_t words[8]; } Variant;
typedef struct { int *d; } List;
struct OldTime { long sec,nsec; };
extern FILE *old_fopen(const char *,const char *) __asm__("fopen");
extern int old_scan(const char *,const char *,...) __asm__("sscanf");
extern int old_clock(int,struct OldTime *) __asm__("clock_gettime");
#define MAX_RULES 1024
struct Rule { char op[16],scope[64],name[80],value[512]; int n[5]; };
static struct Rule rules[MAX_RULES];
static int rule_count,enabled,applying,gui_tid,status_operations;
static int preview_status_test;
static void *preview_root;
static char preview_class[64];
static void *(*ascii)(const char *,int);
static void (*string_destroy)(String *)=(void *)0x386a8;
static void (*object_name)(String *,void *);
static void (*string_mid)(String *,const String *,int,int);
static void (*set_name)(void *,const String *);
static void (*all_widgets)(List *);
static void *(*window_of)(void *);
static void *(*active_window)(void);
static void (*qfree)(void *);
static void (*property)(Variant *,void *,const char *);
static void (*variant_destroy)(Variant *);
static void (*variant_string)(String *,Variant *);
static unsigned char (*variant_bool)(Variant *);
static void (*variant_rect)(Rect *,Variant *);
static void (*variant_bool_ctor)(Variant *,unsigned char);
static unsigned char (*set_property)(void *,const char *,const Variant *);
static void (*geometry)(void *,const Rect *);
static void (*minimum_size)(void *,int,int);
static void (*maximum_size)(void *,int,int);
static void (*real_style)(void *,const String *);
static void (*real_button_text)(void *,const String *);
static void (*real_label_text)(void *,const String *);
static void (*set_visible)(void *,unsigned char);
static void (*label_ctor)(void *,void *,const int *);
static void (*attribute)(void *,int,unsigned char);
static void (*icon_size)(void *,const Size *);
static void (*tool_style)(void *,int);
static void (*word_wrap)(void *,unsigned char);
static void (*text_format)(void *,int);
static void (*row_height)(void *,int,int);
static void (*column_width)(void *,int,int);
static int (*rows)(void *);
static const void *(*label_pixmap)(void *);
static void (*pixmap_scaled)(void *,const void *,const Size *,int,int);
static void (*pixmap_destroy)(void *);
static void (*set_pixmap)(void *,const void *);
static void (*button_icon)(void *,void *);
static void (*icon_pixmap)(void *,const void *,const Size *,int,int);
static void (*icon_destroy)(void *);

static const void *const *meta(void *w) {
    const void *(*fn)(void *)=(*(void ***)w)[0]; return fn(w);
}
static const char *class_of(void *w) {
    const void *const *m=meta(w);return (const char *)m[1]+((const unsigned int *)m[2])[1];
}
static int is_a(void *w,const char *name) {
    const void *const *m=meta(w);
    for(int i=0;m && i<20;i++,m=m[0])
        if(!strcmp((const char *)m[1]+((const unsigned int *)m[2])[1],name))return 1;
    return 0;
}
static void string_copy(const String *v,char *out,size_t length) {
    out[0]=0;if(!v->d || !length)return;
    int n=((int *)v->d)[2];const unsigned short *s=((const unsigned short **)v->d)[3];
    if(n<0 || n>32768 || !s)return;
    size_t j=0;
    for(int i=0;i<n && j+1<length;i++) {
        unsigned int c=s[i];
        if(c<128)out[j++]=(char)c;
        else if(c<2048 && j+2<length){out[j++]=(char)(0xc0|(c>>6));out[j++]=(char)(0x80|(c&63));}
        else if(j+3<length){out[j++]=(char)(0xe0|(c>>12));out[j++]=(char)(0x80|((c>>6)&63));out[j++]=(char)(0x80|(c&63));}
    }
    out[j]=0;
}
static void name_of(void *w,char *out,size_t length) {
    String s={0};object_name(&s,w);string_copy(&s,out,length);string_destroy(&s);
}
static void call_text(void (*fn)(void *,const String *),void *w,const char *text) {
    String s={ascii(text,-1)};fn(w,&s);string_destroy(&s);
}
static int get_bool(void *w,const char *name) {
    Variant v={0};property(&v,w,name);int b=variant_bool(&v);variant_destroy(&v);return b;
}
static void set_bool(void *w,const char *name,int b) {
    Variant v={0};variant_bool_ctor(&v,b!=0);set_property(w,name,&v);variant_destroy(&v);
}
static void get_string(void *w,const char *key,char *out,size_t length) {
    Variant v={0};String s={0};property(&v,w,key);variant_string(&s,&v);
    string_copy(&s,out,length);string_destroy(&s);variant_destroy(&v);
}
static int list_ok(List *l) {
    return l->d && l->d[1]>=0 && l->d[1]<10000 && l->d[2]>=0 && l->d[3]>=l->d[2] && l->d[3]<=l->d[1];
}
static void list_done(List *l) {if(l->d && l->d[0]>0 && --l->d[0]==0)qfree(l->d);}
static void *find(List *l,void *root,const char *name) {
    if(!strcmp(name,"@"))return root;
    if(!list_ok(l))return NULL;
    /* Qt 4.8.6 build's Data has an extra sharable field: nodes at +20. */
    for(int i=l->d[2];i<l->d[3];i++) {
        void *w=((void **)(l->d+5))[i];char n[80];
        if(window_of(w)!=root)continue;
        name_of(w,n,sizeof(n));if(!strcmp(n,name))return w;
    }
    return NULL;
}
static const char *scope_of(void *root) {return root==preview_root?preview_class:class_of(root);}
static struct Rule *rule(const char *op,const char *scope,const char *name) {
    for(int i=0;i<rule_count;i++)
        if(!strcmp(op,rules[i].op) && !strcmp(scope,rules[i].scope) && !strcmp(name,rules[i].name))return &rules[i];
    return NULL;
}
static struct Rule *body(const char *scope) {return rule("body",scope,"@");}
static int list_scope(const char *scope) {
    return !strcmp(scope,"ShowTFCard") || !strcmp(scope,"ShowUDisk") || !strcmp(scope,"ShowCloudPrint") || !strcmp(scope,"Setting") || !strcmp(scope,"Language") || !strcmp(scope,"WIFIPage");
}
static const char *role_css(const char *role) {
#define BUTTON_STATES "QToolButton:disabled,QPushButton:disabled{background:#1d2c39;color:#9bb0be;border-color:#3e5769;}"
#define BUTTON_BASE "color:#f5f8fa;border-radius:12px;padding:6px;font-family:\"Atkinson Hyperlegible\";font-size:20px;"
    if(!strcmp(role,"switch"))return "QToolButton{background:transparent;border:0;border-radius:0;padding:0;}QToolButton:pressed{background:transparent;}";
    if(!strcmp(role,"primary"))return "QToolButton,QPushButton{background:#007bb5;border:1px solid #007bb5;" BUTTON_BASE "color:#ffffff;}QToolButton:pressed,QPushButton:pressed{background:#006a9e;border-color:#7292a9;}" BUTTON_STATES;
    if(!strcmp(role,"danger"))return "QToolButton,QPushButton{background:#713444;border:1px solid #a15d70;" BUTTON_BASE "}QToolButton:pressed,QPushButton:pressed{background:#93475b;}" BUTTON_STATES;
    if(!strcmp(role,"secondary") || !strcmp(role,"back"))return "QToolButton,QPushButton{background:#1d2c39;border:1px solid #3e5769;" BUTTON_BASE "}QToolButton:pressed,QPushButton:pressed{background:#30485b;border-color:#7292a9;}" BUTTON_STATES;
    if(!strcmp(role,"source"))return "QToolButton{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;padding:6px 14px;font-family:\"Atkinson Hyperlegible\";font-size:20px;text-align:left;}QToolButton:pressed{background:#30485b;border-color:#7292a9;}" BUTTON_STATES;
    if(!strcmp(role,"title"))return "QLabel{background:transparent;color:#f5f8fa;font-family:\"Atkinson Hyperlegible\";font-size:24px;font-weight:700;}";
    if(!strcmp(role,"number"))return "QLabel{background:transparent;color:#f5f8fa;font-family:\"Atkinson Hyperlegible\";font-size:26px;font-weight:400;}";
    if(!strcmp(role,"muted"))return "QLabel{background:transparent;color:#b5c7d6;font-family:\"Atkinson Hyperlegible\";font-size:17px;font-weight:400;}";
    if(!strcmp(role,"detail"))return "QLabel{background:transparent;color:#f5f8fa;font-family:\"Atkinson Hyperlegible\";font-size:18px;font-weight:400;}";
    if(!strcmp(role,"key"))return "QPushButton{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:6px;padding:0;font-family:\"Atkinson Hyperlegible\";font-size:22px;font-weight:400;}QPushButton:pressed{background:#30485b;border-color:#7292a9;}QPushButton:disabled{color:#9bb0be;}";
    if(!strcmp(role,"field-panel"))return "QWidget#widgetName,QWidget#widgetPassword{background:#1d2c39;border:1px solid #3e5769;border-radius:8px;}";
    if(!strcmp(role,"numeric-button"))return "QToolButton{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;font-family:\"Atkinson Hyperlegible\";font-size:26px;font-weight:400;}QToolButton:pressed{background:#30485b;border-color:#7292a9;}";
    if(!strcmp(role,"prose"))return "QPlainTextEdit{background:#101820;color:#f5f8fa;border:0;font-family:\"Atkinson Hyperlegible\";font-size:17px;font-weight:400;}QScrollBar:vertical{background:#1d2c39;width:8px;border:0;}QScrollBar::handle:vertical{background:#7292a9;min-height:24px;border-radius:4px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;border:0;background:transparent;}";
    if(!strcmp(role,"scroll"))return "QScrollArea{background:#101820;border:0;}QScrollBar:vertical{background:#1d2c39;width:8px;border:0;}QScrollBar::handle:vertical{background:#7292a9;min-height:32px;border-radius:4px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;border:0;background:transparent;}";
    if(!strcmp(role,"panel"))return "QWidget#widget{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;}";
    if(!strcmp(role,"overlay"))return "QLabel{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;font-family:\"Atkinson Hyperlegible\";font-size:24px;}";
    if(!strcmp(role,"temperature"))return "QLabel{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;font-family:\"Atkinson Hyperlegible\";font-size:24px;padding:4px;}";
    if(!strcmp(role,"digit-selected"))return "QToolButton{background:#30485b;color:#f5f8fa;border:2px solid #73cfff;border-radius:12px;font-family:\"Atkinson Hyperlegible\";font-size:24px;}";
    if(!strcmp(role,"digit"))return "QToolButton{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:12px;font-family:\"Atkinson Hyperlegible\";font-size:24px;}";
    if(!strcmp(role,"progress"))return "QProgressBar{background:#1d2c39;color:#ffffff;border:1px solid #3e5769;border-radius:8px;text-align:center;font-family:\"Atkinson Hyperlegible\";font-size:18px;font-weight:400;}QProgressBar::chunk{background:#007bb5;border-radius:7px;}";
    if(!strcmp(role,"table"))return "QTableWidget{background:#101820;alternate-background-color:#1d2c39;color:#f5f8fa;border:0;gridline-color:#2a3e4c;selection-background-color:#007bb5;selection-color:#ffffff;font-family:\"Atkinson Hyperlegible\";font-size:18px;font-weight:400;}QTableWidget::item{padding:2px;border-bottom:1px solid #2a3e4c;}QTableWidget::item:selected{background:#007bb5;color:#ffffff;}QHeaderView{background:#101820;border:0;}QScrollBar{background:#1d2c39;border:0;}";
    return "QLabel{background:transparent;color:#f5f8fa;font-family:\"Atkinson Hyperlegible\";font-size:20px;font-weight:400;}";
#undef BUTTON_BASE
#undef BUTTON_STATES
}
static const char *widget_role(void *w,const char *scope,const char *name) {
    if(!strcmp(scope,"TempAdjustDialog") && (!strcmp(name,"hundred") || !strcmp(name,"ten") || !strcmp(name,"single")))
        return get_bool(w,"_revival_digit_selected")?"digit-selected":"digit";
    struct Rule *r=rule("role",scope,name);if(r)return !strcmp(r->value,"native")?NULL:r->value;
    if(!strcmp(scope,"Keyboard") && is_a(w,"QAbstractButton"))return "key";
    if(is_a(w,"QAbstractButton"))return !strcmp(name,"back") || !strcmp(name,"cancel")?"back":"secondary";
    if(is_a(w,"QProgressBar"))return "progress";
    if(is_a(w,"QTableWidget"))return "table";
    if(is_a(w,"QLabel"))return "body";
    return NULL;
}
static void rect(void *w,int x,int y,int width,int height) {Rect r={x,y,x+width-1,y+height-1};geometry(w,&r);}
static void style_widget(void *w,const char *scope,const char *name) {
    const char *role=widget_role(w,scope,name);if(role)call_text(real_style,w,role_css(role));
}
static void digit_state(void *w,const char *scope,const char *name,const char *style) {
    if(strcmp(scope,"TempAdjustDialog") || (strcmp(name,"hundred") && strcmp(name,"ten") && strcmp(name,"single")))return;
    if(strstr(style,"border:4px"))set_bool(w,"_revival_digit_selected",1);
    else if(strstr(style,"border:2px solid #999999"))set_bool(w,"_revival_digit_selected",0);
}
static void scale_label(void *w,int width,int height) {
    const void *old=label_pixmap(w);if(!old)return;
    uint64_t pm[8]={0};Size size={width,height};pixmap_scaled(pm,old,&size,1,0);set_pixmap(w,pm);pixmap_destroy(pm);
}
static void *make_label(List *l,void *root,const char *name) {
    void *w=find(l,root,name);if(w)return w;
    int flags=0;w=calloc(1,20);if(!w)return NULL;
    label_ctor(w,root,&flags);call_text(set_name,w,name);attribute(w,51,1);
    list_done(l);memset(l,0,sizeof(*l));all_widgets(l);return w;
}
static void source_text(void *w,const char *scope,const char *name,const String *input) {
    char current[256],text[384];string_copy(input,current,sizeof(current));
    if(!strcmp(scope,"Build") && !strcmp(name,"showTFCard")) {
        if(!strncmp(current,"Printer storage",15)){real_button_text(w,input);return;}
        char numbers[320];size_t n=0;
        for(size_t i=0;current[i] && n+4<sizeof(numbers);i++) {
            if(current[i]=='\n'){numbers[n++]=' ';numbers[n++]='/';numbers[n++]=' ';}
            else {if(i && current[i]>='A' && current[i]<='Z' && current[i-1]>='0' && current[i-1]<='9')numbers[n++]=' ';numbers[n++]=current[i];}
        }
        numbers[n]=0;snprintf(text,sizeof(text),"Printer storage\n%s",numbers);
        call_text(real_button_text,w,text);return;
    }
    if(!strcmp(scope,"Build") && !strcmp(name,"showUDisk")) {
        call_text(real_button_text,w,get_bool(w,"enabled")?"USB drive":"USB drive unavailable");return;
    }
    if(!strcmp(scope,"Build") && !strcmp(name,"showCloudPrint")) {
        call_text(real_button_text,w,"Cloud queue");return;
    }
    struct Rule *r=rule("text",scope,name);
    if(r)call_text(real_button_text,w,r->value);else real_button_text(w,input);
}
static void header(void *root,const char *scope) {
    if(!strcmp(scope,"MainWindow")) {
        void **ui=*(void ***)(root+0xac);
        int indices[]={11,9,8,6,3,7,5,4},left=480;
        for(int i=0;i<8;i++) {
            Variant v={0};Rect r;property(&v,ui[indices[i]],"geometry");variant_rect(&r,&v);variant_destroy(&v);
            int width=r.x2-r.x1+1,height=r.y2-r.y1+1,y=(48-height)/2;
            if(r.y1!=y)rect(ui[indices[i]],r.x1,y,width,height);
            if(get_bool(ui[indices[i]],"visible") && r.x1<left)left=r.x1;
        }
        List l={0};all_widgets(&l);void *subtitle=find(&l,root,"revival_subtitle");
        if(subtitle)set_visible(subtitle,left>=326); /* Status takes priority over branding. */
        list_done(&l);
        return;
    }
    struct Rule *b=body(scope);if(!b || b->n[1]!=48)return;
    List l={0};all_widgets(&l);
    if(!find(&l,root,"revival_flow_title")) {
        void *title=make_label(&l,root,"revival_flow_title"),*line=make_label(&l,root,"revival_flow_rule");
        if(title){rect(title,16,0,360,48);call_text(real_label_text,title,b->value);call_text(real_style,title,role_css("title"));set_visible(title,1);}
        if(line){rect(line,0,47,480,1);call_text(real_style,line,"QLabel{background:#2a3e4c;border:none;}");set_visible(line,1);}
    }
    void *main=*(void **)0x384974;void **ui=*(void ***)(main+0xac);
    /* Same indicator set as the stock header, packed only when visible.
     * Parent-owned originals keep receiving their firmware updates. */
    int indices[]={11,9,8,6,3,7,5,4},right=464;
    for(int i=7;i>=0;i--) {
        char name[64];snprintf(name,sizeof(name),"revival_flow_status_%d",indices[i]);
        int fresh=find(&l,root,name)==NULL;void *w=make_label(&l,root,name);if(!w)continue;
        int show=(root==preview_root && preview_status_test) || get_bool(ui[indices[i]],"visible");
        if(show) {
            uint64_t icon[8]={0},pm[8]={0};Size size={indices[i]==3?32:28,indices[i]==3?32:28};
            button_icon(icon,ui[indices[i]]);icon_pixmap(pm,icon,&size,get_bool(ui[indices[i]],"enabled")?0:1,get_bool(ui[indices[i]],"checked")?0:1);set_pixmap(w,pm);pixmap_destroy(pm);icon_destroy(icon);
            right-=size.w;rect(w,right,(48-size.h)/2,size.w,size.h);right-=4;
        }
        if(fresh)call_text(real_style,w,"QLabel{background:transparent;border:none;}");
        set_visible(w,show);
    }
    void *title=find(&l,root,"revival_flow_title");if(title)rect(title,16,0,right-20<120?120:right-20,48);
    list_done(&l);
}
static void filename(void *root,List *l,const char *scope) {
    if(strcmp(scope,"BuildFileManage") || root==preview_root)return;
    /* Pinned constructor stores the input QString at +0x1c. It is read only. */
    String *path=(String *)(root+0x1c);if(!path->d)return;
    int length=((int *)path->d)[2],start=0;
    const unsigned short *data=((const unsigned short **)path->d)[3];
    if(!data || length<1 || length>4096)return;
    for(int i=0;i<length;i++)if(data[i]=='/')start=i+1;
    void *label=find(l,root,"label");
    if(label && start<length){String base={0};string_mid(&base,path,start,-1);text_format(label,0);real_label_text(label,&base);string_destroy(&base);}
    char location[1024];string_copy(path,location,sizeof(location));
    void *source=find(l,root,"revival_source");
    if(source)call_text(real_label_text,source,!strncmp(location,"/data/",6)?"Printer storage":!strncmp(location,"/media/",7)?"USB drive":"Print file");
}
static void homing_state(void *root,List *l) {
    void *yes=find(l,root,"yes"),*back=find(l,root,"back"),*label=find(l,root,"label");
    if(!yes || !back || !label)return;
    /* Native Home disables yes before movement and enables it only in
     * checkReturnHomeDone(). Neither handler is replaced or invoked here. */
    int done=get_bool(yes,"enabled");
    const char *message=done?"Homing complete":"Homing. Please wait.";
    char current[256];get_string(label,"text",current,sizeof(current));
    if(strcmp(current,message))call_text(real_label_text,label,message);
    if(get_bool(yes,"visible")!=done)set_visible(yes,done);
    if(get_bool(back,"visible")==done)set_visible(back,!done);
}
static void settings_columns(void *root) {
    if(strcmp(class_of(root),"Setting"))return;
    List *page=(List *)((char *)root+0x1c);
    if(!list_ok(page) || page->d[3]-page->d[2]>4)return;
    int value_width=70;
    for(int i=page->d[2];i<page->d[3];i++) {
        int *entry=((int **)(page->d+5))[i];
        if(!entry)return;
        if(entry[1]==2)value_width=152; /* Preserve room for translated text values. */
    }
    void **ui=*(void ***)((char *)root+0x14);
    if(!ui || !ui[4])return;
    column_width(ui[4],1,370-value_width);column_width(ui[4],2,value_width);
}
static int apply(void *root,const char *scope) {
    struct Rule *b=body(scope);if(!b)return 0;
    applying++;
    List l={0};all_widgets(&l);
    if(!list_ok(&l)){list_done(&l);applying--;return 0;}
    if(find(&l,root,"revival_flow_marker")){list_done(&l);header(root,scope);applying--;return 1;}
    int offset=b->n[1]==48?48:0;
    /* Some generated screens pin their old 480x290 size. Release those
     * constraints before adding the taller header, or QWS clips the footer. */
    minimum_size(root,0,0);maximum_size(root,b->n[2],offset?320:b->n[3]);
    rect(root,b->n[0],offset?0:b->n[1],b->n[2],offset?320:b->n[3]);
    call_text(real_style,root,"QWidget{background:#101820;color:#f5f8fa;font-family:\"Atkinson Hyperlegible\";font-size:20px;font-weight:400;}QLabel{background:transparent;}QLineEdit{background:#1d2c39;color:#f5f8fa;border:1px solid #3e5769;border-radius:8px;padding:6px;}QPlainTextEdit{background:#101820;color:#f5f8fa;border:0;}");
    for(int i=l.d[2];i<l.d[3];i++) {
        void *w=((void **)(l.d+5))[i];if(w==root || window_of(w)!=root)continue;
        char name[80],old[2048];name_of(w,name,sizeof(name));get_string(w,"styleSheet",old,sizeof(old));digit_state(w,scope,name,old);style_widget(w,scope,name);
        if(!*name && is_a(w,"QLabel") && list_scope(scope))scale_label(w,48,36);
    }
    for(int i=0;i<rule_count;i++) {
        struct Rule *r=&rules[i];if(strcmp(r->scope,scope) || !strcmp(r->op,"body"))continue;
        void *w=find(&l,root,r->name);
        if(!strcmp(r->op,"label") && !w) {
            int flags=0;w=calloc(1,20);if(!w)continue;
            label_ctor(w,root,&flags);call_text(set_name,w,r->name);attribute(w,51,1);set_visible(w,1);
            list_done(&l);memset(&l,0,sizeof(l));all_widgets(&l);
            continue;
        }
        if(!w)continue;
        if(!strcmp(r->op,"geom") || !strcmp(r->op,"local"))rect(w,r->n[0],r->n[1]+(!strcmp(r->op,"local")?0:offset),r->n[2],r->n[3]);
        else if(!strcmp(r->op,"bounds")){minimum_size(w,r->n[0],r->n[1]);maximum_size(w,r->n[2],r->n[3]);}
        else if(!strcmp(r->op,"role")){if(strcmp(r->value,"native"))call_text(real_style,w,role_css(r->value));}
        else if(!strcmp(r->op,"text"))call_text(is_a(w,"QAbstractButton")?real_button_text:real_label_text,w,r->value);
        else if(!strcmp(r->op,"hide"))set_visible(w,0);
        else if(!strcmp(r->op,"tool"))tool_style(w,r->n[0]);
        else if(!strcmp(r->op,"icon")){Size size={r->n[0],r->n[1]};icon_size(w,&size);}
        else if(!strcmp(r->op,"wrap"))word_wrap(w,r->n[0]!=0);
        else if(!strcmp(r->op,"table")) {
            for(int c=0;c<4;c++)column_width(w,c,r->n[c]);
            int count=rows(w);if(count>=0 && count<10000)for(int row=0;row<count;row++)row_height(w,row,r->n[4]);
            void (*view_icons)(void *,const Size *)=dlsym(RTLD_DEFAULT,"_ZN17QAbstractItemView11setIconSizeERK5QSize");
            if(view_icons){Size size=!strcmp(scope,"Setting")?(Size){76,38}:(Size){48,36};view_icons(w,&size);}
        } else if(!strcmp(r->op,"pixmap")) {
            scale_label(w,r->n[0],r->n[1]);
        }
    }
    if(!strcmp(scope,"Build")) {
        const char *names[]={"showTFCard","showUDisk","showCloudPrint"};
        for(int i=0;i<3;i++){void *w=find(&l,root,names[i]);if(w){String s={0};Variant v={0};property(&v,w,"text");variant_string(&s,&v);source_text(w,scope,names[i],&s);string_destroy(&s);variant_destroy(&v);}}
    }
    filename(root,&l,scope);
    if(!strcmp(scope,"Home"))homing_state(root,&l);
    if(!strcmp(scope,"Setting"))settings_columns(root);
    void *marker=make_label(&l,root,"revival_flow_marker");if(marker){rect(marker,0,0,1,1);set_visible(marker,0);}
    list_done(&l);header(root,scope);applying--;
    fprintf(stderr,"REVIVAL_FLOW applied %s\n",scope);return 1;
}
static int load_api(void) {
#define LOAD(v,s) do{*(void **)(&v)=dlsym(RTLD_DEFAULT,s);if(!v){fprintf(stderr,"REVIVAL_FLOW missing %s\n",s);return 0;}}while(0)
    LOAD(ascii,"_ZN7QString16fromAscii_helperEPKci");
    LOAD(object_name,"_ZNK7QObject10objectNameEv");LOAD(set_name,"_ZN7QObject13setObjectNameERK7QString");
    LOAD(string_mid,"_ZNK7QString3midEii");
    LOAD(all_widgets,"_ZN12QApplication10allWidgetsEv");LOAD(window_of,"_ZNK7QWidget6windowEv");
    LOAD(active_window,"_ZN12QApplication12activeWindowEv");LOAD(qfree,"_Z5qFreePv");
    LOAD(property,"_ZNK7QObject8propertyEPKc");LOAD(variant_destroy,"_ZN8QVariantD1Ev");
    LOAD(variant_string,"_ZNK8QVariant8toStringEv");LOAD(variant_bool,"_ZNK8QVariant6toBoolEv");
    LOAD(variant_rect,"_ZNK8QVariant6toRectEv");
    LOAD(variant_bool_ctor,"_ZN8QVariantC1Eb");LOAD(set_property,"_ZN7QObject11setPropertyEPKcRK8QVariant");
    *(void **)(&geometry)=dlsym(RTLD_NEXT,"_ZN7QWidget11setGeometryERK5QRect");
    if(!geometry)return 0;
    LOAD(minimum_size,"_ZN7QWidget14setMinimumSizeEii");LOAD(maximum_size,"_ZN7QWidget14setMaximumSizeEii");
    *(void **)(&real_style)=dlsym(RTLD_NEXT,"_ZN7QWidget13setStyleSheetERK7QString");
    *(void **)(&real_button_text)=dlsym(RTLD_NEXT,"_ZN15QAbstractButton7setTextERK7QString");
    *(void **)(&real_label_text)=dlsym(RTLD_NEXT,"_ZN6QLabel7setTextERK7QString");
    if(!real_style || !real_button_text || !real_label_text)return 0;
    LOAD(set_visible,"_ZN7QWidget10setVisibleEb");LOAD(label_ctor,"_ZN6QLabelC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE");
    LOAD(attribute,"_ZN7QWidget12setAttributeEN2Qt15WidgetAttributeEb");
    LOAD(icon_size,"_ZN15QAbstractButton11setIconSizeERK5QSize");LOAD(tool_style,"_ZN11QToolButton18setToolButtonStyleEN2Qt15ToolButtonStyleE");
    LOAD(word_wrap,"_ZN6QLabel11setWordWrapEb");
    *(void **)(&row_height)=dlsym(RTLD_NEXT,"_ZN10QTableView12setRowHeightEii");
    LOAD(text_format,"_ZN6QLabel13setTextFormatEN2Qt10TextFormatE");
    *(void **)(&column_width)=dlsym(RTLD_NEXT,"_ZN10QTableView14setColumnWidthEii");
    if(!row_height || !column_width)return 0;
    LOAD(rows,"_ZNK12QTableWidget8rowCountEv");
    LOAD(label_pixmap,"_ZNK6QLabel6pixmapEv");
    LOAD(pixmap_scaled,"_ZNK7QPixmap6scaledERK5QSizeN2Qt15AspectRatioModeENS3_18TransformationModeE");
    LOAD(pixmap_destroy,"_ZN7QPixmapD1Ev");LOAD(set_pixmap,"_ZN6QLabel9setPixmapERK7QPixmap");
    LOAD(button_icon,"_ZNK15QAbstractButton4iconEv");LOAD(icon_pixmap,"_ZNK5QIcon6pixmapERK5QSizeNS_4ModeENS_5StateE");LOAD(icon_destroy,"_ZN5QIconD1Ev");
#undef LOAD
    return 1;
}
int ffinder_flow_init(const char *path) {
    if(!path || !load_api())return 0;
    FILE *f=old_fopen(path,"r");if(!f)return 0;
    char line[2500];int ok=1;
    while(fgets(line,sizeof(line),f)) {
        line[strcspn(line,"\r\n")]=0;if(!*line || *line=='#')continue;
        if(rule_count==MAX_RULES){ok=0;break;}
        struct Rule *r=&rules[rule_count];char *state=NULL;
        char *op=strtok_r(line,"\t",&state),*target=strtok_r(NULL,"\t",&state);
        if(!op || !target || strlen(op)>=sizeof(r->op)){ok=0;break;}
        char *dot=strchr(target,'.');if(!dot || (size_t)(dot-target)>=sizeof(r->scope) || strlen(dot+1)>=sizeof(r->name)){ok=0;break;}
        *dot=0;snprintf(r->op,sizeof(r->op),"%s",op);snprintf(r->scope,sizeof(r->scope),"%s",target);snprintf(r->name,sizeof(r->name),"%s",dot+1);
        if(state){if(strlen(state)>=sizeof(r->value)){ok=0;break;}snprintf(r->value,sizeof(r->value),"%s",state);}
        int required=0;
        if(!strcmp(op,"body")) {
            char *label=strtok_r(NULL,"\t",&state);if(!label){ok=0;break;}
            snprintf(r->value,sizeof(r->value),"%s",label);required=4;
        } else if(!strcmp(op,"geom") || !strcmp(op,"local") || !strcmp(op,"bounds"))required=4;
        else if(!strcmp(op,"table"))required=5;
        else if(!strcmp(op,"icon") || !strcmp(op,"pixmap"))required=2;
        else if(!strcmp(op,"tool") || !strcmp(op,"wrap"))required=1;
        else if(strcmp(op,"role") && strcmp(op,"text") && strcmp(op,"hide") && strcmp(op,"label")){ok=0;break;}
        if(required && (!state || old_scan(state,"%d\t%d\t%d\t%d\t%d",&r->n[0],&r->n[1],&r->n[2],&r->n[3],&r->n[4])!=required)){ok=0;break;}
        if(!strcmp(op,"geom") || !strcmp(op,"body") || !strcmp(op,"local"))if(r->n[0]<0 || r->n[1]<0 || r->n[2]<1 || r->n[3]<1 || r->n[0]+r->n[2]>480 || (strcmp(op,"local") && r->n[1]+r->n[3]>320)){ok=0;break;}
        if(!strcmp(op,"bounds") && (r->n[0]<0 || r->n[1]<0 || r->n[2]<r->n[0] || r->n[3]<r->n[1] || r->n[2]>480 || r->n[3]>1600)){ok=0;break;}
        char *src=r->value,*dest=r->value;
        while(*src){if(src[0]=='\\' && src[1]=='n'){*dest++='\n';src+=2;}else *dest++=*src++;}*dest=0;
        rule_count++;
    }
    fclose(f);if(!ok){rule_count=0;fprintf(stderr,"REVIVAL_FLOW config rejected\n");return 0;}
    gui_tid=(int)syscall(224);enabled=1;fprintf(stderr,"REVIVAL_FLOW enabled %d rules\n",rule_count);return 1;
}
void ffinder_flow_show(void *w,int visible) {
    if(!enabled || applying || syscall(224)!=gui_tid)return;
    const char *scope=scope_of(w);
    if(visible && body(scope))apply(w,scope);
}
void ffinder_flow_event(void *receiver,unsigned short type) {if(type==17)ffinder_flow_show(receiver,1);}
int ffinder_flow_status_enter(void *widget) {
    if(!enabled || applying || syscall(224)!=gui_tid)return 0;
    void *main=*(void **)0x384974;void **ui=*(void ***)(main+0xac);
    int indices[]={11,9,8,6,3,7,5,4};
    for(int i=0;i<8;i++)if(widget==ui[indices[i]]){status_operations++;return 1;}
    return 0;
}
void ffinder_flow_status_leave(int tracked) {if(tracked)status_operations--;}
int ffinder_flow_dispatch_safe(void) {return enabled && !applying && !status_operations && syscall(224)==gui_tid;}
void ffinder_flow_tick(void) {
    static long last=-1;if(!ffinder_flow_dispatch_safe())return;
    struct OldTime now;old_clock(1,&now);if(last==now.sec)return;last=now.sec;
    /* QWS can report no active window on home. The original indicators also
     * get stock geometry updates behind child pages, so normalize their owner
     * explicitly rather than waiting for MainWindow to become active. */
    void *main=*(void **)0x384974,*w=preview_root?preview_root:active_window();
    applying++;header(main,"MainWindow");
    if(w && w!=main) {
        const char *scope=scope_of(w);header(w,scope);
        if(!strcmp(scope,"Home")) {
            List l={0};all_widgets(&l);
            if(list_ok(&l))homing_state(w,&l);
            list_done(&l);
        }
    }
    applying--;
}
int ffinder_flow_preview(void *root,const char *name) {
    if(!enabled || !body(name))return 0;
    preview_root=root;snprintf(preview_class,sizeof(preview_class),"%s",name);return apply(root,name);
}
void ffinder_flow_preview_close(void *root) {if(root==preview_root){preview_root=NULL;preview_class[0]=0;preview_status_test=0;}}
void ffinder_flow_preview_status(void *root,int show) {
    if(root!=preview_root)return;
    preview_status_test=show!=0;applying++;header(root,preview_class);applying--;
}

void flow_geometry(void *,const Rect *) __asm__("_ZN7QWidget11setGeometryERK5QRect");
void flow_geometry(void *w,const Rect *input) {
    if(!geometry)geometry=dlsym(RTLD_NEXT,"_ZN7QWidget11setGeometryERK5QRect");
    if(!geometry)return;
    if(enabled) {
        void *main=*(void **)0x384974;void **ui=*(void ***)(main+0xac);
        int indices[]={11,9,8,6,3,7,5,4};
        for(int i=0;i<8;i++)if(w==ui[indices[i]]) {
            Rect r=*input;int height=r.y2-r.y1+1;
            r.y1=(48-height)/2;r.y2=r.y1+height-1;
            int tracked=ffinder_flow_status_enter(w);geometry(w,&r);ffinder_flow_status_leave(tracked);return;
        }
    }
    geometry(w,input);
}

static void settings_without_cloud(void *root) {
    /* Pinned Setting::getShowList(0), after initData and before showList.
     * Only the real Setting object owns these native QList fields. */
    if(strcmp(class_of(root),"Setting") || *(int *)((char *)root+0x48)!=0)return;
    List *full=(List *)((char *)root+0x18),*page=(List *)((char *)root+0x1c);
    if(!list_ok(full) || !list_ok(page))return;
    int count=full->d[3]-full->d[2],clouds=0;unsigned int seen=0;
    if(count<3 || count>20)return;
    for(int i=full->d[2];i<full->d[3];i++) {
        int *entry=((int **)(full->d+5))[i];
        if(!entry || entry[0]<0 || entry[0]>17 || (seen&(1u<<entry[0])))return;
        seen|=1u<<entry[0];if(entry[0]==3 || entry[0]==4 || entry[0]==17)clouds++;
    }
    if(clouds!=3 || !(seen&1))return;
    void (*clear)(List *)=(void *)0x15b028;
    void (*append)(List *,const void *)=(void *)0x15b080;
    void (*destroy)(List *)=(void *)0x15afe0;
    /* Use native copy/destruction so retained QString/QIcon values and the
     * old current-page copies keep their normal ownership. */
    List filtered=*full;
    filtered.d[0]++;clear(&filtered);
    for(int i=full->d[2];i<full->d[3];i++) {
        int *entry=((int **)(full->d+5))[i];
        if(entry[0]!=3 && entry[0]!=4 && entry[0]!=17)append(&filtered,entry);
    }
    List old=*full;*full=filtered;destroy(&old);
    clear(page);
    for(int i=full->d[2];i<full->d[3] && i<full->d[2]+4;i++)
        append(page,((void **)(full->d+5))[i]);
    fprintf(stderr,"REVIVAL_FLOW Settings: hid cloud/update action IDs 3/4/17; %d items remain; saved flags untouched\n",count-3);
}
void flow_palette(void *,const void *) __asm__("_ZN7QWidget10setPaletteERK8QPalette");
void flow_palette(void *w,const void *palette) {
    static void (*original)(void *,const void *);
    if(!original)original=dlsym(RTLD_NEXT,"_ZN7QWidget10setPaletteERK8QPalette");
    /* Pinned Setting::getShowList reassigns the old bg2 pixmap after every
     * page change, replacing Qt's stylesheet-derived Window brush. Leave
     * that one background restore out; all other palette updates pass through. */
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    if(enabled && caller>=0x1566d4 && caller<0x156b2c && !strcmp(scope_of(w),"Setting")) {
        settings_without_cloud(w);
        settings_columns(w);
        fprintf(stderr,"REVIVAL_FLOW preserved Settings background\n");return;
    }
    if(original)original(w,palette);
}

static struct Rule *table_dimensions(void *table) {
    if(!enabled || applying || syscall(224)!=gui_tid)return NULL;
    const char *scope=scope_of(window_of(table));char name[80];name_of(table,name,sizeof(name));
    return rule("table",scope,name);
}
void flow_row_height(void *,int,int) __asm__("_ZN10QTableView12setRowHeightEii");
void flow_row_height(void *table,int row,int height) {
    if(!row_height)row_height=dlsym(RTLD_NEXT,"_ZN10QTableView12setRowHeightEii");
    struct Rule *r=table_dimensions(table);if(r)height=r->n[4];
    if(row_height)row_height(table,row,height);
}
void flow_column_width(void *,int,int) __asm__("_ZN10QTableView14setColumnWidthEii");
void flow_column_width(void *table,int column,int width) {
    if(!column_width)column_width=dlsym(RTLD_NEXT,"_ZN10QTableView14setColumnWidthEii");
    struct Rule *r=table_dimensions(table);if(r && column>=0 && column<4)width=r->n[column];
    if(column_width)column_width(table,column,width);
}

void flow_style(void *,const String *) __asm__("_ZN7QWidget13setStyleSheetERK7QString");
void flow_style(void *w,const String *s) {
    if(!real_style)real_style=dlsym(RTLD_NEXT,"_ZN7QWidget13setStyleSheetERK7QString");
    if(!real_style)return;
    if(enabled && !applying && syscall(224)==gui_tid) {
        void *root=window_of(w);const char *scope=scope_of(root);
        if(body(scope) && root!=w) {char n[80],old[2048];name_of(w,n,sizeof(n));string_copy(s,old,sizeof(old));digit_state(w,scope,n,old);const char *role=widget_role(w,scope,n);if(role){applying++;call_text(real_style,w,role_css(role));applying--;return;}}
    }
    real_style(w,s);
}

void flow_cell(void *,int,int,void *) __asm__("_ZN12QTableWidget13setCellWidgetEiiP7QWidget");
void flow_cell(void *table,int row,int column,void *widget) {
    static void (*original)(void *,int,int,void *);
    if(!original)original=dlsym(RTLD_NEXT,"_ZN12QTableWidget13setCellWidgetEiiP7QWidget");
    if(!original)return;
    original(table,row,column,widget);
    struct Rule *dimensions=table_dimensions(table);if(dimensions)row_height(table,row,dimensions->n[4]);
    if(enabled && !applying && widget && syscall(224)==gui_tid) {
        const char *scope=scope_of(window_of(table));
        if(list_scope(scope) && is_a(widget,"QLabel")) {
            applying++;scale_label(widget,48,36);call_text(real_style,widget,role_css("body"));applying--;
        }
    }
}
void flow_item(void *,int,int,void *) __asm__("_ZN12QTableWidget7setItemEiiP16QTableWidgetItem");
void flow_item(void *table,int row,int column,void *item) {
    static void (*original)(void *,int,int,void *);
    if(!original)original=dlsym(RTLD_NEXT,"_ZN12QTableWidget7setItemEiiP16QTableWidgetItem");
    if(!original)return;
    if(item && column==2 && enabled && !applying && syscall(224)==gui_tid &&
       !strcmp(scope_of(window_of(table)),"Setting")) {
        void (*integer)(Variant *,int)=dlsym(RTLD_DEFAULT,"_ZN8QVariantC1Ei");
        void (*set_data)(void *,int,const Variant *)=dlsym(RTLD_DEFAULT,"_ZN16QTableWidgetItem7setDataEiRK8QVariant");
        if(integer && set_data) {
            Variant alignment={0};integer(&alignment,0x82); /* AlignRight | AlignVCenter */
            set_data(item,7,&alignment);variant_destroy(&alignment);
        }
    }
    original(table,row,column,item);
    struct Rule *dimensions=table_dimensions(table);if(dimensions)row_height(table,row,dimensions->n[4]);
}
void flow_button_text(void *,const String *) __asm__("_ZN15QAbstractButton7setTextERK7QString");
void flow_button_text(void *w,const String *s) {
    if(!real_button_text)real_button_text=dlsym(RTLD_NEXT,"_ZN15QAbstractButton7setTextERK7QString");
    if(!real_button_text)return;
    if(enabled && !applying && syscall(224)==gui_tid) {
        const char *scope=scope_of(window_of(w));if(body(scope)){char n[80];name_of(w,n,sizeof(n));applying++;source_text(w,scope,n,s);applying--;return;}
    }
    real_button_text(w,s);
}
