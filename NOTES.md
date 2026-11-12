# PowerMix: notes on the original 3dmiX (BeOS R5)

Findings about Be's 3dmiX, the reference for this clone. `vm/` (the VM,
the original's files, reference screenshots) is git-ignored and must stay
out of the public tree.

## The original

- `/boot/beos/apps/3dmiX`, 490,434 bytes, Aug 8 2000, ELF i386 gcc2,
  **not stripped**. Signature `application/x-vnd.be.3dsoundmixer`; project
  file type `application/x-vnd.be.3dsoundmixer_master`.
- Sample mixes in `/boot/optional/sound/3D Mixes/`: `she-loves-it`
  (13 tracks), `the-lynx` (14), `the-price-of-things` (5). Each
  `<name>.3dmix` there is a **symlink** into `<name>/<name>-3dmix/`, next
  to the track files; `<name>-samples/` holds the source WAVs.
  Copies (Be's content, never to be published): `vm/orig/`.
- Opening a project from the command line needs the real file, not the
  symlink: `TrackView::TakeRef` uses `BEntry(ref, false)`.

      /boot/beos/apps/3dmiX "/boot/optional/sound/3D Mixes/the-lynx/the-lynx-3dmix/the-lynx.3dmix" &

## Files

- Project (`.3dmix`): magic `MAST`, big-endian int32 track count, then
  per track the track file's path (NUL-terminated) and more (to decode,
  from `TrackView::SaveMaster/LoadMaster`). The stored paths are absolute
  and stale (`/boot/optional/sound/3dSound/...`); the app still finds
  the tracks, so it falls back to the project's folder.
- Track file: magic `!TRK`, name, `SIMPRAW_`, source WAV name, then raw
  PCM. (`SampleCache`, `TrackObject`, `Channel`.)
- github.com/atomozero/beos-3dmix-protocol describes the format, but by
  byte statistics, with guessed fields; no licence. Trust the binary.

## UI (measured on BeOS at 1024x768x16; screenshots in vm/ref/)

- One window "3DmiX", BRect(30, 30, 730, 430), resizable, **not zoomable**.
- Menus (vm/ref/menus.png):
  - File: Open Project… (Alt-O), Save Project (Alt-S), Save Project As…,
    — Save Mix As WAVE…, — About 3DmiX…, — Quit
  - Edit: Undo (Alt-Z), — Clear, Cut (X), Copy (C), Paste (V)
  - Display: Show/Hide Mixer (Alt-T), Show/Hide Controls (Alt-U),
    Show/Hide beat marks (Alt-B), — Fast Display
  - Tracks: (Un)Mute Selection (M), (Un)Solo Selection (L), — New Track,
    Delete Tracks, Select All Tracks (A)
  - Misc: Ticks from Time Selection, Ticks from sample length, — Pause
- Left strip (Controls, Alt-U hides it): stereo VU meter (L/R, green
  segments, blue below), master volume slider 0%…100% with a diamond thumb.
- Track view: ruler (time 00:00:00 every 8 s, ticks), "Ticks: 467" field,
  "4/4" pop-up, play triangle; per track a name ("0-high-bass"), M/S
  buttons, a waveform box (played part red/green, rest grey/white), source
  WAV name in blue, a red triangle marker.
- Mixer (Alt-T): black 3D view, blue perspective floor grid, each track a
  translucent column (red/green/blue) with a label on a leader line;
  columns move/scale with level while playing.
- Resizing the window while the mixer shows leaves trails and the app
  stops redrawing (original bug).

## Code map (Ghidra, `decomp/3dmiX/`, from tools/ghidra pipeline)

- `TSoundApplication`: BApplication; window frame above; ArgvReceived and
  RefsReceived → `TWaveWindow::TakeRef` → `TrackView::TakeRef`.
- `TWaveWindow`: the window; `Switch`/`Switch_H` swap mixer/track views.
- `TSoundView` (9k lines): the 3D mixer: software renderer (`fline`,
  `anti_fline`, `fill_4`, `sort_polys`, `render_polys`, `draw_labels`,
  `draw_particules`), hit-testing and dragging (`handle_hit`,
  `move_selection`, `vertical_move`), and the audio mix (`bi_mix`, `w_mix`,
  `final_mix`, `get_reverbe_level`). Threads: `window_control_task`
  (mixing, levels, 11.6 ms per 512 frames at 44.1 kHz), `do_anim`.
- `TrackView`/`TrackViewer`/`EditView`/`TimeView`: track lanes, editing,
  clipboard, undo; `SaveMaster`/`LoadMaster` = project I/O.
- DSP: `Biquad`, `FFTSHIFTER`, `Channel` (gain, left/right).
- Audio out: old R3 API, `BDACStream` + `BSubscriber` (`init_stuff`).

## The BeOS VM (`BeOS-3dmix`, vm/beos/)

- Copy of the SoundPlay project's BeOS 5 disk; 1024x768x16; PS/2 mouse.
- **Audio:** VirtualBox has no card BeOS 5 drives, and without an audio
  output 3dmiX's stream never starts, so the 3D scene stays empty. Fix:
  R5's own null driver ("None Out", legacy add-on) made the default
  output with `tools/beos/setout.cpp` (built in the guest:
  `gcc -o setout setout.cpp -lbe -lmedia`), then a cold reboot. The
  Media preferences don't offer it and the server won't pick it itself.
- Files out: `vm/beos/xfer.vdi` (fixed VDI, IDE primary slave). Guest:
  `tar cf /dev/disk/ide/ata/0/slave/0/raw …; sync`; host:
  `tools/xfer-out.sh DEST`. `tools/brun.sh 'cmd'` runs a command in the
  focused Terminal and prints its output here. `tools/btype.sh` types a
  text file in. `tools/wait-desktop.py` waits for the Deskbar after boot.
- Terminal: `N=27 beos-terminal.sh` on this disk (one more app link than
  a fresh install).
