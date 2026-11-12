// WaveReader.h: reads RIFF WAVE files: PCM (8, 16, 24, 32 bit, float) and
// Microsoft ADPCM, which the BeOS R5 sample sounds use and Haiku's decoder
// mangles. Portable.
#ifndef POWERMIX_WAVE_READER_H
#define POWERMIX_WAVE_READER_H

#include <string>
#include <vector>

namespace pmx {

struct DecodedSound {
	std::vector<float> samples;		// interleaved, -1..1
	int channels = 0;
	double rate = 0;
	size_t Frames() const { return channels > 0 ? samples.size() / size_t(channels) : 0; }
};

// false if it isn't a WAVE file this can read
bool ReadWave(const std::string& path, DecodedSound& sound, std::string* error = nullptr);

}	// namespace pmx

#endif
