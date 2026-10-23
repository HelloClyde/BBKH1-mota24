/* H1 saves live independently of the original 9588 directory. The unchanged
   6608-byte Game payload allows explicit import via A:\\MOTA24.SAV / .BAK. */
static const char *data_dirs[]={"A:\\H1MOTA"};
static const char data_root[]="A:\\H1MOTA\\";
#ifdef MOTA_TEST_STORAGE
typedef int storage_file;
#else
typedef mota_file *storage_file;
#define io_open platform_file_open
#define io_read platform_file_read
#define io_write platform_file_write
#define io_close platform_file_close
#define io_mkdir platform_mkdir
#define io_directory platform_directory
/* H1 fclose commits buffered file writes; no 9588 flush ABI is used. */
static void io_flush(void){}
#endif
static int io_valid(storage_file f){return f!=0&&f!=(storage_file)-1;}
#define SAVE_SLOTS 6
static Game save_buffer,save_verify;
static int slot_valid[7],slot_floor[7],slot_hp[7],slot_minutes[7];
static int storage_saving,storage_return,storage_after,confirm_yes;
static int active_slot;
static void slot_path(char *out,int slot,const char *ext){
 int i=0,j=0;while(data_root[i]){out[i]=data_root[i];i++;}
 out[i++]='S';out[i++]='L';out[i++]='O';out[i++]='T';out[i++]=(char)('1'+slot);out[i++]='.';
 while(ext[j])out[i++]=ext[j++];out[i]=0;
}
static int read_save(const char *path,Game *out){
 storage_file f=io_open(path,"rb");int n;if(!io_valid(f))return 0;
 n=io_read(f,out,sizeof(*out));io_close(f);return n==(int)sizeof(*out)&&valid_save(out);
}
static int write_save(const char *path,const Game *s){
 storage_file f=io_open(path,"wb");int n,c;if(!io_valid(f))return 0;
 n=io_write(f,s,sizeof(*s));io_flush();c=io_close(f);return n==(int)sizeof(*s)&&c==0;
}
static int read_slot(int slot,Game *out){
 char path[80];
 if(slot==SAVE_SLOTS)return read_save("A:\\MOTA24.SAV",out)||read_save("A:\\MOTA24.BAK",out);
 slot_path(path,slot,"SAV");if(read_save(path,out))return 1;
 slot_path(path,slot,"BAK");return read_save(path,out);
}
static void scan_slots(void){int i;for(i=0;i<=SAVE_SLOTS;i++){
 slot_valid[i]=read_slot(i,&save_buffer);
 if(slot_valid[i]){slot_floor[i]=save_buffer.floor;slot_hp[i]=save_buffer.hp;slot_minutes[i]=save_buffer.seconds/60;}
}}
static int save_slot(int slot){
 char path[80],bak[80],tmp[80];int i;
 if(slot<0||slot>=SAVE_SLOTS)return 0;
 for(i=0;i<(int)(sizeof(data_dirs)/sizeof(data_dirs[0]));i++)if(!io_directory(data_dirs[i])&&(io_mkdir(data_dirs[i])==-1||!io_directory(data_dirs[i]))){notice="创建存档目录失败";return 0;}
 slot_path(path,slot,"SAV");slot_path(bak,slot,"BAK");slot_path(tmp,slot,"TMP");
 g.checksum=checksum(&g);
 if(!write_save(tmp,&g)||!read_save(tmp,&save_verify)){notice="保存失败，原存档保留";return 0;}
 /* Preserve the last valid main file before replacement. Never overwrite a
    valid backup with a corrupt main file. TMP is not a committed slot. */
 if(read_save(path,&save_buffer)&&(!write_save(bak,&save_buffer)||!read_save(bak,&save_verify))){notice="备份失败，原存档保留";return 0;}
 if(!write_save(path,&g)||!read_save(path,&save_verify)||save_verify.checksum!=g.checksum){notice="保存失败，可读取备份";return 0;}
 active_slot=slot;notice="进度已保存";return 1;
}
static void open_storage(int saving,int after){
 storage_return=mode;storage_saving=saving;storage_after=after;
 scan_slots();selection=active_slot;mode=SLOTS;notice="";repeat_direction=0;
}
