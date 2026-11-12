// Mixer.cpp: see Mixer.h.
#include "Mixer.h"

#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

// 3dmiX converts with C casts (gcc2: fistp with the FPU set to truncate),
// although decompilers show them as ROUND(): truncate toward zero.
int32_t Trunc(float f) { return int32_t(f); }

}	// namespace

Mixer::Mixer(const Project* project)
	:
	fProject(project),
	fMaster(MasterGainForSlider(50)),
	fStart(0),
	fEnd(0),
	fTime(0),
	fBlock(-1),
	fLoop(true),
	fDone(false),
	fLeftLevel(0),
	fRightLevel(0)
{
	Reset();
}

void
Mixer::Reset()
{
	std::vector<Channel> channels(fProject->tracks.size());
	for (size_t i = 0; i < channels.size() && i < fChannels.size(); i++) {
		channels[i].muted = fChannels[i].muted;
		channels[i].soloed = fChannels[i].soloed;
	}
	for (Channel& channel : channels)
		channel.window.assign(kHistoryFrames + kWindowBlocks * kBlockFrames, 0);
	fChannels.swap(channels);
	fBlock = -1;
	fDone = false;
}

void
Mixer::SetMuted(size_t track, bool muted)
{
	if (track < fChannels.size())
		fChannels[track].muted = muted;
}

void
Mixer::SetSoloed(size_t track, bool soloed)
{
	if (track < fChannels.size())
		fChannels[track].soloed = soloed;
}

bool
Mixer::IsMuted(size_t track) const
{
	return track < fChannels.size() && fChannels[track].muted;
}

bool
Mixer::IsSoloed(size_t track) const
{
	return track < fChannels.size() && fChannels[track].soloed;
}

void
Mixer::SetRange(double start, double end)
{
	fStart = start;
	fEnd = end;
	if (fTime < fStart || fTime > fEnd)
		Seek(fStart);
}

void
Mixer::Seek(double time)
{
	fTime = time;
	fBlock = -1;
	fDone = false;
}

float
Mixer::TrackLevel(size_t track) const
{
	return track < fChannels.size() ? fChannels[track].level : 0;
}

// Channel::SetPos: a window of ten blocks plus the history before them
void
Mixer::FillWindows()
{
	const int32_t at = Trunc(float(fTime * kSampleRate));
	for (size_t i = 0; i < fChannels.size(); i++) {
		Channel& channel = fChannels[i];
		channel.playing = fProject->tracks[i].Fill(channel.window.data(),
			int32_t(channel.window.size()), at - kHistoryFrames);
	}
}

// window_control_task, per channel before each block: the level the 3D view
// shows, then pan and gain from the track's place on the floor.
void
Mixer::UpdateChannel(size_t i, int32_t block)
{
	Channel& channel = fChannels[i];
	const Track& track = fProject->tracks[i];

	const int16_t* samples = channel.window.data() + kHistoryFrames + block * kBlockFrames;
	float sum = 0;
	for (int k = 0; k < 32; k++)
		sum += std::fabs(float(samples[k * 20]));
	float level = sum * 0.03125f * 0.001953125f;
	channel.level = std::max(2.0f, std::min(15.0f, level));

	// Channel::SetLeftRight
	float pan = 1.0f - (track.x + 256.0f) * 0.001953125f;
	float angle = std::max(0.0f, std::min(1.0f, pan * 0.76f + 0.12f)) * 3.1415925f * 0.5f;
	float s = float(std::sin(double(angle)));
	float c = float(std::cos(double(angle)));
	channel.panLeft = s / (s + c);
	channel.panRight = c / (s + c);

	float distance = std::max(0.0f, (track.y + 150.0f) / 200.0f);
	channel.gain = distance * distance;
}

