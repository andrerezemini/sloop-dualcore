# Synth upstream submission drafts

These are reviewable local drafts, not posted messages. Replace hardware fields
with captured FM-1 results before publication. The development build and exact
artifact checksums are described in [SYNTH.md](../SYNTH.md) and
[synth-build-manifest.json](synth-build-manifest.json).

## SLOOP issue / pull request: expressive MIDI and modulation

**Title:** Add expressive MIDI sources and four modulation slots per synth part

CC1, channel aftertouch, CC11 and CC121 now follow SLOOP's existing input routing.
Four optional slots map controllers, per-note sources and two LFOs to continuous
synth controls. Audio rendering reads a per-part shadow so concurrent UI edits
are not overwritten during modulation or preset fades. Randomness is per part
or voice, including when a part renders on CPU1. Discrete engine selectors are
excluded using descriptors for the sound actually being rendered.

The integration includes native and web controls, parameter-lockable amounts,
legacy project/preset migration and fixed flash storage bounds. Existing 182
audio hashes are retained under normal, threaded, delayed and timeout modes.
Tests cover controller routing/reset, duplicate destinations, fades, concurrent
edits, enum guards, all second-LFO modes, storage failure and ThreadSanitizer.

Review S1's controller plumbing separately if the matrix/persistence changes
are too large for an initial upstream patch. FM-1 keyboard and CPU figures:
**pending**.

## SLOOP issue / pull request: reverb types and section overrides

**Title:** Add optional ROOM, SPRING and stereo HALL beside the original FDN

This ports Felucca's GPL-3.0-only reverb models with attribution. FDN remains type
0 and preserves existing renders. A union reuses the 17,368-byte lowram line
allocation; HALL runs at half rate and contributes a stereo side signal. Type
changes fade the previous return and clear state, while FREEZE respects the
selected model. Delay and reverb overrides are independently inherited by song
sections. FUN7 reads all six legacy project layouts; the web editor supports
the new section fields and refuses incompatible backup restoration.

Host verification covers twelve-second extremes of all models, stereo output,
FREEZE/model switching, migration, section saving, native/web controls and
dual-core equivalence. Real audio timing and listening comparisons: **pending**.

## SLOOP pull request / Felucca discussion: integer LP24

**Title:** Four-state fixed-point ladder with bounded self-oscillation

ANALOG gains LP24 and the track filter gains optional ladder processing and
resonance. The design follows Surge's economical nonlinear four-pole approach
but solves the linear feedback loop; it is not a direct copy of the five-state
legacy implementation. Audio/coefficient values are Q15, four internal states
are Q23, and existing tanh/cutoff tables are reused. No floating-point render,
allocation or oversampling is introduced. LP/BP/HP and neutral track SVF remain
unchanged. Independent drum buses retain separate filter states, reset on
bypass reentry.

Host measurements show −24.06 dB/octave and bounded ten-second self-oscillation.
Extreme cutoff/resonance inputs pass UBSan. Eight-voice target load and listening
test: **pending**. Source and measurement details: [SYNTH_DSP.md](../SYNTH_DSP.md).

## Follow-up patch groups and release notes

- Budget work: stable `full`/`lite` sample-set numbering, cold function size
  optimization, boot initialization, lossless asset compression and optional
  isolated FPU diagnostic. Exact isolated measurements are in SYNTH_S0.md.
- Wave Terrain: five Plaits analytic surfaces in Q15, MIT notice retained,
  eight presets and independent float-reference validation. Existing engine
  indices remain stable. The optional sample-lite build supplies flash headroom.
- Presets: eight expressive ANALOG sounds plus eight TERRAIN sounds, measured
  using the existing loudness pipeline. Only their trims and golden entries
  are appended; existing values remain unchanged.
- Release: attach normal lite `.fwsc`, source commit, manifest, limitations and
  actual hardware results. Clearly identify the omitted FLUTE/SCRCH sets,
  neutral defaults, new formats and backup/rollback procedure. Keep the FPU
  bench image separate from the ordinary package.
- Community listing: propose the fork and its SYNTH.md to `aroum/fm1-custom-fw`
  after the release URL exists; do not advertise a hardware pass before one.
