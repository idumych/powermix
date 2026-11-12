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

PowerMix remembers the look and its window sizes in `PowerMix_settings`:
delete `~/config/settings/PowerMix_settings` (Haiku) or
`~/.config/cosmoe/PowerMix_settings` (Linux) before testing, so it starts
in the classic look.

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

### modern-look
Steps: `PowerMix --do 'open $FIX/demo/demo.3dmix; look modern split; select 2; dump; quit'`.
PASS when: the output has `look modern` and `tracks=6 ` (EVIDENCE_4). Also
screenshot the window while it is in the modern look (before `quit`): the
toolbar, the track list with bass selected, the stage with six columns,
the lanes and the inspector showing bass.

### fades-moves
Steps: copy the demo to a scratch folder, then
`PowerMix --do 'open $T/demo.3dmix; fade 0 0 2 3; move 1 0 -250 200; move 1 8 250 200; save $T/demo.3dmix; quit'`
and `PowerMix --do 'open $T/demo.3dmix; dump; quit'`.
PASS when: the second run shows kick's fade (`fade0=2.00/3.00`, EVIDENCE_5) and
hats' two-point path (`path=2`, EVIDENCE_6): both were saved in
`demo.3dmix.powermix` and read back.

### undo-redo
Steps: `PowerMix --do 'open $FIX/demo/demo.3dmix; fade 0 0 1 1; fade 2 0 1 1; undo; undo; redo; dump; quit'`.
PASS when: the output has `redo steps=1` (EVIDENCE_7) after `undo steps=0`,
and the dump shows kick's fade but not bass's.

### open-link
A user's copy of the BeOS 5 sample mixes had its top-level `.3dmix` links
flattened into files holding their target's path. Steps: in a scratch
folder `$L` holding a copy of the demo folder as `$L/demo`, write a file
`$L/demo.3dmix` containing just `demo/demo.3dmix`, then
`PowerMix --do 'open $L/demo.3dmix; dump; quit'`.
PASS when: the output has `tracks=6 ` (EVIDENCE_8): the file was followed to
the project and its tracks were found beside it.

### record (not a gate check)
Linux: with `COSMOE_AUDIO_CAPTURE_FILE=$FIX/click.wav`,
`PowerMix --do 'open $FIX/demo/demo.3dmix; countin off; arm 1; record 2; dump; quit'`
must print `record track=1 name=hats clips=2` with a non-zero peak.
Haiku: the test VM has no audio input; `record 1` must print
`record failed` (PowerMix shows an alert) and leave the project as it was.
