# Synth effects and project compatibility

The `synth` branch adds `GLO > REVERB > TYPE`: FDN (0), ROOM (1), SPRNG (2), HALL (3).
FDN is the original SLOOP algorithm and remains the default. ROOM's four combs,
SPRING's ten dispersive allpasses, and HALL's eight-line stereo conference network
are adapted from Leo Kuroshita's GPL-3.0-only [Felucca fx.c](https://github.com/hugelton/Felucca/blob/main/firmware/src/fx.c).
The copyright notice stays with the port.

HALL processes the reverb at half the sample rate, interpolates its output, and
adds its stereo difference to the left bus while subtracting it from the right.
Changing type fades the preceding model's return over one audio block and clears
all shared buffers and state before the new model starts. Delay and chorus keep
their tails. FREEZE preserves the new models' state without interpreting HALL's
indices as FDN samples.

The reverb line allocation remains 17,368 bytes in `.lowram`: one union holds
either the legacy lines or the new comb lines and HALL state. The diffuser/spring
union uses 1,996 bytes including alignment (the old diffuser was 1,994 bytes).
No extra delay lines or allocation occur in the audio callback. The small model
indices and spring integrators are part of the existing `fx` control state.

On SONG, turn SELECT for the chain, section REVERB, and section DELAY pages.
The REVERB knobs edit MODE (INHERIT/SECTION), TYPE, SIZE and DAMP. The DELAY knobs
edit TIME, FEEDBACK, COLOR and MIX. The FX button toggles inheritance on either
page. SAVE persists the edited section settings with the chain; playback must be
stopped to edit. INHERIT retains the current global values on a section change,
including overrides from the preceding section. Stopping song mode restores the
working project's globals. The two effect groups are independent. Sections from
older versions inherit both groups, preserving previous playback behavior.

## Stored format

The planning document anticipated format 4, but upstream already writes FUN6.
This branch therefore writes FUN7 and reads FUN1 through FUN6. The FUN6 layout is
frozen at 3,816 bytes and decoded with its original engine parameter boundary 63;
old engine locks move to the new boundary 81. All new parameters receive their
defaults. Legacy projects load with FDN and section FX inheritance.

FUN7 uses 3,684 bytes, within the existing 3,840-byte flash object. Generic track
parameters are signed bytes; the engine's eight parameters remain 16-bit values.
All current generic parameter minima and maxima fit the signed-byte encoding.

| Offset | Content |
|---|---|
| 0 | magic `FUN7`, uint32 little endian |
| 4 | size 3684, uint32 |
| 8 | legacy global array, 32 int16 |
| 72 | selected track, drum delay, two existing texture bytes |
| 76 | reverb type, section FX mask (bit 0 delay, bit 1 reverb), two padding bytes |
| 80 | four tracks, 900 bytes each |
| 3680 | FNV-1a checksum of preceding bytes |

Inside each track: signed parameters 0..80 at offset 0, one padding byte,
eight int16 engine parameters at 82, engine/preset at 98/99, 64 ten-byte steps
at 100, nudge/condition bytes at 740, and 24 four-byte locks at 804.

Validation: `tests/project_test.c` verifies all six migrations, checksums, the
full signed parameter range, moved locks and separate FX overrides.
`tests/reverb_types_test.c` renders every model at both size/damping extremes
for twelve seconds, checks bounded tails and HALL stereo, exercises FREEZE and
repeated model switches, and checks shorter even bus blocks. `song_ui_test.c`
checks page navigation, edits, stopped-only guards and framebuffer bounds.
Real FM-1 `audio_max_us` and listening tests remain hardware measurements.
