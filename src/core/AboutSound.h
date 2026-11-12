// AboutSound.h: the sound of the About window, as 3dmiX's about_p,
// next_slice and next_slice1 make it: 13 oscillators per channel glide
// from low, random pitches into a wide chord while the window shows, then
// fade out. Portable.
#ifndef POWERMIX_ABOUT_SOUND_H
#define POWERMIX_ABOUT_SOUND_H

#include <cstdint>
#include <vector>

namespace pmx {

class AboutSound {
public:
	explicit AboutSound(unsigned seed = 1);

	// 512 interleaved stereo frames per call (a 3dmiX slice)
	void NextBlock(int16_t* out);
	void FadeOut() { fFading = true; }
	bool Finished() const { return fFading && fGain <= 0.1f; }

private:
	std::vector<int16_t> fWave;		// 1024 entries, one cycle
	double fFreq[27], fSpeed[27], fPhase[27];
	float fTarget[27];
	float fGain;
	bool fFading;
};

}	// namespace pmx

#endif
