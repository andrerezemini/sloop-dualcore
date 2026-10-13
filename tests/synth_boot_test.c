/* SPDX-License-Identifier: GPL-3.0-only */
/* Compare the production zero-initialized boot path with traditional .data defaults.
 * Compile/run with FELUCCA_BOOT_BSS=0 and =1; the state and render hash must match. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
int main(void)
{
    uint32_t i, b;
    uint64_t hash = 14695981039346656037ULL;
    int32_t out[CTL * 2];
#if FELUCCA_BOOT_BSS
    drums.set = -2;
    fx_boot_init();
#endif
    assert(drums.set == -2);
    assert(lim_dac.env == LIM_T && lim_dac.g == 32768);
    assert(lim_usb.env == LIM_T && lim_usb.g == 32768);
    assert(dust.rnd == 0x2545F491 && tx.rnd == 0x2545F491);
    assert(punch.req == -1 && punch.cur == -1 && punch.fg == 8192);
    host_tracks_init();
    host_preset_req(&trk[0], 0, 0);
    for (b = 0; b < 4096u; b++) {
        if (b % 512u == 0u) drum_on(36, 100);
        mix_block(out, CTL);
        for (i = 0; i < CTL * 2u; i++) {
            hash ^= (uint32_t)out[i]; hash *= 1099511628211ULL;
        }
    }
    printf("%016llx\n", (unsigned long long)hash);
    return 0;
}
