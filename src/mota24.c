#include "h1_platform.h"
#include "core.c"
#include "font.h"
#include "sprites.h"
#include "story.h"
/* GCC emits this for structure copies in freestanding builds. */
void *memcpy(void *dst,const void *src,unsigned int n){unsigned char *d=dst;const unsigned char *s=src;while(n--)*d++=*s++;return dst;}

enum { SCREEN_W=480, SCREEN_H=272, MAP_X=108, MAP_Y=4, TILE_SIZE=24, MAP_SIZE=264,
       SIDE_X=378, SIDE_W=98, TOOL_Y=6, TOOL_H=32, TOOL_STEP=36,
       PANEL_X=92, PANEL_Y=8, PANEL_W=296, PANEL_H=256,
       TITLE_X=252, TITLE_Y=73, TITLE_W=188, TITLE_STEP=32,
       MENU_X=110, MENU_Y=44, MENU_W=124, MENU_STEP_X=136, MENU_STEP_Y=36,
       SLOT_X=32, SLOT_Y=46, SLOT_W=202, SLOT_H=42, SLOT_STEP_X=214, SLOT_STEP_Y=48,
       CONFIRM_Y=198, CANCEL_X=106, ACCEPT_X=254, CONFIRM_W=120,
       DIALOG_X=92, DIALOG_Y=100, DIALOG_W=296, DIALOG_H=164 };
static u16 screen[SCREEN_W*SCREEN_H];
static volatile int dirty,detached,touch_active,touch_pending;
static volatile u32 touch_value;
static u32 previous,resume,second_tick;
static int touch_escape_suppressed,escape_pending;
static u32 escape_due;
static u32 repeat_direction,repeat_due;
/* 25 ms ticks: first repeat after 300 ms, then one step every 125 ms. */
static u32 direction_repeat(u32 current,u32 now,int enabled){
    u32 direction=current&15u;
    if(!enabled||!direction||(direction&(direction-1u))){repeat_direction=0;return 0;}
    if(direction!=repeat_direction){repeat_direction=direction;repeat_due=now+12;return 0;}
    if((s32)(now-repeat_due)>=0){repeat_due=now+5;return direction;}
    return 0;
}
/* docs/gam4980_sdk_optimization.md: touch also raises packet Escape.
 * Match gam4980_payload.c: release the latch only after a clean low sample. */
