# PowerMix

A 3D sound mixer for Haiku and Linux in the spirit of **3dmiX**, the mixer
that came with BeOS 5. Each track of a mix stands as a translucent column
on a floor in front of the listener: drag it left or right to pan it,
nearer or farther to make it louder or quieter. The columns pulse with the
music.

![The 3D mixer on Haiku](docs/screenshots/haiku-mixer.png)
![The tracks on Haiku](docs/screenshots/haiku-tracks.png)

PowerMix is a new program, written from a study of the original: it was
run on BeOS 5 in a virtual machine, and its behaviour, file formats and
drawing were read from its code. It contains none of Be's code, images,
sounds or text (beyond menu labels). It opens 3dmiX's `.3dmix` projects,
including the sample mixes that came with BeOS 5.

## What it does

- **The 3D mixer** (Display ▸ Show/hide mixer): drag tracks around the
  floor, drag empty space to turn the view (flick it and it keeps
  spinning), Shift-drag to move in depth only. Keys while it shows: arrows
  move the selection, + and − zoom, A selects all, M mutes, S solos, D
  turns the view slowly, P throws particles from the loudest tracks, W
  goes back to the tracks. Muted tracks sink.
- **The mix** is 3dmiX's: gain from depth, a sine/cosine pan and the delay
  between the ears from the place on the floor. Against the original's
  own "Save mix as WAVE", half of the mixing blocks match sample for
  sample and the rest differ by a frame at some block boundaries.
- **Tracks** hold clips that loop part of a sound. Drag a clip to move it
  (Shift-click to select several: they move together),
  its ends to trim it, the red handle below it to set its loop; Ctrl snaps
  to the beat, Shift-resizing resamples the sound to fit and Option-resizing
  stretches it. The small squares along a clip's top are its fades: drag
  them in to fade it in from and out to silence. Clips never overlap: later ones are pushed back. Cut, copy,
  paste, clear, and undo and redo as far back as 256 MB of history holds
  (3dmiX had one level).
- **Sounds** come in by dragging files onto a lane (a clip there) or
  anywhere else (a new track). WAVE files (PCM, and MS ADPCM as the BeOS
  sample sounds use) are read by PowerMix itself; other formats go through
  the Media Kit.
- **Recording** (Tracks ▸ Record, Alt-R; Ctrl-R on Linux): arm a track
  (its R in the modern look's track list, or Tracks ▸ Arm selected track
  for recording), press record and play along: the mix plays while the
  default audio input is recorded, and the take becomes a clip in the armed
  track where you started. A clip playing at that moment ends there, clips
  the take reaches are pushed back, and Undo removes it. With no track
  armed the take gets a track of its own. It stops by itself where the loop
  starts over. A click sounds on the ruler's beats while you record, after
  a one-bar count-in (both can be turned off in the Tracks menu), and
  Misc ▸ Recording latency… lines takes up with what you heard. Not on Haiku arm64, which has no Media Kit yet.
