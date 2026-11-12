// Effects.cpp: see Effects.h. Arithmetic in float with truncating
// conversions, as 3dmiX's EditView does it.
#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

void
Clamp(const Sound& s, size_t& from, size_t& to)
{
	to = std::min(to, s.size());
	from = std::min(from, to);
}

float
Energy(const Sound& s, size_t from, size_t to)
{
	float sum = 0;
	for (size_t i = from; i < to; i++)
		sum += float(std::abs(int(s[i])));
	return sum;
}

// Echo, chorus and metal end by scaling the selection back to the energy
// it had before
void
RestoreEnergy(Sound& s, size_t from, size_t to, float before)
{
	float after = Energy(s, from, to);
	for (size_t i = from; i < to; i++) {
		float v = (before / (after + 1.0f)) * float(s[i]);
		v = std::max(-32760.0f, std::min(32760.0f, v));
		s[i] = int16_t(int(v));
	}
}

}	// namespace

void
Zero(Sound& s, size_t from, size_t to)
{
	Clamp(s, from, to);
	std::fill(s.begin() + long(from), s.begin() + long(to), 0);
}

void
ChangeVolume(Sound& s, size_t from, size_t to, float percent)
{
	Clamp(s, from, to);
	const float factor = percent / 100.0f;
	for (size_t i = from; i < to; i++) {
		float v = factor * float(s[i]);
		v = std::max(-32767.0f, std::min(32767.0f, v));
		s[i] = int16_t(int(v));
	}
}

void
MaxVolume(Sound& s, size_t from, size_t to)
{
	Clamp(s, from, to);
	float peak = 0;
	for (size_t i = from; i < to; i++)
		peak = std::max(peak, float(std::abs(int(s[i]))));
	if (peak == 0)
		return;		// 3dmiX divides by zero here
	for (size_t i = from; i < to; i++)
		s[i] = int16_t(int((32760.0f / peak) * float(s[i])));
}

void
Fade(Sound& s, size_t from, size_t to, float finalPercent)
{
	Clamp(s, from, to);
	const float target = finalPercent / 100.0f;
	const size_t count = to - from;
	float gain = 1.0f;
	for (size_t i = from; i < to; i++) {
		float v = gain * float(s[i]);
		v = std::max(-32760.0f, std::min(32760.0f, v));
		gain -= (1.0f - target) / float(count);
		s[i] = int16_t(int(v));
	}
}

// Feedback echo: each sample takes in the (already echoed) one delay before
void
Echo(Sound& s, size_t from, size_t to, float strength, float delayMs)
{
	Clamp(s, from, to);
	const float amount = strength / 100.0f;
	const size_t delay = size_t(int(delayMs * 44.1f));
	if (from < delay)
		from = delay;
	if (from >= to)
		return;
	float before = Energy(s, from, to);
	for (size_t i = from; i < to; i++)
		s[i] = int16_t(int((1.0f / (amount + 1.0f)) * (amount * float(s[i - delay]) + float(s[i]))));
	RestoreEnergy(s, from, to, before);
}

// Four slowly wandering delays (0.3, 0.5, 0.7 and 0.9 of the depth), mixed
// in from the end of the selection backwards
void
Chorus(Sound& s, size_t from, size_t to, float strength)
{
	Clamp(s, from, to);
	const float depth = (strength / 1000.0f) * 44100.0f;
	int d1 = int((std::sin(0.0) * 0.02 + 0.3) * depth);
	int d2 = int((std::sin(0.0) * 0.02 + 0.5) * depth);
	int d3 = int((std::sin(0.0) * 0.02 + 0.7) * depth);
	int d4 = int((std::sin(0.0) * 0.03 + 0.9) * depth);
	size_t start = std::max(from, size_t(std::max(0, d4)));
	if (start >= to)
		return;
	float before = Energy(s, start, to);
	float phase = 0;
	// the original starts at the frame after the selection; stay inside
	for (size_t u = to - 1; u > start; u--) {
		if ((u & 3) == 0) {
			double t = phase;
			d1 = int((std::sin(t * 0.8) * 0.02 + 0.3) * depth);
			d2 = int((std::sin(t * 1.1) * 0.02 + 0.5) * depth);
			d3 = int((std::sin(t * 0.75) * 0.02 + 0.7) * depth);
			d4 = int((std::sin(t * 1.4) * 0.03 + 0.9) * depth);
			phase += 0.003f;
		}
		auto at = [&](int d) {
			long k = long(u) - d;
			return k >= 0 ? float(s[size_t(k)]) : 0.0f;
		};
		float v = (at(d3) * 0.9f + at(d2) * 0.9f + at(d1) * 0.9f + float(s[u]) + at(d4) * 0.9f)
			* 0.22624435f;
		s[u] = int16_t(int(v));
	}
	RestoreEnergy(s, start, to, before);
}

