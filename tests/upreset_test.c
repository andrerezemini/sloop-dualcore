/* SPDX-License-Identifier: GPL-3.0-only */
/* v1 -> v2 migration, byte-packed records, parser and A/B power-loss tests. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __attribute__(x)
#define UP_HOST 1
#include "../firmware/src/core.h"
static uint8_t nor[0x100000];
static int fail_after = -1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst,nor+off,n); return 0; }
static int st_erase(uint32_t off) { memset(nor+off,255,4096); return 0; }
static int st_prog(uint32_t off,const void *src,uint32_t n) {
    const uint8_t *p=src;
    for(uint32_t i=0;i<n;i++) { if(fail_after==0) return -1; if(fail_after>0) fail_after--; nor[off+i]&=p[i]; }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
static void find_bank(uint32_t b) {
    st_hdr_t h; int c=st_current(OBJ_UPRESET0+b,&h);
    const up_bank_t *p=c<0?0:(const up_bank_t *)(nor+st_sector(OBJ_UPRESET0+b,c)+ST_PAYLOAD_OFF);
    up_bank_xip[b]=p&&up_bank_ok(p,h.len)?p:0;
}
static uint32_t frame(uint8_t *a) {
    uint32_t n=0;
    a[n++]=17; a[n++]=0; memcpy(a+n,"MATRIX",7); n+=7;
    for(uint32_t i=0;i<P_COUNT;i++) { int32_t v=(int32_t)i-64; uint32_t u=(uint32_t)(v+8192); a[n++]=u&127; a[n++]=(u>>7)&127; }
    for(uint32_t i=0;i<16;i++) { a[n++]=i?60+i:0; a[n++]=i==2?4:3; }
    return n;
}
int main(void) {
    uint8_t a[640]; uint32_t n=frame(a),slot=0;
    up_rec_t r,got; up_bank_t bank;
    int16_t values[P_COUNT],def[P_COUNT];
    memset(nor,255,sizeof nor);
    assert(sizeof r==256 && sizeof bank==2568 && sizeof bank<=ST_PAYLOAD_MAX);
    assert(!up_parse(a,n,&r,&slot) && slot==17 && up_valid(&r));
    assert(r.p[P_M1AMT]==P_M1AMT-64 && r.note[0]==0 && r.flags[0]==0 && r.note[2]==0 && r.flags[2]==4);
    assert(up_parse(a,n-1,&got,&slot)==1);
    a[0]=32; assert(up_parse(a,n,&got,&slot)==1); a[0]=17;
    a[1]=NENGINES; assert(up_parse(a,n,&got,&slot)==1); a[1]=0;
    assert(!up_bank_convert(&bank,0)); assert(!up_encode(&bank.r[1],&r));
    assert(!st_save(OBJ_UPRESET0+1,&bank,sizeof bank)); find_bank(1);
    got=*up_rec(17); assert(up_valid(&got) && !memcmp(&got,&r,sizeof r));
    assert(!up_used(16)&&!up_used(18)&&!up_used(0));
    r.p[0]=128; assert(!up_encode(&bank.r[2],&r)); up_decode(&bank,2,&got); assert(got.p[0]==127);     /* clamped, */
    r.p[0]=-129; assert(!up_encode(&bank.r[2],&r)); up_decode(&bank,2,&got); assert(got.p[0]==-128);   /* not refused */
    r.p[0]=-128; assert(!up_encode(&bank.r[2],&r)); up_decode(&bank,2,&got); assert(got.p[0]==-128);
    {   /* map by count (SLOOP 1.0 .. 2.6 records): ids below the record's count keep their values, the ones a
         * newer firmware added take the defaults, E0..E7 are the record's last eight (up_params) */
        static const uint32_t olds[] = {53u, 58u, 61u, 71u, P_COUNT - 2u};
        for (uint32_t o = 0; o < 5u; o++) {
            uint32_t np = olds[o], nc = np - 8u, i;
            for (i = 0; i < P_COUNT; i++) { def[i] = (int16_t)(1000 + i); r.p[i] = (int16_t)(i < np ? 2000 + i : 7777); }
            r.np = (uint8_t)np;
            up_params(&r, values, def);
            for (i = 0; i < P_E0; i++) assert(values[i] == (i < nc ? (int16_t)(2000 + i) : def[i]));
            for (i = 0; i < 8u; i++) assert(values[P_E0 + i] == (int16_t)(2000 + nc + i));
        }
        r.np = P_COUNT;
    }
    /* Known 192-byte records from SLOOP 2.6, held in their original sectors. */
    struct legacy_rec { uint8_t used,ver,engine,np; char name[12]; int16_t p[72]; uint8_t note[16],flags[16]; };
    struct legacy_bank { uint32_t magic; uint16_t rsize,nslot; struct legacy_rec r[16]; } old={0};
    _Static_assert(sizeof(struct legacy_rec)==192,"legacy record");
    old.magic=UP_BANK_MAGIC; old.rsize=192; old.nslot=16;
    for(uint32_t j=0;j<16;j++) {
        old.r[j].used=UP_USED; old.r[j].ver=1; old.r[j].np=71; old.r[j].engine=j%12;
        memcpy(old.r[j].name,"OLD SOUND",10);
        for(uint32_t i=0;i<71;i++) old.r[j].p[i]=(int16_t)i-32;
        old.r[j].note[0]=60+j; old.r[j].flags[0]=1;
    }
    assert(!st_save(OBJ_UPRESET0,&old,sizeof old)); find_bank(0);
    assert(up_used(0)&&up_used(15)); got=*up_rec(15);
    memset(def,0,sizeof def); up_params(&got,values,def);
    assert(P_M1SRC==63&&P_E0==81&&P_COUNT==89);
    for(uint32_t i=0;i<63;i++) assert(values[i]==(int)i-32);
    for(uint32_t i=63;i<P_E0;i++) assert(values[i]==0);
    for(uint32_t i=0;i<8;i++) assert(values[P_E0+i]==(int)(63+i)-32);
    assert(!up_bank_convert(&bank,up_bank_xip[0]));
    /* Each torn v2 save must leave the full v1 bank current. */
    for(int cut=0;cut<2600;cut+=257) {
        fail_after=cut; assert(st_save(OBJ_UPRESET0,&bank,sizeof bank)); fail_after=-1;
        find_bank(0); assert(up_bank_xip[0]->rsize==192&&up_used(15));
    }
    assert(!st_save(OBJ_UPRESET0,&bank,sizeof bank)); find_bank(0);
    assert(up_bank_xip[0]->rsize==160);
    for(uint32_t i=0;i<16;i++) { got=*up_rec(i); assert(up_valid(&got)&&got.ver==2&&got.note[0]==60+i&&got.np==71); }
    assert(st_sector(OBJ_UPRESET0,0)==0xDC000u && st_sector(OBJ_UPRESET0+1,1)==0xDF000u);
    bank.rsize=256; assert(!up_bank_ok(&bank,sizeof bank));
    bank.rsize=160; bank.r[0].ver=3; up_decode(&bank,0,&got); assert(!up_valid(&got));
    puts("user presets: v1/v2, every slot, signed params, parser, legacy mapping and torn migration PASS");
    return 0;
}
