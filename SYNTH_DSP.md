# synth: ladder, expressive presets and Wave Terrain

This note covers S5–S7. Hardware timing and listening checks remain separate
acceptance steps; the numbers below are host measurements, not FM-1 results.

## Ladder

`dsp.c` adds four trapezoidal one-pole stages with a solved linear feedback
loop and one interpolated tanh per sample. Audio and coefficients are Q15;
four `int32_t` states retain eight extra fractional bits (Q23) to avoid a
low-frequency deadband. Integer 64-bit multiply intermediates are used; no
floating point, allocation, 64-bit division or oversampling enters the filter.
The existing 257-point tanh table and the SVF cutoff table are reused: no new
lookup table or per-voice storage is allocated. ANALOG uses formerly unused
voice state words 3–6. Track filters have independent channel states.

This follows the economical nonlinear four-pole idea of Surge's legacy
[LPMOOGquad](https://github.com/surge-synthesizer/sst-filters/blob/main/include/sst/filters/QuadFilterUnit_Impl.h).
It is **not** a verbatim port: the legacy implementation has five states and
explicit delayed feedback; this four-state implementation solves the linear
loop to stay stable at high cutoff without oversampling. RES 127 maps to the
nominal k=4 self-oscillation endpoint, with a 1% fixed-point loss compensation
(internal feedback 4.04). The tanh bounds the oscillation.

LP/BP/HP keep their original arithmetic. LP24 is appended as ANALOG TYPE 3;
track TYPE 1 selects the ladder only on the low-pass side. The track resonance
is an offset over the original 40, so zero retains the original filter.

Host acceptance (`tests/synth_dsp_test.c`, also run with UBSan):

- Impulse response at cutoff index 70: **−24.06 dB/octave** from 3,820 to 7,640 Hz.
- Ten seconds at RES 127, cutoff 80, after one impulse: peak **1,617**, final
  second RMS **1,143** Q15 units, bounded and still oscillating.
- Zero state/input remains exactly zero; all 128 cutoff positions and the
  resonance endpoints tolerate full-scale alternating input without overflow.

## Terrain

`eng_terrain.c` adapts Emilie Gillet's five analytic
[Plaits Wave Terrain surfaces](https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/engine2/wave_terrain_engine.cc)
to Q15. The MIT notice is in `LICENSES/plaits-wave-terrain.txt`. This is the
analytic portion of Plaits, not its wavetable/user-terrain modes. Two phase
accumulators trace the X/Y orbit; the path is sampled twice per output sample,
as in Plaits. Adjacent surfaces crossfade. High notes reduce the orbit radius;
a DC blocker with error feedback and an output SVF keep offset trajectories
usable. All render state belongs to the voice. No arena or shared PRNG is used.

The two EDIT pages are TERRAIN (TERR, RAD, X, Y) and ORBIT (RATIO, ROT, DRV, CUT).
The eight factory presets cover keys, bass, bells, plucks, lead, pad and drone.
Engine numbers 0–11 are unchanged. Optional SLICE remains 12 when enabled;
TERRAIN is appended at 12 without SLICE, 13 with it. The preset trim table
uses `ENGI_TERRAIN` so the optional engine cannot select another bank's gains.

The independent float reference in `tests/synth_dsp_test.c` compares all five
surface equations at 85,050 coordinates: RMS error **0.000377**, maximum
**0.001932** (normalized full scale). Complete one-second renders of the eight
preset trajectories, including drive, DC removal and filter, have RMS error
**0.000291–0.002335**, maximum **0.006672**. Acceptance limits are 0.005 RMS and
0.015 peak. All 256 min/max combinations of the eight parameters, at low/high
notes and extreme modulation, pass UBSan. This reference lives only in the
host test; the firmware contains no float calculation for the engine.

## Presets and measurement

ANALOG's original indices 0–21 are preserved. Indices 22–29 append LADDER ACID,
WHEEL LEAD, AFTER BRASS, SLEW PAD, EXPR KEYS, RAND PLUCK, DUAL ORBIT and HALL SWELL.
They demonstrate velocity, modulation wheel, aftertouch, expression, note
randomness, slew and LFO 2. SLEW PAD, HALL SWELL and TERRAIN's ISLAND DRONE request
the HALL global reverb when selected; older presets retain the current type.

`tools/level_synth_presets.py` uses the existing `level_presets.py` BS.1770
measurement and its phrase roles/target, but updates only the sixteen appended
sounds. It preserves every upstream trim and the optional SLICE table. The
existing −40..23 half-dB trim limits still apply; heavily filtered/plucked
presets may reach the upper limit before reaching the target loudness.

```sh
cc -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal tests/preset_level.c -lm -pthread -o build/host/synth_preset_level
build/host/synth_preset_level build/host/synth-presets.wav build/host/synth-presets.txt build/host/synth-presets.raw
python3 tools/level_synth_presets.py build/host/synth-presets.raw build/host/synth-presets.txt
# Rebuild and re-render until changes=0; do not reuse audio after changing trims.
cc -O2 -Ibuild/gen -Ifirmware/src tests/synth_dsp_test.c -lm -o build/host/synth_dsp_test
build/host/synth_dsp_test
cc -O1 -g -fsanitize=undefined -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src tests/synth_dsp_test.c -lm -o build/host/synth_dsp_ubsan
build/host/synth_dsp_ubsan
```

Golden hashes are added only after leveling. Existing hashes must never be
rewritten to accommodate these features. FM-1 maximum interrupt time, audible
aliasing at extreme terrain ratios, ladder sound and the eight-voice CPU cost
still require the device acceptance procedure in `SYNTH.md`.
