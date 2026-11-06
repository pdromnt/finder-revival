/* Temporary stock-screen inspection. No application constructors are invoked
 * for layout previews: only Qt widgets and the verified generated setupUi. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct qt_string { void *d; };
struct qt_list { int *d; };
struct qt_rect { int x1,y1,x2,y2; };
struct variant { uint64_t words[8]; };
struct old_time { long sec,nsec; };
extern FILE *old_fopen(const char *,const char *) __asm__("fopen");
extern int old_scan(const char *,const char *,...) __asm__("sscanf");
extern int old_clock(int,struct old_time *) __asm__("clock_gettime");
static void (*widgets)(struct qt_list *);
static void (*property)(struct variant *,void *,const char *);
static void (*variant_destroy)(struct variant *);
static void (*to_string)(struct qt_string *,struct variant *);
static void (*to_rect)(struct qt_rect *,struct variant *);
static unsigned char (*to_bool)(struct variant *);
static void (*qfree)(void *);
static void *(*window_of)(void *);
static void (*widget_ctor)(void *,void *,const int *);
static void (*dialog_ctor)(void *,void *,const int *);
static void (*main_ctor)(void *,void *,const int *);
static void (*visible)(void *,unsigned char);
static void (*raise_widget)(void *);
static void (*delete_later)(void *);
static const void *(*palette)(void *);
static void (*set_palette)(void *,const void *);
static void (*single_shot)(int,void *,const char *);
static void *(*ascii)(const char *,int);
static void (*set_name)(void *,const struct qt_string *);
static void (*set_style)(void *,const struct qt_string *);
static void *(*table_item)(void *,int,int);
static void (*string_destroy)(struct qt_string *)=(void *)0x386a8;
static int enabled,busy,gui_tid;
static void *preview;
static struct { const char *name; void *object; } known_windows[64];
static int known_count;

struct layout { const char *name; uintptr_t setup; int kind; };
static struct layout layouts[] = {
    {"About",0xc484c,0},{"Build",0xcf680,0},{"BuildFileManage",0xdac30,0},
    {"BuildPrint",0xe58a4,0},{"BuildSettingDialog",0x558f4,1},
    {"CloseButtonDialog",0x581d8,1},{"DoorOpenDialog",0x598a4,1},
    {"ExtruderCalibration",0xf4444,0},{"Filament",0xf9784,0},
    {"Home",0xfb710,0},{"Keyboard",0x102b68,2},{"Level",0x116694,0},
    {"Licenses",0x118b94,0},{"MainWindow",0x138e9c,2},
    {"Manual",0x142ab0,0},{"NoButtonDialog",0x5ac70,1},
    {"OneButtonDialog",0x5c40c,1},{"Preheat",0x14919c,0},
    {"Scanner",0x14f25c,0},{"ServoCalibration",0x151d8c,0},
    {"ShowList",0x10b74c,0},{"Status",0x174744,0},
    {"TempAdjustDialog",0x61958,1},{"Tools",0x178dac,0},
    {"TwoButtonDialog",0x64670,1},{"WIFIPage",0x182058,0},
    {"WLanHotspot",0xec2ac,0},{"ZOffset",0x1b6658,0}
};
/* Only menu navigation/read-only pages. No heater, motor, print, delete,
 * update, network-toggle, credential-save, or calibration actions. */
struct action { const char *class_name,*slot; };
static struct action actions[] = {
    {"MainWindow","on_showBuild_pressed"},{"MainWindow","on_showTools_pressed"},
    {"MainWindow","on_showPreheat_pressed"},
    {"Tools","on_showAbout_pressed"},{"Tools","on_showStatus_pressed"},
    {"Tools","on_showSetting_pressed"},{"Tools","on_showFilament_pressed"},
    {"About","on_licenses_pressed"},
    {"Build","on_memory_pressed"},{"Build","on_showTFCard_pressed"},
    {"Setting","on_down_pressed"},{"Setting","on_up_pressed"},
    {"Language","on_down_pressed"},{"Language","on_up_pressed"},
    {"Licenses","on_down_pressed"},{"Licenses","on_up_pressed"},
    {"About","on_back_pressed"},{"Tools","on_back_pressed"},
    {"Build","on_back_pressed"},{"Preheat","on_back_pressed"},
    {"Setting","on_back_pressed"},{"Status","on_back_pressed"},
    {"Filament","on_back_pressed"},{"Language","on_back_pressed"},
    {"Licenses","on_back_pressed"},{"ShowTFCard","on_back_pressed"}
    ,{"WIFIPage","on_back_pressed"},{"WLanHotspot","on_back_pressed"},
    {"CloudLogin","on_back_pressed"},{"BuildFileManage","on_back_pressed"}
};

