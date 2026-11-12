// AudioEngine.h: plays the project's mix through the Media Kit. Owns the
// project and the mixer; everything that touches either takes Lock().
#ifndef POWERMIX_AUDIO_ENGINE_H
#define POWERMIX_AUDIO_ENGINE_H

#include <Locker.h>

#include <cstdio>
#include <MediaDefs.h>

#include "AboutSound.h"
#include "Mixer.h"
#include "Project.h"

class BSoundPlayer;

class AudioEngine {
public:
	AudioEngine();
	~AudioEngine();

	bool Lock() { return fLock.Lock(); }
	void Unlock() { fLock.Unlock(); }

	// call with the lock held
	pmx::Project& Project() { return fProject; }
	pmx::Mixer& Mixer() { return fMixer; }

	status_t Start();
	void Stop();
	// Misc ▸ Pause: the stream keeps running, the mix stops (silence)
	void SetPaused(bool paused);
	bool IsPaused() const { return fPaused; }

	// The About window's sound replaces the mix while it plays (3dmiX's
	// about_producer); it fades out after FadeOutAbout()
	void StartAbout();
	void FadeOutAbout();

	// The click (Ardour's metronome): while on, a short tick on each beat
	// of the ruler's grid, higher on the first of each bar. It's only
	// played, never mixed into a saved mix.
	void SetClick(bool on, double beatSeconds, int beatsPerBar);
	// Count-in: the mix holds still while `beats` clicks play, then goes on
	void StartCountIn(int beats);
	bool CountingIn() const { return fCountIn > 0; }

	// How long after the mix computes a moment you hear it, in seconds: the
	// output's latency and one buffer (0 without an audio output)
	double OutputDelay() const;

	// Levels of the last block for the meters, read without the lock
	float LeftLevel() const { return fLeftLevel; }
	float RightLevel() const { return fRightLevel; }

private:
	static void PlayBuffer(void* cookie, void* buffer, size_t size,
		const media_raw_audio_format& format);
	void Fill(int16* out, size_t frames);
	void AddClicks(double from, bool countIn);
	static status_t SilentClock(void* cookie);

	BLocker fLock;
	pmx::Project fProject;
	pmx::Mixer fMixer;
	BSoundPlayer* fPlayer;
	thread_id fClock;		// paces the mix when there's no audio output
	volatile bool fClockRunning;
	int16 fBlock[2 * pmx::kBlockFrames];
	int32 fBlockUsed;
	bool fPaused;
	pmx::AboutSound* fAbout;
	float fLeftLevel, fRightLevel;

	bool fClickOn;
	double fBeat;				// seconds
	int fBeatsPerBar;
	volatile int32 fCountIn;	// frames of count-in left
	int32 fCountInTotal;
	int32 fClickAt;				// frames into the sounding tick; -1: none
	float fClickPitch;
	FILE* fDump;				// POWERMIX_DUMP_OUTPUT: what's played, for tests
};

#endif
