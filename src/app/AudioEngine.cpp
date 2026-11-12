// AudioEngine.cpp: 3dmiX streamed 512-frame blocks at 44.1 kHz through the
// old BDACStream (init_stuff, record_func); BSoundPlayer does the same job.
#include "AudioEngine.h"

#ifndef POWERMIX_NO_MEDIA_KIT
#	include <SoundPlayer.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
	fRightLevel(0),
	fClickOn(false),
	fBeat(0.5),
	fBeatsPerBar(4),
	fCountIn(0),
	fCountInTotal(0),
	fClickAt(-1),
	fClickPitch(1000),
	fDump(NULL)
{
	// for tests: everything played, raw 16-bit stereo at 44.1 kHz
	if (const char* dump = getenv("POWERMIX_DUMP_OUTPUT"))
		fDump = fopen(dump, "wb");
}

AudioEngine::~AudioEngine()
{
	Stop();
	delete fAbout;
	if (fDump != NULL)
		fclose(fDump);
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
#ifdef POWERMIX_NO_MEDIA_KIT
	// Haiku arm64 has no Media Kit: mix silently in time
	if (fClock < 0) {
		fClockRunning = true;
		fClock = spawn_thread(SilentClock, "silent clock", B_URGENT_DISPLAY_PRIORITY, this);
		resume_thread(fClock);
	}
	return B_NOT_SUPPORTED;
#else
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
#endif
}

void
AudioEngine::SetClick(bool on, double beatSeconds, int beatsPerBar)
{
	if (!Lock())
		return;
	fClickOn = on;
	fBeat = beatSeconds > 0.05 ? beatSeconds : 0.5;
	fBeatsPerBar = std::max(1, beatsPerBar);
	if (!on && fCountIn == 0)
		fClickAt = -1;
	Unlock();
}

void
AudioEngine::StartCountIn(int beats)
{
	if (!Lock())
		return;
	fCountInTotal = int32(beats * fBeat * pmx::kSampleRate);
	fCountIn = fCountInTotal;
	Unlock();
}

// A tick starts on every beat that falls in this block, which begins `from`
// seconds into the mix (or into the count-in); the sounding tick goes on
void
AudioEngine::AddClicks(double from, bool countIn)
{
	const double frame = 1.0 / pmx::kSampleRate;
	const int32 tickFrames = pmx::kSampleRate / 30;	// 33 ms
	for (int32 i = 0; i < pmx::kBlockFrames; i++) {
		const double t = from + i * frame;
		const double beat = t / fBeat;
		const long index = long(floor(beat + 1e-9));
		// this frame is the first one at or after a beat
		if (beat - index < frame / fBeat - 1e-12) {
			fClickAt = 0;
			fClickPitch = index % fBeatsPerBar == 0 ? 1600 : 1000;
		}
		if (fClickAt < 0)
			continue;
		const float envelope = expf(-fClickAt / 200.0f);
		const float v = 9000 * envelope
			* sinf(2 * 3.1415926f * fClickPitch * fClickAt / pmx::kSampleRate);
		for (int ch = 0; ch < 2; ch++) {
			int32 sum = fBlock[2 * i + ch] + int32(v);
			fBlock[2 * i + ch] = int16(std::max(-32768, std::min(32767, int(sum))));
		}
		if (++fClickAt >= tickFrames)
			fClickAt = -1;
	}
	(void)countIn;
}

double
AudioEngine::OutputDelay() const
{
#ifdef POWERMIX_NO_MEDIA_KIT
	return 0;
#else
	if (fPlayer == NULL)
		return 0;
	const media_raw_audio_format& format = fPlayer->Format();
	const size_t frameSize = std::max<size_t>(1, (format.format & 0xf)
		* std::max<uint32>(1, format.channel_count));
	const double buffer = format.frame_rate > 0
		? double(format.buffer_size / frameSize) / format.frame_rate : 0;
	return fPlayer->Latency() / 1e6 + buffer;
#endif
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
#ifndef POWERMIX_NO_MEDIA_KIT
	if (fPlayer == NULL)
		return;
	fPlayer->Stop();
	delete fPlayer;
	fPlayer = NULL;
#endif
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
			} else if (fCountIn > 0) {
				// counting in: clicks only, the mix waits
				memset(fBlock, 0, sizeof(fBlock));
				AddClicks(double(fCountInTotal - fCountIn) / pmx::kSampleRate, true);
				fCountIn = std::max(int32(0), fCountIn - int32(pmx::kBlockFrames));
				fLeftLevel = fRightLevel = 0;
				fLock.Unlock();
			} else {
				const double from = fMixer.Time();
				fMixer.NextBlock(fBlock);
				fLeftLevel = fMixer.LeftLevel();
				fRightLevel = fMixer.RightLevel();
				if (fClickOn)
					AddClicks(from, false);
				fLock.Unlock();
			}
			if (fDump != NULL)
				fwrite(fBlock, sizeof(fBlock), 1, fDump);
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
