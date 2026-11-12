# Test plan: PowerMix

## Fixtures

- `tests/fixtures/demo/demo.3dmix`: PowerMix's demo project (six
  synthesised tracks, made by `tools/host/make_demo.cpp`). Copy the folder
  to the test machine (Haiku VM: `/boot/home/pmx-test/demo`; Linux: the
  scratch folder) as `$FIX/demo`.
- A sound to import: `$FIX/demo/../click.wav`, any 16-bit WAV; the
  release script makes one with `tools/host/render_mix`, or use a BeOS R5
  sample WAV (MS ADPCM) if one is at hand.

Every check runs the app's script option and saves its stdout as
`dist/V/evidence/{p}-<check>.txt`. On Linux run with
`COSMOE_AUDIO_BACKEND=null` and a throwaway `HOME`.

## Fresh state

PowerMix writes no settings; nothing to delete.

## Feature checks

### open-mix
Steps: `PowerMix --do 'open $FIX/demo/demo.3dmix; dump; quit'`.
PASS when: the output has `tracks=6 ` (EVIDENCE_1). Also screenshot the
window after the Show/hide mixer shortcut: six labelled columns on the floor.

### export-mix
Steps: `PowerMix --do 'open $FIX/demo/demo.3dmix; export $TMP/mix-{p}.wav; quit'`.
PASS when: `frames=353280` and a four-digit RMS (EVIDENCE_2): the demo's
eight seconds, mixed, not silent.

### import-sound
Steps: `PowerMix --do 'open $FIX/demo/demo.3dmix; drop $FIX/click.wav 1 9; dump; quit'`.
PASS when: track 1 (hats) has `clips=2 ` (EVIDENCE_3).
