/* Host UI/action regression. It does not execute the H1 event queue or device
 * keyboard loop. input_edges is retained BBK9588 logic, unused by h1_main.h. */
#include <assert.h>
#include <stdio.h>
#define bda_main device_main
#include "../src/mota24.c"
static void tap(int x,int y){touch(((u32)y<<16)|(u32)x);}
static void test_landscape_touch(void){
    int i,edge;
    const int dx[4]={1,-1,0,0},dy[4]={0,0,1,-1};
    assert(SCREEN_W==480&&SCREEN_H==272);
    assert(MAP_SIZE==11*TILE_SIZE&&MAP_X>=0&&MAP_Y>=0);
    assert(MAP_X+MAP_SIZE<=SCREEN_W&&MAP_Y+MAP_SIZE<=SCREEN_H);
    assert(MAP_X+MAP_SIZE<=SIDE_X);
    for(i=0;i<9;i++){
        mode=MENU;selection=i;action(0);assert(selection==(i<5?(i+5>8?8:i+5):i));
        selection=i;action(1);assert(selection==(i>=5?i-5:i));
        selection=i;action(2);assert(selection==(i+1)%9);
        selection=i;action(3);assert(selection==(i+8)%9);
    }
    for(edge=0;edge<2;edge++)for(i=0;i<6+edge;i++){
        int count=6+edge;mode=SLOTS;storage_saving=!edge;
        selection=i;action(0);assert(selection==(i<6&&!(i&1)?i+1:i));
        selection=i;action(1);assert(selection==(i<6&&(i&1)?i-1:i));
        selection=i;action(2);assert(selection==(i+1)%count);
        selection=i;action(3);assert(selection==(i+count-1)%count);
    }
    puts("PASS landscape action navigation: menu columns, paired slots, legacy row and vertical wrap");
    for(i=0;i<4;i++){
        new_game();g.x=g.y=5;set(g.floor,5+dx[i],5+dy[i],0);
        tap(MAP_X+(5+dx[i])*TILE_SIZE+TILE_SIZE/2,MAP_Y+(5+dy[i])*TILE_SIZE+TILE_SIZE/2);
        assert(g.x==5+dx[i]&&g.y==5+dy[i]);
    }
    new_game();g.x=g.y=5;
    tap(MAP_X+7*TILE_SIZE+1,MAP_Y+5*TILE_SIZE+1);assert(g.x==5&&g.y==5);
    action(5);assert(mode==MENU);
    for(edge=0;edge<2;edge++)for(i=0;i<3;i++){
        new_game();g.floor=1;g.flags|=BOOKFLAG|FLYFLAG;
        tap(SIDE_X+(edge?SIDE_W-1:0),TOOL_Y+i*TOOL_STEP+(edge?TOOL_H-1:0));
        assert(mode==(i==0?MENU:i==1?BOOK:FLY));
    }
    new_game();mode=TITLE;
    tap(TITLE_X+TITLE_W-1,TITLE_Y+1);assert(mode==OPENING);
    mode=TITLE;tap(TITLE_X+1,TITLE_Y+TITLE_STEP*2+1);assert(mode==HELP&&help_return==TITLE);
    action(4);assert(mode==TITLE);
    new_game();g.flags|=BOOKFLAG|FLYFLAG;mode=MENU;
    tap(MENU_X+MENU_STEP_X+1,MENU_Y+1);assert(mode==HELP&&help_return==PLAY);
    action(4);assert(mode==PLAY);mode=MENU;
    tap(MENU_X+1,MENU_Y+MENU_STEP_Y+1);assert(mode==BOOK);
    for(i=0;i<6;i++){
        mode=SLOTS;storage_saving=1;storage_return=MENU;storage_after=PLAY;
        tap(SLOT_X+(i%2)*SLOT_STEP_X+1,SLOT_Y+(i/2)*SLOT_STEP_Y+1);
        assert(mode==SAVE_CONFIRM&&selection==i&&!confirm_yes);
        tap(CANCEL_X+CONFIRM_W/2,CONFIRM_Y+10);assert(mode==SLOTS);
    }
    new_game();input_now=100;walk(0,-1);assert(mode==DIALOG);
    tap(DIALOG_X-1,DIALOG_Y+10);assert(dialog_page==0&&mode==DIALOG);
    tap(DIALOG_X+DIALOG_W-1,DIALOG_Y+DIALOG_H-1);assert(dialog_page==1);
    new_game();g.floor=1;g.flags|=BOOKFLAG|FLYFLAG;g.visited=(1u<<1)|(1u<<3)|(1u<<9);book();
    tap(287,259);assert(mode==BOOK&&bookpage==1);
    tap(106,234);assert(bookpage==0);
    tap(375,259);assert(mode==PLAY);
    fly();tap(240,97);assert(selection==3);tap(240,180);assert(selection==1);
    tap(193,236);assert(mode==PLAY&&g.floor==1);
    fly();tap(330,236);assert(mode==PLAY);
    g.gold=1000;open_shop(0);{int hp=g.hp;tap(240,77);assert(g.hp==hp+800&&g.gold==975);}
    tap(340,247);assert(mode==PLAY);
    mode=RESTART;tap(172,208);assert(mode==PLAY);
    mode=RESTART;tap(308,208);assert(mode==OPENING);
    tap(347,211);assert(story_page==1);tap(347,246);assert(mode==PLAY);
    mode=ABOUT;tap(240,236);assert(mode==TITLE);
    mode=HELP;help_return=PLAY;tap(340,247);assert(mode==PLAY);
    mode=SLOTS;storage_saving=0;slot_valid[SAVE_SLOTS]=1;
    tap(240,207);assert(mode==SAVE_CONFIRM&&selection==SAVE_SLOTS&&!confirm_yes);
    tap(CANCEL_X+1,CONFIRM_Y+1);assert(mode==SLOTS);
    new_game();g.floor=1;g.x=4;g.y=5;g.hp=10000;g.atk=20;set(1,5,5,201);
    input_now=0;walk(1,0);assert(mode==BATTLE);
    tap(109,245);assert(mode==BATTLE);tap(369,257);assert(mode==PLAY&&g.x==5);
    puts("PASS H1 touch: complete map, four adjacent directions, toolbar edges, two-column menu, six slots and dialogue");
    puts("PASS H1 modal touch: book pages, compass, shop, restart/cancel, opening/skip, help/about, legacy import and battle skip");
}
int main(void){
    int i;
    assert(direction_repeat(1,0,1)==0);
    assert(direction_repeat(1,11,1)==0);
    assert(direction_repeat(1,12,1)==1);
    assert(direction_repeat(1,16,1)==0);
    assert(direction_repeat(1,17,1)==1);
    assert(direction_repeat(0,18,1)==0);
    assert(direction_repeat(0,100,1)==0);
    assert(direction_repeat(2,101,1)==0);
    assert(direction_repeat(2,113,1)==2);
    assert(direction_repeat(4,114,1)==0);
    assert(direction_repeat(4,126,0)==0);
    assert(direction_repeat(16,150,1)==0);
    assert(direction_repeat(32,170,1)==0);
    assert(direction_repeat(3,190,1)==0);
    puts("PASS direction repeat: delay, rate, release, direction change, modal gating, no ESC/Enter repeat");
    new_game();g.x=5;g.y=5;g.map[g.floor][5*11+6]=0;g.map[g.floor][5*11+7]=0;
    input_now=200;walk(1,0);assert(g.x==6&&hero_frame==1);
    animate_hero(204);assert(hero_frame==1);
    input_now=205;walk(1,0);assert(g.x==7&&hero_frame==3);
    animate_hero(210);assert(hero_frame==0);
    g.map[g.floor][5*11+8]=1;input_now=211;walk(1,0);assert(g.x==7&&hero_frame==0);
    puts("PASS walking frames: alternate successful steps, stop idle, wall does not animate");
    const u32 esc=1u<<4;
    previous=0;touch_active=touch_escape_suppressed=escape_pending=0;
    assert(input_edges(esc,0)==0);
    assert(input_edges(0,4)==esc); /* Real short press is preserved. */
    touch_active=1;suppress_touch_escape();
    for(i=5;i<100;i++)assert(!(input_edges(esc,i)&esc));
    touch_active=0;suppress_touch_escape();
    for(i=100;i<200;i++)assert(!(input_edges(esc,i)&esc));
    assert(input_edges(esc|1,200)==1); /* Other keys are not masked. */
    assert(input_edges(0,201)==0&&!touch_escape_suppressed);
    assert(input_edges(esc,202)==0);
    assert(input_edges(esc,206)==esc);
    assert(input_edges(esc,210)==0); /* Held real key fires once. */
    input_edges(0,211);
    assert(input_edges(esc,212)==0);
    touch_active=1;suppress_touch_escape(); /* Packet precedes touch message. */
    assert(input_edges(esc,216)==0);
    touch_active=0;suppress_touch_escape();input_edges(0,217);
    assert(input_edges(esc,218)==0&&input_edges(0,222)==esc);
    puts("PASS retained BBK9588 input_edges helper; this is not a test of the H1 device event loop");
    mode=MENU;touch((80u<<16)|80u);assert(mode==MENU);
    new_game();book();assert(mode==PLAY);fly();assert(mode==PLAY);
    g.flags|=BOOKFLAG|FLYFLAG;g.floor=1;
    book();assert(mode==BOOK&&bookpage==0);
    for(i=0;i<30;i++)action(0);
    assert(bookpage==2);render();
    for(i=0;i<30;i++)action(1);
    assert(bookpage==0);action(4);assert(mode==PLAY);
    g.visited=(1u<<0)|(1u<<1)|(1u<<9);fly();assert(mode==FLY&&selection==1);
    action(3);assert(selection==9);action(3);assert(selection==9);
    action(2);assert(selection==1);action(5);assert(mode==PLAY&&g.floor==1);
    new_game();g.flags|=FLYFLAG;g.visited=(1u<<21)-1;
    fly();selection=1;action(5);assert(mode==PLAY&&g.floor==1&&g.x==5&&g.y==9);
    fly();selection=3;action(5);assert(g.floor==3&&g.x==1&&g.y==10);
    g.floor=9;fly();selection=3;action(5);assert(g.floor==3&&g.x==10&&g.y==9);
    g.x=6;g.y=5;fly();action(5);assert(mode==PLAY&&g.x==6&&g.y==5);
    puts("PASS compass UI: ascending/descending entry and same-floor no-op");
    g.floor=21;fly();assert(mode==PLAY);
    for(i=0;i<6;i++){
        int hp,atk,def;
        new_game();g.gold=g.exp=10000;g.keys[0]=g.keys[1]=g.keys[2]=3;
        hp=g.hp;atk=g.atk;def=g.def;open_shop(i);render();
        action(5);action(2);action(5);action(2);action(5);
        if(i<4)assert(g.hp>hp&&g.atk>atk&&g.def>def);
        if(i==4)assert(g.keys[0]==4&&g.keys[1]==4&&g.keys[2]==4&&g.gold==9840);
        if(i==5)assert(g.keys[0]==2&&g.keys[1]==2&&g.keys[2]==2&&g.gold==10112);
        action(4);assert(mode==PLAY);
    }
    new_game();action(5);assert(mode==MENU);for(i=0;i<6;i++)action(2);
    action(5);assert(mode==RESTART);action(4);assert(mode==PLAY);
    g.floor=8;action(5);for(i=0;i<6;i++)action(2);action(5);action(5);
    assert(g.floor==0&&mode==OPENING);action(4);assert(mode==PLAY);
    mode=TITLE;selection=0;action(5);assert(mode==OPENING&&story_page==0);
    action(5);assert(mode==OPENING&&story_page==1);for(i=1;i<PAGE_COUNT(opening_pages);i++)action(5);assert(mode==PLAY);
    mode=TITLE;selection=2;action(5);assert(mode==HELP);action(4);assert(mode==TITLE);
    selection=0;tap(TITLE_X+TITLE_W/2,TITLE_Y+TITLE_STEP/2);assert(mode==OPENING);action(4);assert(mode==PLAY);
    g.floor=26;g.x=4;g.y=5;g.atk=g.def=g.hp=1000000;g.map[26][5*11+5]=257;
    walk(1,0);assert(mode==BATTLE);finish_battle();assert(mode==WIN&&story_page==0);action(5);assert(mode==WIN&&story_page==1);
    action(5);assert(mode==TITLE);action(4);assert(mode==TITLE);
    puts("PASS title/opening/help/skip and actual final-enemy ending transition");
    new_game();input_now=100;walk(0,-1);assert(mode==DIALOG&&dialog_count==16);
    assert(g.keys[0]==1&&g.keys[1]==1&&g.keys[2]==1);
    {int x=g.x,y=g.y;action(3);action(4);assert(mode==DIALOG&&g.x==x&&g.y==y&&dialog_page==0);}
    for(i=0;i<16;i++)action(5);assert(mode==PLAY);
    g.x=4;g.y=9;walk(0,-1);assert(dialog_pages==fairy_reminder&&g.keys[0]==1);
    for(i=0;i<dialog_count;i++)action(5);
    g.flags|=ICE;walk(0,-1);assert(dialog_pages==fairy_ice&&(g.flags&SECRET));
    for(i=0;i<dialog_count;i++)action(5);
    g.flags|=CROSS;g.hp=900;walk(0,-1);assert(dialog_pages==fairy_bless&&g.hp==1200&&(g.flags&BLESS));
    for(i=0;i<dialog_count;i++)action(5);walk(0,-1);assert(g.hp==1200&&mode==PLAY);
    g.floor=22;g.x=5;g.y=5;g.map[22][4*11+5]=124;walk(0,-1);assert(dialog_pages==fairy_hidden);
    for(i=0;i<dialog_count;i++)action(5);
    g.flags|=FIRE|HEART;walk(0,-1);assert(dialog_pages==fairy_unseal&&(g.flags&BLOOD));
    for(i=0;i<dialog_count;i++)action(5);
    puts("PASS fairy: first/revisit/ice/cross/hidden/unseal, dialogue input lock and single rewards");

    new_game();g.floor=4;g.x=4;g.y=5;set(4,5,5,123);walk(1,0);assert(mode==DIALOG&&dialog_pages==thief_first&&(g.flags&THIEF));
    for(i=0;i<dialog_count;i++)action(5);walk(1,0);assert(dialog_pages==thief_wait);
    for(i=0;i<dialog_count;i++)action(5);g.flags|=HAMMER;walk(1,0);assert(dialog_pages==thief_done&&tile(4,5,5)==0);
    for(i=0;i<dialog_count;i++)action(5);walk(1,0);assert(mode==PLAY);
    new_game();g.floor=2;g.x=4;g.y=5;set(2,5,5,121);walk(1,0);assert(mode==DIALOG&&g.atk==80);action(5);walk(1,0);assert(g.atk==80);
    new_game();g.floor=3;g.x=4;g.y=5;set(3,5,5,131);walk(1,0);assert(mode==DIALOG&&dialog_return==SHOP);action(5);assert(mode==SHOP);
    new_game();g.floor=18;g.x=4;g.y=5;set(18,5,5,132);walk(1,0);assert(mode==DIALOG&&dialog_pages==princess_dialog&&(g.flags&PRINCESS));
    puts("PASS NPC dialogs: thief quest states, once-only prisoner rewards, shop return, princess");

    for(i=PLAY;i<=OPENING;i++){mode=i;selection=0;render();}
    puts("PASS UI: book gates/pagination, fly visited bounds, six shops, restart/cancel, all panel rendering");
    test_landscape_touch();
    return 0;
}
