// AudioEngine.h: plays the project's mix through the Media Kit. Owns the
// project and the mixer; everything that touches either takes Lock().
#ifndef POWERMIX_AUDIO_ENGINE_H
#define POWERMIX_AUDIO_ENGINE_H

#include <Locker.h>
#include <MediaDefs.h>

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

	// Levels of the last block for the meters, read without the lock
	float LeftLevel() const { return fLeftLevel; }
	float RightLevel() const { return fRightLevel; }

private:
	static void PlayBuffer(void* cookie, void* buffer, size_t size,
		const media_raw_audio_format& format);
	void Fill(int16* out, size_t frames);
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
	float fLeftLevel, fRightLevel;
};

#endif
