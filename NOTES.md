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

## The 3D view's renderer (TSoundView)

- Software renderer into a **1024x480 B_CMAP8 bitmap**; `Draw` blits it at
  x = min(0, round((window width - 1024) / 2)), y = 0 (centred on the
  *window*, not the view), only the dirty rectangle (`clear` zeroes the
  union of this and the previous frame's boxes; index 0 = black).
- Projection (`change_view_point*`), angles (a, b, c), for (x, y, z):
  x1 = x cos a - y sin a; y1 = x sin a + y cos a; z1 = x1 sin c + z cos c;
  d = y1 sin b + z1 cos b; s = 300 / |d - 430|;
  X = round((x1 cos c - z sin c) s + 512); Y = round((y1 cos b - z1 sin b) s + 256).
  Start angles (0, -0.32, 0.1) (fields 0x1348/-0x134c/0x1350). y is up
  when negative.
- Scene vectors (constructor): floor grid x, z = -256..256 step 21.333 at
  y = 0; in front (z = 290) an x double arrow -140..140 with arrowheads and
  the letters L and R from segments (colour 200 = red ramp); a z double
  arrow at x = 290. Line colour: 200 red ramp, 100 green-grey ramp, lines
  with constant z one table (0x38a50, set elsewhere), others blue.
- Ramps (32 entries, colour of intensity i, snapped with IndexForColor):
  blue (0, 0, G[7i]); red (G[7i], 0, 0); green-grey (G[i], G[2.6i], G[i]);
  20 depth blues (0, 0, G[G[(k+8)/3.3 i]]), k = 0..19; grey G[2.2857i] x3.
  G = the `Gamma` table (0x584e0, 256 bytes; regenerate, don't copy).
- Lines (`fline` clips, then `anti_fline`): Wu-style, two pixels per step,
  writing ramp[31 - f] and ramp[f] (overwrite, no blend).
- Translucent faces (`BuildMappers`, `hline`, `fill_4`): 6 colours x 4
  levels, each a 256-entry map index -> IndexForColor(G[cmix(c, bg)] per
  channel); faces remap the pixels under them. Colours: red/green/blue
  c = 55, 93, 131, 229; cyan/magenta/yellow c = 35, 73, 111, 229.
  cmix(a, b) = min(255, round((((a*b) >> 9) + round(b/1.9) + round(a*0.95)) * 0.8)).
- Polygons: 4 vertex indices + mapper; Shell sort by mean vertex depth,
  far to near (`sort_polys`), then `fill_4` each (`render_polys`).
- Frame (`draw_vectors`): vectors, particles (2048 random points, r/g/b/w),
  polygons, labels (under the channel-list semaphore).
- **Every float-to-int conversion truncates.** Ghidra shows `ROUND(x)`, but
  gcc2 emits fnstcw / `mov ah,0xc` / fldcw / fistp (round toward zero) for
  all 389 conversions in the binary. Using lrint instead put lines and
  faces a pixel off (78% vs 94% identical floor pixels). The Gamma table
  is (int)(255 (i/255)^(1/1.45)), all 256 entries.
- `b_sqrt` (projection's |d|): bit-trick guess + one refinement; accurate
  enough that it makes no pixel difference, kept anyway.
- IndexForColor: R5's 15-bit inverse map (dumped with tools/beos/dumpcmap.cpp
  to vm/orig/cmap/cmap.bin, not shipped) follows no simple nearest-colour
  rule; Haiku's (<<3 expansion, weighted distance) agrees on 95.6% of it.
  PowerMix uses Haiku's rule; `PMX_CMAP=vm/orig/cmap/cmap.bin` makes
  tools/host/render_scene use R5's for comparisons.
- Result (tools/host/render_scene vs vm/ref/mixer-controls.png, the-lynx,
  view origin at screen (30, 50)): 98.0% identical pixels with R5's map,
  97.6% with Haiku's; the rest is label text, the level bars (the reference
  was playing) and a dot where shallow lines step a row (the first pixel
  after a step is one ramp entry brighter; cause not found).
- Hidden mixer keys (TSoundView::KeyDown): arrows move the selection 15,
  +/- zoom (focal x1.05, 30..900), , and . move the camera (. clamps the
  wrong way), A select all, D demo rotation, M mute, S solo, P particle
  fountains, W back to the tracks. Command-drag = vertical_move (visual
  height only; Channel's reverb level is always 0). The transport panels
  (composit_pannel1/2) are empty in this build.
