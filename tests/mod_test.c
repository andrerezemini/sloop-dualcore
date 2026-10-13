/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static void slot(track_t *t,int k,int src,int dst,int amt) {
    t->p[P_M1SRC+3*k]=src; t->p[P_M1DST+3*k]=dst; t->p[P_M1AMT+3*k]=amt;
}
static void midi(uint32_t st,uint32_t d1,uint32_t d2) {
    midi_in_q[mi_w++%MQ]=((st&0xf0u)>>4)|(st<<8)|(d1<<16)|(d2<<24);
    events_block(CTL);
}
int main(void) {
    int32_t out[CTL]; int16_t base[P_COUNT];
    host_tracks_init(); for(int i=0;i<NPART;i++)host_preset(&trk[i],0,0);
    track_t *t=&trk[2];
    /* Actual MIDI dispatcher, per-channel, selected-channel and CLOCK routes. */
    midi(0xB2,1,90); midi(0xD2,65,0); midi(0xB2,11,30);
    assert(t->mw==90&&t->at==65&&t->ex_off==97&&trk[0].mw==0);
    midi(0xB2,121,0); assert(!(t->mw|t->at|t->ex_off));
    song.g[G_ROUTE]=1; midi(0xD2,100,0); assert(t->at==0);
    song.g[G_ROUTE]=4; song.sel=1; midi(0xD2,70,0); assert(trk[1].at==70&&t->at==0);
    song.g[G_ROUTE]=0;
    /* Shadow never restores over a UI edit; duplicate destinations add then clamp. */
    t->mw=127; t->p[P_PAN]=0; slot(t,0,MS_MODW,MD_PAN,10);slot(t,1,MS_MODW,MD_PAN,10);
    memcpy(base,t->p,sizeof base);mod_begin(t);
    assert(render_p(t)[P_PAN]>30&&render_p(t)[P_PAN]<42&&!memcmp(base,t->p,sizeof base));
    t->p[P_PAN]=-23; track_render(t,out,CTL); t->rp=0; assert(t->p[P_PAN]==-23);
    /* Enum engine selectors never become matrix destinations. */
    memset(t->p+P_M1SRC,0,12*sizeof(int16_t));slot(t,0,MS_MODW,MD_E1,63);
    int wave=t->p[P_E0];mod_begin(t);assert(render_p(t)[P_E0]==wave);t->rp=0;
    slot(t,0,MS_MODW,MD_E1+4,63); t->p[P_E4]=60;mod_begin(t);assert(render_p(t)[P_E4]==127);t->rp=0;
    /* Dynamic descriptors must use the fading sound's model, never the new UI model. */
    host_preset(t,3,0);t->p[P_E1]=RW_WRAM;t->p[P_E2]=32;
    slot(t,0,MS_MODW,MD_E1+2,63);mod_begin(t);assert(render_p(t)[P_E2]==32);t->rp=0;
    memcpy(t->p_old,t->p,sizeof t->p);memcpy(t->pe_old,t->p+P_E0,sizeof t->pe_old);
    t->pold=t->xf_on=1;t->xf=8;t->p[P_E1]=0;
    mod_begin(t);assert(render_p(t)[P_E2]==32);t->rp=0;t->pold=t->xf_on=t->xf=0;
    host_preset(t,0,0);
    /* Notes retain their own random/velocity values; another part never changes their stream. */
    memset(t->p+P_M1SRC,0,12*sizeof(int16_t));
    mod_note(t,60,100);uint32_t seed=t->mod_seed; mod_note(&trk[0],61,80); assert(t->mod_seed==seed);
    trk_note_on(t,60,90);voice_t *v=0;for(int i=0;i<NVOICE;i++)if(t->v[i].active)v=&t->v[i];assert(v&&v->mvel==90);
    slot(t,0,MS_MODW,MD_PITCH,63);slot(t,1,MS_MODW,MD_CUT,63);slot(t,2,MS_MODW,MD_SHP,-64);slot(t,3,MS_EXPR,MD_AMP,-64);
    vmod_t m={.pitch16=960,.inc=pitch_inc(960),.amp0=12000,.amp1=12000,.shape=16384,.envq15=16000};
    mod_begin(t);mod_voice(t,v,&m,0);assert(m.pitch16>1100&&m.cutoff>15000&&m.shape<1000&&m.amp1<10);t->rp=0;
    memset(t->p+P_M1SRC,0,12*sizeof(int16_t));slot(t,0,MS_MODW,MD_VEL,-64);assert(mod_note(t,60,100)==1);
    slot(t,0,MS_MODW,MD_TIME,63);assert(mod_time(t,10000)>19000);
    slot(t,0,MS_MODW,MD_TIME,0);assert(mod_time(t,10000)==10000);
    /* ARP TIME sees the note it just started, including its KEY/RAND/VEL. */
    host_preset(t,0,0);memset(t->p+P_M1SRC,0,12*sizeof(int16_t));
    slot(t,0,MS_KEY,MD_TIME,63);t->m_key=0;t->p[P_AMODE]=1;t->p[P_APROB]=127;
    t->nheld=1;t->held[0]=84;t->arp_new=1;t->arp_note=0;song.playing=0;
    arp_tick(t,0);
    assert(t->m_key==84&&t->arp_note==84);
    assert(t->arp_off==mod_time(t,div_units(t->p[P_ARATE]%NDIV_SHORT)*(uint32_t)t->p[P_AGATE]/128u));
    t->p[P_AMODE]=0;t->nheld=0;
    /* RATE0 LFO2 is neutral; every waveform and sync setting stays Q15 bounded. */
    for(int w=0;w<5;w++)for(int sy=0;sy<11;sy++) {
        t->p[P_L2RATE]=60;t->p[P_L2WAVE]=w;t->p[P_L2SYNC]=sy;
        for(int b=0;b<2000;b++){track_lfo2_tick(t);assert(t->lfo2_val>=-32768&&t->lfo2_val<=32767);}
    }
    t->p[P_L2RATE]=0;track_lfo2_tick(t);assert(t->lfo2_val==0);
    /* The legacy preset fade also uses a shadow, even with the matrix off. */
    memset(t->p+P_M1SRC,0,12*sizeof(int16_t));memcpy(t->p_old,t->p,sizeof t->p);memcpy(t->pe_old,t->p+P_E0,sizeof t->pe_old);
    t->pold=t->xf_on=1;t->xf=8;t->p[P_E4]=12;mod_begin(t);assert(render_p(t)[P_E4]==t->pe_old[4]);
    t->p[P_E4]=22;track_render(t,out,CTL);t->rp=0;assert(t->p[P_E4]==22);
    assert(p_lockable(P_M1AMT)&&p_lockable(P_M4AMT)&&!p_lockable(P_M1SRC)&&p_lockable(P_TRES));
    /* A fully bypassed LP24 must not revive old/self-oscillating channel state. */
    t->rp=0;t->xf=t->xf_on=t->pold=0;t->p[P_TFTYPE]=1;t->p[P_TRES]=87;
    djf_t f={0};tsvf_t fc;
    for(int ch=0;ch<4;ch++) {
        f.mode=0;assert(djf_block(&f,-45,&fc));
        for(int i=0;i<CTL;i++)out[i]=16000;
        track_filter_run(t,&f,&fc,out,CTL,ch);
        assert(!(f.seed&(1u<<ch))&&track_ladder_on[2]&(1u<<ch));
        while(djf_block(&f,0,&fc))track_filter_run(t,&f,&fc,out,CTL,ch);
        assert(f.mode==0);
        for(int i=0;i<4;i++)track_ladder[2][ch].s[i]=123456;
        assert(djf_block(&f,-45,&fc));memset(out,0,sizeof out);
        track_filter_run(t,&f,&fc,out,CTL,ch);
        for(int i=0;i<CTL;i++)assert(out[i]==0);
        for(int i=0;i<4;i++)assert(track_ladder[2][ch].s[i]==0);
    }
    for(uint32_t e=0;e<NENGINES;e++)for(uint32_t p=0;p<8;p++) assert(ENGINES[e]->edit[p].min>=-128&&ENGINES[e]->edit[p].max<=127);
    puts("synth modulation: MIDI routes, four slots, immutable shadows, enum guards, per-part RNG, voice/event destinations, LFO2 and fades PASS");
}
