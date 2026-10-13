/* SPDX-License-Identifier: MIT
 * Copyright 2021 Emilie Gillet (Mutable Instruments).
 * Copyright 2026 sloop-lab contributors.
 * The five analytic surfaces below are a fixed-point adaptation of Plaits'
 * wave_terrain_engine.cc; see LICENSES/plaits-wave-terrain.txt for its license.
 * https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/engine2/wave_terrain_engine.cc
 * No Plaits wavetables, user terrain, allocator, float or libm are required.
 * Coordinates and audio are Q15. Two phases, one DC blocker and one SVF live
 * in each voice: no arena, global random source or per-part render scratch.
 */
static inline int32_t terrain_mul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 15);
}
static inline int32_t terrain_abs(int32_t a) { return a < 0 ? -a : a; }
/* Signed Q15 cycles -> wrapping phase. Unsigned multiplication avoids shifting
 * a negative integer, including for negative surface coordinates. */
static inline int32_t terrain_sine(int32_t q)
{
    return sine_i((uint32_t)q * 131072u);
}
static int32_t terrain_surface(int32_t x, int32_t y, uint32_t terrain)
{
    int32_t a, xy = terrain_mul(x, y), xy2 = terrain_mul(xy, xy);
    switch (terrain) {
    case 0:
        a = 2 * terrain_sine(terrain_mul(x, 41714));    /* squash(sin(1.273*x), 2) */
        a = (int32_t)((a * 16384) / (32768 + terrain_abs(a))) * 2;
        return terrain_mul(a - terrain_sine(terrain_mul(terrain_mul(y, x + 51479), 20873)), 18678);
    case 1:
        a = terrain_sine(terrain_mul(x + y, 20873));
        return terrain_sine((a * 5210) / (6554 + xy2));
    case 2:
        a = terrain_sine(terrain_mul(xy, 78217));
        return terrain_sine((a * 5210) / (11469 + xy2));
    case 3:
        a = 65536 + 5 * terrain_abs(terrain_mul(x - 8192, y + 8192));
        /* Divide first: numerator stays in signed 32-bit at |xy| <= 32768. */
        return terrain_sine(terrain_mul((xy * 32768) / a, 208601));
    default:
        a = (5210 * 32768) / (5571 + terrain_abs(y - 8192));
        a += (15630 * 32768) / (11469 + terrain_abs(terrain_mul(x + 16384, y + 49152)));
        return terrain_sine(a);
    }
}
/* TERR sweeps continuously across the five surfaces. Only the adjacent two
 * are evaluated; exactly on a surface the second evaluation is skipped. */
