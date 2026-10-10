# sloop-fm1, branch `dualcore`: project memory

This checkout is a **thin fork** of [isod89/sloop-fm1](https://github.com/isod89/sloop-fm1) (SLOOP, a
groovebox firmware for the M-VAVE FM-1). Branch `dualcore` adds the FM-1's second processor core to
SLOOP 2.5 as a short series of patches; `main` is upstream, untouched. Public fork:
https://github.com/andrerezemini/sloop-fm1. The owner's planning documents (Portuguese) live in a private
repository, not here. Status and the patch table: [DUALCORE.md](DUALCORE.md).

## Rules of this fork

- **Only the second core.** Anything else SLOOP might want goes upstream as an issue or into another
  branch. Patches stay small (at most six files each) and additive, so they can be re-applied on each
  SLOOP release: upstream publishes one squashed commit per release and merges no pull requests, so a
  new release means `git checkout vX.Y` and re-applying the series.
- **Bit-identical audio.** The render split only sums integers in private buffers; the sums commute. Every
  change must keep all 182 hashes of `tests/golden.txt` unchanged, under every mode of the host double
  (below). No `GOLDEN_UPDATE=1` on this branch.
- **Credits travel with the code.** `hal/fm1_cpu1.{h,S}` are Charles Vestal's (X0X, GPL-3.0-only), after
  Melodee's findings (Kerem Kilic); SLOOP is isod89's; Felucca is Leo Kuroshita's. Keep the notices.
- **Upstream strategy.** The HAL part is offered to Felucca (the root of every FM-1 firmware; its
  Discussion 141 asks for CPU1 DSP); the SLOOP-specific split is offered to SLOOP as an issue with
  measurements and a link here. Neither is a precondition for this branch.

## What the branch changed (as of 10/10/2026)

| Patch | Files | What |
|---|---|---|
| 1 hal | `firmware/hal/fm1_cpu1.h`, `fm1_cpu1.S` (new), `fm1_guard.h` (`fm1_guard_pc_open`, `fm1_guard_top_locked`), `fm1_sys.h` (core 1 held before a reboot and before the loader's core reset) | the second core's start, hold, one-job hand-off and wait, from X0X |
| 2 build and boot | `firmware/app.ld` (`.c1_text` in RAMTEXT, `_rt_end = _c1_end`), `tools/build.py` (assembles `fm1_cpu1.S`, `c1text.bin`, flag `FELUCCA_CPU1`), `firmware/src/main.c` (copies `.c1_text`, `cpu1_boot()` before `audio_init()`), `firmware/src/cpu1.c` (new), `ui_menu.c` (ABOUT: "2 CORES" / "1 CORE"), `console.c` (`cpu1`, `cpu1_trace`, `cpu1_timeouts`), `felucca.c` (includes) | core 1 started at power-on, self-tested with a checksum job, reported; fallback to one core |
| 2b host double | `firmware/src/cpu1.h` (new: `cpu1_run` / `cpu1_wait` / `cpu1_on`), `tests/cpu1_host.c` (new), `tests/cpu1_test.c` (new), `tests/hostsim.c` (includes the double), `tests/run_tests.sh` (five runs of `cpu1_test`) | the render's job API, with a host implementation for the tests |
| 3 (next) | `firmware/src/fx.c` | the three parts and the drums as render units with private accumulator sets, handed to core 1 through `cpu1_run` |

Memory of the current build against 2.5: RAM `.data+.bss` 87,928 B of 98,304 (+3,108: core 1's stacks),
pool 333,948 of 344,064 (unchanged), image 565,484 B, `.c1_text` 60 B. Not yet run on an FM-1.

## Building

```sh
export JIELI_TOOLCHAIN=$HOME/.jieli/toolchain      # tools/get_toolchain.sh installs it (Linux x86-64)
export AC79_SDK=$HOME/fw-AC79_AIoT_SDK              # only cpu/wl82/tools/{uboot.boot,cfg_tool.bin,cfg/eq_cfg_hw.bin}
sh build.sh                                         # build/felucca.fwsc; ~8 s; on macOS Docker must be running
FELUCCA_CPU1=0 sh build.sh                          # one core, as SLOOP 2.5
sh build.sh --release 2.5                           # identity FM-1_925 (upstream publishes the dev identity FM-1_900)
```

`sh build.sh`, not `./build.sh`: the files carry no execute bit. On macOS every toolchain call runs in a
`linux/amd64` Debian container; on Linux x86-64 it runs natively. The JieLi toolchain ships no libc or libm:
the firmware brings its own `libc.c`. SLOOP's generators run on Apple's Python; X0X's need Pillow with raqm.

## Testing

```sh
sh tests/run_tests.sh                               # ~3 min; after a build
CORES=2 CHAOS=7 build/host/cpu1_test                # env vars as separate words (zsh does not split a quoted string)
```

- Two failures are **pre-existing in SLOOP 2.5** and not this branch's: three PHASE presets over the host
  CPU instruction baseline (written on the author's machine), and `target_budget`'s `fm1_alnk0_irq` 268 vs
  174 (the upstream budget file is stale). Everything else, including the 182 golden renders and the
  10-minute soak, must pass.
- The second core's host double (`tests/cpu1_host.c`): `CORES=1` runs a job inline but only at
  `cpu1_wait` (the default; deterministic), `CORES=2` on a POSIX thread, `CHAOS=N` with random delays
  before and after each job, `CPU1_TIMEOUT_AT=K` loses the K-th job and holds the core (the fallback path).
  `tests/cpu1_test.c` runs in all of them and under `-fsanitize=thread`. A render change is done when
  `regress` gives the same hashes in every mode.
- Host tests include single firmware files (e.g. `ui_pages_test.c` includes `ui_menu.c` without `cpu1.c`):
  anything that only exists in the firmware build goes under `#if FELUCCA_CPU1`.
- `tools/build.py` refuses a literal that looks like a register address outside `hal/` (a test seed of
  `0x5100F` tripped it); keep such constants decimal.

## On the device

Back up with the web editor first. Install from a local copy of the site (Web MIDI needs localhost or https):

```sh
python3 web/make_site.py build/felucca.fwsc dev ../sloop-site && cd ../sloop-site && python3 -m http.server 8000
# Chrome or Edge: http://localhost:8000/webapp/installer/
```

First boot checklist: ABOUT (hold HOME, SYSTEM, ABOUT) says **2 CORES**; the console (`fm1t`, USB SERIAL on)
prints `cpu1 = 3` and `cpu1_trace = 0xC1000002`. "1 CORE" with `cpu1_trace 0` means core 1 never reached
its entry; `0xC1000001` means it reached the entry but not its loop. Rescue: hold OCT- while switching on,
then install again. The package keeps identity FM-1_900, so the official 2.5 installs back over it.

## What the device alone can tell

Core 1's start (registers, ROM, PC limits), real timing (cycles per sample, I-cache contention between the
cores, XIP latency, late DMA halves), USB and OTA on the hardware. The host answers everything else.
