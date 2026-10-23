#include <assert.h>
#include <stdio.h>
#define MOTA_TEST_STORAGE
#define bda_main device_main
static struct {char path[100];unsigned char bytes[7000];int len,exists;} files[32];
static int writes,fail_write,fail_main,fail_dirs;
static int equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void copychars(char*d,const char*s){while((*d++=*s++));}
static int io_open(const char*p,const char*m){int i,free=-1;for(i=0;i<32;i++){if(files[i].exists&&equal(files[i].path,p))break;if(!files[i].exists&&free<0)free=i;}
 if(m[0]=='r')return i<32?i+1:0;
 if(i==32)i=free;if(i<0)return 0;files[i].exists=1;files[i].len=0;copychars(files[i].path,p);return i+1;}
static int io_read(int f,void*p,unsigned int n){unsigned int i;if(n>(unsigned)files[f-1].len)n=files[f-1].len;for(i=0;i<n;i++)((unsigned char*)p)[i]=files[f-1].bytes[i];return n;}
static int io_write(int f,const void*p,unsigned int n){unsigned int i;const char *leaf=files[f-1].path,*t=leaf;while(*t){if(*t=='\\')leaf=t+1;t++;}writes++;if(fail_write||(fail_main&&equal(leaf,"SLOT1.SAV")))return 0;for(i=0;i<n;i++)files[f-1].bytes[i]=((const unsigned char*)p)[i];files[f-1].len=n;return n;}
static int io_close(int f){return 0;}
static void io_flush(void){}
static int io_directory(const char*p){return !fail_dirs;}
static int io_mkdir(const char*p){return fail_dirs?-1:0;}
static int io_remove(const char*p){int i;for(i=0;i<32;i++)if(files[i].exists&&equal(p,files[i].path)){files[i].exists=0;return 0;}return -1;}
#include "../src/mota24.c"
int main(void){
 int i,before;char path[80];Game old;
 assert(sizeof(Game)==6608);assert(equal(data_root,"A:\\H1MOTA\\"));
 slot_path(path,0,"SAV");assert(equal(path,"A:\\H1MOTA\\SLOT1.SAV"));
 new_game();old=g;old.hp=4321;old.checksum=checksum(&old);assert(write_save("A:\\MOTA24.SAV",&old));
 mode=TITLE;open_storage(0,PLAY);assert(mode==SLOTS&&slot_valid[6]&&!slot_valid[0]);selection=6;action(5);assert(mode==SAVE_CONFIRM&&!confirm_yes);action(5);assert(mode==SLOTS&&g.hp==1000);
 action(5);action(0);action(5);assert(mode==PLAY&&g.hp==4321);assert(read_save("A:\\MOTA24.SAV",&save_buffer)&&save_buffer.hp==4321);
 mode=MENU;before=writes;open_storage(1,PLAY);action(5);render();assert(mode==SAVE_CONFIRM&&!confirm_yes);action(5);assert(mode==SLOTS&&writes==before);action(4);assert(mode==MENU);
 for(i=0;i<6;i++){g.hp=1000+i;assert(save_slot(i));}scan_slots();for(i=0;i<6;i++){assert(slot_valid[i]);assert(read_slot(i,&save_buffer)&&save_buffer.hp==1000+i);}
 g.hp=9999;assert(save_slot(0));slot_path(path,0,"BAK");assert(read_save(path,&save_buffer)&&save_buffer.hp==1000);
 fail_write=1;g.hp=8888;assert(!save_slot(0));fail_write=0;assert(read_slot(0,&save_buffer)&&save_buffer.hp==9999);
 fail_dirs=1;assert(!save_slot(0));fail_dirs=0;fail_main=1;assert(!save_slot(0));fail_main=0;assert(read_slot(0,&save_buffer)&&save_buffer.hp==9999);assert(save_slot(0));
 slot_path(path,0,"SAV");i=io_open(path,"rb");files[i-1].bytes[24]^=1;assert(read_slot(0,&save_buffer)&&save_buffer.hp==9999);
 mode=PLAY;open_storage(0,PLAY);selection=1;action(5);i=g.hp;slot_path(path,1,"SAV");io_remove(path);action(0);action(5);assert(mode==SLOTS&&g.hp==i);
 mode=MENU;open_storage(1,TITLE);selection=2;action(5);action(0);action(5);assert(mode==TITLE);
 mode=MENU;open_storage(1,-1);selection=3;action(5);action(0);action(5);assert(quit_requested);
 assert(read_save("A:\\MOTA24.SAV",&save_buffer)&&save_buffer.hp==4321);
 puts("PASS storage: six slots, legacy import, cancel, overwrite backup, short write, directory/primary write failure, corrupt main fallback, failed load preservation, save/exit");
 return 0;
}
