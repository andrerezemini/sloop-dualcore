# Synth validation, 2026-10-13

Firmware revision: `bc9b5b01f022066cb6fbaa65d115b2dd0d858fb3`.
The complete acceptance run used test revision `f525aa6`; subsequent test and
documentation commits do not change the firmware package.
The target manifests record the full source/input fingerprint and every
artifact's SHA-256. Files copied to `build/` were checked against the normal
lite manifest; all three variant directories were checked independently.

## Automated acceptance

| Check | Result |
|---|---|
| Full host regression | 198 renders; 0 changed, missing, health, routing, CPU-budget or crash failures |
| Lite host regression | 195 renders; same zero-failure result; only FLUTE/SCRCH-dependent presets excluded |
| Existing golden baseline | All 182 original rows identical to `ram`; 16 appended |
| Existing host CPU baseline | All 158 original rows identical to `ram`; 16 appended |
| Target instruction budgets | All 23 functions pass against the final target disassembly |
| Dual-core golden modes | CORES=1/2, CHAOS=7, timeout fallback, forced split off/all: identical |
| Unit partition tests | Nine scenes, eleven masks; single/threaded/delayed/timeout modes pass |
| ThreadSanitizer | Three split masks, CORES=2 CHAOS=3: pass, no race |
| UI | New MOD/LFO2/LP24/HALL controls, ninth GLO page and 20,000-frame fuzz pass |
| Stress | 40,000 frames (10.7 simulated minutes); clean stop; ASan/UBSan pass |
| Soak | Ten simulated minutes; released voices drain in 7.11 seconds, gates stay off, voice count never increases, final tail is zero |
| Modulation | MIDI routing/reset, shadow edits/fades, dynamic enum guards, per-part RNG, voice/event destinations, ARP gate source and all LFO2 wave/sync combinations pass |
| Persistence | FUN1–FUN6 migrations, compact FUN7, section inheritance across save/transition/stop, v1/v2 preset banks and interrupted writes pass |
| DSP | Ladder slope/self-oscillation/extremes and independent Terrain float reference pass |
| Reverb | All four models at size/damping extremes, tails, stereo, model changes and FREEZE pass |
| Assets and boot | 87 lossless asset blocks, rejected truncation, stable full/lite sample IDs and identical BSS/non-BSS boot render pass |
| Complete `tests/run_tests.sh` | Exit 0: ALL HOST TESTS PASSED; editor, backup, migration, installer and recovery scenarios included |

The full host suite explicitly generates `build/host-full-gen`, independent of
the target's sample profile. Its generated sample header was verified identical
to the previously validated full header. Lite uses `build/host-lite-gen`.
Native MOD, LP24 and HALL screen captures were visually inspected at 240×240.
The complete run and lite regression logs are retained locally in
`build/synth-validation/acceptance.log` and `build/synth-validation/lite.log`.

The target budget corrections are explicit: five new functions; previously
missing FM6 entries measured against `ram`; and the stale IRQ entry corrected
to the same 268 measured on both branches. No other old budget was increased.
PHASE's unused second-oscillator setup was removed, preserving audio and
bringing RESO PLUCK comfortably below its original host limit.

The old soak test assumed every release ended within five seconds. The new
engine/preset count changes the seeded random selection, exposing the existing
FM6 GLASS BELL's longer operator-envelope release. Instrumentation confirmed
gates were already off and the final voice ended after 7.11 seconds. Soak now
uses the existing stress test's bounded 30-second release deadline, additionally
asserting gates remain off and active voice count never increases. The original
all-voices-free, stopped transport, bounded output and silence checks remain.

## Target packages

| Variant | App bytes | App headroom |
|---|---:|---:|
| Normal lite | 528,376 | 53,188 |
| Lite FPU diagnostic | 533,032 | 48,532 |
| Lite single core | 526,480 | 55,084 |

Normal lite RAM is 59,324 bytes, lowram 23,280, pool 333,636. Full samples exceed
XIP by 1,368 bytes; the linker refuses the image and removes stale output.
The installable normal `.fwsc` SHA-256 is
`84f262293287b9904820ffc446478b800bfb2fd4c5f93102f00eb29434a6e229`.

These are compiler, host and artifact-integrity results. No FM-1 was attached
for this acceptance run. Hardware audio timing, shedding, stack minima, FPU
timings/preservation, MIDI keyboard behavior and listening remain pending as
listed in [SYNTH.md](../SYNTH.md). Public upstream submissions/release are also
pending; [local drafts](synth-upstream.md) are prepared.
