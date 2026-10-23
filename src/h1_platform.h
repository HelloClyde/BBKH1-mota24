#ifndef MOTA_H1_PLATFORM_H
#define MOTA_H1_PLATFORM_H
/* H1/Y100 V1.41 only. No BBK9588 runtime addresses or window ABI. */
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

#ifdef MOTA_HOST_TEST
typedef void mota_file;
static mota_file *platform_file_open(const char *p,const char *m){(void)p;(void)m;return 0;}
static int platform_file_read(mota_file *f,void *p,unsigned n){(void)f;(void)p;(void)n;return 0;}
static int platform_file_write(mota_file *f,const void *p,unsigned n){(void)f;(void)p;(void)n;return 0;}
static int platform_file_close(mota_file *f){(void)f;return 0;}
static int platform_directory(const char *p){(void)p;return 1;}
static int platform_mkdir(const char *p){(void)p;return 0;}
#else
#include "h1_sdk.h"
typedef h1_file mota_file;
static mota_file *platform_file_open(const char *p,const char *m){return h1_fopen(p,m);}
static int platform_file_read(mota_file *f,void *p,unsigned n){return (int)h1_fread(p,1,n,f);}
static int platform_file_write(mota_file *f,const void *p,unsigned n){return (int)h1_fwrite(p,1,n,f);}
static int platform_file_close(mota_file *f){return h1_fclose(f);}
static int platform_mkdir(const char *p){
    typedef int (*Fn)(const char *);
    Fn fn=(Fn)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_FS_TABLE_SLOT),0x30u);
    return fn?fn(p):-1;
}
static int platform_directory(const char *p){
    /* Same native stat lock used by h1-debug/files.c. FS+0x54 itself does not
       take the filesystem semaphore; stdio/mkdir must run outside this lock. */
    typedef int (*Stat)(const char *,void *);
    typedef void (*Pend)(void *,unsigned,u8 *);
    typedef unsigned (*Post)(void *);
    void *sem=*(void *volatile *)0x8071d378u;
    u32 st[6]={0};u8 error=255;int result;
    Stat fn=(Stat)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_FS_TABLE_SLOT),0x54u);
    if(!fn||(uintptr_t)sem<0x80004000u||(uintptr_t)sem>=0x83c00000u)return 0;
    ((Pend)0x8000d484u)(sem,0,&error);
    if(error)return 0;
    result=fn(p,st);
    ((Post)0x8000d598u)(sem);
    return result==0&&(st[0]&0x40000000u)!=0;
}
static int platform_open(void){
    typedef int (*Fn)(void);
    Fn fn=(Fn)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_GUI_TABLE_SLOT),0x84cu);
    return fn&&fn()!=0;
}
static void platform_close(void){
    typedef void (*Fn)(void);
    Fn fn=(Fn)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_GUI_TABLE_SLOT),0x850u);
    if(fn)fn();
}
static int platform_present(const u16 *pixels){
    typedef volatile u32 *(*Fn)(u16 *,void *);
    Fn fn=(Fn)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_GUI_TABLE_SLOT),0x8f4u);
    u16 info[6]={0};unsigned i;
    volatile u32 *out=fn?fn(info,0):0;
    if(!out||info[1]!=480||info[2]!=272||info[3]!=1920||info[5]!=32)return 0;
    for(i=0;i<480u*272u;i++){
        u32 p=pixels[i];
        out[i]=((p&0xf800u)<<8)|((p&0x07e0u)<<5)|((p&0x001fu)<<3);
    }
    return 1;
}
static int platform_pen(int *x,int *y){
    typedef void (*Fn)(u16 *,u16 *);
    const u32 *fn=(const u32 *)h1_runtime_entry(h1_runtime_table(H1_RUNTIME_GUI_TABLE_SLOT),0x6c0u);
    u16 px=65535,py=65535;
    if(!fn||fn[0]!=0x27bdffa8u||fn[1]!=0xafbf0050u)return 0;
    ((Fn)fn)(&px,&py);*x=px;*y=py;return px<480&&py<272;
}
/* Core directions: right, left, down, up, back, confirm. */
static int platform_key(int key){
    switch(key){
    case H1_KEY_RIGHT:case H1_KEY_RIGHT_ALT:return 0;
    case H1_KEY_LEFT:case H1_KEY_LEFT_ALT:return 1;
    case H1_KEY_DOWN:return 2;case H1_KEY_UP:return 3;
    case H1_KEY_BACK:case H1_KEY_ESCAPE:return 4;
    case H1_KEY_ENTER:case H1_KEY_CONFIRM:case H1_KEY_SPACE:return 5;
    default:return -1;
    }
}
#endif
#endif
