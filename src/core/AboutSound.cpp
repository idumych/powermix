// AboutSound.cpp: see AboutSound.h. Frequencies are in steps of a
// 1024-entry wave per sample. 3dmiX's wave table is its own data; this one
// is built from harmonics in about the same proportions (a strong fourth).
#include "AboutSound.h"

#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

unsigned sState;
int Random()
{
	sState = sState * 1103515245 + 12345;
	return int((sState >> 16) & 0x7fff);
}

}	// namespace

AboutSound::AboutSound(unsigned seed)
	:
	fWave(1024),
	fGain(0.8f),
	fFading(false)
{
	sState = seed;
	const double harmonics[8] = {0, 1404, 447, 962, 3100, 858, 171, 259};
	std::vector<double> wave(1024, 0);
	double peak = 0;
	for (int i = 0; i < 1024; i++) {
		for (int h = 1; h < 8; h++)
			wave[size_t(i)] += harmonics[h] * std::sin(2 * M_PI * h * i / 1024);
		peak = std::max(peak, std::fabs(wave[size_t(i)]));
	}
	for (int i = 0; i < 1024; i++)
		fWave[size_t(i)] = int16_t(int(wave[size_t(i)] / peak * 10452 * 0.5));

	// init_about_sound: voices 0-4 start low and glide slowly, 13-20 and
	// 21-26 start higher; 5-12 have no speed and sit on the chord at once
	for (int i = 0; i < 27; i++) {
		fFreq[i] = 0;
		fSpeed[i] = 0;
		fPhase[i] = 0;
	}
	for (int i = 0; i <= 4; i++) {
		fFreq[i] = double(Random() % 300 + 450) / 5000.0;
		fSpeed[i] = double(Random() % 12000 + 98000);
	}
	for (int i = 13; i < 27; i++) {
		fFreq[i] = double(Random() % 100 + 8500) / 5000.0;
		fSpeed[i] = double(Random() % 12000 + 95000);
	}
	for (int i = 21; i < 27; i++) {
		fFreq[i] = double(Random() % 100 + 17500) / 5000.0;
		fSpeed[i] = double(Random() % 12000 + 95000);
	}
	// the chord, in three groups of three voices per triad
	const float high[3] = {8.0f, 5.3333335f, 3.3333333f};
	const float middle[3] = {2.0f, 0.8333333f, 1.3333334f};
	const float low[3] = {1.0f, 0.41666667f, 0.6666667f};
	for (int i = 0; i < 9; i++)
		fTarget[i] = high[i % 3] + float(Random() & 0x7fff) / 950000.0f;
	for (int i = 9; i < 18; i++)
		fTarget[i] = middle[(i - 9) % 3] + float(Random() & 0x7fff) / 950000.0f;
	for (int i = 17; i < 27; i++)
		fTarget[i] = low[(i - 17) % 3] + float(Random() & 0x7fff) / 950000.0f;
}

void
AboutSound::NextBlock(int16_t* out)
{
	for (int frame = 0; frame < 512; frame++) {
		int left = 0, right = 0;
		for (int i = 0; i < 13; i++) {
			fPhase[i] += fFreq[i];
			left += fWave[size_t(int(fPhase[i]) & 0x3ff)];
		}
		for (int i = 13; i < 26; i++) {
			fPhase[i] += fFreq[i];
			right += fWave[size_t(int(fPhase[i]) & 0x3ff)];
		}
		out[2 * frame] = int16_t(int(fGain * float(int16_t(int(left * 0.07407407407407407)))));
		out[2 * frame + 1] = int16_t(int(fGain * float(int16_t(int(right * 0.07407407407407407)))));
	}
	for (int i = 0; i < 27; i++) {
		fFreq[i] = (fFreq[i] * fSpeed[i] + fTarget[i]) / (fSpeed[i] + 1);
		fSpeed[i] *= 0.99915;
	}
	if (fFading)
		fGain = std::max(0.1f, fGain - 0.0045f);
	else
		fGain = std::min(1.0f, fGain + 0.001f);
}

}	// namespace pmx
