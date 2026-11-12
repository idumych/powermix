// Project.cpp: reading, writing and playing back 3dmiX projects.
#include "Project.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace pmx {

// #pragma mark - Big-endian file helpers

namespace {

class File {
public:
	File(const std::string& path, const char* mode)
		: fFile(fopen(path.c_str(), mode)) {}
	~File() { if (fFile) fclose(fFile); }
	bool IsOpen() const { return fFile != nullptr; }
	bool Ok() const { return fOk; }

	bool Read(void* data, size_t size)
	{
		if (fOk && fread(data, 1, size, fFile) != size)
			fOk = false;
		return fOk;
	}
	void Write(const void* data, size_t size)
	{
		if (fOk && fwrite(data, 1, size, fFile) != size)
			fOk = false;
	}
	uint32_t ReadU32()
	{
		uint8_t b[4] = {0, 0, 0, 0};
		Read(b, 4);
		return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
	}
	int32_t Read32() { return int32_t(ReadU32()); }
	float ReadFloat()
	{
		uint32_t u = ReadU32();
		float f;
		memcpy(&f, &u, 4);
		return f;
	}
	void Write32(uint32_t v)
	{
		uint8_t b[4] = { uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v) };
		Write(b, 4);
	}
	void WriteFloat(float f)
	{
		uint32_t u;
		memcpy(&u, &f, 4);
		Write32(u);
	}
	// int32 length + string, NUL-terminated and padded to an even length
	// (TrackViewer::Save, SimpleObject::Save)
	void WriteString(const std::string& s)
	{
		size_t n = s.size() + 1;
		if (n & 1)
			n++;
		Write32(uint32_t(n));
		std::vector<char> buffer(n, 0);
		memcpy(buffer.data(), s.data(), s.size());
		Write(buffer.data(), n);
	}
	bool ReadString(std::string& s, uint32_t limit = 1 << 20)
	{
		uint32_t n = ReadU32();
		if (!fOk || n > limit) {
			fOk = false;
			return false;
		}
		std::vector<char> buffer(n + 1, 0);
		Read(buffer.data(), n);
		s = buffer.data();
		return fOk;
	}
	void Close()
	{
		if (fFile && fclose(fFile) != 0)
			fOk = false;
		fFile = nullptr;
	}

private:
	FILE* fFile;
	bool fOk = true;
};

// 3dmiX converts with C casts (gcc2: fistp with the FPU set to truncate),
// although decompilers show them as ROUND(): truncate toward zero.
int32_t Trunc(float f) { return int32_t(f); }

std::string Leaf(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Parent(const std::string& path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return ".";
	return slash == 0 ? "/" : path.substr(0, slash);
}

void SetError(std::string* error, const std::string& message)
{
	if (error)
		*error = message;
}

}	// namespace

// #pragma mark - Playback

// The arithmetic follows the original's x87 evaluation: long double
// intermediates, rounded where 3dmiX stores into a float. TrackObject
// keeps start, end, loop in and out as doubles.
bool
Clip::GetData(int16_t* out, int32_t offset, int32_t count) const
{
	const long double rate = kSampleRate;
	const int32_t loopStart = int32_t((long double)loopIn * rate);
	const int32_t loopLength = std::max(0, int32_t((long double)loopOut * rate) - loopStart);
	const int32_t available = int32_t(samples.size()) - loopStart;
	if (loopLength == 0)
		return false;	// the original divides by zero here

	bool wrote = false;
	int32_t phase = offset % loopLength;
	float time = float((long double)start + (long double)phase / rate);
	while (count > 0) {
		int32_t n;
		if (phase < available) {
			n = std::min({count, loopLength - phase, available - phase});
			bool last = false;
			float left = float(((long double)end - time) * 44100.0L);
			if (left < float(n)) {
				n = std::max(0, Trunc(left));
				last = true;
			}
			if (loopStart + phase >= 0)
				memcpy(out, samples.data() + loopStart + phase, n * sizeof(int16_t));
			wrote = true;
			if (last)
				return true;
		} else
			n = std::min(count, loopLength - phase);
		time = float((long double)n / 44100.0L + time);
		count -= n;
		phase += n;
		out += n;
		if (phase == loopLength)
			phase = 0;
	}
	return wrote;
}

