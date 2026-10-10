# SLOOP on both cores: the `dualcore` branch of sloop-lab

A thin fork of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) that makes the M-VAVE FM-1's second
processor core available to the audio render. Everything else is SLOOP, unchanged; the branch is a short
series of patches meant to be re-applied on each SLOOP release, and offered upstream.

The second core's start-up sequence is [X0X](https://github.com/charlesvestal/fm1-x0x)'s (Charles Vestal),
after the findings of [Melodee](https://github.com/keremimo/melodee) (Kerem Kilic); X0X has run its drum
machines on core 1 since 1.0 (10/10/2026). This branch brings that HAL into SLOOP and, in the following
patches, splits SLOOP's `mix_block` into units that render on both cores with a bit-identical result.

## Status

| Patch | What | State |
|---|---|---|
| 1 | `hal/fm1_cpu1.{h,S}` from X0X; `fm1_guard_pc_open`, `fm1_guard_top_locked`; the second core held before a reboot and before the loader (`fm1_sys.h`) | done, builds |
| 2 | `.c1_text` in the linker script and `tools/build.py`; the loop copied to RAM and the core started at boot (`main.c`, `cpu1.c`) with a self-test job; "2 CORES" / "1 CORE" on ABOUT; `cpu1`, `cpu1_trace` in the console; `FELUCCA_CPU1=0` builds the old way | done, builds; **not yet run on an FM-1** |
| 2b | **the host double of core 1** (`firmware/src/cpu1.h` API: `cpu1_run` / `cpu1_wait` / `cpu1_on`; `tests/cpu1_host.c`): `CORES=1` runs a job inline at the wait, `CORES=2` on a POSIX thread, `CHAOS=N` with random delays, `CPU1_TIMEOUT_AT=K` loses a job and holds the core; `tests/cpu1_test.c` in all modes and under ThreadSanitizer, from `tests/run_tests.sh` | done |
| 3 | **the render split** (`fx.c`): the three parts and the drums as render units (`mix_part`, `mix_drums`, `mix_units`), each adding into an accumulator set of its own (the dry mix and the three sends); core 1's share handed over through `cpu1_run` before this core renders its own, added after `cpu1_wait`; the 8 ms timeout fallback (core 1 held, its units rendered here once). FM6's operator buses per part (`fm6_part_t`, in the engine arena, `fm6_core.c`, `eng_fm6.c`, `engines.c`). What stays on core 0 (`units_split`): two FORMANT parts together (one breath-noise xorshift), a part whose LFO wraps this block or whose PHYS model changed under a note (`rng()`, so the draws keep their order). `cpu1_units` (`cpu1.h`) forces a split: 0 none, a mask, -1 the firmware's choice | done: the 182 golden hashes unchanged under every mode of the host double and with no split (`tests/cpu1_modes.sh`); `tests/mix_split_test.c` (three FORMANT, FM6 or PHYS parts, S&H LFOs, mutes, the drums through the SLICER and the FILTER, the metronome) gives the same output for every split of the units, and no race under ThreadSanitizer; builds; **not yet run on an FM-1** |
| 4 | **the switch and the figures**: MENU > SYSTEM > **2ND CORE** ON / OFF (`ui_menu.c`; a bit of the settings word in `panel.c`, kept across restarts, old settings read as ON; live: the next block renders so, and the sound is the same either way); core 1's time in its job and core 0's time waiting for it, measured in `fx.c` with TIMER4 and smoothed per half as the CPU figure is (`cpu1_load_half`, called from the audio interrupt); on the console: `cpu1_busy_pct`, `cpu1_wait_pct`; ABOUT says `C1 OFF` when switched off. The INFO page keeps its one CPU column (core 0's load): it has no free column, so the figures live on the console | done, builds; **not yet run on an FM-1** |
| 5 | the first run on the device (the checklist below), then perf patterns (light, typical, worst: three PHYS SYMP / FM6 / GRAIN parts with 4-note chords, full drums, delay and reverb) and the measurements in this file, with 2ND CORE ON and OFF | next |
| 6 | the greedy per-block split by measured cost | |

Later, outside the dual-core series: reverb and delay settings that change per song section (after shifl's
song fragments), as a separate patch set.

## Testing on the Mac first

The render reaches core 1 only through `cpu1.h`'s three calls, and the host tests have their own
definitions of them (`tests/cpu1_host.c`), so the same code runs on the Mac in every way the device can
behave: `CORES=1` (inline, the job deferred to the wait), `CORES=2` (a real thread), `CORES=2 CHAOS=7` (a
slow, erratic thread), `CPU1_TIMEOUT_AT=K` (a lost job: the fallback and the hold), `CPU1_UNITS=mask` (a
forced split: 0 none, 15 every unit that may go), and `CORES=2` under ThreadSanitizer. `tests/cpu1_modes.sh`
(run by `tests/run_tests.sh`) runs the golden renders and `tests/mix_split_test.c` under all of them: a render
that gives the same hashes in every mode is bit-exact and race-free as far as the host can tell; the device then
answers only timing.

What a block does now (`fx.c mix_block`): the sequencer's events and the DUCK on core 0; then the split for
this block (`units_split`); core 1's units handed over (`acc[1]`, its own `part_buf`); core 0's units into
`acc[0]` meanwhile; the wait (a timeout holds core 1 and renders its units here); `acc[0] += acc[1]`; then
the buses, DUST, PUNCH, the DJ filter and the master, serial as before. Nothing but the render runs while
core 1 works: the main loop is preempted by the audio interrupt, and TIMER5 only scans the panel and feeds
USB audio.

## What the device will tell

On a first boot of this build: ABOUT (hold HOME, SYSTEM, ABOUT) shows **2 CORES** when core 1 started and
passed its self-test, **1 CORE** otherwise; the console (`fm1t`, USB SERIAL on) prints `cpu1` (0 not built,
1 did not report in within 20 ms, 2 answered wrong or late, 3 running), `cpu1_trace` (0xC1000001: reached
its entry; 0xC1000002: reached its loop), `cpu1_timeouts` (blocks core 1 did not finish in 8 ms: after the
first, it is held and SLOOP runs on one core), `cpu1_units` (`cpu1 units MASK|auto` sets it live), `cpu1_busy_pct`
and `cpu1_wait_pct` (core 1's share of a half spent rendering, core 0's share spent waiting for it) and
`cpu1_stack_free` (bytes of core 1's 2,816 B stack the render never reached: it must stay well above 0 on the
heaviest patterns). With 2 CORES the last synth part and the drums render on core 1; the audio is bit for bit
SLOOP 2.5's. MENU > SYSTEM > 2ND CORE switches the split off and on while playing: `cpu_pct` (the console, or
INFO on the SYSTEM page) should drop with it on, by roughly the drums' and one part's share.

## Installing a build of this branch on an FM-1

The package keeps SLOOP's development identity (FM-1_900); Felucca's update path does not refuse a package
with the same identity as the installed one, so it installs over the official 2.5 and the official 2.5
installs back over it. Your projects, presets, samples and settings are kept. From the browser:

```sh
python3 web/make_site.py build/felucca.fwsc dev ../sloop-site
cd ../sloop-site && python3 -m http.server 8000
# Chrome or Edge: http://localhost:8000/webapp/installer/  (Web MIDI needs localhost or https)
```

Or `python3 tools/fm1_install.py build/felucca.fwsc` (needs mido and python-rtmidi). Rescue as SLOOP's:
hold OCT- while switching on, then install again.

## Building

As SLOOP's `BUILDING.md`; on macOS run `sh build.sh` with Docker up. Memory of this build against 2.5:
RAM `.data+.bss` 88,472 B of 98,304 (+3.6 KiB: the core's two stacks, the second accumulator set, the
second part buffer, the load counters), pool unchanged (the FM6 buses fit in the engine arena), image 567,296 B
(+2,096 B), `.c1_text` 60 B in RAMTEXT.

## Licence and credits

GPL-3.0-only, as SLOOP, Felucca and X0X. `hal/fm1_cpu1.h` and `hal/fm1_cpu1.S` are Charles Vestal's
(X0X), brought in unchanged but for a notice; the guard and system additions follow X0X's; the rest of the
branch is Copyright (C) 2026 Andre Rezemini. SLOOP is isod89's; Felucca is Leo Kuroshita's (Hügelton
Instruments). See `LICENSING.md`.
