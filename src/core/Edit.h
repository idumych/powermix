// Edit.h: changing clips and tracks as 3dmiX's TrackObject, TrackViewer
// and TrackView do. Portable: no Haiku headers.
//
// A clip has a position (start), a length (end - start), a skip (loop in:
// where in its sound playing begins) and a loop length (loop out - loop
// in). In a track, clips play in order and never overlap: after a change,
// later clips are pushed back (TrackViewer::redo_time).
#ifndef POWERMIX_EDIT_H
#define POWERMIX_EDIT_H

#include "Project.h"

#include <set>
#include <utility>

namespace pmx {

// With Ctrl held, times snap to a grid of 60 / ticks seconds (get_ibeat);
// grid 0 = no snapping.
float ToGrid(float time, float grid);

// TrackObject's setters
void SetClipPos(Clip& clip, float pos, float grid);
void SetClipLength(Clip& clip, float length, float grid);
void SetClipSkip(Clip& clip, float skip);
void SetClipLoop(Clip& clip, float loop, float grid);
float ClipLength(const Clip& clip);
float ClipLoop(const Clip& clip);
float SoundLength(const Clip& clip);	// SampleCache::Length

// TrackViewer::redo_time: push clips back so none starts before the end of
// the one before it
void RedoTime(Track& track);
// TrackViewer::InsertAfter: put clip at index, starting where the clip
// before it ends (or at 0)
void InsertClip(Track& track, size_t index, Clip clip);

// A recording made from `start` seconds becomes a clip there. A clip that
// was playing at that moment ends there (the take replaces what followed),
// and clips the take reaches are pushed back after it, so none overlap.
// Returns the take's index in the track.
size_t PlaceRecording(Track& track, float start, Clip take);

// Latency compensation (as Ardour aligns captures): a take recorded from
// `start` is heard `shift` seconds late, so it moves that much earlier;
// what would fall before 0 is dropped.
void AlignTake(float& start, std::vector<int16_t>& samples, float shift);

// Clips are addressed as (track, clip index)
typedef std::pair<size_t, size_t> ClipRef;
typedef std::set<ClipRef> ClipSelection;

// The edits behind the Edit menu, with one level of undo (TrackView's
// ghosts: undo brings back what the last edit changed).
class Editor {
public:
	explicit Editor(Project* project);

	// Call before every change; Undo() returns to that state.
	void Commit();
	bool CanUndo() const { return fHasUndo; }
	void Undo();

	void Copy(const ClipSelection& selection);
	void Clear(ClipSelection& selection);		// deletes the clips
	void Cut(ClipSelection& selection);
	bool HasClipboard() const { return !fClipboard.empty(); }
	// TrackView::Paste: after clip `after` of track (the boundary nearest
	// the last click), clips in clipboard order
	void Paste(size_t track, size_t after);

	// Tracks ▸ New track / Delete tracks
	size_t NewTrack(const std::string& name);
	void DeleteTracks(const std::set<size_t>& tracks);

private:
	Project* fProject;
	std::vector<Track> fUndoTracks;
	bool fHasUndo;
	std::vector<Clip> fClipboard;
};

// A clip of a whole sound, starting at 0, its loop the whole sound
// (SimpleObject made from a dropped file)
Clip MakeClip(const std::string& source, std::vector<int16_t> samples);
// Decoded audio to 3dmiX's format: mono 16-bit at 44.1 kHz. Channels are
// averaged; other rates are resampled linearly (3dmiX assumed 44.1 kHz).
std::vector<int16_t> ToClipSamples(const float* interleaved, size_t frames, int channels,
	double rate);

// "untitled", "untitled 2"... a name no track has yet
std::string UniqueTrackName(const Project& project, const std::string& base);

}	// namespace pmx

#endif
