// Project.h: a 3dmiX project (the .3dmix file and its track files) in
// memory. Portable: no Haiku headers.
//
// Formats (all integers and floats big-endian), as 3dmiX reads and writes
// them (TrackView::LoadMaster/SaveMaster, TrackViewer::Load/Save,
// SimpleObject::Load/Save, SampleCache::Load/Save, TimeView, TCtrlView):
//
//   project  "MAST", int32 track count,
//            per track: char path[2048] (NUL-padded), float x, float y,
//            then float selection start, float selection end, int32 (23),
//            float ticks, int32 beats per bar, char ticks_text[32]
//   track    "!TRK", int32 n, char name[n] (NUL-terminated, n even),
//            int32 clip count, per clip:
//            "SIMP" "RAW_", int32 n, char source[n] (as above),
//            float start, float end, float loop_in, float loop_out,
//            int32 frames, int16 samples[frames] (mono, 44.1 kHz)
//
// The track paths stored in a project are absolute and usually stale; 3dmiX
// opens the file of the same name in the project's own folder (mangle()).
#ifndef POWERMIX_PROJECT_H
#define POWERMIX_PROJECT_H

#include <cstdint>
#include <string>
#include <vector>

namespace pmx {

const int kSampleRate = 44100;
const float kPositionLimit = 256.0f;	// x and y are clamped to +-256

// A sample placed on a track: it repeats [loopIn, loopOut) of its samples
// from start to end (seconds on the project's timeline).
struct Clip {
	std::string source;		// name of the sound it came from ("fill_1.wav")
	float start = 0, end = 0;
	float loopIn = 0, loopOut = 0;
	std::vector<int16_t> samples;

	// SimpleObject::GetData: writes count frames starting `offset` frames
	// after the clip's start into out. Frames past the end of the sound are
	// left alone (silence, since the caller cleared them). Returns true if
	// it wrote anything. Keeps the original's quirk: the end-of-clip check
	// counts time from the start of the current loop pass, so after the
	// first pass up to one buffer may play past `end`.
	bool GetData(int16_t* out, int32_t offset, int32_t count) const;
};

struct Track {
	std::string name;		// also the track file's name
	float x = 0, y = 0;		// position on the mixer's floor, -256..256
	bool addedByHand = false;	// runtime only: its 3D level bar is blue
	std::vector<Clip> clips;

	// TrackViewer::fill_buffer: count frames starting at frame `at` of the
	// timeline. Clips are copied in order, so a later clip overwrites an
	// earlier one where they overlap (they're not summed). Returns true if
	// any clip played.
	bool Fill(int16_t* out, int32_t count, int32_t at) const;
	float Length() const;	// end of the last clip, seconds
};

struct Project {
	std::vector<Track> tracks;
	float selectionStart = 0, selectionEnd = 0;
	int32_t timeUnknown = 23;	// written by TimeView::Save, meaning unknown
	float ticks = 0;			// the "Ticks" field's value
	int32_t beatsPerBar = 4;	// the "4/4" pop-up
	std::string ticksText;		// the "Ticks" field as typed

	float Length() const;
};

// Reading and writing. Errors return false and set *error to a message.
bool ReadTrack(const std::string& path, Track& track, std::string* error);
bool WriteTrack(const std::string& path, const Track& track, std::string* error);
// Reads the project and every track it names, from the project's folder.
// A missing or unreadable track is skipped, as 3dmiX does; its name is
// added to *skipped if given.
bool ReadProject(const std::string& path, Project& project, std::string* error,
	std::vector<std::string>* skipped = nullptr);
// Writes the track files and the project into folder (TrackView::Save).
bool WriteProject(const std::string& folder, const std::string& fileName,
	const Project& project, std::string* error);

}	// namespace pmx

#endif