static int32_t terrain_lookup(int32_t x, int32_t y, int32_t terrain_q15)
{
    uint32_t k = (uint32_t)terrain_q15 >> 15;
    int32_t frac = terrain_q15 & 32767, a = terrain_surface(x, y, k);
    return frac && k < 4u ? a + terrain_mul(terrain_surface(x, y, k + 1u) - a, frac) : a;
}
static void terrain_note_on(track_t *t, voice_t *v)
{
    (void)t;
    if (!v->env && !v->env_out) {
        v->ph[0] = v->ph[1] = 0;
        v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = 0;
    }
}
static void terrain_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = render_p(t);
    uint32_t i, j, phx = v->ph[0], phy = v->ph[1];
    uint32_t ratio = (uint32_t)clamp(p[P_E4], 1, 8);
    uint32_t ix = m->inc >> 1, iy = (uint32_t)(((uint64_t)m->inc * ratio) >> 1);
    uint32_t rot = (uint32_t)p[P_E5] * (0x80000000u / 127u);
    int32_t terrain = clamp(p[P_E0] * 4 * 32768 / 127 + m->shape - (64 << 8), 0, 4 * 32768);
    int32_t radius = 3277 + p[P_E1] * 29490 / 127;
    int32_t xo = p[P_E2] * 512, yo = p[P_E3] * 512;
    int32_t xs = 32768 - terrain_abs(xo), ys = 32768 - terrain_abs(yo);
    int32_t drive = p[P_E6] * 258;
    int32_t dc = v->s[0], prev = v->s[1], ic1 = v->s[2], ic2 = v->s[3], err = v->s[4];
    tsvf_t fc;
    /* Plaits' high-note radius reduction, using the faster orbit for non-1:1
     * paths. The small 0.1 base radius remains, as in the original model. */
    int32_t att = 32768 - clamp((int32_t)(((uint64_t)m->inc * ratio) >> 14), 0, 32768);
    radius = 3277 + terrain_mul(radius - 3277, terrain_mul(att, 65536 - att));
    tsvf_coef(&fc, (p[P_E7] << 8) + m->cutoff, 0);
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        /* Two trajectory samples, as Plaits: no extra audio buffers. */
        for (j = 0; j < 2u; j++) {
            int32_t x = terrain_mul(terrain_mul(sine_i(phx), radius), xs) + xo;
            int32_t y = terrain_mul(terrain_mul(sine_i(phy + 0x40000000u + rot), radius), ys) + yo;
            s += terrain_lookup(x, y, terrain);
            phx += ix;
            phy += iy;
        }
        s /= 2;
        if (drive)
            s += terrain_mul(softclip(s * 3) - s, drive);
        /* Surfaces are not necessarily centered on the selected trajectory. */
        {   /* ~20 Hz DC blocker, with error feedback so truncation does not
             * leave a small negative offset on a stationary orbit. */
            int64_t feedback = (int64_t)dc * 32675 + err;
            dc = s - prev + (int32_t)(feedback >> 15);
            err = (int32_t)(feedback & 32767);
        }
        prev = s;
        s = tsvf_lp(&fc, dc >> 1, &ic1, &ic2);
        out[i] += voice_amp(s, m, i) * 2;
    }
    v->ph[0] = phx; v->ph[1] = phy;
    v->s[0] = dc; v->s[1] = prev; v->s[2] = ic1; v->s[3] = ic2;
    v->s[4] = err;
}
static const preset_t TERRAIN_PRESETS[] = {
    /* TERR RAD X Y RATIO ROT DRIVE CUT. Every preset keeps two EDIT pages. */
    {"ORBIT KEYS", {0, 62, 0, 0, 1, 0, 0, 104}, {4, 70, 65, 48}, 0, 0, FX(0, 14, 20, 32)},
    {"RIDGE BASS", {30, 87, -22, 12, 1, 0, 24, 78}, {0, 68, 85, 26}, 16, 1, FX(0, 0, 0, 12), XP(P_TRANS + 1, -24)},
    {"GLASS ORBIT", {64, 43, 18, -12, 2, 32, 0, 115}, {0, 90, 18, 76}, 0, 0, FX(0, 12, 35, 58)},
    {"RIDGE PAD", {95, 58, -8, 20, 1, 24, 0, 89}, {78, 86, 108, 92}, 0, 0, FX(0, 42, 24, 68),
     XP(P_M1SRC + 1, MS_LFO, P_M1DST + 1, MD_E1, P_M1AMT + 1, 12, P_LRATE + 1, 19)},
    {"TERRAIN BELL", {127, 38, 9, 12, 3, 0, 0, 116}, {0, 98, 0, 76}, 0, 0, FX(0, 8, 34, 48)},
    {"CONTOUR LD", {46, 86, 18, -8, 1, 62, 38, 102}, {2, 62, 108, 38}, 0, 1, FX(0, 10, 25, 28),
     XP(P_M1SRC + 1, MS_MODW, P_M1DST + 1, MD_E1, P_M1AMT + 1, 30, P_GLIDE + 1, 32)},
    {"TOPO PLUCK", {82, 73, -26, -8, 2, 18, 14, 108}, {0, 66, 0, 52}, 20, 0, FX(0, 12, 30, 42)},
    {"ISLAND DRONE", {112, 104, -15, 18, 1, 80, 10, 88}, {90, 96, 120, 98}, 0, 0, FX(0, 48, 26, 74),
     XP(P_M1SRC + 1, MS_LFO2, P_M1DST + 1, MD_E1 + 2, P_M1AMT + 1, 18, P_L2RATE + 1, 15), .rtype = 4},
};
static const engine_t ENG_TERRAIN = {
    "TERRAIN", {"TERRAIN", "ORBIT"},
    {
        {"TERR", F_PCT, 0, 127, 0, 0, 0},
        {"RAD", F_PCT, 0, 127, 64, 0, 0},
        {"X", F_BIPCT, -63, 63, 0, 0, 0},
        {"Y", F_BIPCT, -63, 63, 0, 0, 0},
        {"RATIO", F_INT, 1, 8, 1, 0, 0},
        {"ROT", F_PCT, 0, 127, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},
    },
    TERRAIN_PRESETS, NELEM(TERRAIN_PRESETS), 1, terrain_note_on, terrain_render,
    0x5EBA, {P_E0, P_E1, P_E2, P_E7},
};
