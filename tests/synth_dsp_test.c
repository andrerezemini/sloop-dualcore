/* SPDX-License-Identifier: GPL-3.0-only
 * synth DSP acceptance: independent float terrain reference, ladder response,
 * 10-second self-oscillation, the complete cutoff/resonance range under UBSan.
 * cc -O2 -Ibuild/gen -Ifirmware/src tests/synth_dsp_test.c -lm -o build/host/synth_dsp_test
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define __attribute__(x)
#include "felucca_tables.h"
#include "../firmware/src/core.h"
#include "../firmware/src/dsp.c"
static inline int32_t voice_amp(int32_t s, const vmod_t *m, uint32_t i)
{ return mulq15(mulq15(s, amp_at(m, i)), VOICE_FS); }
#include "../firmware/src/eng_terrain.c"
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); failures++; } } while (0)
static double fsine(double x) { return sin(2.0 * M_PI * x); }
/* Independent transcription of the five equations in Emilie Gillet's Plaits
 * WaveTerrainEngine::Terrain (MIT, LICENSES/plaits-wave-terrain.txt). */
static double reference_surface(double x, double y, int t)
{
    double xy = x*y, a;
    switch (t) {
    case 0: a = 2*fsine(1.273*x); return (a/(1+fabs(a))-fsine(y*(x+1.571)*.637))*.57;
    case 1: return fsine(fsine((x+y)*.637)/(.2+xy*xy)*.159);
    case 2: return fsine(fsine(2.387*xy)/(.350+xy*xy)*.159);
    case 3: return fsine(xy/(2+fabs(5*(x-.25)*(y+.25)))*6.366);
    default: return fsine(.159/(.170+fabs(y-.25))+.477/(.350+fabs((x+.5)*(y+1.5))));
    }
}
static double reference_lookup(double x, double y, double z)
{
    int k = (int)z;
    double a = reference_surface(x,y,k);
    return k < 4 ? a + (reference_surface(x,y,k+1)-a)*(z-k) : a;
}
static void terrain_test(void)
{
    double energy=0, maxerr=0;
    unsigned n=0;
    for (int t=0;t<5;t++) for(int x=-32768;x<=32768;x+=521) for(int y=-32768;y<=32768;y+=487) {
        double r=reference_surface(x/32768.0,y/32768.0,t);
        double e=terrain_surface(x,y,t)/32768.0-r;
        energy+=e*e; n++; if(fabs(e)>maxerr)maxerr=fabs(e);
    }
    printf("terrain surfaces: RMS %.6f, peak %.6f (%u coordinates)\n",sqrt(energy/n),maxerr,n);
    CHECK(sqrt(energy/n)<0.0015); CHECK(maxerr<0.008);
    /* Full two-oscillator renders, including the DC blocker and output SVF.
     * The float render uses analytic sine/tanh and independent filter arithmetic.
     * Q15 table/coordinate/feedback rounding is permitted, bounded to -46 dB RMS. */
    for(int preset=0;preset<8;preset++) {
        track_t t={0}; voice_t v={0}; vmod_t m={0};
        const preset_t *pr=&TERRAIN_PRESETS[preset];
        for(int k=0;k<8;k++)t.p[P_E0+k]=pr->e[k];
        m.inc=pitch_inc(60*16); m.pitch16=60*16; m.shape=64<<8; m.amp0=m.amp1=32767;
        double phase=0, phasey=0, dc=0, prev=0, s1=0, s2=0;
        double inc=(m.inc>>1)/4294967296.0, ratio=t.p[P_E4];
        double iy=(uint32_t)(((uint64_t)m.inc*t.p[P_E4])>>1)/4294967296.0;
        double att=fmax(1.-8.*m.inc/4294967296.*ratio,0);
        double radius=.1+.9*t.p[P_E1]/127.*att*(2-att);
        double xo=t.p[P_E2]/64.,yo=t.p[P_E3]/64.;
        double rot=(uint32_t)t.p[P_E5]*(0x80000000u/127u)/4294967296.0;
        tsvf_t fc;tsvf_coef(&fc,t.p[P_E7]<<8,0);
        double a1=fc.a1/8192.,a2=fc.a2/8192.,a3=fc.a3/8192.;
        double ee=0,rr=0,peak=0;
        for(unsigned b=0;b<FS/CTL;b++) {
            int32_t out[CTL]={0};terrain_render(&t,&v,out,CTL,&m);
            for(unsigned i=0;i<CTL;i++) {
                double s=0;
                for(int j=0;j<2;j++) {
                    double x=fsine(phase)*radius*(1-fabs(xo))+xo;
                    double y=fsine(phasey+.25+rot)*radius*(1-fabs(yo))+yo;
                    s+=reference_lookup(x,y,t.p[P_E0]*4./127.);
                    phase+=inc;phasey+=iy;
                    phase-=floor(phase);phasey-=floor(phasey);
                }
                s*=.5;
                if(t.p[P_E6])s+=(tanh(3*s)-s)*(t.p[P_E6]*258/32768.);
                dc=s-prev+dc*(32675/32768.);prev=s;
                double v3=dc/2-s2,v1=a1*s1+a2*v3,v2=s2+a2*s1+a3*v3;
                s1=2*v1-s1;s2=2*v2-s2;
                double ref=v2*2*(32767./32768.)*(24000./32768.);
                double er=out[i]/32768.-ref;
                ee+=er*er;rr+=ref*ref;if(fabs(er)>peak)peak=fabs(er);
            }
        }
        printf("terrain preset %d float render: RMS %.6f, peak %.6f\n",preset,sqrt(ee/(FS/CTL*CTL)),peak);
        CHECK(sqrt(rr/(FS/CTL*CTL))>.005);
        CHECK(sqrt(ee/(FS/CTL*CTL))<.005);CHECK(peak<.015);
        CHECK(sqrt(ee)<sqrt(rr)*.05);                   /* and relative: an error 26 dB under the signal, however quiet */
    }
    /* Render extreme controls and modulation, including the highest MIDI note
     * and 8:1 orbit, so fixed-point bounds cover more than the factory bank. */
    for(unsigned corner=0;corner<256;corner++) {
        track_t t={0};voice_t v={0};vmod_t m={0};
        for(unsigned k=0;k<8;k++)t.p[P_E0+k]=(corner&(1u<<k))?ENG_TERRAIN.edit[k].max:ENG_TERRAIN.edit[k].min;
        m.inc=pitch_inc((corner&1)?127*16:24*16);m.amp0=m.amp1=32767;
        m.shape=(corner&2)?127<<8:0;m.cutoff=(corner&4)?127<<8:-(127<<8);
        for(unsigned b=0;b<24;b++) {
            int32_t out[CTL]={0};terrain_render(&t,&v,out,CTL,&m);
            for(unsigned i=0;i<CTL;i++)CHECK(out[i]>=-100000&&out[i]<=100000);
        }
    }
}
static double impulse_gain(double hz)
{
    ladder_t s={{0}};ladder_coef_t c;ladder_coef(&c,70<<8,0);
    double re=0,im=0;
    /* Small-signal impulse. Remove the integer tail's DC contribution. */
    enum{N=16384};int32_t impulse[N];
    for(int i=0;i<N;i++)impulse[i]=ladder_q15(&s,&c,i==0?16000:0);
    for(int i=0;i<N;i++) {
        double v=impulse[i]-impulse[N-1],p=2*M_PI*hz*i/FS;
        re+=v*cos(p);im-=v*sin(p);
    }
    return hypot(re,im);
}
static void ladder_test(void)
{
    ladder_t s={{0}};ladder_coef_t c;
    ladder_coef(&c,80<<8,127);
    double e=0;int peak=0;
    for(int i=0;i<FS*10;i++) {
        int y=ladder_q15(&s,&c,i==0?8000:0);
        if(abs(y)>peak)peak=abs(y);
        if(i>=9*FS)e+=(double)y*y;
        for(int j=0;j<4;j++)CHECK(abs(s.s[j])<=16777216);
    }
    printf("ladder 10 s k=4: peak %d, last-second RMS %.1f\n",peak,sqrt(e/FS));
    CHECK(peak<32768);CHECK(sqrt(e/FS)>200);CHECK(sqrt(e/FS)<5000);
    memset(&s,0,sizeof s);
    for(int i=0;i<FS;i++)CHECK(ladder_q15(&s,&c,0)==0);
    /* Every parameter corner with full-scale alternating impulses: under the
     * sanitizer, any signed overflow/division/invalid shift aborts this test. */
    for(int cut=0;cut<=127;cut++)for(int res=0;res<=127;res+=127) {
        memset(&s,0,sizeof s);ladder_coef(&c,cut<<8,res);
        for(int i=0;i<2048;i++) {
            int y=ladder_q15(&s,&c,(i&1)?32767:-32768);
            CHECK(y>=-32768&&y<=32767);
        }
    }
    double f=CUTOFF_HZ[70],g1=impulse_gain(f*4),g2=impulse_gain(f*8);
    double slope=20*log10(g2/g1);
    printf("ladder impulse octave %.0f..%.0f Hz: %.2f dB/oct\n",f*4,f*8,slope);
    CHECK(slope< -22.);CHECK(slope> -26.);
    /* Self-oscillation at RES 127 holds a level from 49 Hz (cut 10) to the top: the feedback above 4
     * (LADDER_K127) pays for the fixed-point losses at a low cutoff, where the stages' steps round away. */
    for(int cut=10;cut<=127;cut+=cut<30?5:cut<100?10:27) {
        memset(&s,0,sizeof s);ladder_coef(&c,cut<<8,127);double e2=0;
        for(int i=0;i<FS*3;i++){int y=ladder_q15(&s,&c,i==0?8000:0);if(i>=2*FS)e2+=(double)y*y;}
        double db=20*log10(sqrt(e2/FS)/32768+1e-12);
        if(db<-30||db>-12)printf("ladder self-oscillation at cut %d: %.1f dBFS\n",cut,db);
        CHECK(db>-30.);CHECK(db<-12.);
    }
    /* After a full-scale bipolar burst the output returns to exactly 0, and so do the stages (the deadband
     * walks them down when a low cutoff's steps round to nothing), at every cutoff, with and without resonance. */
    for(int res=0;res<=100;res+=100)for(int cut=0;cut<=127;cut+=cut<80?40:47) {
        int nz=0;memset(&s,0,sizeof s);ladder_coef(&c,cut<<8,res);
        for(int i=0;i<FS*4;i++){int y=ladder_q15(&s,&c,i<2000?((i&1)?30000:-30000):0);if(i>=FS*3&&y)nz++;}
        if(nz||s.s[0]||s.s[1]||s.s[2]||s.s[3])printf("ladder burst cut %d res %d: %d nonzero, states %d %d %d %d\n",cut,res,nz,s.s[0],s.s[1],s.s[2],s.s[3]);
        CHECK(!nz);CHECK(!s.s[0]&&!s.s[1]&&!s.s[2]&&!s.s[3]);
    }
}
int main(void)
{
    ladder_test();terrain_test();
    printf("synth DSP: %s\n",failures?"FAIL":"PASS");return failures?1:0;
}
