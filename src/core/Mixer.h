// Mixer.h: 3dmiX's spatial mix of a project's tracks into 16-bit stereo,
// one 512-frame block at a time. Portable: no Haiku headers.
//
// Follows TSoundView::window_control_task, bi_mix/w_mix, final_mix and
// Channel: each track sits at (x, y) on the floor, x left (-256) to right
// (256), y far (-256) to near (256).
//   gain   ((y + 150) / 200)^2, 0 at y <= -150
//   pan    equal-sum sine/cosine law on x, smoothed (31/32 per half block)
//   delay  round(x / 3.5) frames on the far ear (interaural time difference)
//   output sum * master / number of tracks, clipped to +-32766
// Tracks are read in windows of ten blocks with 768 frames of history (for
// the delay), as Channel::SetPos does.
#ifndef POWERMIX_MIXER_H
#define POWERMIX_MIXER_H

#include "Project.h"

namespace pmx {

const int32_t kBlockFrames = 512;
const int32_t kWindowBlocks = 10;
const int32_t kHistoryFrames = 768;
const double kBlockSeconds = 512.0 / 44100.0;	// 0.011609977...

// The master slider: 3dmiX's TSlider runs 0 (top, "100%") to 100 (bottom,
// "0%"); get_global_gain() is (100 - value) / 15.
inline float MasterGainForSlider(float value) { return (100.0f - value) / 15.0f; }

class Mixer {
public:
	// The project must outlive the mixer; tracks are looked up by index, so
	// call Reset() after adding or removing tracks.
	explicit Mixer(const Project* project);

	void Reset();
	void SetMasterGain(float gain) { fMaster = gain; }
	// Muted tracks (and, while any track is soloed, the others) are skipped.
	void SetMuted(size_t track, bool muted);
	void SetSoloed(size_t track, bool soloed);
	bool IsMuted(size_t track) const;
	bool IsSoloed(size_t track) const;

	// Playback loops over [start, end) seconds; get_start_time/get_end_time
	// in the original come from the time ruler's selection.
	void SetRange(double start, double end);
	void Seek(double time);
	double Time() const { return fTime; }

	// Mixes the next block into out (kBlockFrames interleaved L/R frames).
	// Returns false when a range without looping (export) has ended.
	bool NextBlock(int16_t* out);
	// For "Save Mix As WAVE": play the range once, then NextBlock() stops.
	void SetLoop(bool loop) { fLoop = loop; }

	// The last block's levels: per track (SetChannelLevel's argument
	// before it's divided by -11: 2..15) and the output's sum of |sample|
	// >> 8 per channel, as the VU meter reads them.
	float TrackLevel(size_t track) const;
	float LeftLevel() const { return fLeftLevel; }
	float RightLevel() const { return fRightLevel; }

private:
	struct Channel {
		std::vector<int16_t> window;	// kHistoryFrames + kWindowBlocks blocks
		bool playing = false;			// fill_buffer found data
		float panLeft = 0.5f, panRight = 0.5f;		// Channel::SetLeftRight
		float smoothLeft = 0, smoothRight = 0;		// bi_mix's 31/32 filter
		float gain = 0;
		float level = 2;
		bool muted = false, soloed = false;
	};

	void FillWindows();
	void UpdateChannel(size_t i, int32_t block);
	void MixBlock(int32_t block, int16_t* out);

	const Project* fProject;
	std::vector<Channel> fChannels;
	float fMaster;
	double fStart, fEnd, fTime;
	int32_t fBlock;		// block within the current window, or -1 to refill
	bool fLoop;
	bool fDone;
	float fLeftLevel, fRightLevel;
};

}	// namespace pmx

#endif
