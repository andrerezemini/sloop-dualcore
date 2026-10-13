/* SPDX-License-Identifier: GPL-3.0-only */
/* Real DSP: all models, long tails, stereo, alias-safe switches and FREEZE. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static int32_t zero[CTL], send[CTL], left[CTL], right[CTL];
static void reset_reverb(uint32_t type, int size, int damp)
{
    memset(&fx, 0, sizeof fx);
    rev_clear();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    memset(&punch, 0, sizeof punch);
    punch.req = punch.cur = -1;
    rev_frz = 0;
    host_tracks_init();
    song.g[G_RTYPE] = type;
    song.g[G_RSIZE] = size;
    song.g[G_RDAMP] = damp;
    fx_buses(zero, zero, zero, left, right, CTL); /* switch from a silent legacy model */
    assert(fx.rtype == type);
}
static int check_tail(uint32_t type, int size, int damp)
{
    uint32_t b, i, different = 0, peak = 0;
    uint64_t energy = 0, early = 0, late = 0;
    reset_reverb(type, size, damp);
    memset(send, 0, sizeof send);
    send[0] = 30000;
    for (b = 0; b < FS * 12u / CTL; b++) {
        fx_buses(zero, zero, b ? zero : send, left, right, CTL);
        for (i = 0; i < CTL; i++) {
            int64_t l = left[i], r = right[i];
            uint32_t a = (uint32_t)(l < 0 ? -l : l), c = (uint32_t)(r < 0 ? -r : r);
            if (a > peak) peak = a;
            if (c > peak) peak = c;
            assert(a < 131072 && c < 131072);
            energy += a + c;
            if (b < FS / CTL) early += a + c;
            if (b >= FS * 11u / CTL) late += a + c;
            different |= l != r;
        }
    }
    printf("reverb %u size %3d damp %3d: peak %u energy %llu late %llu stereo %u\n",
           type, size, damp, peak, (unsigned long long)energy, (unsigned long long)late, different);
    assert(energy > 1000 && peak > 20);
    assert(late < early); /* stable decay, including maximum SIZE */
    assert(type != 3u || different);
    assert(type == 0u || type == 3u || !different);
    return 0;
}
int main(void)
{
    uint32_t type, size, damp, b, i;
    assert(sizeof rev_mem == 17368u && sizeof rev_u <= 1996u);
    for (type = 0; type < 4; type++)
        for (size = 0; size < 2; size++)
            for (damp = 0; damp < 2; damp++)
                check_tail(type, size ? 127 : 0, damp ? 127 : 0);
    for (type = 0; type < 4; type++) {
        reset_reverb(type, 100, 30);
        for (b = 0; b < 3000; b++) {
            for (i = 0; i < CTL; i++) send[i] = b < 500 ? osc_sine((b * CTL + i) * 21000000u) : 0;
            punch.cur = b >= 200 && b < 1800 ? PX_FREEZE : -1;
            punch_freeze_feed(send, zero, zero, CTL);
            if (b == 900) song.g[G_RTYPE] = (type + 1u) & 3u;
            fx_buses(zero, zero, send, left, right, CTL);
            for (i = 0; i < CTL; i++) {
                assert(left[i] > -600000 && left[i] < 600000);
                assert(right[i] > -600000 && right[i] < 600000);
            }
        }
        /* Repeated switches clear every aliased buffer and all HALL indices. */
        for (b = 0; b < 40; b++) {
            song.g[G_RTYPE] = b & 3u;
            fx_buses(zero, zero, zero, left, right, CTL);
            assert(fx.rtype == (b & 3u));
            if (b) for (i = 0; i < CTL; i++) assert(!left[i] && !right[i]);
        }
    }
    /* HALL accepts shorter even bus blocks without overrunning their output. */
    reset_reverb(3, 80, 50);
    for (i = 0; i < CTL; i++) left[i] = right[i] = 123456;
    fx_buses(zero, zero, zero, left, right, CTL / 2u);
    for (i = CTL / 2u; i < CTL; i++) assert(left[i] == 123456 && right[i] == 123456);
    puts("reverb types: tails, stereo, switches, shared buffers and FREEZE PASS");
    return 0;
}
