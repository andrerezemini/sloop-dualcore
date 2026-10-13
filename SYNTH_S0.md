# S0: firmware budget and FPU probe

The `synth` branch keeps all eight built-in sample-set indices stable. `FELUCCA_SAMPLES=lite`
emits FLUTE (index 5) and SCRCH (index 6) as empty sets; selecting them from an older project
produces silence. The other six sets, including PERC, retain their audio bytes. Factory
presets LOFI FLUTE, SCRATCH and FLUTE DUST are omitted from the lite browser. Full remains
the default sample profile. Their existing full-profile golden hashes are never rewritten.

```sh
sh build.sh                                      # full sample library
FELUCCA_SAMPLES=lite sh build.sh                  # installable synth profile when full exceeds flash
FELUCCA_SAMPLES=lite FELUCCA_FPU_PROBE=1 sh build.sh # bench image, then console: fpu
```

`FELUCCA_COLD`, `FELUCCA_BOOT_BSS` and `FELUCCA_ASSET_LZ4` default to `1`; setting an
option to `0` reproduces the corresponding unoptimized path for comparison. The float
probe defaults to `0`; `fpu` explains how to enable it when it is absent. No floating-point
DSP is added to the audio render. Invalid sample-profile and Boolean option values fail
the build. Every linked image writes `build/flash-report.json` and `build/flash-symbols.tsv`
from the target symbol table, sorted by size, including section and address. Symbol aliases
are retained in that list and must not be summed as separate storage. Oversized images
are refused before packaging, with a recommendation to use the lite profile.

## Isolated measurements

Measured with the existing JieLi toolchain in Docker (`linux/amd64`) against the `ram`
base, with only the S0 files applied. These isolate the budget options from S1–S8, which
have their own final build figures. The untouched base image was **576,944 bytes**; adding
the disabled probe command/help path made the control image **577,020 bytes**.

| Profile | App bytes | Savings vs S0 control |
|---|---:|---:|
| All S0 options disabled, full samples | 577,020 | 0 |
| Cold entry points only | 574,924 | 2,096 |
| Boot initialization only | 574,012 | 3,008 |
| LZ4 UI assets only | 574,988 | 2,032 |
| All three options, full samples | 570,028 | 6,992 |
| All three options, lite samples | 515,500 | 61,520 |
| Lite plus optional FPU probe | 520,188 | 56,832 |

The sample-data reduction itself is exactly **54,284 bytes** (303,054 → 248,770).
Removing the three dependent presets and unused zone metadata saves a further 244 bytes
in the combined image. The complete optimization changes `.data` from 2,912 to 112 bytes;
`.data + .bss` remains **58,220 bytes**, `.lowram` **22,772 bytes**, and the pool **333,900 bytes**.
Changes in instruction selection/alignment explain why individual image deltas need not
sum exactly. Reports for these measurements are summarized in `docs/synth-s0-budget.json`.

The firmware is one compilation unit. `cold.h` therefore uses clang's `minsize` function
attribute, the size preference used by `-Oz`, on `ui_draw`, `ui_input`, `ed_handle`,
`usb_poll` and `con_exec`. Audio functions retain the original `-Os` preference.

Boot initialization moves `drums`, both master limiter structures, DUST, TEXTURE and
punch state to zero-initialized storage. Their few nonzero fields are installed after
`.data` has been copied and before persistence, recovery or any interrupt can read them.
Host builds preserve the old static initializers unless explicitly testing the new path.

The logo's existing RLE bytes compress from 4,635 to 2,313 bytes; all 86 icons use individual
LZ4 blocks (3,096 → 2,933 bytes including offsets). A shared bounded decoder writes to
short-lived stack arrays: 4,635 bytes only during splash drawing and 36 bytes for an icon.
No persistent RAM is reserved, and decoding is outside the audio interrupt. The drawing
input bytes and pixels are unchanged.

## FPU diagnostic