void
Track::PositionAt(double time, float& px, float& py) const
{
	if (path.empty()) {
		px = x;
		py = y;
		return;
	}
	if (time <= path.front().time) {
		px = path.front().x;
		py = path.front().y;
		return;
	}
	if (time >= path.back().time) {
		px = path.back().x;
		py = path.back().y;
		return;
	}
	// the last point at or before time, and the one after it
	size_t lo = 0, hi = path.size() - 1;
	while (hi - lo > 1) {
		size_t mid = (lo + hi) / 2;
		if (path[mid].time <= time)
			lo = mid;
		else
			hi = mid;
	}
	const PathPoint& a = path[lo];
	const PathPoint& b = path[hi];
	const double f = b.time > a.time ? (time - a.time) / (b.time - a.time) : 1;
	px = float(a.x + (b.x - a.x) * f);
	py = float(a.y + (b.y - a.y) * f);
}

// A clip's fades over the frames it wrote: a straight rise from silence at
// its start and fall to silence at its end. Past the end (3dmiX's quirk can
// play up to a buffer more) a faded clip is silent.
static void
ApplyFades(const Clip& clip, int16_t* out, int32_t count, int32_t at)
{
	const double length = std::max(0.0, double(clip.end) - clip.start);
	const double in = std::min<double>(clip.fadeIn, length);
	const double outLength = std::min<double>(clip.fadeOut, length - in);
	for (int32_t i = 0; i < count; i++) {
		const double t = double(at + i) / kSampleRate;
		if (t < clip.start)
			continue;
		double gain = 1;
		if (t > clip.end)
			gain = clip.fadeOut > 0 ? 0 : 1;
		else {
			if (in > 0 && t < clip.start + in)
				gain = (t - clip.start) / in;
			if (outLength > 0 && t > clip.end - outLength)
				gain = std::min(gain, (clip.end - t) / outLength);
		}
		if (gain < 1)
			out[i] = int16_t(std::lround(out[i] * std::max(0.0, gain)));
	}
}

bool
Track::Fill(int16_t* out, int32_t count, int32_t at) const
{
	const float from = float((long double)float(at) / 44100.0L);
	const float to = float((long double)float(at + count) / 44100.0L);
	memset(out, 0, count * sizeof(int16_t));
	bool played = false;
	for (const Clip& clip : clips) {
		if (!(clip.start <= to && from <= clip.end))
			continue;
		int32_t delay = int32_t(((long double)clip.start - from) * 44100.0L);
		if (delay < 0)
			played |= clip.GetData(out, -delay, count);
		else if (delay < count)
			played |= clip.GetData(out + delay, 0, count - delay);
		if (clip.fadeIn > 0 || clip.fadeOut > 0)
			ApplyFades(clip, out, count, at);
	}
	return played;
}

float
Track::Length() const
{
	float length = 0;
	for (const Clip& clip : clips)
		length = std::max(length, clip.end);
	return length;
}

float
Project::Length() const
{
	float length = 0;
	for (const Track& track : tracks)
		length = std::max(length, track.Length());
	return length;
}

// #pragma mark - Files

bool
ReadTrack(const std::string& path, Track& track, std::string* error)
{
	File file(path, "rb");
	if (!file.IsOpen()) {
		SetError(error, "Can't open " + path);
		return false;
	}
	if (file.ReadU32() != 0x2154524b) {	// "!TRK"
		SetError(error, path + " is not a 3dmiX track");
		return false;
	}
	track = Track();
	file.ReadString(track.name);
	int32_t count = file.Read32();
	for (int32_t i = 0; file.Ok() && i < count; i++) {
		Clip clip;
		if (file.ReadU32() != 0x53494d50 || file.ReadU32() != 0x5241575f) {	// "SIMP" "RAW_"
			SetError(error, path + ": unknown clip type");
			return false;
		}
		file.ReadString(clip.source);
		clip.start = file.ReadFloat();
		clip.end = file.ReadFloat();
		clip.loopIn = file.ReadFloat();
		clip.loopOut = file.ReadFloat();
		uint32_t frames = file.ReadU32();
		if (!file.Ok() || frames > (1u << 28)) {
			SetError(error, path + " is damaged");
			return false;
		}
		std::vector<uint8_t> raw(size_t(frames) * 2);
		file.Read(raw.data(), raw.size());
		clip.samples.resize(frames);
		for (uint32_t f = 0; f < frames; f++)
			clip.samples[f] = int16_t(raw[2 * f] << 8 | raw[2 * f + 1]);
		track.clips.push_back(std::move(clip));
	}
	if (!file.Ok()) {
		SetError(error, path + " is truncated");
		return false;
	}
	return true;
}