// A comb filter 951 frames long
void
Metal(Sound& s, size_t from, size_t to)
{
	Clamp(s, from, to);
	if (from < 0x6af)
		from = 0x6af;
	if (from >= to)
		return;
	float before = Energy(s, from, to);
	for (size_t i = from; i < to; i++)
		s[i] = int16_t(int((float(s[i]) + float(s[i - 0x3b7]) * 0.8f) * 0.5f));
	RestoreEnergy(s, from, to, before);
}

// An envelope follower pulls loud and quiet parts towards 8000
void
Compress(Sound& s, size_t from, size_t to, float strength)
{
	Clamp(s, from, to);
	const float k = strength / 100.0f;
	float envelope = 8000.0f;
	for (size_t i = from; i < to; i++) {
		float x = float(s[i]);
		envelope = (std::fabs(x) + envelope * 255.0f) * 0.00390625f;
		float v = ((8000.0f / envelope) * x * k + x) / (k + 1.0f);
		v = std::max(-32767.0f, std::min(32767.0f, v));
		s[i] = int16_t(int(v));
	}
}

void
CopyFrames(const Sound& s, size_t from, size_t to, Sound& clipboard)
{
	to = std::min(to, s.size());
	from = std::min(from, to);
	clipboard.assign(s.begin() + long(from), s.begin() + long(to));
}

void
DeleteFrames(Sound& s, size_t from, size_t to)
{
	Clamp(s, from, to);
	s.erase(s.begin() + long(from), s.begin() + long(to));
}

void
InsertFrames(Sound& s, size_t at, const Sound& frames)
{
	at = std::min(at, s.size());
	s.insert(s.begin() + long(at), frames.begin(), frames.end());
}

Sound
Resample(const Sound& s, double factor)
{
	if (s.empty() || factor <= 0)
		return s;
	size_t n = size_t(double(s.size()) * factor);
	Sound out(n);
	for (size_t i = 0; i < n; i++) {
		double at = double(i) / factor;
		size_t k = size_t(at);
		double f = at - double(k);
		double a = k < s.size() ? s[k] : 0;
		double b = k + 1 < s.size() ? s[k + 1] : a;
		out[i] = int16_t(a + (b - a) * f);
	}
	return out;
}

// Overlap-add of 1024-frame Hann-windowed grains, read at 1 / factor speed
Sound
TimeStretch(const Sound& s, double factor)
{
	if (s.empty() || factor <= 0)
		return s;
	const size_t grain = 1024, hop = grain / 2;
	size_t n = size_t(double(s.size()) * factor);
	std::vector<float> out(n + grain, 0), weight(n + grain, 0);
	for (size_t o = 0; o < n; o += hop) {
		size_t in = size_t(double(o) / factor);
		for (size_t k = 0; k < grain; k++) {
			float w = 0.5f - 0.5f * std::cos(6.2831853f * float(k) / float(grain));
			float v = in + k < s.size() ? float(s[in + k]) : 0;
			out[o + k] += v * w;
			weight[o + k] += w;
		}
	}
	Sound result(n);
	for (size_t i = 0; i < n; i++) {
		float v = weight[i] > 0.01f ? out[i] / weight[i] : 0;
		result[i] = int16_t(std::max(-32767.0f, std::min(32767.0f, v)));
	}
	return result;
}

}	// namespace pmx