static void suppress_touch_escape(void){touch_escape_suppressed=1;escape_pending=0;previous&=~(1u<<4);}
static u32 input_edges(u32 current,u32 now){
    const u32 esc=1u<<4;u32 pressed;
    if(touch_escape_suppressed){
        if(!touch_active&&!(current&esc))touch_escape_suppressed=0;
        current&=~esc;escape_pending=0;
    }
    pressed=current&~previous;previous=current;
    /* Allow queued touch-prefix/coordinate messages to identify an ESC that
     * appeared in the hardware packet just before its window message. */
    if(pressed&esc){escape_pending=1;escape_due=now+4;}
    pressed&=~esc;
    if(escape_pending&&(s32)(now-escape_due)>=0){escape_pending=0;pressed|=esc;}
    return pressed;
}
#include "storage.h"
static u16 color(int r,int g,int b){u16 c=(u16)(((r&248)<<8)|((g&252)<<3)|(b>>3));return c?c:1;}
static void pixel(int x,int y,u16 c){if(x<0||x>=SCREEN_W||y<0||y>=SCREEN_H)return;screen[y*SCREEN_W+x]=c;}
static void rect(int x,int y,int w,int h,u16 c){int a,b;for(b=y;b<y+h;b++)for(a=x;a<x+w;a++)pixel(a,b,c);}
static void border(int x,int y,int w,int h,u16 c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
#define WHITE 0xef7d
#define GOLD 0xf66b
#define MUTED 0x8493
#define DARK 0x1084
#define BLUE 0x4d5d
#define RED 0xf28c
static void text(int x,int y,const char *s,u16 c){
    while(*s){unsigned int ch=(u8)*s++;int lo=0,hi=(int)(sizeof(glyphs)/sizeof(glyphs[0]))-1,j,k;
        if(ch>=224){ch=((ch&15)<<12)|(((u8)s[0]&63)<<6)|((u8)s[1]&63);s+=2;}
        else if(ch>=192){ch=((ch&31)<<6)|((u8)*s++&63);}
        while(lo<=hi){int m=(lo+hi)/2;if(glyphs[m].code<ch)lo=m+1;else if(glyphs[m].code>ch)hi=m-1;else {for(j=0;j<12;j++)for(k=0;k<12;k++)if(glyphs[m].rows[j]&(1<<(11-k)))pixel(x+k,y+j,c);break;}}
        x+=ch<128?6:12;
    }
}
static void number(int x,int y,int v,u16 c){char b[16];int i=14;if(v<0){text(x,y,"无法破防",RED);return;}b[15]=0;do{b[i--]=(char)('0'+v%10);v/=10;}while(v&&i>=0);text(x,y,b+i+1,c);}
static void right_number(int right,int y,int v,u16 c){int n=v,digits=1;while(n>=10){n/=10;digits++;}number(right-digits*6,y,v,c);}
static void centered_number(int x,int y,int w,int v,u16 c){int n=v,digits=1;while(n>=10){n/=10;digits++;}number(x+(w-digits*6)/2,y,v,c);}
static void key_icon(int x,int y,int kind){int a,b;for(b=0;b<14;b++)for(a=0;a<14;a++){u16 c=key_icons[kind][b*14+a];if(c)pixel(x+a,y+b,c);}}
static u32 input_now;
static const char *last_notice;
static u32 notice_started;
static void message_text(const char *s){
    int x=SIDE_X+4,y=132,width=0,skip=0;char b[4];const char *p=s;
    if(mode==PLAY&&s){
        if(s!=last_notice){last_notice=s;notice_started=input_now;}
        while(*p){int n=(u8)*p>=224?3:(u8)*p>=192?2:1;width+=n==1?6:12;p+=n;}
        if(width>810){unsigned int steps=(width-810+5)/6,phase=(input_now-notice_started)%(160+steps*12);if(phase>80){skip=(phase-80)/12;if(skip>(int)steps)skip=steps;skip*=6;}}
    }
    while(s&&*s){int n=(u8)*s>=224?3:(u8)*s>=192?2:1,w=n==1?6:12,i;
        if(mode==PLAY&&skip>0){skip-=w;s+=n;continue;}
        if(x+w>SCREEN_W-6){x=SIDE_X+4;y+=14;}
        if(y>250)break;for(i=0;i<n;i++)b[i]=*s++;b[n]=0;text(x,y,b,GOLD);x+=w;
    }
}
static int animation_frame,hero_direction;
static int story_page,help_return;
static u32 animation_tick;
static int hero_frame,hero_stride;
static u32 hero_step_tick;
static const StoryPage *dialog_pages;
static int dialog_page,dialog_count,dialog_return;
static void begin_dialog(const StoryPage *pages,int count){
    dialog_return=PLAY;dialog_pages=pages;dialog_count=count;dialog_page=0;mode=DIALOG;hero_frame=0;repeat_direction=0;
}
/* Presentation advances independently; the deterministic core commits once. */
static Enemy battle_enemy;
static int battle_id,battle_dx,battle_dy,battle_hp,battle_hero_hp;
static int battle_phase,battle_round,battle_last,battle_amount,battle_special;
static u32 battle_due;
static void finish_battle(void){
    if(mode!=BATTLE)return;
    mode=PLAY;move(battle_dx,battle_dy);hero_frame=0;repeat_direction=0;
    if(mode==WIN)story_page=0;
    dirty=1;
}
static int begin_battle(int dx,int dy,int id){
    int loss=damage(id);
    if(loss<0||loss>=g.hp)return 0;
    battle_enemy=enemy(id);battle_id=id;battle_dx=dx;battle_dy=dy;
    battle_hp=battle_enemy.hp;battle_hero_hp=g.hp;
    battle_special=special_damage(battle_enemy);
    battle_phase=battle_special?0:1;battle_round=0;battle_last=3;battle_amount=0;
    battle_due=input_now+6;mode=BATTLE;repeat_direction=0;hero_frame=0;dirty=1;return 1;
}
static void advance_battle(u32 now){
    if(mode!=BATTLE||(s32)(now-battle_due)<0)return;
    dirty=1;battle_due=now+6;
    if(battle_phase==0){battle_hero_hp-=battle_special;battle_amount=battle_special;battle_last=2;battle_phase=1;}
    else if(battle_phase==1){
        int hit=g.atk-battle_enemy.def;
        battle_round++;battle_amount=hit<battle_hp?hit:battle_hp;
        battle_hp-=battle_amount;battle_last=0;
        battle_phase=battle_hp?2:3;if(!battle_hp)battle_due=now+12;
    }else if(battle_phase==2){
        battle_amount=max(0,battle_enemy.atk-g.def);battle_hero_hp-=battle_amount;
        battle_last=1;battle_phase=1;
    }else finish_battle();
}
/* Wrap every remaining NPC response into the same modal dialogue UI. */
static StoryPage npc_response;
static char npc_lines[3][64];
static void response_dialog(int id,int next_mode){
    const char *p=notice;int line=0,col=0,j=0,k,n;
    for(k=0;k<3;k++)npc_lines[k][0]=0;
    while(*p&&line<3){
        n=(u8)*p>=224?3:(u8)*p>=192?2:1;
        if(col+(n==1?1:2)>32){npc_lines[line][j]=0;line++;col=j=0;if(line>=3)break;}
        for(k=0;k<n;k++)npc_lines[line][j++]=*p++;
        npc_lines[line][j]=0;col+=n==1?1:2;
    }
    npc_response.speaker=id==121?"老人":id==122?"商人":id==123?"小偷":id==131?"商店":"公主";
    npc_response.portrait=id;npc_response.line1=npc_lines[0];npc_response.line2=npc_lines[1];npc_response.line3=npc_lines[2];
    begin_dialog(&npc_response,1);dialog_return=next_mode;
}
static void walk(int dx,int dy){
    int x=g.x,y=g.y,floor=g.floor,t=tile(g.floor,g.x+dx,g.y+dy);u32 flags=g.flags;
    if(mode==PLAY&&t>=201&&t<300&&enemy_base[t].hp&&
       !(floor==20&&x+dx==5&&y+dy==7)&&
       !(floor==18&&x+dx==10&&y+dy==10&&!(g.flags&PRINCESS))&&
       !(floor==21&&x+dx==5&&y+dy==0&&!(g.flags&BOSS21))&&begin_battle(dx,dy,t))return;
    move(dx,dy);if(mode==WIN)story_page=0;
    if(mode==PLAY&&t==124){
        if(floor==0){
            if(!(flags&INTRO))begin_dialog(fairy_first,PAGE_COUNT(fairy_first));
            else if(flags&ICE)begin_dialog(fairy_ice,PAGE_COUNT(fairy_ice));
            else if(flags&CROSS)begin_dialog(fairy_bless,PAGE_COUNT(fairy_bless));
            else begin_dialog(fairy_reminder,PAGE_COUNT(fairy_reminder));
        }else if(floor==22){
            if((flags&(FIRE|HEART))==(FIRE|HEART))begin_dialog(fairy_unseal,PAGE_COUNT(fairy_unseal));
            else begin_dialog(fairy_hidden,PAGE_COUNT(fairy_hidden));
        }
    }
    if((mode==PLAY||mode==SHOP)&&((t>=121&&t<=123)||t==131||t==132)){
        if(t==123&&floor!=2){
            if(!(flags&THIEF))begin_dialog(thief_first,PAGE_COUNT(thief_first));
            else if(flags&HAMMER)begin_dialog(thief_done,PAGE_COUNT(thief_done));
            else begin_dialog(thief_wait,PAGE_COUNT(thief_wait));
        }else if(t==132)begin_dialog(princess_dialog,PAGE_COUNT(princess_dialog));
        else if(floor==16){
            if(g.flags&ICE)begin_dialog(elder_secret,PAGE_COUNT(elder_secret));
            else begin_dialog(elder_missed,PAGE_COUNT(elder_missed));
        }else response_dialog(t,mode);
    }
    if(g.x!=x||g.y!=y||g.floor!=floor){hero_stride^=1;hero_frame=hero_stride?1:3;hero_step_tick=input_now;}
}
static void animate_hero(u32 now){
    if(hero_frame&&now-hero_step_tick>=5){hero_frame=0;dirty=1;}
}
static void sprite(int x,int y,int t){
    int a,b,frame=t>=300?0:animation_frame;
    const unsigned short *pixels=t>=300?hero_pixels[t-300][hero_frame]:sprite_pixels[sprite_index[t]*2+frame];
    for(b=0;b<TILE_SIZE;b++)for(a=0;a<TILE_SIZE;a++)pixel(x+a,y+b,pixels[(b*20/TILE_SIZE)*20+a*20/TILE_SIZE]);
}

static const char *title_items[]={"开始冒险","继续存档","操作说明","关于","退出游戏"};
static const char *menu_items[]={"继续冒险","怪物手册","楼层传送","保存进度","读取进度","操作说明","重新开始","保存回标题","保存并退出"};
static const char *shop_lines[6][3]={
    {"25金币  生命 +800","25金币  攻击 +4","25金币  防御 +4"},
    {"100金币 生命 +4000","100金币 攻击 +20","100金币 防御 +20"},
    {"100经验 升一级","30经验  攻击 +5","30经验  防御 +5"},
    {"270经验 升三级","95经验  攻击 +17","95经验  防御 +17"},
    {"10金币  黄钥匙","50金币  蓝钥匙","100金币 红钥匙"},
    {"黄钥匙 卖7金币","蓝钥匙 卖35金币","红钥匙 卖70金币"}};
static int text_width(const char *s){int width=0;while(*s){int n=(u8)*s>=224?3:(u8)*s>=192?2:1;width+=n==1?6:12;s+=n;}return width;}
static void centered(int x,int y,int w,const char *s,u16 c){text(x+(w-text_width(s))/2,y,s,c);}
static void button(int x,int y,int w,int h,const char *label,int active){rect(x,y,w,h,active?0x39ab:0x2147);border(x,y,w,h,active?GOLD:0x52aa);centered(x,y+(h-12)/2,w,label,active?GOLD:WHITE);}
static void panel(const char *title){rect(PANEL_X,PANEL_Y,PANEL_W,PANEL_H,DARK);border(PANEL_X,PANEL_Y,PANEL_W,PANEL_H,GOLD);text(PANEL_X+14,PANEL_Y+12,title,GOLD);rect(PANEL_X+12,PANEL_Y+30,PANEL_W-24,1,0x52aa);}
static void footer(const char *hint){text(106,243,hint,MUTED);button(304,234,72,26,"返回",0);}
static void full_backdrop(void){rect(0,0,SCREEN_W,SCREEN_H,DARK);border(6,6,SCREEN_W-12,SCREEN_H-12,0x52aa);border(9,9,SCREEN_W-18,SCREEN_H-18,GOLD);}
static void dim_backdrop(void){int i;for(i=0;i<SCREEN_W*SCREEN_H;i++)screen[i]=(u16)((screen[i]&0xf7deu)>>1);}
static void story_backdrop(const char *heading){
    int x,y;full_backdrop();centered(22,26,196,"魔塔二十四层",GOLD);centered(22,47,196,"H1",MUTED);
    for(y=0;y<4;y++)for(x=0;x<5;x++)sprite(60+x*TILE_SIZE,83+y*TILE_SIZE,1);
    for(x=0;x<5;x+=2)sprite(60+x*TILE_SIZE,59,1);
    sprite(108,155,85);sprite(108,179,300);
    centered(22,216,196,heading,GOLD);centered(22,241,196,"HelloClyde",MUTED);
    rect(220,22,1,228,0x52aa);
}
static void render_story(void){
    int i;
    if(mode==TITLE){
        story_backdrop("经典冒险 · 勇者启程");
        text(TITLE_X,37,"方向选择  确定进入",MUTED);
        for(i=0;i<5;i++)button(TITLE_X,TITLE_Y+i*TITLE_STEP,TITLE_W,28,title_items[i],selection==i);
        if(notice&&notice[0])text(246,244,notice,RED);
    }else if(mode==OPENING){
        const StoryPage *p=&opening_pages[story_page];
        story_backdrop("经典冒险 · 勇者启程");
        text(238,35,p->speaker,GOLD);if(p->portrait)sprite(420,28,p->portrait);
        text(238,79,p->line1,WHITE);text(238,108,p->line2,WHITE);text(238,137,p->line3,WHITE);
        number(404,170,story_page+1,MUTED);text(413,170,"/8",MUTED);
        button(238,194,218,34,story_page==PAGE_COUNT(opening_pages)-1?"进入魔塔":"继续",1);
        text(238,241,"返回：跳过片头",MUTED);
    }else{
        story_backdrop(g.floor==26?"终章 · 魔塔的黎明":"终章 · 勇者的凯旋");
        if(!story_page){
            text(238,57,"魔王倒下，塔内恢复了宁静。",WHITE);
            text(238,89,g.floor==26?"隐藏的旅程，终于迎来终点。":"你的勇气，为王国带回希望。",WHITE);
            text(238,121,g.floor==26?"晨光照亮了勇者归来的道路。":"本次未开启冰杖隐藏路线。",WHITE);
            button(238,194,218,34,"冒险记录",1);
        }else{
            text(238,57,"最终生命",MUTED);right_number(450,57,g.hp,WHITE);
            text(238,89,"攻击 / 防御",MUTED);right_number(394,89,g.atk,WHITE);right_number(450,89,g.def,WHITE);
            text(238,121,"冒险用时（分钟）",MUTED);right_number(450,121,g.seconds/60,GOLD);
            text(238,158,g.floor==26?"感谢游玩，愿勇气与你同在。":"隐藏路线可继续至二十四层。",WHITE);
            button(238,194,218,34,"返回主菜单",1);
        }
        text(238,241,"确定 / 点击：继续",MUTED);
    }
}
static void render_help(void){
    panel("操作与隐藏路线");
    text(108,49,"方向移动 / 确定交互",WHITE);
    text(108,71,"罗盘：上升层，下键降层",WHITE);
    text(108,93,"六个存档，保存需确认",WHITE);
    text(108,119,"隐藏：25分钟内到16层",GOLD);
    text(108,141,"此前不要取得七层十字架",WHITE);
    text(108,163,"冰杖先交仙子，再交十字架",WHITE);
    text(108,185,"21层为普通结局",MUTED);
    text(108,207,"隐藏路线通往22至24层",MUTED);
    footer("确定 / 返回关闭");
}
static void render_playfield(void){
    int x,y;rect(0,0,SCREEN_W,SCREEN_H,DARK);
    text(8,8,"魔塔",GOLD);right_number(76,8,g.floor==26?24:g.floor>=23?23:g.floor,GOLD);text(80,8,"层",GOLD);
    text(8,29,"等级",MUTED);right_number(88,29,g.level,WHITE);
    text(8,51,"生命",MUTED);right_number(88,68,mode==BATTLE?battle_hero_hp:g.hp,WHITE);
    text(8,90,"攻",RED);right_number(88,90,g.atk,WHITE);
    text(8,111,"防",BLUE);right_number(88,111,g.def,WHITE);
    text(8,132,"金",GOLD);right_number(88,132,g.gold,WHITE);
    text(8,153,"经验",MUTED);right_number(88,169,g.exp,WHITE);
    key_icon(7,190,0);centered_number(2,206,24,g.keys[0],WHITE);
    key_icon(41,190,1);centered_number(36,206,24,g.keys[1],WHITE);
    key_icon(75,190,2);centered_number(70,206,24,g.keys[2],WHITE);
    border(MAP_X-2,MAP_Y-2,MAP_SIZE+4,MAP_SIZE+4,0xbdf7);
    for(y=0;y<11;y++)for(x=0;x<11;x++)sprite(MAP_X+x*TILE_SIZE,MAP_Y+y*TILE_SIZE,tile(g.floor,x,y));
    sprite(MAP_X+g.x*TILE_SIZE,MAP_Y+g.y*TILE_SIZE,300+hero_direction);
    message_text(notice);
    button(SIDE_X,TOOL_Y,SIDE_W,TOOL_H,"菜单",0);
    button(SIDE_X,TOOL_Y+TOOL_STEP,SIDE_W,TOOL_H,"手册",0);
    button(SIDE_X,TOOL_Y+TOOL_STEP*2,SIDE_W,TOOL_H,"传送",0);
    rect(SIDE_X+4,120,SIDE_W-8,1,0x52aa);
}
static void render_slots(void){
    int i;full_backdrop();text(32,23,storage_saving?"选择保存位置":"选择读取位置",GOLD);
    if(mode==SLOTS){
        for(i=0;i<SAVE_SLOTS;i++){
            int xx=SLOT_X+(i%2)*SLOT_STEP_X,yy=SLOT_Y+(i/2)*SLOT_STEP_Y;
            rect(xx,yy,SLOT_W,SLOT_H,i==selection?0x39ab:0x2147);border(xx,yy,SLOT_W,SLOT_H,i==selection?GOLD:0x52aa);
            text(xx+10,yy+6,"存档",WHITE);number(xx+40,yy+6,i+1,GOLD);
            if(slot_valid[i]){number(xx+64,yy+6,slot_floor[i]==26?24:slot_floor[i]>=23?23:slot_floor[i],GOLD);text(xx+89,yy+6,"层",MUTED);number(xx+118,yy+6,slot_minutes[i],WHITE);text(xx+165,yy+6,"分钟",MUTED);text(xx+10,yy+24,"生命",MUTED);number(xx+46,yy+24,slot_hp[i],WHITE);}
            else text(xx+70,yy+23,"空",MUTED);
        }
        if(!storage_saving)button(32,190,416,34,"导入旧档",selection==SAVE_SLOTS);
        else text(32,199,"覆盖前保留一份备份",MUTED);
        text(32,241,notice&&notice[0]?notice:"确定选择 / 返回取消",notice&&notice[0]?GOLD:MUTED);
        button(352,234,96,26,"返回",0);
    }else{
        if(selection==SAVE_SLOTS)text(48,59,"旧根目录存档",WHITE);else {text(48,59,"存档",WHITE);number(84,59,selection+1,WHITE);}
        text(48,86,storage_saving?(slot_valid[selection]?"覆盖此存档？":"保存到此位置？"):"读取此存档？",GOLD);
        if(storage_saving){text(48,115,"当前生命",MUTED);number(188,115,g.hp,WHITE);}
        if(slot_valid[selection]){text(48,142,"存档生命",MUTED);number(188,142,slot_hp[selection],WHITE);}
        text(48,171,storage_saving?"覆盖前保留一份备份":"未保存的进度将丢失",WHITE);
        button(CANCEL_X,CONFIRM_Y,CONFIRM_W,34,"取消",!confirm_yes);
        button(ACCEPT_X,CONFIRM_Y,CONFIRM_W,34,"确定",confirm_yes);
        centered(32,246,416,"方向选择 / 确定执行",MUTED);
    }
}
static void render(void){
    int i,n=0,seen[300];
    if(mode==SLOTS||mode==SAVE_CONFIRM){render_slots();return;}
    if(mode==ABOUT){
        full_backdrop();text(48,31,"魔塔二十四层",GOLD);text(48,67,"作者",MUTED);text(110,67,"HelloClyde",WHITE);text(48,111,"H1  480 x 272",WHITE);
        text(252,67,"感谢：",GOLD);text(252,102,"QQ 群",MUTED);text(252,132,"「步步高电子词典游戏群」",WHITE);text(252,167,"群号：830340878",GOLD);
        button(164,220,152,32,"返回主菜单",0);return;
    }
    if(mode==HELP&&help_return==TITLE){full_backdrop();render_help();return;}
    if(mode==TITLE||mode==OPENING||mode==WIN){render_story();return;}
    render_playfield();if(mode!=PLAY)dim_backdrop();
    if(mode==MENU){
        panel("冒险菜单");for(i=0;i<9;i++)button(MENU_X+(i/5)*MENU_STEP_X,MENU_Y+(i%5)*MENU_STEP_Y,MENU_W,32,menu_items[i],selection==i);
        footer("方向选择 / 确定");
    }
    if(mode==SHOP){
        panel(shop<2?"金币商店":shop<4?"经验商店":"钥匙商人");
        for(i=0;i<3;i++)button(110,58+i*48,260,38,shop_lines[shop][i],selection==i);
        text(110,210,"确定购买 / 返回离开",MUTED);footer("");
    }
    if(mode==BOOK){
        panel("怪物手册  左右翻页");for(i=0;i<300;i++)seen[i]=0;
        for(i=0;i<121;i++){int t=g.map[g.floor][i];if(t>=201&&t<300&&enemy_base[t].hp)seen[t]=1;}
        for(i=201;i<300;i++)if(seen[i]){
            if(n/3==bookpage){
                Enemy e=enemy(i);int yy=47+(n%3)*59,d=damage(i);
                sprite(106,yy,i);text(136,yy,e.name,WHITE);
                text(136,yy+14,"生命",MUTED);number(166,yy+14,e.hp,WHITE);
                text(250,yy+14,"损",MUTED);number(267,yy+14,d,d<0||d>=g.hp?RED:GOLD);
                text(136,yy+28,"攻",MUTED);number(153,yy+28,e.atk,WHITE);text(250,yy+28,"防",MUTED);number(267,yy+28,e.def,WHITE);
                if(e.special==11)text(136,yy+42,e.drain_divisor==3?"战前扣当前生命1/3":"战前扣当前生命1/4",RED);
                else if(e.special==22){text(136,yy+42,"战前固定损血",RED);number(220,yy+42,e.extra,RED);}
            }n++;
        }
        if(!n)text(116,112,"本层没有怪物",WHITE);
        button(106,234,62,26,"<",0);text(179,242,"页",MUTED);number(198,242,bookpage+1,WHITE);button(226,234,62,26,">",0);button(304,234,72,26,"返回",0);
    }
    if(mode==FLY){
        panel("风之罗盘");text(110,53,"上键升层 / 下键降层",WHITE);
        button(200,80,80,34,"上",0);text(163,132,"前往楼层",GOLD);number(283,132,selection,GOLD);button(200,163,80,34,"下",0);
        button(110,219,166,34,"确定传送",1);button(290,219,80,34,"返回",0);
    }
    if(mode==RESTART){
        panel("重新开始？");text(110,81,"当前未保存进度将丢失",WHITE);text(110,123,"确定开始新冒险",GOLD);
        button(110,189,124,38,"返回",0);button(246,189,124,38,"确定",1);footer("返回取消");
    }
    if(mode==HELP)render_help();
    if(mode==BATTLE){
        int hw=battle_hero_hp/max(1,g.hp/96),ew=battle_hp/max(1,battle_enemy.hp/96);
        if(hw>96)hw=96;if(ew>96)ew=96;
        panel("战斗中");text(112,50,"勇士",GOLD);text(256,50,battle_enemy.name,WHITE);
        sprite(146+(battle_last==0?6:0),78,300+2);sprite(302-(battle_last==1?6:0),78,battle_id);
        if(battle_last==0)border(298,74,32,32,RED);if(battle_last==1||battle_last==2)border(142,74,32,32,RED);
        text(234,84,"VS",GOLD);border(111,118,98,8,MUTED);rect(112,119,hw,6,RED);border(255,118,98,8,MUTED);rect(256,119,ew,6,RED);
        right_number(208,134,battle_hero_hp,WHITE);right_number(352,134,battle_hp,WHITE);text(112,159,"回合",MUTED);number(146,159,battle_round,WHITE);
        text(112,183,battle_last==3?"准备交战":battle_last==0?"勇士攻击":battle_last==1?"怪物反击":battle_enemy.special==11?"战前吸血":"战前伤害",GOLD);
        if(battle_last!=3){text(272,183,"-",RED);number(284,183,battle_amount,RED);}
        if(!battle_hp){text(112,208,"胜利！金币",GOLD);number(180,208,battle_enemy.gold,WHITE);text(253,208,"经验",GOLD);number(284,208,battle_enemy.exp,WHITE);}
        button(110,232,260,26,"确定：快速结束战斗",0);
    }
    if(mode==DIALOG){
        const StoryPage *p=&dialog_pages[dialog_page];
        rect(DIALOG_X,DIALOG_Y,DIALOG_W,DIALOG_H,DARK);border(DIALOG_X,DIALOG_Y,DIALOG_W,DIALOG_H,GOLD);
        sprite(106,112,p->portrait);text(139,117,p->speaker,GOLD);right_number(335,117,dialog_page+1,MUTED);text(338,117,"/",MUTED);number(348,117,dialog_count,MUTED);
        text(108,153,p->line1,WHITE);text(108,176,p->line2,WHITE);text(108,199,p->line3,WHITE);
        button(106,232,268,25,"确定 / 点击：继续",1);
    }
}
static void book(void){if(g.flags&BOOKFLAG){mode=BOOK;bookpage=0;}else notice="先找到一层圣光徽";}
static void fly(void){if((g.flags&FLYFLAG)&&g.floor<21){mode=FLY;selection=g.floor;}else notice="需要风之罗盘，21层起无法传送";}
static void action(int key){
    dirty=1;
    if(mode==SLOTS){
        int count=storage_saving?SAVE_SLOTS:SAVE_SLOTS+1;
        if(key==4){mode=storage_return;selection=mode==TITLE?1:0;notice="";return;}
        if(key==0&&selection<SAVE_SLOTS&&!(selection&1))selection++;
        if(key==1&&selection<SAVE_SLOTS&&(selection&1))selection--;
        if(key==2)selection=(selection+1)%count;if(key==3)selection=(selection+count-1)%count;
        if(key==5){if(!storage_saving&&!slot_valid[selection]){notice="此位置没有有效存档";return;}confirm_yes=0;mode=SAVE_CONFIRM;notice="";}return;
    }
    if(mode==SAVE_CONFIRM){
        if(key==4){mode=SLOTS;return;}
        if(key<4)confirm_yes=!confirm_yes;
        if(key==5){
            if(!confirm_yes){mode=SLOTS;return;}
            if(storage_saving){if(save_slot(selection)){if(storage_after<0){quit_requested=1;mode=PLAY;}else {mode=storage_after;if(mode==TITLE){selection=1;notice="";}}}else {scan_slots();mode=SLOTS;}}
            else if(read_slot(selection,&save_buffer)){g=save_buffer;if(selection<SAVE_SLOTS)active_slot=selection;mode=PLAY;notice=selection==SAVE_SLOTS?"旧档已读取，请保存到新目录":"进度已读取";hero_frame=0;repeat_direction=0;}
            else {mode=SLOTS;notice="读取失败，当前进度保留";scan_slots();}
        }return;
    }
    if(mode==BATTLE){if(key==5)finish_battle();return;}
    if(mode==ABOUT){if(key==4||key==5){mode=TITLE;selection=3;}return;}
    if(mode==DIALOG){if(key==5&&++dialog_page>=dialog_count){mode=dialog_return;selection=0;repeat_direction=0;}return;}
    if(mode==TITLE){
        if(key==2)selection=(selection+1)%5;if(key==3)selection=(selection+4)%5;
        if(key==5){if(selection==0){new_game();hero_frame=0;story_page=0;mode=OPENING;}
            else if(selection==1)open_storage(0,PLAY);
            else if(selection==2){help_return=TITLE;mode=HELP;}
            else if(selection==3)mode=ABOUT;else quit_requested=1;}
        return;
    }
    if(mode==OPENING){if(key==4||(key==5&&++story_page>=PAGE_COUNT(opening_pages))){mode=PLAY;notice="方向键移动，确定打开菜单";}return;}
    if(mode==WIN){if(key==5){if(story_page++==1){mode=TITLE;selection=0;notice="";}}return;}
    if(mode==HELP&&(key==4||key==5)){mode=help_return;return;}
    if(key==4){if(mode==PLAY){mode=MENU;selection=0;}else mode=PLAY;return;}
    if(mode==PLAY){if(key<4){static const int dirs[]={2,1,0,3};hero_direction=dirs[key];}if(key==0)walk(1,0);if(key==1)walk(-1,0);if(key==2)walk(0,1);if(key==3)walk(0,-1);if(key==5){mode=MENU;selection=0;}return;}
    if(mode==MENU){if(key==0&&selection<5)selection=selection+5>8?8:selection+5;if(key==1&&selection>=5)selection-=5;if(key==2)selection=(selection+1)%9;if(key==3)selection=(selection+8)%9;if(key==5){switch(selection){case 0:mode=PLAY;break;case 1:book();break;case 2:fly();break;case 3:open_storage(1,PLAY);break;case 4:open_storage(0,PLAY);break;case 5:help_return=PLAY;mode=HELP;break;case 6:mode=RESTART;break;case 7:open_storage(1,TITLE);break;case 8:open_storage(1,-1);break;}}return;}
    if(mode==SHOP){if(key==2)selection=(selection+1)%3;if(key==3)selection=(selection+2)%3;if(key==5)buy(selection);return;}
    if(mode==BOOK){int seen[300],i,n=0;for(i=0;i<300;i++)seen[i]=0;for(i=0;i<121;i++){int t=g.map[g.floor][i];if(t>=201&&t<300&&enemy_base[t].hp)seen[t]=1;}for(i=201;i<300;i++)n+=seen[i];if((key==0||key==2)&&(bookpage+1)*3<n)bookpage++;if((key==1||key==3)&&bookpage>0)bookpage--;if(key==5)mode=PLAY;return;}
    if(mode==FLY){if(key==2||key==3){int d=key==3?1:-1,i=selection+d;while(i>=0&&i<21){if(g.visited&(1u<<i)){selection=i;break;}i+=d;}}if(key==5){if(fly_to(selection)){mode=PLAY;hero_frame=0;repeat_direction=0;}else notice="无法传送到该位置";}return;}
    if(key==5){if(mode==RESTART){new_game();story_page=0;mode=OPENING;}else if(mode==WIN){mode=MENU;selection=0;}else mode=PLAY;}
}
static int hit(int x,int y,int left,int top,int width,int height){return x>=left&&x<left+width&&y>=top&&y<top+height;}
static void touch(u32 packed){
    int x=packed&65535,y=packed>>16,i;
    if(x>=SCREEN_W||y>=SCREEN_H)return;
    if(mode==SLOTS){
        for(i=0;i<SAVE_SLOTS;i++)if(hit(x,y,SLOT_X+(i%2)*SLOT_STEP_X,SLOT_Y+(i/2)*SLOT_STEP_Y,SLOT_W,SLOT_H)){selection=i;action(5);break;}
        if(!storage_saving&&hit(x,y,32,190,416,34)){selection=SAVE_SLOTS;action(5);}
        else if(hit(x,y,352,234,96,26))action(4);
    }else if(mode==SAVE_CONFIRM){
        if(hit(x,y,CANCEL_X,CONFIRM_Y,CONFIRM_W,34)){confirm_yes=0;action(5);}
        else if(hit(x,y,ACCEPT_X,CONFIRM_Y,CONFIRM_W,34)){confirm_yes=1;action(5);}
    }else if(mode==BATTLE){if(hit(x,y,110,232,260,26))action(5);}
    else if(mode==DIALOG){if(hit(x,y,DIALOG_X,DIALOG_Y,DIALOG_W,DIALOG_H))action(5);}
    else if(mode==TITLE){
        for(i=0;i<5;i++)if(hit(x,y,TITLE_X,TITLE_Y+i*TITLE_STEP,TITLE_W,28)){selection=i;action(5);break;}
    }else if(mode==OPENING||mode==WIN){
        if(hit(x,y,238,194,218,34))action(5);
        else if(mode==OPENING&&hit(x,y,238,234,218,25))action(4);
    }else if(mode==PLAY){
        if(hit(x,y,SIDE_X,TOOL_Y,SIDE_W,TOOL_H)){mode=MENU;selection=0;}
        else if(hit(x,y,SIDE_X,TOOL_Y+TOOL_STEP,SIDE_W,TOOL_H))book();
        else if(hit(x,y,SIDE_X,TOOL_Y+TOOL_STEP*2,SIDE_W,TOOL_H))fly();
        else if(hit(x,y,MAP_X,MAP_Y,MAP_SIZE,MAP_SIZE)){
            int dx=(x-MAP_X)/TILE_SIZE-g.x,dy=(y-MAP_Y)/TILE_SIZE-g.y;
            if(dx*dx+dy*dy==1){hero_direction=dy<0?3:dy>0?0:dx<0?1:2;walk(dx,dy);}
        }
    }else if(mode==MENU){
        for(i=0;i<9;i++)if(hit(x,y,MENU_X+(i/5)*MENU_STEP_X,MENU_Y+(i%5)*MENU_STEP_Y,MENU_W,32)){selection=i;action(5);break;}
        if(hit(x,y,304,234,72,26))action(4);
    }else if(mode==SHOP){
        for(i=0;i<3;i++)if(hit(x,y,110,58+i*48,260,38)){selection=i;action(5);break;}
        if(hit(x,y,304,234,72,26))action(4);
    }else if(mode==BOOK){
        if(hit(x,y,304,234,72,26))action(4);
        else if(hit(x,y,106,234,62,26))action(1);
        else if(hit(x,y,226,234,62,26))action(0);
    }else if(mode==FLY){
        if(hit(x,y,200,80,80,34))action(3);
        else if(hit(x,y,200,163,80,34))action(2);
        else if(hit(x,y,110,219,166,34))action(5);
        else if(hit(x,y,290,219,80,34))action(4);
    }else if(mode==RESTART){
        if(hit(x,y,246,189,124,38))action(5);
        else if(hit(x,y,110,189,124,38)||hit(x,y,304,234,72,26))action(4);
    }else if(mode==HELP){if(hit(x,y,304,234,72,26))action(4);}
    else if(mode==ABOUT){if(hit(x,y,164,220,152,32))action(4);}
    dirty=1;
}
#include "h1_main.h"