bool
WriteTrack(const std::string& path, const Track& track, std::string* error)
{
	File file(path, "wb");
	if (!file.IsOpen()) {
		SetError(error, "Can't write " + path);
		return false;
	}
	file.Write32(0x2154524b);
	file.WriteString(track.name);
	file.Write32(uint32_t(track.clips.size()));
	for (const Clip& clip : track.clips) {
		file.Write32(0x53494d50);
		file.Write32(0x5241575f);
		file.WriteString(clip.source);
		file.WriteFloat(clip.start);
		file.WriteFloat(clip.end);
		file.WriteFloat(clip.loopIn);
		file.WriteFloat(clip.loopOut);
		file.Write32(uint32_t(clip.samples.size()));
		std::vector<uint8_t> raw(clip.samples.size() * 2);
		for (size_t f = 0; f < clip.samples.size(); f++) {
			raw[2 * f] = uint8_t(uint16_t(clip.samples[f]) >> 8);
			raw[2 * f + 1] = uint8_t(clip.samples[f]);
		}
		file.Write(raw.data(), raw.size());
	}
	file.Close();
	if (!file.Ok()) {
		SetError(error, "Couldn't write " + path);
		return false;
	}
	return true;
}

bool
ReadProject(const std::string& path, Project& project, std::string* error,
	std::vector<std::string>* skipped)
{
	File file(path, "rb");
	if (!file.IsOpen()) {
		SetError(error, "Can't open " + path);
		return false;
	}
	if (file.ReadU32() != 0x4d415354) {	// "MAST"
		SetError(error, path + " is not a 3dmiX project");
		return false;
	}
	project = Project();
	int32_t count = file.Read32();
	const std::string folder = Parent(path);
	for (int32_t i = 0; file.Ok() && i < count; i++) {
		char stored[2049] = {};
		file.Read(stored, 2048);
		float x = file.ReadFloat();
		float y = file.ReadFloat();
		if (!file.Ok())
			break;
		x = std::max(-kPositionLimit, std::min(kPositionLimit, x));
		y = std::max(-kPositionLimit, std::min(kPositionLimit, y));
		// mangle(): the stored path's leaf, in the project's folder
		std::string trackPath = folder + "/" + Leaf(stored);
		Track track;
		if (!ReadTrack(trackPath, track, nullptr)) {
			if (skipped)
				skipped->push_back(Leaf(stored));
			continue;
		}
		track.x = x;
		track.y = y;
		project.tracks.push_back(std::move(track));
	}
	project.selectionStart = file.ReadFloat();
	project.selectionEnd = file.ReadFloat();
	project.timeUnknown = file.Read32();
	project.ticks = file.ReadFloat();
	project.beatsPerBar = file.Read32();
	if (!file.Ok()) {
		SetError(error, path + " is truncated");
		return false;
	}
	// TCtrlView::Load: optional, 32 bytes
	char text[33] = {};
	if (file.Read(text, 32))
		project.ticksText = text;
	ReadExtras(path + ".powermix", project);
	return true;
}

