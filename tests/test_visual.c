/* These images come from the same renderer compiled into the device BDA. */
#include <assert.h>
#include <stdio.h>
#include "../src/mota24.c"

static unsigned short first[480 * 272];
static int rendered_count;

static void capture(const char *directory,const char *name){
    char path[1024];FILE *file;int i,changed=0;
    assert(sizeof(screen)==480u*272u*sizeof(unsigned short));
    for(i=0;i<480*272;i++)screen[i]=0x1234;
    render();
    for(i=0;i<480*272;i++)changed+=screen[i]!=0x1234;
    assert(changed==480*272); /* Every screen is freshly composed. */
    snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
    file=fopen(path,"wb");assert(file);
    fprintf(file,"P6\n480 272\n255\n");
    for(i=0;i<480*272;i++){
        unsigned short c=screen[i];unsigned char rgb[3];
        rgb[0]=(unsigned char)(((c>>11)&31)*255/31);
        rgb[1]=(unsigned char)(((c>>5)&63)*255/63);
        rgb[2]=(unsigned char)((c&31)*255/31);
        assert(fwrite(rgb,1,3,file)==3);
    }
    assert(fclose(file)==0);rendered_count++;
}

int main(int argc,char **argv){
    int i,j;const char *output=argc>1?argv[1]:"reports/screenshots";
    const int fixed[]={0,1,3,6,7,8,21,22,23,27,28,31,32,81,82,83,85,86,87,88};
    for(i=0;i<(int)(sizeof(fixed)/sizeof(fixed[0]));i++)
        for(j=0;j<(int)(sizeof(sprite_pixels[0])/sizeof(sprite_pixels[0][0]));j++)
            assert(sprite_pixels[sprite_index[fixed[i]]*2][j]==sprite_pixels[sprite_index[fixed[i]]*2+1][j]);
    new_game();mode=TITLE;selection=0;notice="";capture(output,"01-title");
    mode=OPENING;story_page=0;capture(output,"02-opening");
    mode=ABOUT;capture(output,"03-about");
    mode=HELP;help_return=TITLE;capture(output,"04-help-title");
    new_game();capture(output,"05-start-map");
    g.floor=1;g.flags|=BOOKFLAG|FLYFLAG;g.visited=(1u<<21)-1;
    capture(output,"06-floor-one");
    mode=MENU;selection=8;capture(output,"07-adventure-menu");
    mode=HELP;help_return=PLAY;capture(output,"08-help-map");
    mode=BOOK;bookpage=0;capture(output,"09-monster-book");
    g.floor=12;g.hp=1001;g.atk=g.def=1000;bookpage=-1;
    {int seen[300]={0},count=0;
        for(i=0;i<121;i++){int t=g.map[g.floor][i];if(t>=201&&t<300&&enemy_base[t].hp)seen[t]=1;}
        for(i=201;i<300;i++)if(seen[i]){if(enemy(i).special==11){bookpage=count/3;break;}count++;}
    }
    assert(bookpage>=0);capture(output,"10-special-monsters");
    mode=FLY;selection=12;capture(output,"11-floor-compass");
    mode=SHOP;shop=0;selection=1;capture(output,"12-gold-shop");
    mode=SHOP;shop=4;selection=2;capture(output,"13-key-shop");
    mode=RESTART;capture(output,"14-restart-confirm");
    mode=WIN;g.floor=26;story_page=0;capture(output,"15-hidden-ending");
    story_page=1;capture(output,"16-ending-statistics");
    mode=SLOTS;storage_saving=1;selection=5;
    for(i=0;i<7;i++){
        slot_valid[i]=i!=1;slot_floor[i]=i*4;slot_minutes[i]=i*19;slot_hp[i]=1000+i*1234;
    }
    capture(output,"17-save-slots");
    storage_saving=0;selection=6;capture(output,"18-load-slots-legacy");
    mode=SAVE_CONFIRM;storage_saving=1;selection=0;confirm_yes=0;
    capture(output,"19-save-confirm-cancel");
    storage_saving=0;confirm_yes=1;capture(output,"20-load-confirm");
    new_game();input_now=100;walk(0,-1);assert(mode==DIALOG);
    capture(output,"21-fairy-dialogue");
    new_game();g.floor=1;g.x=4;g.y=5;g.hp=10000;g.atk=20;g.map[1][60]=201;
    input_now=0;walk(1,0);assert(mode==BATTLE);capture(output,"22-battle-ready");
    advance_battle(20);capture(output,"23-battle-turn");
    new_game();g.floor=3;g.x=5;g.y=1;g.hp=1234567;g.atk=12345;g.def=23456;g.gold=1234567;g.exp=9876543;
    g.keys[0]=123;g.keys[1]=234;g.keys[2]=345;g.flags|=BOOKFLAG|FLYFLAG;
    capture(output,"24-large-statistics");
    animation_frame=0;render();
    for(i=0;i<480*272;i++)first[i]=screen[i];
    animation_frame=1;render();
    /* Independent animation frames must leave the status sidebar unchanged. */
    for(i=0;i<SCREEN_H;i++)for(j=SIDE_X;j<SCREEN_W;j++)assert(first[i*SCREEN_W+j]==screen[i*SCREEN_W+j]);
    printf("PASS visual: %d actual 480x272 screens exported; static assets and sidebar stable\n",rendered_count);
    return 0;
}
