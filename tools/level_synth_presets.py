#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Level only synth's appended factory sounds, preserving every upstream trim.

Render tests/preset_level.c first, then:
  python3 tools/level_synth_presets.py build/host/synth-presets.raw build/host/synth-presets.txt
Rebuild/re-render until changes=0. Uses level_presets.py's BS.1770 measurement,
phrase roles and target; keeps conditional SLICE/TERRAIN rows intact.
"""
import re
import sys
from pathlib import Path
import numpy as np
import level_presets as level

NAMES = {
    'LADDER ACID', 'WHEEL LEAD', 'AFTER BRASS', 'SLEW PAD', 'EXPR KEYS',
    'RAND PLUCK', 'DUAL ORBIT', 'HALL SWELL', 'ORBIT KEYS', 'RIDGE BASS',
    'GLASS ORBIT', 'RIDGE PAD', 'TERRAIN BELL', 'CONTOUR LD', 'TOPO PLUCK',
    'ISLAND DRONE',
}

def main(raw, report):
    samples = np.memmap(raw, dtype=np.int32, mode='r')
    source = level.OUT.read_text()
    before = {name: int(value) for name, value in
              re.findall(r'/\* \d+\.\d+ (.*?) \*/ (-?\d+)', source)}
    measured = {}
    for i, row in enumerate(Path(report).read_text().splitlines()):
        engine, preset, role, name = row.split(None, 3)
        if name not in NAMES:
            continue
        audio = samples[i * level.SEG:(i + 1) * level.SEG].astype(float) / 32768.
        loudness = level.lufs(audio)
        if loudness is None:
            raise ValueError(f'{name}: silent render')
        trim = max(-40, min(23, before[name] +
                   round((level.TARGET + level.ROLE[int(role)] - loudness) * 2)))
        measured[name] = trim
        print(f'{name:14s}: {loudness:6.2f} LUFS  trim {before[name]:3d} -> {trim:3d}')
    if measured.keys() != NAMES:
        raise ValueError(f'missing synth presets: {NAMES - measured.keys()}')
    def update(match):
        prefix, name, value = match.groups()
        return f'{prefix}{measured.get(name, int(value))}'
    updated = re.sub(r'(/\* \d+\.\d+ (.*?) \*/ )(-?\d+)', update, source)
    level.OUT.write_text(updated)
    print(f'changes={sum(before[n] != t for n, t in measured.items())}; upstream trims preserved')

if __name__ == '__main__':
    main(*sys.argv[1:])
