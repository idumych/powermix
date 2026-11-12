// Host tests for src/core/Project: the R5 sample mixes (vm/orig, not in
// git) must read, and their track files must write back byte for byte.
// usage: test_project [MIXES_DIR]
#include "Project.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)

static std::string Slurp(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), {});
}

static void TestClipLoop()
{
	Clip clip;
	clip.start = 1.0f;
	clip.end = 2.0f;
	clip.loopIn = 0;
	// a 4-frame loop (4 / 44100 itself is 3.99999 as a float, which
	// truncates to 3 frames as in 3dmiX)
	clip.loopOut = 4.5f / kSampleRate;
	clip.samples = {1, 2, 3, 4, 5, 6};
	int16_t out[10] = {};
	CHECK(clip.GetData(out, 2, 10));
	const int16_t want[10] = {3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
	CHECK(memcmp(out, want, sizeof(want)) == 0);

	// Track::Fill places the clip at its start time and clears the rest
	Track track;
	track.clips.push_back(clip);
	// (1 - 44097 / 44100) * 44100 is 2.99999 in float and 3dmiX truncates,
	// so the clip starts one frame early
	int16_t buffer[8];
	CHECK(track.Fill(buffer, 8, kSampleRate - 3));
	const int16_t want2[8] = {0, 0, 1, 2, 3, 4, 1, 2};
	CHECK(memcmp(buffer, want2, sizeof(want2)) == 0);
	CHECK(!track.Fill(buffer, 8, 3 * kSampleRate));
}

// PowerMix's fades: a rise and a fall over the clip's ends, saved in
// <project>.powermix and read back; a project without them writes none
static void TestFades()
{
	Clip clip;
	clip.start = 0;
	clip.end = 1;
	clip.loopIn = 0;
	clip.loopOut = 1;
	clip.samples.assign(kSampleRate, 10000);
	clip.fadeIn = 0.5f;
	clip.fadeOut = 0.25f;
	Track track;
	track.name = "faded";
	track.clips.push_back(clip);
	int16_t out[1];
	track.Fill(out, 1, 0);
	CHECK(out[0] == 0);						// silent at the start
	track.Fill(out, 1, kSampleRate / 4);
	CHECK(out[0] == 5000);					// half way up
	track.Fill(out, 1, kSampleRate / 2 + 10);
	CHECK(out[0] == 10000);					// full between the fades
	track.Fill(out, 1, kSampleRate * 7 / 8);
	CHECK(out[0] == 5000);					// half way down

	char dir[] = "/tmp/pmx-fades-XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	Project project;
	project.tracks.push_back(track);
	std::string error;
	CHECK(WriteProject(dir, "f.3dmix", project, &error));
	Project back;
	CHECK(ReadProject(std::string(dir) + "/f.3dmix", back, &error, nullptr));
	CHECK(back.tracks.size() == 1 && back.tracks[0].clips.size() == 1);
	CHECK(back.tracks[0].clips[0].fadeIn == 0.5f && back.tracks[0].clips[0].fadeOut == 0.25f);
	// a path is saved there too
	project.tracks[0].path = {{0.5f, -100, 50}, {2.25f, 120, -30}};
	CHECK(WriteProject(dir, "f.3dmix", project, &error));
	CHECK(ReadProject(std::string(dir) + "/f.3dmix", back, &error, nullptr));
	CHECK(back.tracks[0].path.size() == 2 && back.tracks[0].path[1].time == 2.25f
		&& back.tracks[0].path[1].x == 120 && back.tracks[0].path[0].y == 50);
	project.tracks[0].path.clear();
	// without fades the extras file goes away
	project.tracks[0].clips[0].fadeIn = project.tracks[0].clips[0].fadeOut = 0;
	CHECK(WriteProject(dir, "f.3dmix", project, &error));
	CHECK(fopen((std::string(dir) + "/f.3dmix.powermix").c_str(), "r") == NULL);
	remove((std::string(dir) + "/f.3dmix").c_str());
	remove((std::string(dir) + "/faded").c_str());
	rmdir(dir);
}

// Opening through links: a working one, one flattened into a file holding
// its target's path (as copying through zip or FAT leaves it), a broken one,
// and a file that's neither
static void TestLinks(const std::string& mixes)
{
	char dir[] = "/tmp/pmx-links-XXXXXX";
	CHECK(mkdtemp(dir) != NULL);
	const std::string d = dir;
	CHECK(mkdir((d + "/mix").c_str(), 0755) == 0);
	Project project;
	Track track;
	track.name = "t1";
	Clip clip;
	clip.end = clip.loopOut = 0.1f;
	clip.samples.assign(4410, 1);
	track.clips.push_back(clip);
	project.tracks.push_back(track);
	std::string error;
	CHECK(WriteProject(d + "/mix", "song.3dmix", project, &error));

	// a link elsewhere: the track files are found beside the project
	CHECK(symlink("mix/song.3dmix", (d + "/song.3dmix").c_str()) == 0);
	Project back;
	CHECK(ReadProject(d + "/song.3dmix", back, &error, nullptr) && back.tracks.size() == 1);
	std::string resolved;
	CHECK(ResolveProjectPath(d + "/song.3dmix", resolved, &error)
		&& resolved.size() > 15 && resolved.compare(resolved.size() - 15, 15, "/mix/song.3dmix") == 0);

	// flattened: a file holding the link's target
	FILE* flat = fopen((d + "/flat.3dmix").c_str(), "w");
	fputs("mix/song.3dmix\n", flat);
	fclose(flat);
	back = Project();
	CHECK(ReadProject(d + "/flat.3dmix", back, &error, nullptr) && back.tracks.size() == 1);

	// broken: says where it pointed
	CHECK(symlink("mix/gone.3dmix", (d + "/broken.3dmix").c_str()) == 0);
	CHECK(!ReadProject(d + "/broken.3dmix", back, &error, nullptr));
	CHECK(error.find("mix/gone.3dmix") != std::string::npos);

	// neither: text that names nothing, or binary junk
	FILE* junk = fopen((d + "/junk.3dmix").c_str(), "w");
	fputs("hello\n", junk);
	fclose(junk);
	CHECK(!ReadProject(d + "/junk.3dmix", back, &error, nullptr));
	CHECK(error.find("not a 3dmiX project") != std::string::npos);

	for (const char* f : {"song.3dmix", "flat.3dmix", "broken.3dmix", "junk.3dmix",
			"mix/song.3dmix", "mix/t1"})
		remove((d + "/" + f).c_str());
	rmdir((d + "/mix").c_str());
	rmdir(dir);

	// BeOS 5's own: the top-level she-loves-it.3dmix is a link
	const std::string sample = mixes + "/she-loves-it.3dmix";
	if (access(sample.c_str(), R_OK) == 0) {
		back = Project();
		CHECK(ReadProject(sample, back, &error, nullptr) && back.tracks.size() == 12);
	}
}

static void TestMix(const std::string& dir, const char* name, size_t tracks)
{
	std::string folder = dir + "/" + name + "/" + name + "-3dmix";
	Project project;
	std::string error;
	std::vector<std::string> skipped;
	bool ok = ReadProject(folder + "/" + name + ".3dmix", project, &error, &skipped);
	if (!ok) {
		printf("SKIP %s: %s\n", name, error.c_str());
		return;
	}
	CHECK(project.tracks.size() == tracks);
	CHECK(skipped.empty());
	CHECK(project.beatsPerBar == 4);
	for (const Track& track : project.tracks) {
		std::string out = "/tmp/pmx-test-track";
		CHECK(WriteTrack(out, track, &error));
		// Same bytes, except the padding after a string's NUL, which 3dmiX
		// wrote from uninitialised memory ("bass\0i")
		std::string mine = Slurp(out), theirs = Slurp(folder + "/" + track.name);
		CHECK(mine.size() == theirs.size());
		size_t differ = 0;
		for (size_t i = 0; i < mine.size() && i < theirs.size(); i++) {
			if (mine[i] != theirs[i] && !(mine[i] == 0 && i > 0 && mine[i - 1] == 0))
				differ++;
		}
		CHECK(differ == 0);
		remove(out.c_str());
	}
	printf("ok %s: %zu tracks, %.2f s, ticks %s\n", name, project.tracks.size(),
		project.Length(), project.ticksText.c_str());
}

int main(int argc, char** argv)
{
	TestClipLoop();
	TestFades();
	std::string dir = argc > 1 ? argv[1] : "vm/orig/boot/optional/sound/3D Mixes";
	TestLinks(dir);
	TestMix(dir, "the-lynx", 14);
	TestMix(dir, "she-loves-it", 12);
	TestMix(dir, "the-price-of-things", 5);
	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