static const char *class_of(void *object) {
    const void *(*meta_fn)(const void *)=(*(void ***)object)[0];
    const void *const *meta=meta_fn(object);
    return (const char *)meta[1]+((const unsigned int *)meta[2])[1];
}
static void list_done(struct qt_list *list) {
    if (list->d && list->d[0]>0 && --list->d[0]==0) qfree(list->d);
}
static int list_range(struct qt_list *list) {
    return list->d && list->d[1]>=0 && list->d[1]<10000 &&
           list->d[2]>=0 && list->d[2]<=list->d[3] && list->d[3]<=list->d[1];
}
static void *find_window(const char *name) {
    /* Stock pages delete themselves on Back. Cached addresses can already be
     * freed/reused when the next diagnostic command arrives. Resolve from
     * QApplication's current live-widget list before invoking any slot. */
    struct qt_list list={0};void *result=NULL;widgets(&list);
    if(list_range(&list))for(int i=list.d[2];i<list.d[3];i++) {
        void *w=((void **)(list.d+5))[i];
        if(w==window_of(w) && !strcmp(class_of(w),name)){result=w;break;}
    }
    list_done(&list);return result;
}
static void json_string(FILE *f,const char *text) {
    fputc('"',f);
    for (const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if (*p=='"' || *p=='\\') {fputc('\\',f);fputc(*p,f);}
        else if (*p<32) fprintf(f,"\\u%04x",*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static void json_qstring(FILE *f,struct qt_string *value) {
    fputc('"',f);
    if (value->d) {
        int size=((int *)value->d)[2];
        const unsigned short *p=((const unsigned short **)value->d)[3];
        if (size>=0 && size<32768 && p) for (int i=0;i<size;i++) {
            unsigned short c=p[i];
            if (c=='"' || c=='\\') {fputc('\\',f);fputc(c,f);}
            else if (c<32 || c>=127) fprintf(f,"\\u%04x",c);
            else fputc(c,f);
        }
        string_destroy(value);
    }
    fputc('"',f);
}
static int bool_property(void *w,const char *key) {
    struct variant v={0};property(&v,w,key);int result=to_bool(&v);variant_destroy(&v);return result;
}
static void text_property(FILE *f,void *w,const char *key) {
    struct variant v={0};struct qt_string text={0};
    property(&v,w,key);to_string(&text,&v);json_qstring(f,&text);variant_destroy(&v);
}
static int dump(void) {
    FILE *f=old_fopen("/run/revival-ui/audit-widgets.json.new","w");if (!f) return 0;
    setbuf(f,NULL);
    fprintf(stderr,"SCREEN_AUDIT dump: listing widgets\n");
    struct qt_list list={0};widgets(&list);
    fprintf(stderr,"SCREEN_AUDIT dump: list=%p range=%d..%d alloc=%d\n",list.d,list.d?list.d[2]:0,list.d?list.d[3]:0,list.d?list.d[1]:0);
    fprintf(f,"[");int count=0;
    if (list_range(&list)) for (int i=list.d[2];i<list.d[3];i++) {
        /* This Qt 4 build has the sharable bitfield after end: nodes at +20.
         * Confirmed in the stock list access instructions (e.g. 0x1570d8).
         * Qt 5's commonly documented +16 layout does not apply. */
        void *w=((void **)(list.d+5))[i];
        const char *c=class_of(w);
        fprintf(stderr,"SCREEN_AUDIT dump: widget=%p class=%s\n",w,c);
        struct variant v={0};struct qt_rect rect;
        property(&v,w,"geometry");to_rect(&rect,&v);variant_destroy(&v);
        if (count++)fputc(',',f);
        fprintf(f,"\n{\"pointer\":\"%p\",\"class\":",w);json_string(f,c);
        fprintf(f,",\"window\":");json_string(f,class_of(window_of(w)));
        fprintf(f,",\"window_pointer\":\"%p\"",window_of(w));
        fprintf(f,",\"name\":");text_property(f,w,"objectName");
        fprintf(f,",\"geometry\":[%d,%d,%d,%d],\"visible\":%d,\"enabled\":%d,\"text\":",
            rect.x1,rect.y1,rect.x2-rect.x1+1,rect.y2-rect.y1+1,
            bool_property(w,"visible"),bool_property(w,"enabled"));
        if (!strcmp(c,"QLineEdit"))json_string(f,"<input omitted>");else text_property(f,w,"text");
        fprintf(f,",\"style\":");text_property(f,w,"styleSheet");
        if(!strcmp(c,"QTableWidget") || !strcmp(c,"QLongClickedTableWidget")) {
            int (*count)(void *)=dlsym(RTLD_DEFAULT,"_ZNK12QTableWidget8rowCountEv");
            int (*height)(void *,int)=dlsym(RTLD_DEFAULT,"_ZNK10QTableView9rowHeightEi");
            int (*width)(void *,int)=dlsym(RTLD_DEFAULT,"_ZNK10QTableView11columnWidthEi");
            if(count && height && width) {
                int n=count(w);fprintf(f,",\"table\":{\"rows\":%d,\"row_heights\":[",n);
                for(int row=0;row<n && row<8;row++)fprintf(f,"%s%d",row?",":"",height(w,row));
                fprintf(f,"],\"column_widths\":[");for(int column=0;column<4;column++)fprintf(f,"%s%d",column?",":"",width(w,column));
                fprintf(f,"],\"cells\":[");
                void (*data)(struct variant *,void *,int)=dlsym(RTLD_DEFAULT,"_ZNK16QTableWidgetItem4dataEi");
                if(data)for(int row=0;row<n && row<8;row++) {
                    fprintf(f,"%s[",row?",":"");
                    for(int col=0;col<4;col++) {
                        if(col)fputc(',',f);
                        void *item=table_item(w,row,col);
                        if(item) {
                            struct variant v={0};struct qt_string s={0};
                            data(&v,item,0);to_string(&s,&v);json_qstring(f,&s);variant_destroy(&v);
                        } else json_string(f,"");
                    }
                    fputc(']',f);
                }
                fprintf(f,"]}");
            }
        }
        fprintf(f,"}");
    }
    list_done(&list);fprintf(f,"\n]\n");fclose(f);
    return rename("/run/revival-ui/audit-widgets.json.new","/run/revival-ui/audit-widgets.json")==0;
}
static void close_preview(void) {
    if (preview) {
        void (*close_skin)(void *)=dlsym(RTLD_DEFAULT,"ffinder_flow_preview_close");
        if(close_skin)close_skin(preview);
        visible(preview,0);delete_later(preview);preview=NULL;
    }
}
static int show_layout(const char *name) {
    const char *template_name=!strcmp(name,"ShowTFCard")?"ShowList":name;
    for (unsigned int i=0;i<sizeof(layouts)/sizeof(layouts[0]);i++) if (!strcmp(template_name,layouts[i].name)) {
        close_preview();int flags=0x800; /* FramelessWindowHint: stock constructors set this. */
        void *root=calloc(1,256),*ui=calloc(1,4096);
        if (!root || !ui) {free(root);free(ui);return 0;}
        void (*ctor)(void *,void *,const int *)=layouts[i].kind==1?dialog_ctor:layouts[i].kind==2?main_ctor:widget_ctor;
        ctor(root,NULL,&flags);
        void *main=*(void **)0x384974;
        set_palette(root,palette(main));
        ((void (*)(void *,void *))layouts[i].setup)(ui,root);
        struct qt_string root_name={ascii("AuditPreview",-1)};
        set_name(root,&root_name);string_destroy(&root_name);
        const char *background=!strcmp(name,"MainWindow")?"bg.png":!strcmp(name,"Keyboard")?"bg1.png":"bg2.png";
        char style[256];snprintf(style,sizeof(style),"QWidget#AuditPreview {background-image:url(:/Resources/icon/Finder/%s);}",background);
        struct qt_string root_style={ascii(style,-1)};
        set_style(root,&root_style);string_destroy(&root_style);
        int (*skin)(void *,const char *)=dlsym(RTLD_DEFAULT,"ffinder_flow_preview");
        if(skin)skin(root,name);
        free(ui);preview=root;visible(root,1);raise_widget(root);
        return 1;
    }
    return 0;
}
static void *window_widget(void *root,const char *name) {
    if(!root)return NULL;
    struct qt_list list={0};widgets(&list);void *found=NULL;
    if(list_range(&list))for(int i=list.d[2];i<list.d[3];i++) {
        void *w=((void **)(list.d+5))[i];if(window_of(w)!=root)continue;
        struct variant v={0};struct qt_string s={0};property(&v,w,"objectName");to_string(&s,&v);
        int equal=0;
        if(s.d){int n=((int *)s.d)[2];const unsigned short *data=((const unsigned short **)s.d)[3];
            if(n==(int)strlen(name)){equal=1;for(int j=0;j<n;j++)if(data[j]!=(unsigned char)name[j])equal=0;}}
        if(s.d)string_destroy(&s);
        variant_destroy(&v);
        if(equal){found=w;break;}
    }
    list_done(&list);return found;
}
static void *preview_widget(const char *name) {return window_widget(preview,name);}
static void preview_text(const char *name,const char *text,int button) {
    void *w=preview_widget(name);if(!w)return;
    (void)button;button=0;
    const void *(*meta_fn)(void *)=(*(void ***)w)[0];
    const void *const *m=meta_fn(w);
    for(int i=0;m && i<20;i++,m=m[0])
        if(!strcmp((const char *)m[1]+((const unsigned int *)m[2])[1],"QAbstractButton"))button=1;
    void (*set)(void *,const struct qt_string *)=dlsym(RTLD_DEFAULT,button?"_ZN15QAbstractButton7setTextERK7QString":"_ZN6QLabel7setTextERK7QString");
    struct qt_string value={ascii(text,-1)};set(w,&value);string_destroy(&value);
}
static void preview_visible(const char *name,int show) {void *w=preview_widget(name);if(w)visible(w,show!=0);}
static void preview_progress(const char *name,int value) {
    void *w=preview_widget(name);if(!w)return;
    void (*range)(void *,int,int)=dlsym(RTLD_DEFAULT,"_ZN12QProgressBar8setRangeEii");
    void (*set)(void *,int)=dlsym(RTLD_DEFAULT,"_ZN12QProgressBar8setValueEi");
    if(range && set){range(w,0,100);set(w,value);}
}
static int fixture(const char *state) {
    if(!preview)return 0;
    if(!strcmp(state,"preheat-idle") || !strcmp(state,"preheat-heating")) {
        int heating=!strcmp(state,"preheat-heating");
        const char *bed[]={"progressBarPlatform","switchPlatform","extruderPlatform","labelTemperaturePlatform","extruderPlatform_2","buttonTemperaturePlatform"};
        for(int i=0;i<6;i++)preview_visible(bed[i],0);
        const char *idle[]={"extruder1","switch_2","buttonTemperature"};
        const char *active[]={"extruder2","extruder3","labelTemperature","progressBar"};
        for(int i=0;i<3;i++)preview_visible(idle[i],!heating);
        for(int i=0;i<4;i++)preview_visible(active[i],heating);
        preview_text("revival_flow_title","Preheat preview",0);
        preview_text("extruder2","Fixture: Heating the nozzle...",0);
        preview_text("labelTemperature","120 / 220 C",0);preview_text("start_stop",heating?"Stop":"Start",1);
        preview_progress("progressBar",55);return 1;
    }
    if(!strcmp(state,"filament-idle") || !strcmp(state,"filament-heating")) {
        int heating=!strcmp(state,"filament-heating");
        preview_visible("load",!heating);preview_visible("unload",!heating);
        const char *active[]={"label","progressBar","extruder","extruderTemp"};
        for(int i=0;i<4;i++)preview_visible(active[i],heating);
        preview_text("revival_flow_title","Filament preview",0);
        preview_text("label","Fixture: Heating the nozzle. Please wait before loading filament.",0);
        preview_text("extruderTemp","120 / 220 C",0);preview_progress("progressBar",55);return 1;
    }
    if(!strcmp(state,"status")) {
        preview_text("revival_flow_title","Status preview",0);
        preview_text("labelExtruder","25 / 0 C",0);preview_text("labelFilamentStatus","Detected",0);
        preview_text("labelDoorStatus","Closed",0);preview_text("labelX","0.0 mm",0);
        preview_text("labelY","0.0 mm",0);preview_text("labelZ","10.2 mm",0);return 1;
    }
    if(!strcmp(state,"home") || !strcmp(state,"home-done")) {
        void *yes=preview_widget("yes");
        void (*set_enabled)(void *,unsigned char)=dlsym(RTLD_DEFAULT,"_ZN7QWidget10setEnabledEb");
        if(!yes || !set_enabled)return 0;
        set_enabled(yes,!strcmp(state,"home-done"));return 1;
    }
    if(!strcmp(state,"level")) {
        preview_text("label","Fixture: Turn the adjustment wheel until the paper slides beneath the nozzle with light resistance.",0);return 1;
    }
    if(!strcmp(state,"hotspot-view") || !strcmp(state,"hotspot-edit")) {
        int edit=!strcmp(state,"hotspot-edit");
        preview_visible("widget",!edit);preview_visible("labelSetup",edit);
        preview_visible("label",!edit);preview_visible("setup",!edit);
        preview_visible("widgetName",edit);preview_visible("widgetPassword",edit);preview_visible("save",edit);
        preview_text("revival_flow_title","Hotspot preview",0);preview_text("label","Fixture - Finder",0);
        preview_text("name","Fixture - Finder",0);preview_text("password","example-only",0);return 1;
    }
    if(!strcmp(state,"selection")) {
        void *table=preview_widget("tableWidget");if(!table)return 0;
        void (*set_rows)(void *,int)=dlsym(RTLD_DEFAULT,"_ZN12QTableWidget11setRowCountEi");
        void *(*horizontal)(void *)=dlsym(RTLD_DEFAULT,"_ZNK10QTableView16horizontalHeaderEv");
        void *(*vertical)(void *)=dlsym(RTLD_DEFAULT,"_ZNK10QTableView14verticalHeaderEv");
        void (*height)(void *,int,int)=dlsym(RTLD_DEFAULT,"_ZN10QTableView12setRowHeightEii");
        void (*item_ctor)(void *,const struct qt_string *,int)=dlsym(RTLD_DEFAULT,"_ZN16QTableWidgetItemC1ERK7QStringi");
        void (*set_item)(void *,int,int,void *)=dlsym(RTLD_DEFAULT,"_ZN12QTableWidget7setItemEiiP16QTableWidgetItem");
        void (*set_data)(void *,int,const struct variant *)=dlsym(RTLD_DEFAULT,"_ZN16QTableWidgetItem7setDataEiRK8QVariant");
        void (*icon_ctor)(void *,const struct qt_string *)=dlsym(RTLD_DEFAULT,"_ZN5QIconC1ERK7QString");
        void (*icon_destroy)(void *)=dlsym(RTLD_DEFAULT,"_ZN5QIconD1Ev");
        void (*value_ctor)(struct variant *,int,const void *)=dlsym(RTLD_DEFAULT,"_ZN8QVariantC1EiPKv");
        int (*meta_type)(const char *)=dlsym(RTLD_DEFAULT,"_ZN9QMetaType4typeEPKc");
        if(!set_rows || !horizontal || !vertical || !height || !item_ctor || !set_item || !set_data || !icon_ctor || !icon_destroy || !value_ctor || !meta_type)return 0;
        int icon_type=meta_type("QIcon");if(icon_type<1)return 0;
        const char *files[]={"Fixture - calibration cube.gx","Fixture - bracket_long_filename.gcode","Fixture - gear.gcode","Fixture - enclosure.gx"};
        /* Match the real ShowTFCard constructor's hidden table headers. */
        visible(horizontal(table),0);visible(vertical(table),0);set_rows(table,4);
        for(int row=0;row<4;row++) {
            height(table,row,50);
            for(int col=0;col<4;col++) {
                void *item=calloc(1,256);if(!item)return 0;
                struct qt_string text={ascii(col==2?files[row]:"",-1)};item_ctor(item,&text,0);string_destroy(&text);
                if(col==1 || (col==3 && (row==0 || row==2))) {
                    uint64_t icon[8]={0};struct variant v={0};
                    struct qt_string path={ascii(col==1?":/Resources/icon/Finder/wenjian.png":":/Resources/icon/Finder/xuanze.png",-1)};
                    icon_ctor(icon,&path);string_destroy(&path);value_ctor(&v,icon_type,icon);set_data(item,1,&v);variant_destroy(&v);icon_destroy(icon);
                }
                set_item(table,row,col,item);
            }
        }
        void *empty=preview_widget("label_2");if(empty)visible(empty,0);
        void *del=preview_widget("deleteFile");if(del)visible(del,1);
        preview_text("label","1/2",0);return 1;
    }
    if(!strcmp(state,"statuses")) {
        void (*statuses)(void *,int)=dlsym(RTLD_DEFAULT,"ffinder_flow_preview_status");
        if(!statuses)return 0;
        statuses(preview,1);preview_text("fileName","Fixture - all status indicators",0);return 1;
    }
    if(!strcmp(state,"printing") || !strcmp(state,"paused")) {
        preview_text("fileName","Fixture - calibration cube.gx",0);
        preview_text("buildTime","08:00 / 16:00",0);preview_text("buildTimeLabel","Elapsed / estimated",0);
        preview_text("extruder1","210 / 210 C",1);
        preview_text("play_pause",!strcmp(state,"paused")?"Resume":"Pause",1);
        void *play=preview_widget("play_pause");
        if(play) {
            uint64_t icon[8]={0};
            void (*ctor)(void *,const struct qt_string *)=dlsym(RTLD_DEFAULT,"_ZN5QIconC1ERK7QString");
            void (*set)(void *,const void *)=dlsym(RTLD_DEFAULT,"_ZN15QAbstractButton7setIconERK5QIcon");
            void (*destroy)(void *)=dlsym(RTLD_DEFAULT,"_ZN5QIconD1Ev");
            struct qt_string path={ascii(!strcmp(state,"paused")?":/Resources/icon/Finder/kaishi.png":":/Resources/icon/Finder/zanting.png",-1)};
            ctor(icon,&path);set(play,icon);destroy(icon);string_destroy(&path);
        }
        void *overlay=preview_widget("label");if(overlay)visible(overlay,0);
        void *bed=preview_widget("platform1");if(bed)visible(bed,0);
        void *bar=preview_widget("progressBar");
        void (*set_value)(void *,int)=dlsym(RTLD_DEFAULT,"_ZN12QProgressBar8setValueEi");
        void (*set_range)(void *,int,int)=dlsym(RTLD_DEFAULT,"_ZN12QProgressBar8setRangeEii");
        if(bar){set_range(bar,0,100);set_value(bar,50);}
        /* Reuse the real file preview's image while that page remains alive.
         * The fixture is still a plain Qt widget, never a BuildPrint object. */
        void *source=find_window("BuildFileManage");
        if(source) {
            struct qt_list list={0};widgets(&list);
            if(list_range(&list))for(int i=list.d[2];i<list.d[3];i++) {
                void *w=((void **)(list.d+5))[i];if(window_of(w)!=source)continue;
                struct variant v={0};struct qt_string s={0};property(&v,w,"objectName");to_string(&s,&v);
                int match=0;if(s.d){const unsigned short *p=((const unsigned short **)s.d)[3];int n=((int *)s.d)[2];match=n==9;for(int j=0;match && j<n;j++)if(p[j]!=(unsigned char)"thumbnail"[j])match=0;string_destroy(&s);}variant_destroy(&v);
                if(match) {
                    const void *(*pixmap)(void *)=dlsym(RTLD_DEFAULT,"_ZNK6QLabel6pixmapEv");
                    void (*scaled)(void *,const void *,const void *,int,int)=dlsym(RTLD_DEFAULT,"_ZNK7QPixmap6scaledERK5QSizeN2Qt15AspectRatioModeENS3_18TransformationModeE");
                    void (*set)(void *,const void *)=dlsym(RTLD_DEFAULT,"_ZN6QLabel9setPixmapERK7QPixmap");
                    void (*destroy)(void *)=dlsym(RTLD_DEFAULT,"_ZN7QPixmapD1Ev");
                    void *model=preview_widget("modelView");const void *pm=pixmap(w);
                    if(model && pm){uint64_t image[8]={0};int size[]={132,99};scaled(image,pm,size,1,0);set(model,image);destroy(image);}break;
                }
            }
            list_done(&list);
        }
        return 1;
    }
    if(!strcmp(state,"settings")) {
        preview_text("labelSpeedRatio","100%",0);preview_text("labelBuildTime","Elapsed: 8 min",0);
        preview_text("labelZAxis","Z: 10.2 mm",0);preview_text("labelFilamentUsed","Filament used: 1.6 m",0);return 1;
    }
    if(!strcmp(state,"stop") || !strcmp(state,"complete") || !strcmp(state,"error")) {
        const char *title=!strcmp(state,"stop")?"Fixture: Stop print?":!strcmp(state,"complete")?"Fixture: Print complete":"Fixture: File unavailable";
        const char *message=!strcmp(state,"stop")?"The current print will be canceled.":!strcmp(state,"complete")?"Your print has finished.":"The USB drive was removed. Reconnect it and choose the file again.";
        preview_text("title",title,0);preview_text("labelTitle",title,0);
        preview_text("label",message,0);preview_text("message",message,0);
        preview_text("ok",!strcmp(state,"stop")?"Stop print":"OK",1);
        return 1;
    }
    return 0;
}
static int navigate(const char *c,const char *slot) {
    for (unsigned int i=0;i<sizeof(actions)/sizeof(actions[0]);i++)
        if (!strcmp(c,actions[i].class_name) && !strcmp(slot,actions[i].slot)) {
            void *w=find_window(c);if (!w) return 0;
            char method[100];snprintf(method,sizeof(method),"1%s()",slot);
            single_shot(0,w,method);return 1;
        }
    return 0;
}
static int select_readonly(const char *name) {
    void *w=NULL;int row=-1,expected=-1;uintptr_t slot=0;
    if(!strcmp(name,"file")) {
        w=find_window("ShowTFCard");row=0;expected=2;slot=0x167620;
        if(w && *(unsigned char *)(w+0x28))return 0; /* Selection mode: not normal browse. */
    } else {
        const char *names[]={"language","wifi","hotspot","cloud"};
        for(int i=0;i<4;i++)if(!strcmp(name,names[i]))row=expected=i;
        w=find_window("Setting");slot=0x156eb0;
        if(w && *(int *)(w+0x48)!=0)return 0; /* First settings page only. */
    }
    if(!w || row<0)return 0;
    int *data=*(int **)(w+0x1c);
    if(!data || data[3]-data[2]<=row)return 0;
    int *record=((int **)(data+5))[data[2]+row];
    if(!record || *record!=expected)return 0; /* Pin the actual action id/type. */
    void **ui=*(void ***)(w+0x14);
    void *item=table_item(ui[4],row,1);
    if(!item)item=table_item(ui[4],row,0);
    if(!item)return 0;
    ((void (*)(void *,void *))slot)(w,item);
    return 1;
}
static int inline_progress(const char *name) {
    uintptr_t ctor=!strcmp(name,"ProgressDialog")?0x5e3e8:!strcmp(name,"ProgressDialogUpdate")?0x1d8350:0;
    if(!ctor)return 0;
    close_preview();void *root=calloc(1,256);if(!root)return 0;
    struct qt_string title={ascii("Layout preview",-1)},message={ascii("No operation is running.",-1)};
    ((void (*)(void *,struct qt_string *,struct qt_string *,void *))ctor)(root,&title,&message,NULL);
    string_destroy(&message);string_destroy(&title);
    preview=root;visible(root,1);raise_widget(root);return 1;
}
static void poll(void) {
    static long last=-1;struct old_time now;old_clock(1,&now);
    if (now.sec==last || busy) return;
    last=now.sec;busy=1;
    FILE *f=old_fopen("/run/revival-ui/audit-command","r");
    if (f) {
        char command[128]="",op[32]="",a[64]="",b[64]="";
        fgets(command,sizeof(command),f);fclose(f);
        if (!rename("/run/revival-ui/audit-command","/run/revival-ui/audit-command-last")) {
            old_scan(command,"%31s %63s %63s",op,a,b);int ok=0;
            if (!strcmp(op,"dump"))ok=dump();
            else if(!strcmp(op,"ready")){void *w=find_window(a);ok=w && bool_property(w,"visible");}
            else if(!strcmp(op,"scroll") && (!strcmp(a,"About") || !strcmp(a,"Licenses"))) {
                void *root=find_window(a),*area=window_widget(root,!strcmp(a,"About")?"scrollArea":"plainTextEdit");
                void *(*bar)(void *)=dlsym(RTLD_DEFAULT,"_ZNK19QAbstractScrollArea17verticalScrollBarEv");
                int (*maximum)(void *)=dlsym(RTLD_DEFAULT,"_ZNK15QAbstractSlider7maximumEv");
                void (*set)(void *,int)=dlsym(RTLD_DEFAULT,"_ZN15QAbstractSlider8setValueEi");
                if(area && bar && maximum && set && (!strcmp(b,"top") || !strcmp(b,"bottom"))) {
                    void *scroll=bar(area);set(scroll,!strcmp(b,"bottom")?maximum(scroll):0);ok=1;
                }
            }
            else if (!strcmp(op,"layout"))ok=show_layout(a);
            else if (!strcmp(op,"close")) {close_preview();ok=1;}
            else if (!strcmp(op,"navigate"))ok=navigate(a,b);
            else if (!strcmp(op,"select"))ok=select_readonly(a);
            else if (!strcmp(op,"inline"))ok=inline_progress(a);
            else if (!strcmp(op,"fixture"))ok=fixture(a);
            FILE *ack=old_fopen("/run/revival-ui/audit-ack","w");
            if (ack) {fprintf(ack,"%d %s",ok,command);fclose(ack);}
            fprintf(stderr,"SCREEN_AUDIT ok=%d command=%s",ok,command);
        }
    }
    busy=0;
}
static int load(void) {
    char exe[256];ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
    if (n<0)return 0;
    exe[n]=0;
    if (strcmp(exe,"/PROGRAM/software/2.8.4/finder-plus-arm"))return 0;
    static const unsigned char guard[]={0xf8,0x40,0x2d,0xe9,0,0x50,0x90,0xe5};
    if(memcmp((void *)string_destroy,guard,sizeof(guard)))return 0;
#define LOAD(v,s) do { *(void **)(&v)=dlsym(RTLD_DEFAULT,s); if(!v){fprintf(stderr,"SCREEN_AUDIT missing %s\n",s);return 0;} } while(0)
    LOAD(widgets,"_ZN12QApplication10allWidgetsEv");
    LOAD(property,"_ZNK7QObject8propertyEPKc");
    LOAD(variant_destroy,"_ZN8QVariantD1Ev");
    LOAD(to_string,"_ZNK8QVariant8toStringEv");
    LOAD(to_rect,"_ZNK8QVariant6toRectEv");
    LOAD(to_bool,"_ZNK8QVariant6toBoolEv");
    LOAD(qfree,"_Z5qFreePv");
    LOAD(window_of,"_ZNK7QWidget6windowEv");
    LOAD(widget_ctor,"_ZN7QWidgetC1EPS_6QFlagsIN2Qt10WindowTypeEE");
    LOAD(dialog_ctor,"_ZN7QDialogC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE");
    LOAD(main_ctor,"_ZN11QMainWindowC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE");
    LOAD(visible,"_ZN7QWidget10setVisibleEb");
    LOAD(raise_widget,"_ZN7QWidget5raiseEv");
    LOAD(delete_later,"_ZN7QObject11deleteLaterEv");
    LOAD(palette,"_ZNK7QWidget7paletteEv");
    LOAD(set_palette,"_ZN7QWidget10setPaletteERK8QPalette");
    LOAD(single_shot,"_ZN6QTimer10singleShotEiP7QObjectPKc");
    LOAD(ascii,"_ZN7QString16fromAscii_helperEPKci");
    LOAD(set_name,"_ZN7QObject13setObjectNameERK7QString");
    LOAD(set_style,"_ZN7QWidget13setStyleSheetERK7QString");
    LOAD(table_item,"_ZNK12QTableWidget4itemEii");
#undef LOAD
    return 1;
}
int audit_exec(void) __asm__("_ZN12QApplication4execEv");
int audit_exec(void) {
    int (*original)(void)=dlsym(RTLD_NEXT,"_ZN12QApplication4execEv");
    if(!original)return 1;
    if(getenv("REVIVAL_UI_SCREEN_AUDIT") && load()) {
        gui_tid=(int)syscall(224);enabled=1;
        known_windows[0].name="MainWindow";
        known_windows[0].object=*(void **)0x384974;
        known_count=1;
        fprintf(stderr,"SCREEN_AUDIT ready; stock UI, generated-layout previews only\n");
    }
    return original();
}
unsigned char audit_notify(void *,void *,void *) __asm__("_ZN12QApplication6notifyEP7QObjectP6QEvent");
unsigned char audit_notify(void *app,void *receiver,void *event) {
    static unsigned char (*original)(void *,void *,void *);
    if(!original)original=dlsym(RTLD_NEXT,"_ZN12QApplication6notifyEP7QObjectP6QEvent");
    if(!original)return 0;
    struct old_time before,after;
    int measured=enabled && syscall(224)==gui_tid;
    if(measured)old_clock(1,&before);
    unsigned char result=original(app,receiver,event);
    if(measured) {
        old_clock(1,&after);
        long us=(after.sec-before.sec)*1000000L+(after.nsec-before.nsec)/1000L;
        if(us>40000)fprintf(stderr,"SCREEN_AUDIT event_us=%ld\n",us);
        static int checked;static int (*safe)(void);
        if(!checked){safe=dlsym(RTLD_DEFAULT,"ffinder_flow_dispatch_safe");checked=1;}
        /* Stock Licenses streams text with processEvents(). Keep those nested
         * loops usable; defer only actual status/skin mutation boundaries. */
        if(!safe || safe())poll();
    }
    return result;
}

void audit_visible(void *,unsigned char) __asm__("_ZN7QWidget10setVisibleEb");
void audit_visible(void *widget,unsigned char show) {
    static void (*original)(void *,unsigned char);
    if(!original)original=dlsym(RTLD_NEXT,"_ZN7QWidget10setVisibleEb");
    if(!original)return;
    original(widget,show);
    if(enabled && show && syscall(224)==gui_tid) {
        const char *name=class_of(widget);
        /* Only real named app windows, not generated layout preview roots. */
        int app_window=0;
        for(unsigned int i=0;i<sizeof(actions)/sizeof(actions[0]);i++)
            if(!strcmp(name,actions[i].class_name))app_window=1;
        if(app_window) {
            int i;for(i=0;i<known_count;i++)if(!strcmp(known_windows[i].name,name))break;
            if(i<64) {
                known_windows[i].name=name;known_windows[i].object=widget;
                if(i==known_count)known_count++;
                fprintf(stderr,"SCREEN_AUDIT window=%s shown\n",name);
            }
        }
    }
}
