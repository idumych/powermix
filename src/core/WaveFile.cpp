// WaveFile.cpp: see WaveFile.h.
#include "WaveFile.h"

#include <cstdio>

namespace {

void Put32(FILE* f, uint32_t v)
{
	uint8_t b[4] = { uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24) };
	fwrite(b, 1, 4, f);
}

void Put16(FILE* f, uint16_t v)
{
	uint8_t b[2] = { uint8_t(v), uint8_t(v >> 8) };
	fwrite(b, 1, 2, f);
}

}	// namespace

int
WriteWave(const char* path, const std::vector<int16_t>& frames)
{
	FILE* f = fopen(path, "wb");
	if (f == nullptr)
		return -1;
	const uint32_t data = uint32_t(frames.size() * 2);
	fwrite("RIFF", 1, 4, f);
	Put32(f, 36 + data);
	fwrite("WAVEfmt ", 1, 8, f);
	Put32(f, 16);
	Put16(f, 1);			// PCM
	Put16(f, 2);			// stereo
	Put32(f, 44100);
	Put32(f, 44100 * 4);
	Put16(f, 4);
	Put16(f, 16);
	fwrite("data", 1, 4, f);
	Put32(f, data);
	for (int16_t s : frames)
		Put16(f, uint16_t(s));
	bool ok = ferror(f) == 0;
	if (fclose(f) != 0)
		ok = false;
	return ok ? 0 : -1;
}
