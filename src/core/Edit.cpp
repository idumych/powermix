// Edit.cpp: see Edit.h. Each function names the 3dmiX function it follows.
#include "Edit.h"

#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

// TrackObject's setters all end by keeping a clip at least 0.01 s long and
// its loop at least 0.01 s past the skip
void
Normalize(Clip& clip)
{
	if (clip.end - clip.start < 0.01f)
		clip.end = clip.start + 0.01f;
	if (clip.loopOut < clip.loopIn + 0.01f)
		clip.loopOut = clip.loopIn + 0.01f;
}

}	// namespace

// TrackObject::ToGrid
float
ToGrid(float time, float grid)
{
	if (grid <= 0)
		return time;
	return float(grid * (long double)int(float(0.5L + (long double)time / grid)));
}

float
ClipLength(const Clip& clip)
{
	return clip.end - clip.start;
}

float
ClipLoop(const Clip& clip)
{
	return clip.loopOut - clip.loopIn;
}

float
SoundLength(const Clip& clip)
{
	return float(clip.samples.size()) / kSampleRate;
}

// TrackObject::SetPos: moves the clip, keeping its length
void
SetClipPos(Clip& clip, float pos, float grid)
{
	pos = ToGrid(pos, grid);
	if (pos < 0)
		pos = 0;
	float length = clip.end - clip.start;
	clip.start = pos;
	clip.end = pos + length;
	Normalize(clip);
}

// TrackObject::SetLength: the loop shrinks with a clip made shorter than it
void
SetClipLength(Clip& clip, float length, float grid)
{
	if (length < ClipLoop(clip))
		SetClipLoop(clip, length, 0);
	if (length < 0)
		length = 0;
	clip.end = ToGrid(clip.start + length, grid);
	Normalize(clip);
}

// TrackObject::SetStartSkip: where in the sound the clip starts playing
void
SetClipSkip(Clip& clip, float skip)
{
	if (skip >= clip.loopOut)
		skip = clip.loopOut - 0.01f;
	if (skip >= SoundLength(clip))
		skip = SoundLength(clip) - 0.01f;
	if (skip < 0)
		skip = 0;
	clip.loopIn = skip;
	Normalize(clip);
}

// TrackObject::SetLoopPoint: the loop can't be longer than the clip
void
SetClipLoop(Clip& clip, float loop, float grid)
{
	loop = ToGrid(loop, grid);
	if (loop > ClipLength(clip))
		loop = ClipLength(clip);
	float out = clip.loopIn + loop;
	if (out < 0.01f)
		out = 0.01f;
	clip.loopOut = out;
	Normalize(clip);
}

// TrackViewer::redo_time
void
RedoTime(Track& track)
{
	for (size_t i = 1; i < track.clips.size(); i++) {
		float previousEnd = track.clips[i - 1].end;
		if (track.clips[i].start < previousEnd)
			SetClipPos(track.clips[i], previousEnd, 0);
	}
}

// TrackViewer::InsertAfter
void
InsertClip(Track& track, size_t index, Clip clip)
{
	index = std::min(index, track.clips.size());
	float pos = 0;
	if (index > 0)
		pos = track.clips[index - 1].end;
	SetClipPos(clip, pos, 0);
	track.clips.insert(track.clips.begin() + long(index), std::move(clip));
	RedoTime(track);
}

// #pragma mark - Editor

Editor::Editor(Project* project)
	:
	fProject(project),
	fHasUndo(false)
{
}

// TrackView::comit
void
Editor::Commit()
{
	fUndoTracks = fProject->tracks;
	fHasUndo = true;
}

// TrackView::undo: one level; undoing twice redoes
void
Editor::Undo()
{
	if (!fHasUndo)
		return;
	std::swap(fUndoTracks, fProject->tracks);
}

// TrackView::Copy: up to 1024 selected clips
void
Editor::Copy(const ClipSelection& selection)
{
	fClipboard.clear();
	for (const ClipRef& ref : selection) {
		if (fClipboard.size() >= 0x400)
			break;
		if (ref.first < fProject->tracks.size()
			&& ref.second < fProject->tracks[ref.first].clips.size())
			fClipboard.push_back(fProject->tracks[ref.first].clips[ref.second]);
	}
}

// TrackView::Clear: TrackViewer::delete_selection in every track
void
Editor::Clear(ClipSelection& selection)
{
	if (selection.empty())
		return;
	Commit();
	for (auto it = selection.rbegin(); it != selection.rend(); ++it) {
		if (it->first < fProject->tracks.size()) {
			Track& track = fProject->tracks[it->first];
			if (it->second < track.clips.size())
				track.clips.erase(track.clips.begin() + long(it->second));
		}
	}
	for (Track& track : fProject->tracks)
		RedoTime(track);
	selection.clear();
}

void
Editor::Cut(ClipSelection& selection)
{
	Copy(selection);
	Clear(selection);
}

void
Editor::Paste(size_t track, size_t after)
{
	if (fClipboard.empty() || track >= fProject->tracks.size())
		return;
	Commit();
	Track& target = fProject->tracks[track];
	size_t index = std::min(after, target.clips.size());
	for (const Clip& clip : fClipboard)
		InsertClip(target, index++, clip);
}

// TrackView::new_track
size_t
Editor::NewTrack(const std::string& name)
{
	Commit();
	Track track;
	track.name = UniqueTrackName(*fProject, name);
	fProject->tracks.push_back(track);
	return fProject->tracks.size() - 1;
}

// TrackView::DeleteChannels
void
Editor::DeleteTracks(const std::set<size_t>& tracks)
{
	if (tracks.empty())
		return;
	Commit();
	for (auto it = tracks.rbegin(); it != tracks.rend(); ++it) {
		if (*it < fProject->tracks.size())
			fProject->tracks.erase(fProject->tracks.begin() + long(*it));
	}
}

std::string
UniqueTrackName(const Project& project, const std::string& base)
{
	std::string stem = base.empty() ? "untitled" : base;
	for (char& c : stem) {
		if (c == '/')
			c = '-';
	}
	for (int n = 1; ; n++) {
		std::string name = n == 1 ? stem : stem + " " + std::to_string(n);
		bool used = false;
		for (const Track& track : project.tracks)
			used |= track.name == name;
		if (!used)
			return name;
	}
}

}	// namespace pmx
