# SLOOP on both cores: the `dualcore` branch

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
| 3 | `fx.c`: the three parts and the drums as render units with private accumulator sets, handed to core 1 through `cpu1_run`; the sums commute, so `tests/golden.txt` must not change, with `CORES=1` and `CORES=2` | next |
| 4 | a static split (drums + one part on core 1), the 8 ms timeout fallback, the load figures | |
| 5 | a threaded host test of the hand-off (TSan); perf patterns; measurements on the device | |
| 6 | the greedy per-block split by measured cost | |

Later, outside the dual-core series: reverb and delay settings that change per song section (after shifl's
song fragments), as a separate patch set.

## Testing on the Mac first

The render will reach core 1 only through `cpu1.h`'s three calls, and the host tests have their own
definitions of them (`tests/cpu1_host.c`), so the same code runs on the Mac in every way the device can
behave: `CORES=1` (inline, the job deferred to the wait), `CORES=2` (a real thread), `CORES=2 CHAOS=7` (a
slow, erratic thread), `CPU1_TIMEOUT_AT=K` (a lost job: the fallback and the hold), and `CORES=2` under
ThreadSanitizer. A render that gives the same `golden.txt` hashes in all of them is bit-exact and race-free
as far as the host can tell; the device then answers only timing.

## What the device will tell

On a first boot of this build: ABOUT (hold HOME, SYSTEM, ABOUT) shows **2 CORES** when core 1 started and
passed its self-test, **1 CORE** otherwise; the console (`fm1t`, USB SERIAL on) prints `cpu1` (0 not built,
1 did not report in within 20 ms, 2 answered wrong or late, 3 running) and `cpu1_trace` (0xC1000001: reached
its entry; 0xC1000002: reached its loop). Audio is untouched by patches 1 and 2: SLOOP 2.5 as released.

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
RAM `.data+.bss` +3.1 KiB (the core's two stacks), image +250 B, `.c1_text` 60 B in RAMTEXT.

## Licence and credits

GPL-3.0-only, as SLOOP, Felucca and X0X. `hal/fm1_cpu1.h` and `hal/fm1_cpu1.S` are Charles Vestal's
(X0X), brought in unchanged but for a notice; the guard and system additions follow X0X's; the rest of the
branch is Copyright (C) 2026 Andre Rezemini. SLOOP is isod89's; Felucca is Leo Kuroshita's (Hügelton
Instruments). See `LICENSING.md`.