The optional `firmware/hal/fm1_fpu_probe.c` is a separate compilation unit using
`-mcpu=r3 -mfprev1 -ffp-contract=off` and the toolchain's `pi32v2/lib/r3/libm.a`.
The rest of the firmware keeps its existing compiler flags and integer DSP.

Stop playback and release synth notes, then run `fpu` in the serial console. The command
reports 1,024 dependent operations each for multiplication, division, `sqrtf` and `sinf`,
on CPU0 and on CPU1 when available. Each duration is a raw 24 MHz TIMER4 tick count;
divide by 24 for microseconds and by 1,024 for time per iteration. These timings include
loop/input-load overhead and are useful comparisons, not isolated instruction latencies.
Each result's exact IEEE-754 bits are printed and compared between the cores.

CPU0 disables interrupts around the benchmark and CPU1 hand-off/wait. The audio IRQ
therefore cannot submit another job to the single-job mailbox while the probe owns it.
The normal CPU1 timeout/fallback handling remains in force. The command rejects running
transport or active synth voices before this short diagnostic interval.

For interrupt preservation the same eight independent live float accumulators are run
once with interrupts disabled and once with interrupts enabled. While the latter runs,
TIMER5 executes an independent float expression through the real `fm1_isr.S` wrapper.
All eight resulting bit patterns are compared, not merely their printed checksum.
`fpu_irq_preserved` is **1** for equality with at least one observed interrupt, **0** for
a mismatch, and **-1** if no interrupt was observed. `fpu_irq_count` records the coverage.
This verifies the real TIMER5 wrapper and float register ABI under the running firmware;
it is not evidence about arbitrary future ISR implementations or changes to compiler flags.

**Hardware FPU timings and preservation results remain pending.** Compilation and
assembly inspection cannot substitute for an FM-1 run. No timing or hardware pass is
claimed in this document. Capture the entire `fpu` reply alongside the usual `status`
reply (`cpu1_timeouts`, stack minima and audio load) before considering float DSP work.

## Verification

- `python3 tests/synth_s0_test.py`: stable full/lite set IDs, exactly 54,284 sample bytes
  removed, retained GM kit, invalid profile rejected; all 87 UI asset blocks compare
  byte-for-byte against their raw variants. Truncated blocks and undersized output
  buffers are rejected under AddressSanitizer and UndefinedBehaviorSanitizer.
- `tests/synth_boot_test.c`, built with `FELUCCA_BOOT_BSS=0` and `=1`: nonzero boot fields
  checked explicitly, then 131,072 samples rendered. Both hashes: `bfe20079f86c1d68`.
- Target builds: full control, each individual optimization, combined full, combined lite,
  and lite with FPU probe all pass app slot, RAM, lowram, pool, RAM-code and HAL-access checks.
- Existing full-profile golden hashes are validated by the branch integration suite.
  The lite profile intentionally lacks the three preset entries named above.

## Integrated synth images

The integrated S1–S8 target builds are recorded separately in
`docs/synth-build-manifest.json`, with SHA-256 hashes of every delivered binary and report.
The normal lite image is **528,216 bytes**, leaving **53,348 bytes** in the application slot;
the optional FPU image is **532,872 bytes** and the single-core lite image **526,288 bytes**.
The final full-library link exceeds XIP by **1,208 bytes** and is deliberately refused.
A failed build removes its old package, app binary and reports, so an earlier successful
profile cannot be mistaken for the requested one.

All target-budget entries now have explicit measurements. The five new functions cost
139 (`mod_begin`), 59 (`mod_voice`), 22 (`ladder_run`), 2,034 (`rev_hall`) and 390
(`terrain_render`) in the existing instruction-loop cost model. These are static code
budgets, not on-device CPU timings. The old IRQ budget 174 was already stale on `ram`:
both `ram` and `synth` measure 268. The previously missing FM6 budgets were measured on
`ram` as 72/26/362; `synth` is 72/26/361. Only those previously missing/stale entries and
the five new functions were added; every other existing budget remains unchanged.
