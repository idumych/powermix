// Host tests for src/core/Effects.
#include "Effects.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)

static Sound Sine(size_t n, float amplitude)
{
	Sound s(n);
	for (size_t i = 0; i < n; i++)
		s[i] = int16_t(amplitude * std::sin(i * 0.05));
	return s;
}

static float Energy(const Sound& s, size_t from, size_t to)
{
	float e = 0;
	for (size_t i = from; i < to; i++)
		e += std::abs(int(s[i]));
	return e;
}

int main()
{
	Sound s = Sine(1000, 10000);
	Zero(s, 100, 200);
	CHECK(s[150] == 0 && s[99] != 0 && s[200] != 0);

	s = Sine(1000, 10000);
	int16_t before = s[300];
	ChangeVolume(s, 0, 1000, 50);
	CHECK(s[300] == int16_t(int(0.5f * before)));
	ChangeVolume(s, 0, 1000, 500);		// clipped at 32767
	for (int16_t v : s)
		CHECK(std::abs(int(v)) <= 32767);

	s = Sine(1000, 1000);
	MaxVolume(s, 0, 1000);
	int peak = 0;
	for (int16_t v : s)
		peak = std::max(peak, std::abs(int(v)));
	CHECK(peak >= 32700 && peak <= 32760);

	s = Sine(1000, 10000);
	Fade(s, 0, 1000, 0);
	CHECK(std::abs(int(s[990])) < 200);

	// echo, chorus and metal keep the selection's energy
	for (int which = 0; which < 3; which++) {
		s = Sine(20000, 8000);
		float e0 = Energy(s, 5000, 20000);
		if (which == 0)
			Echo(s, 5000, 20000, 50, 100);
		else if (which == 1)
			Chorus(s, 5000, 20000, 20);
		else
			Metal(s, 5000, 20000);
		float e1 = Energy(s, 5000, 20000);
		CHECK(std::fabs(e1 / e0 - 1) < 0.02f);
	}

	// compress pulls a quiet part up and a loud part down
	s = Sound(20000);
	for (size_t i = 0; i < 20000; i++)
		s[i] = int16_t((i < 10000 ? 1000 : 30000) * std::sin(i * 0.05));
	Compress(s, 0, 20000, 100);
	CHECK(Energy(s, 8000, 10000) > Energy(Sine(2000, 1000), 0, 2000) * 1.5f);

	Sound clip;
	s = Sine(1000, 1000);
	CopyFrames(s, 100, 200, clip);
	DeleteFrames(s, 100, 200);
	CHECK(s.size() == 900 && clip.size() == 100);
	InsertFrames(s, 100, clip);
	CHECK(s == Sine(1000, 1000));

	CHECK(Resample(Sine(1000, 1000), 2).size() == 2000);
	CHECK(TimeStretch(Sine(10000, 1000), 0.5).size() == 5000);

	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
