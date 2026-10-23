/* Included after the portable game/UI so host tests exercise the same code. */
#ifndef MOTA_HOST_TEST
volatile u32 mota_diagnostic[16];
/* SDK packs BSS into .data. A live initialized word makes the whole output
   section PROGBITS, so every new BDA load resets the game/UI state to zero. */
volatile u32 mota_port_version=0x00010000u;
__attribute__((section(".h1_noinit.stack"),aligned(16),used))
u8 mota_stack[32768];
static int present(void){
    int ok;render();ok=platform_present(screen);dirty=0;
    mota_diagnostic[2]++;return ok;
}
int h1_app_main(void){
    u8 held[45]={0};int pen_down=0,pen_valid=0,pen_x=0,pen_y=0;
    u32 second_tick,animation_tick=0;int result=0,i;
    new_game();mode=TITLE;selection=story_page=0;notice="";
    help_return=PLAY;hero_direction=3;animation_frame=0;hero_frame=hero_stride=0;
    input_now=hero_step_tick=0;repeat_direction=repeat_due=0;quit_requested=0;
    active_slot=0;last_notice=0;notice_started=0;dirty=1;
    mota_diagnostic[0]=0x4d4f5441u;mota_diagnostic[1]=1;mota_diagnostic[15]=mota_port_version;
    if(!platform_open())return 1;
    h1_timer_start();second_tick=h1_timer_read_ms()/25u;
    if(!present()){result=2;goto done;}
    while(!quit_requested){
        u32 now=h1_timer_read_ms()/25u,cur=0,repeat;
        int count;
        input_now=now;
        for(count=0;count<64&&!quit_requested;count++){
            int code=-1,value=-1,key;
            h1_event_fetch(&code,&value);
            if(code==-1&&value==-1)break;
            if((code==H1_EVENT_KEY_DOWN||code==H1_EVENT_KEY_UP)&&value>=1&&value<=44){
                if(code==H1_EVENT_KEY_UP)held[value]=0;
                else if(!held[value]){
                    held[value]=1;key=platform_key(value);
                    if(key>=0)action(key);
                    else if(value==H1_KEY_B&&mode==PLAY){book();dirty=1;}
                    else if(value==H1_KEY_F&&mode==PLAY){fly();dirty=1;}
                    else if(value==H1_KEY_M&&mode==PLAY)action(5);
                }
            }else if(code==11){if(!pen_down){pen_down=1;pen_valid=0;}}
            else if(code==8){
                if(pen_down){
                    if(platform_pen(&pen_x,&pen_y))pen_valid=1;
                    if(pen_valid){touch(((u32)pen_y<<16)|(u32)pen_x);mota_diagnostic[12]++;}
                }
                pen_down=pen_valid=0;
            }
        }
        if(pen_down&&platform_pen(&pen_x,&pen_y))pen_valid=1;
        for(i=1;i<=44;i++)if(held[i]){int key=platform_key(i);if(key>=0&&key<4)cur|=1u<<key;}
        repeat=direction_repeat(cur,now,mode==PLAY&&!pen_down);
        for(i=0;i<4;i++)if(repeat&(1u<<i)){action(i);break;}
        animate_hero(now);advance_battle(now);
        if(now-second_tick>=40){
            if(mode==PLAY||mode==BATTLE)g.seconds+=(now-second_tick)/40;
            second_tick+=((now-second_tick)/40)*40;
        }
        if(now-animation_tick>=16){animation_tick=now;animation_frame^=1;if(mode==PLAY)dirty=1;}
        mota_diagnostic[1]=mode+16;mota_diagnostic[3]=now;
        mota_diagnostic[4]=g.floor;mota_diagnostic[5]=g.x;mota_diagnostic[6]=g.y;
        mota_diagnostic[7]=g.hp;mota_diagnostic[8]=selection;
        mota_diagnostic[9]=g.flags;mota_diagnostic[10]=g.seconds;
        mota_diagnostic[11]=quit_requested;mota_diagnostic[13]=sizeof(Game);
        if(dirty&&!present()){result=2;break;}
    }
done:
    h1_timer_stop();platform_close();mota_diagnostic[1]=result?255:9;
    return result;
}
#endif