- **Moves** (floor automation, as Ardour automates a mix): turn on Tracks ▸
  Record moves (or the inspector's button) and drag a track while the mix
  plays, in the 3D view, with the arrows or on the inspector's floor plan.
  Its path is written as you go, and moving through a stretch again
  replaces it. On playback the track follows its path, which the modern
  look draws on the floor. Without Record moves, dragging a track with a
  path moves the whole path. Tracks ▸ Clear moves of selected tracks (or
  the inspector's Clear moves) leaves a track where it is. Saved mixes
  follow the paths.
- **The sound editor** (double-click a clip): select, cut, copy, paste, and
  the filters Zero, Change volume, Max volume, Echo, Chorus, Metal, Fade and
  Compress, done as 3dmiX did them.

  ![The sound editor on Linux](docs/screenshots/linux-editor.png)
- **What 3dmiX's files can't hold** (fades, moves) is saved beside the project in
  `<project>.powermix`, so 3dmiX still opens the project without it.
- **The time ruler** sets the part that loops (and is saved by File ▸ Save
  mix as WAVE); Ticks and the 3/4…7/4 menu set the beat grid, and Misc ▸
  Ticks from… works it out from the selection or a clip.
- **A modern look** (Display ▸ Modern look), if you'd rather: the same
  mixer in a dark layout with a toolbar (start, pause, time, beat grid,
  master volume, meter, Save mix), a track list with mute and solo, the
  3D stage drawn as smooth translucent columns over a glowing floor, the
  lanes, and an inspector for the selected track. The inspector shows its
  place on a floor plan you can drag, and what the mix makes of it: gain,
  pan, delay between the ears. Stage | Split | Timeline chooses what
  shows. PowerMix remembers the choice and each look's window size.

  ![The modern look on Haiku](docs/screenshots/haiku-modern.png)
- **About PowerMix** forms the name from a swirl of dots while a chord
  swells, as 3dmiX's About box did.

  ![About PowerMix](docs/screenshots/haiku-about.png)

## What's different from 3dmiX

On purpose:

- Haiku's conventions: menu items in sentence case with "…", Quit has a
  shortcut (Alt-Q, Ctrl-Q on Linux), the window is titled PowerMix.
- Links to projects are followed (3dmiX opened the BeOS samples' links as
  empty projects), and so are links that copying through zip, a FAT or
  NTFS volume or Windows turned into small files holding their target's
  path. A link to nothing says where it pointed.
- A saved mix is a correct WAVE file (3dmiX's header overwrote its first
  11 frames).
- Without an audio output the mix still runs, silently (3dmiX stopped).
- Sounds at other sample rates are resampled to 44.1 kHz (3dmiX assumed
  44.1 kHz); stereo sounds become mono, as in 3dmiX.
- The About window and its sound are PowerMix's own, made the same way.

Not (yet) like the original:

- Colours in the 3D view are chosen from BeOS's palette by Haiku's rule;
  BeOS 5's own table differs for about 4% of colours.
- The time ruler's tick spacing follows the original's look, not its code.
- On Linux the Ticks box's controls are a little cramped.

![The 3D mixer on Linux](docs/screenshots/linux-mixer.png)

## Platforms

| Platform | Package | Tested |
|---|---|---|
| Haiku R1/beta6, Renku (x86_64) | `.hpkg` | installed and run in a Haiku R1/beta6 VM: the release's script checks (open, export, import, modern look, fades and moves saved and reopened, undo and redo) and screenshots; recording into a track from the VM's sound card, whose input is silent (VirtualBox's null driver), so a real microphone's sound reaching the take is untested |
| Linux (x86_64, aarch64) | AppImage through [Hokku](https://github.com/idumych/hokku) | x86_64: run under Xvfb (X11) for the release's script checks, and recording from a test file through Hokku's capture; both started in CI under X11 and headless Wayland |
| Haiku arm64 | not released yet | cross-built only; not yet run on Haiku arm64 |

## Building

Haiku: `make` (needs the Media Kit, which Haiku has). Linux with Hokku:
`make COSMOE_ROOT=/usr` (recording needs a Hokku with `BMediaRecorder`,
from November 2026 on). Host tests on any system: `make test` (more of
them run when a copy of the BeOS 5 sample mixes is in
`vm/orig/boot/optional/sound/3D Mixes`).

`tests/fixtures/demo/` is a small demo project made by
`tools/host/make_demo.cpp`. `PowerMix --do 'open FILE; dump; export
OUT.wav; drop SOUND LANE TIME; quit'` drives the app from scripts.

`NOTES.md` records what was learned about 3dmiX and how; the tools used to
study it (a BeOS 5 VM, Ghidra scripts) are in `tools/`.

## Licence

MIT. 3dmiX was made by Be, Inc.; PowerMix is not affiliated with Be or
its successors.
