// Host tests for src/core/WaveReader and WaveFile. With the R5 mixes
// present, a sample WAV (MS ADPCM) is decoded and compared with the same
// sound as 3dmiX imported it into she-loves-it's drums track.
#include "Edit.h"
#include "Project.h"
#include "WaveFile.h"
#include "WaveReader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)

int main(int argc, char** argv)
{
	// round trip through our own writer
	std::vector<int16_t> frames = {0, 0, 1000, -1000, 32767, -32768};
	CHECK(WriteWave("/tmp/pmx-test.wav", frames) == 0);
	DecodedSound sound;
	CHECK(ReadWave("/tmp/pmx-test.wav", sound));
	CHECK(sound.channels == 2 && sound.rate == 44100 && sound.Frames() == 3);
	CHECK(std::fabs(sound.samples[2] - 1000 / 32768.0f) < 1e-6f);
	remove("/tmp/pmx-test.wav");

	std::string dir = argc > 1 ? argv[1] : "vm/orig/boot/optional/sound/3D Mixes";
	std::string wav = dir + "/she-loves-it/she-loves-it-samples/drums/fill_1.wav";
	Track drums;
	if (ReadWave(wav, sound) && ReadTrack(dir + "/she-loves-it/she-loves-it-3dmix/drums",
			drums, nullptr)) {
		std::vector<int16_t> mine = ToClipSamples(sound.samples.data(), sound.Frames(),
			sound.channels, sound.rate);
		const std::vector<int16_t>& theirs = drums.clips[0].samples;
		// BeOS's decoder started 4 samples later than the standard one
		const size_t lag = 4;
		double xy = 0, xx = 0, yy = 0;
		for (size_t i = 0; i + lag < mine.size() && i < theirs.size(); i++) {
			xy += double(mine[i + lag]) * theirs[i];
			xx += double(mine[i + lag]) * mine[i + lag];
			yy += double(theirs[i]) * theirs[i];
		}
		double correlation = xy / std::sqrt(xx * yy);
		printf("fill_1.wav (MS ADPCM): %zu frames, 3dmiX's import %zu; correlation %.5f\n",
			mine.size(), theirs.size(), correlation);
		CHECK(correlation > 0.999);
	} else
		printf("SKIP R5 sample\n");
	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
