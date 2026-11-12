// AudioEngine.cpp: 3dmiX streamed 512-frame blocks at 44.1 kHz through the
// old BDACStream (init_stuff, record_func); BSoundPlayer does the same job.
#include "AudioEngine.h"

#include <SoundPlayer.h>

#include <algorithm>
#include <cstring>

AudioEngine::AudioEngine()
	:
	fLock("project"),
	fMixer(&fProject),
	fPlayer(NULL),
	fClock(-1),
	fClockRunning(false),
	fBlockUsed(pmx::kBlockFrames),
	fPaused(false),
	fAbout(NULL),
	fLeftLevel(0),
	fRightLevel(0)
{
}

AudioEngine::~AudioEngine()
{
	Stop();
	delete fAbout;
}

void
AudioEngine::StartAbout()
{
	if (!fLock.Lock())
		return;
	delete fAbout;
	fAbout = new pmx::AboutSound(unsigned(system_time()));
	fLock.Unlock();
}

void
AudioEngine::FadeOutAbout()
{
	if (fLock.Lock()) {
		if (fAbout != NULL)
			fAbout->FadeOut();
		fLock.Unlock();
	}
}

status_t
AudioEngine::Start()
{
	if (fPlayer != NULL)
		return B_OK;
	media_raw_audio_format format = media_raw_audio_format::wildcard;
	format.frame_rate = pmx::kSampleRate;
	format.channel_count = 2;
	format.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = pmx::kBlockFrames * 4;
	fPlayer = new BSoundPlayer(&format, "PowerMix", PlayBuffer, NULL, this);
	status_t status = fPlayer->InitCheck();
	if (status != B_OK) {
		// No audio output: 3dmiX stalled here; PowerMix keeps mixing in
		// time, silently, so the view and meters still move.
		delete fPlayer;
		fPlayer = NULL;
		fClockRunning = true;
		fClock = spawn_thread(SilentClock, "silent clock", B_URGENT_DISPLAY_PRIORITY, this);
		resume_thread(fClock);
		return status;
	}
	fPlayer->Start();
	fPlayer->SetHasData(true);
	return B_OK;
}

void
AudioEngine::Stop()
{
	if (fClock >= 0) {
		fClockRunning = false;
		status_t result;
		wait_for_thread(fClock, &result);
		fClock = -1;
	}
	if (fPlayer == NULL)
		return;
	fPlayer->Stop();
	delete fPlayer;
	fPlayer = NULL;
}

void
AudioEngine::SetPaused(bool paused)
{
	fPaused = paused;
}

void
AudioEngine::PlayBuffer(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	AudioEngine* engine = static_cast<AudioEngine*>(cookie);
	if (format.format != media_raw_audio_format::B_AUDIO_SHORT
		|| format.channel_count != 2) {
		memset(buffer, 0, size);
		return;
	}
	engine->Fill(static_cast<int16*>(buffer), size / 4);
}

void
AudioEngine::Fill(int16* out, size_t frames)
{
	while (frames > 0) {
		if (fBlockUsed == pmx::kBlockFrames) {
			if (fPaused || !fLock.Lock()) {
				memset(fBlock, 0, sizeof(fBlock));
				fLeftLevel = fRightLevel = 0;
			} else if (fAbout != NULL) {
				fAbout->NextBlock(fBlock);
				if (fAbout->Finished()) {
					delete fAbout;
					fAbout = NULL;
				}
				fLock.Unlock();
			} else {
				fMixer.NextBlock(fBlock);
				fLeftLevel = fMixer.LeftLevel();
				fRightLevel = fMixer.RightLevel();
				fLock.Unlock();
			}
			fBlockUsed = 0;
		}
		size_t n = std::min<size_t>(frames, pmx::kBlockFrames - fBlockUsed);
		memcpy(out, fBlock + 2 * fBlockUsed, n * 4);
		out += 2 * n;
		frames -= n;
		fBlockUsed += int32(n);
	}
}

status_t
AudioEngine::SilentClock(void* cookie)
{
	AudioEngine* engine = static_cast<AudioEngine*>(cookie);
	int16 scratch[2 * pmx::kBlockFrames];
	bigtime_t next = system_time();
	while (engine->fClockRunning) {
		engine->Fill(scratch, pmx::kBlockFrames);
		next += bigtime_t(pmx::kBlockSeconds * 1000000.0);
		snooze_until(next, B_SYSTEM_TIMEBASE);
	}
	return B_OK;
}
