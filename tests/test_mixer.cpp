// Host tests for src/core/Mixer. With a mix folder argument it also renders
// the-lynx's selection to /tmp/pmx-the-lynx.wav for listening.
#include "Mixer.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)

// One track with a single click at 1.0 s, placed at x, y
static Project ClickProject(float x, float y)
{
	Project project;
	Track track;
	track.name = "click";
	track.x = x;
	track.y = y;
	Clip clip;
	clip.start = 1.0f;
	clip.end = 1.5f;
	clip.loopOut = 0.5f;
	clip.samples.assign(kSampleRate / 2, 0);
	clip.samples[0] = 10000;
	track.clips.push_back(clip);
	project.tracks.push_back(track);
	return project;
}

// Plays two seconds and returns the frame and value of each channel's peak
static void Peaks(const Project& project, int& leftAt, int& rightAt, int& left, int& right)
{
	Mixer mixer(&project);
	mixer.SetMasterGain(1.0f);
	mixer.SetRange(0, 2.0);
	int16_t block[2 * kBlockFrames];
	left = right = 0;
	leftAt = rightAt = -1;
	for (int b = 0; b * kBlockFrames < 2 * kSampleRate; b++) {
		mixer.NextBlock(block);
		for (int f = 0; f < kBlockFrames; f++) {
			if (std::abs(block[2 * f]) > std::abs(left)) {
				left = block[2 * f];
				leftAt = b * kBlockFrames + f;
			}
			if (std::abs(block[2 * f + 1]) > std::abs(right)) {
				right = block[2 * f + 1];
				rightAt = b * kBlockFrames + f;
			}
		}
	}
}

static void TestPlacement()
{
	int leftAt, rightAt, left, right;
	Peaks(ClickProject(-256, 256), leftAt, rightAt, left, right);
	CHECK(left > 4 * right && right > 0);
	CHECK(rightAt - leftAt == 73);	// round(256 / 3.5)
	printf("hard left: L %d at %d, R %d at %d\n", left, leftAt, right, rightAt);

	Peaks(ClickProject(0, 256), leftAt, rightAt, left, right);
	CHECK(left == right && leftAt == rightAt);

	Peaks(ClickProject(0, -150), leftAt, rightAt, left, right);
	CHECK(left == 0 && right == 0);	// at y <= -150 a track is silent

	// nearer is louder: ((y + 150) / 200)^2
	int nearL, farL;
	Peaks(ClickProject(0, 250), leftAt, rightAt, nearL, right);
	Peaks(ClickProject(0, 50), leftAt, rightAt, farL, right);
	CHECK(std::fabs(float(nearL) / farL - 4.0f) < 0.05f);
}

static void TestMuteSolo()
{
	Project project = ClickProject(0, 256);
	Mixer mixer(&project);
	mixer.SetMuted(0, true);
	mixer.SetRange(0, 2.0);
	int16_t block[2 * kBlockFrames];
	int peak = 0;
	for (int b = 0; b < 180; b++) {
		mixer.NextBlock(block);
		for (int i = 0; i < 2 * kBlockFrames; i++)
			peak = std::max(peak, std::abs(int(block[i])));
	}
	CHECK(peak == 0);
}

static void WriteWav(const char* path, const std::vector<int16_t>& frames)
{
	FILE* f = fopen(path, "wb");
	if (!f)
		return;
	uint32_t data = uint32_t(frames.size() * 2);
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32(kSampleRate); u32(kSampleRate * 4); u16(4); u16(16);
	fwrite("data", 1, 4, f); u32(data);
	fwrite(frames.data(), 2, frames.size(), f);
	fclose(f);
}

static void RenderLynx(const std::string& dir)
{
	Project project;
	std::string error;
	if (!ReadProject(dir + "/the-lynx/the-lynx-3dmix/the-lynx.3dmix", project, &error)) {
		printf("SKIP render: %s\n", error.c_str());
		return;
	}
	Mixer mixer(&project);
	mixer.SetMasterGain(MasterGainForSlider(50));
	mixer.SetRange(project.selectionStart, project.selectionEnd);
	mixer.SetLoop(false);
	std::vector<int16_t> all;
	int16_t block[2 * kBlockFrames];
	int clipped = 0;
	double energy = 0;
	while (mixer.NextBlock(block)) {
		for (int i = 0; i < 2 * kBlockFrames; i++) {
			clipped += std::abs(int(block[i])) >= 32766;
			energy += double(block[i]) * block[i];
		}
		all.insert(all.end(), block, block + 2 * kBlockFrames);
	}
	double rms = std::sqrt(energy / all.size());
	printf("the-lynx: %.2f s, rms %.0f, clipped samples %d\n",
		all.size() / 2.0 / kSampleRate, rms, clipped);
	CHECK(rms > 300 && clipped < int(all.size() / 1000));
	WriteWav("/tmp/pmx-the-lynx.wav", all);
}

int main(int argc, char** argv)
{
	TestPlacement();
	TestMuteSolo();
	RenderLynx(argc > 1 ? argv[1] : "vm/orig/boot/optional/sound/3D Mixes");
	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
