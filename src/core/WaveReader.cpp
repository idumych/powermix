// WaveReader.cpp: see WaveReader.h. MS ADPCM as Microsoft documents it
// (the adaptation table, the predictor coefficients from the fmt chunk).
#include "WaveReader.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace pmx {

namespace {

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }
int16_t S16(const uint8_t* p) { return int16_t(U16(p)); }

const int kAdaptation[16] = {230, 230, 230, 230, 307, 409, 512, 614,
	768, 614, 512, 409, 307, 230, 230, 230};
const int kDefaultCoefficients[7][2] = {{256, 0}, {512, -256}, {0, 0}, {192, 64},
	{240, 0}, {460, -208}, {392, -232}};

void
SetError(std::string* error, const char* text)
{
	if (error)
		*error = text;
}

// One MS ADPCM block: per channel a predictor index, then per channel the
// delta, the newer and the older sample, then 4-bit codes, high nibble
// first, channels interleaved
void
DecodeAdpcmBlock(const uint8_t* block, size_t size, int channels, size_t samplesPerBlock,
	const std::vector<int>& coef1, const std::vector<int>& coef2, std::vector<float>& out)
{
	const size_t header = size_t(7 * channels);
	if (size < header)
		return;
	int c1[2], c2[2], delta[2], s1[2], s2[2];
	for (int c = 0; c < channels; c++) {
		size_t index = std::min<size_t>(block[c], coef1.size() - 1);
		c1[c] = coef1[index];
		c2[c] = coef2[index];
		delta[c] = S16(block + channels + 2 * c);
		s1[c] = S16(block + 3 * channels + 2 * c);
		s2[c] = S16(block + 5 * channels + 2 * c);
	}
	for (int c = 0; c < channels; c++)
		out.push_back(s2[c] / 32768.0f);
	for (int c = 0; c < channels; c++)
		out.push_back(s1[c] / 32768.0f);
	size_t produced = 2;
	int channel = 0;
	for (size_t i = header; i < size && produced < samplesPerBlock; i++) {
		for (int shift = 4; shift >= 0; shift -= 4) {
			int code = (block[i] >> shift) & 0xf;
			int signedCode = code >= 8 ? code - 16 : code;
			int c = channel;
			int predicted = (s1[c] * c1[c] + s2[c] * c2[c]) / 256;
			int sample = std::max(-32768, std::min(32767, predicted + signedCode * delta[c]));
			delta[c] = std::max(16, kAdaptation[code] * delta[c] / 256);
			s2[c] = s1[c];
			s1[c] = sample;
			out.push_back(sample / 32768.0f);
			channel = (channel + 1) % channels;
			if (channel == 0 && ++produced >= samplesPerBlock)
				break;
		}
	}
}

}	// namespace

bool
ReadWave(const std::string& path, DecodedSound& sound, std::string* error)
{
	FILE* file = fopen(path.c_str(), "rb");
	if (file == nullptr) {
		SetError(error, "can't open the file");
		return false;
	}
	std::vector<uint8_t> data;
	uint8_t chunk[65536];
	size_t n;
	while ((n = fread(chunk, 1, sizeof(chunk), file)) > 0)
		data.insert(data.end(), chunk, chunk + n);
	fclose(file);
	if (data.size() < 12 || memcmp(data.data(), "RIFF", 4) != 0
		|| memcmp(data.data() + 8, "WAVE", 4) != 0) {
		SetError(error, "not a WAVE file");
		return false;
	}

	int format = 0, channels = 0, bits = 0;
	uint32_t rate = 0;
	size_t blockAlign = 0, samplesPerBlock = 0;
	std::vector<int> coef1, coef2;
	const uint8_t* body = nullptr;
	size_t bodySize = 0;
	for (size_t at = 12; at + 8 <= data.size();) {
		uint32_t size = U32(&data[at + 4]);
		size_t available = std::min<size_t>(size, data.size() - at - 8);
		const uint8_t* p = &data[at + 8];
		if (memcmp(&data[at], "fmt ", 4) == 0 && available >= 16) {
			format = U16(p);
			channels = U16(p + 2);
			rate = U32(p + 4);
			blockAlign = U16(p + 12);
			bits = U16(p + 14);
			if (format == 0xfffe && available >= 26)
				format = U16(p + 24);	// WAVE_FORMAT_EXTENSIBLE: the sub-format
			if (format == 2 && available >= 22) {
				samplesPerBlock = U16(p + 18);
				size_t count = available >= 22 ? U16(p + 20) : 0;
				for (size_t k = 0; k < count && 22 + 4 * k + 4 <= available; k++) {
					coef1.push_back(S16(p + 22 + 4 * k));
					coef2.push_back(S16(p + 24 + 4 * k));
				}
			}
		} else if (memcmp(&data[at], "data", 4) == 0) {
			body = p;
			bodySize = available;
		}
		at += 8 + size + (size & 1);
	}
	if (body == nullptr || channels < 1 || channels > 8 || rate == 0) {
		SetError(error, "no audio in the file");
		return false;
	}

	sound = DecodedSound();
	sound.channels = channels;
	sound.rate = rate;
	std::vector<float>& out = sound.samples;
	if (format == 1 || format == 3) {
		const size_t bytes = size_t(bits / 8);
		if (bytes == 0)
			return false;
		for (size_t i = 0; i + bytes <= bodySize; i += bytes) {
			const uint8_t* s = body + i;
			float v = 0;
			if (format == 3 && bits == 32) {
				uint32_t u = U32(s);
				memcpy(&v, &u, 4);
			} else if (bits == 8)
				v = (s[0] - 128) / 128.0f;
			else if (bits == 16)
				v = S16(s) / 32768.0f;
			else if (bits == 24)
				v = int32_t(uint32_t(s[0]) << 8 | uint32_t(s[1]) << 16 | uint32_t(s[2]) << 24)
					/ 2147483648.0f;
			else if (bits == 32)
				v = int32_t(U32(s)) / 2147483648.0f;
			else
				return false;
			out.push_back(v);
		}
	} else if (format == 2 && channels <= 2 && blockAlign > 0) {
		if (coef1.empty()) {
			for (const auto& c : kDefaultCoefficients) {
				coef1.push_back(c[0]);
				coef2.push_back(c[1]);
			}
		}
		if (samplesPerBlock == 0)
			samplesPerBlock = (blockAlign - 7 * size_t(channels)) * 2 / size_t(channels) + 2;
		else {
			// The R5 sample sounds say 1024-byte blocks but hold 1012 samples
			// per block, which is 512 bytes; the samples count wins (the
			// sounds 3dmiX imported from them agree).
			blockAlign = 7 * size_t(channels) + (samplesPerBlock - 2) * size_t(channels) / 2;
		}
		for (size_t at = 0; at < bodySize; at += blockAlign) {
			DecodeAdpcmBlock(body + at, std::min(blockAlign, bodySize - at), channels,
				samplesPerBlock, coef1, coef2, out);
		}
	} else {
		SetError(error, "a WAVE encoding PowerMix doesn't read itself");
		return false;
	}
	out.resize(sound.Frames() * size_t(channels));
	return !out.empty();
}

}	// namespace pmx