// bi_mix: frames are taken in pairs; pan, gain and delays are refreshed at
// frames 0 and 256 (the original tests the low byte of the frame index).
void
Mixer::MixBlock(int32_t block, int16_t* out)
{
	const size_t count = fChannels.size();
	bool anySolo = false;
	for (const Channel& channel : fChannels)
		anySolo |= channel.soloed;

	std::vector<size_t> active;
	for (size_t i = 0; i < count; i++) {
		const Channel& channel = fChannels[i];
		if (!channel.muted && !(anySolo && !channel.soloed) && channel.playing)
			active.push_back(i);
	}

	std::vector<float> gainLeft(count), gainRight(count);
	std::vector<int32_t> delayLeft(count), delayRight(count);
	const float scale = count > 0 ? fMaster / float(count) : 0;
	uint32_t sumLeft = 0, sumRight = 0;

	auto clip = [](float v) {
		if (v > 32766.0f)
			v = 32766.0f;
		if (v <= -32766.0f)
			v = -32766.0f;
		return int16_t(Trunc(v));
	};

	for (int32_t frame = 0; frame < kBlockFrames; frame += 2) {
		if ((frame & 0xff) == 0) {
			for (size_t i = 0; i < count; i++) {
				Channel& channel = fChannels[i];
				channel.smoothRight = (channel.smoothRight * 31.0f + channel.panRight) * 0.03125f;
				channel.smoothLeft = (channel.smoothLeft * 31.0f + channel.panLeft) * 0.03125f;
				gainRight[i] = channel.gain * channel.smoothRight;
				gainLeft[i] = channel.gain * channel.smoothLeft;
				int32_t delay = Trunc(fProject->tracks[i].x / 3.5f);
				delayLeft[i] = std::max(delay, 0);
				delayRight[i] = std::max(-delay, 0);
			}
		}
		float left0 = 0, left1 = 0, right0 = 0, right1 = 0;
		for (size_t i : active) {
			const int16_t* samples = fChannels[i].window.data() + kHistoryFrames
				+ block * kBlockFrames + frame;
			const int16_t* r = samples - delayRight[i];
			right0 += gainRight[i] * float(r[0]);
			right1 += gainRight[i] * float(r[1]);
			const int16_t* l = samples - delayLeft[i];
			left0 += gainLeft[i] * float(l[0]);
			left1 += gainLeft[i] * float(l[1]);
		}
		int16_t l0 = clip(left0 * scale);
		int16_t r0 = clip(right0 * scale);
		out[2 * frame] = l0;
		out[2 * frame + 1] = r0;
		out[2 * frame + 2] = clip(left1 * scale);
		out[2 * frame + 3] = clip(right1 * scale);
		sumLeft += uint32_t(std::abs(int32_t(l0)));
		sumRight += uint32_t(std::abs(int32_t(r0)));
	}
	fLeftLevel = float(sumLeft >> 8);
	fRightLevel = float(sumRight >> 8);
}

void
Mixer::SettlePan()
{
	for (size_t i = 0; i < fChannels.size(); i++) {
		UpdateChannel(i, 0);
		fChannels[i].smoothLeft = fChannels[i].panLeft;
		fChannels[i].smoothRight = fChannels[i].panRight;
	}
}

bool
Mixer::NextBlock(int16_t* out)
{
	if (fDone || fEnd <= fStart) {
		std::fill(out, out + 2 * kBlockFrames, 0);
		fLeftLevel = fRightLevel = 0;
		return !fDone;
	}
	if (fBlock < 0) {
		FillWindows();
		fBlock = 0;
	}
	for (size_t i = 0; i < fChannels.size(); i++)
		UpdateChannel(i, fBlock);
	MixBlock(fBlock, out);

	fTime += kBlockSeconds;
	fBlock++;
	if (fEnd < fTime) {
		// past the selection: loop to its start (final_mix stops instead)
		if (!fLoop)
			fDone = true;
		fTime = fStart;
		fBlock = -1;
	} else if (fBlock == kWindowBlocks) {
		if (fTime <= fStart)
			fTime = fStart;
		fBlock = -1;
	}
	return true;
}

}	// namespace pmx
