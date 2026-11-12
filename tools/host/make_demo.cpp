// make_demo FOLDER: writes PowerMix's demo project (demo.3dmix and six
// track files) from synthesised loops, for screenshots and release tests.
// Everything in it is generated here, so it can be shipped.
#include "Project.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace pmx;

static const double kBeat = 60.0 / 120.0;		// 120 BPM
static const int kBar = int(4 * kBeat * kSampleRate);

static std::vector<int16_t> Loop(int frames, double (*voice)(double t, int frame))
{
	std::vector<int16_t> s(static_cast<size_t>(frames));
	for (int i = 0; i < frames; i++)
		s[size_t(i)] = int16_t(std::max(-32000.0, std::min(32000.0, voice(double(i) / kSampleRate, i))));
	return s;
}

static unsigned sNoise = 7;
static double Noise() { sNoise = sNoise * 1103515245 + 12345; return ((sNoise >> 9) & 0xffff) / 32768.0 - 1; }

static double Kick(double t, int)
{
	double b = std::fmod(t, kBeat);
	return 26000 * std::exp(-b * 9) * std::sin(2 * M_PI * (50 * b + 90 * (1 - std::exp(-b * 30)) / 30));
}
static double Hat(double t, int)
{
	double b = std::fmod(t + kBeat / 2, kBeat / 2);
	return 9000 * std::exp(-b * 60) * Noise();
}
static double Bass(double t, int)
{
	static const double notes[4] = {55.0, 55.0, 65.41, 49.0};
	int step = int(t / kBeat) % 4;
	double b = std::fmod(t, kBeat);
	double f = notes[step];
	double saw = 2 * std::fmod(t * f, 1.0) - 1;
	return 14000 * saw * std::min(1.0, b * 40) * std::exp(-b * 2);
}
static double Pad(double t, int)
{
	double v = 0;
	for (double f : {220.0, 261.63, 329.63, 392.0})
		v += std::sin(2 * M_PI * f * t + 0.4 * std::sin(2 * M_PI * 0.3 * t));
	return 4000 * v;
}
static double Lead(double t, int)
{
	static const double notes[8] = {659.25, 587.33, 523.25, 587.33, 659.25, 783.99, 659.25, 523.25};
	int step = int(t / (kBeat / 2)) % 8;
	double b = std::fmod(t, kBeat / 2);
	double f = notes[step];
	return 9000 * std::sin(2 * M_PI * f * t + 2 * std::sin(2 * M_PI * f * 2 * t) * std::exp(-b * 8))
		* std::exp(-b * 4);
}
static double Sweep(double t, int)
{
	static double low = 0;
	double cut = 0.02 + 0.2 * (0.5 + 0.5 * std::sin(2 * M_PI * t / (2 * 4 * kBeat)));
	low += (Noise() - low) * cut;
	return 12000 * low;
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return 2;
	struct Voice { const char* name; double (*fn)(double, int); float x, y; float start; };
	const Voice voices[] = {
		{"kick", Kick, 0, 240, 0},
		{"hats", Hat, 120, 180, 4 * float(kBeat)},
		{"bass", Bass, -40, 220, 0},
		{"pad", Pad, -200, 60, 0},
		{"lead", Lead, 180, 40, 8 * float(kBeat)},
		{"sweep", Sweep, -120, -60, 8 * float(kBeat)},
	};
	Project project;
	const float length = 16 * float(kBeat);
	for (const Voice& v : voices) {
		Track track;
		track.name = v.name;
		track.x = v.x;
		track.y = v.y;
		Clip clip;
		clip.source = std::string(v.name) + ".wav";
		clip.samples = Loop(kBar, v.fn);
		clip.start = v.start;
		clip.end = length;
		clip.loopIn = 0;
		clip.loopOut = float(kBar) / kSampleRate;
		track.clips.push_back(clip);
		project.tracks.push_back(track);
	}
	project.selectionStart = 0;
	project.selectionEnd = length;
	project.ticks = 120;
	project.ticksText = "120.000";
	project.beatsPerBar = 4;
	std::string error;
	if (!WriteProject(argv[1], "demo.3dmix", project, &error)) {
		fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}
	return 0;
}