bool
WriteProject(const std::string& folder, const std::string& fileName,
	const Project& project, std::string* error)
{
	for (const Track& track : project.tracks) {
		if (!WriteTrack(folder + "/" + track.name, track, error))
			return false;
	}
	const std::string path = folder + "/" + fileName;
	File file(path, "wb");
	if (!file.IsOpen()) {
		SetError(error, "Can't write " + path);
		return false;
	}
	file.Write32(0x4d415354);
	file.Write32(uint32_t(project.tracks.size()));
	for (const Track& track : project.tracks) {
		char stored[2048] = {};
		snprintf(stored, sizeof(stored), "%s/%s", folder.c_str(), track.name.c_str());
		file.Write(stored, sizeof(stored));
		file.WriteFloat(track.x);
		file.WriteFloat(track.y);
	}
	file.WriteFloat(project.selectionStart);
	file.WriteFloat(project.selectionEnd);
	file.Write32(uint32_t(project.timeUnknown));
	file.WriteFloat(project.ticks);
	file.Write32(uint32_t(project.beatsPerBar));
	char text[32] = {};
	strncpy(text, project.ticksText.c_str(), sizeof(text) - 1);
	file.Write(text, sizeof(text));
	file.Close();
	if (!file.Ok()) {
		SetError(error, "Couldn't write " + path);
		return false;
	}
	if (!WriteExtras(path + ".powermix", project)) {
		SetError(error, "Couldn't write " + path + ".powermix");
		return false;
	}
	return true;
}

// <project>.powermix: see Project.h
bool
ReadExtras(const std::string& path, Project& project)
{
	FILE* file = fopen(path.c_str(), "r");
	if (file == NULL)
		return false;
	char line[4096];
	if (fgets(line, sizeof(line), file) == NULL || strncmp(line, "PowerMix 1", 10) != 0) {
		fclose(file);
		return false;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		line[strcspn(line, "\r\n")] = '\0';
		std::vector<std::string> fields;
		for (char* field = line; ; ) {
			char* tab = strchr(field, '\t');
			if (tab != NULL)
				*tab = '\0';
			fields.push_back(field);
			if (tab == NULL)
				break;
			field = tab + 1;
		}
		Track* track = NULL;
		if (fields.size() >= 2) {
			for (Track& t : project.tracks) {
				if (t.name == fields[1])
					track = &t;
			}
		}
		if (track == NULL)
			continue;
		if (fields[0] == "move" && fields.size() >= 5) {
			PathPoint point = {float(atof(fields[2].c_str())), float(atof(fields[3].c_str())),
				float(atof(fields[4].c_str()))};
			point.x = std::max(-kPositionLimit, std::min(kPositionLimit, point.x));
			point.y = std::max(-kPositionLimit, std::min(kPositionLimit, point.y));
			if (track->path.empty() || track->path.back().time <= point.time)
				track->path.push_back(point);
		} else if (fields[0] == "fade" && fields.size() >= 5) {
			size_t index = size_t(atol(fields[2].c_str()));
			if (index < track->clips.size()) {
				track->clips[index].fadeIn = std::max(0.0f, float(atof(fields[3].c_str())));
				track->clips[index].fadeOut = std::max(0.0f, float(atof(fields[4].c_str())));
			}
		}
	}
	fclose(file);
	return true;
}

bool
WriteExtras(const std::string& path, const Project& project)
{
	std::string text;
	char line[256];
	for (const Track& track : project.tracks) {
		for (size_t k = 0; k < track.clips.size(); k++) {
			const Clip& clip = track.clips[k];
			if (clip.fadeIn <= 0 && clip.fadeOut <= 0)
				continue;
			snprintf(line, sizeof(line), "\t%zu\t%.6f\t%.6f\n", k, clip.fadeIn, clip.fadeOut);
			text += "fade\t" + track.name + line;
		}
		for (const PathPoint& point : track.path) {
			snprintf(line, sizeof(line), "\t%.4f\t%.2f\t%.2f\n", point.time, point.x, point.y);
			text += "move\t" + track.name + line;
		}
	}
	// nothing 3dmiX can't hold: no file (an old one would be stale)
	if (text.empty()) {
		remove(path.c_str());
		return true;
	}
	FILE* file = fopen(path.c_str(), "w");
	if (file == NULL)
		return false;
	bool ok = fputs("PowerMix 1\n", file) >= 0 && fputs(text.c_str(), file) >= 0;
	return fclose(file) == 0 && ok;
}

}	// namespace pmx
