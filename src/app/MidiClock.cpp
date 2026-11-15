// MidiClock.cpp: see MidiClock.h.
#include "MidiClock.h"

#ifdef POWERMIX_HAVE_MIDI_KIT
#include <midi2/Midi2Defs.h>
#include <midi2/MidiProducer.h>
#endif

#include <algorithm>

// How far ahead clocks are sent: the Midi Kit delivers each at its time
static const bigtime_t kAhead = 20000;

MidiClock::MidiClock()
	:
	fProducer(NULL),
	fThread(-1),
	fRunning(false),
	fStart(0),
	fTick(0)
{
#ifdef POWERMIX_HAVE_MIDI_KIT
	fProducer = new BMidiLocalProducer("PowerMix output");
	if (fProducer->IsValid())
		fProducer->Register();
	else {
		fProducer->Release();
		fProducer = NULL;
	}
#endif
}

MidiClock::~MidiClock()
{
	Stop();
#ifdef POWERMIX_HAVE_MIDI_KIT
	if (fProducer != NULL) {
		fProducer->Unregister();
		fProducer->Release();
	}
#endif
}

bool
MidiClock::Available()
{
#ifdef POWERMIX_HAVE_MIDI_KIT
	return true;
#else
	return false;
#endif
}

void
MidiClock::Start(bigtime_t when, double bpm)
{
#ifdef POWERMIX_HAVE_MIDI_KIT
	Stop();
	if (fProducer == NULL || bpm <= 0)
		return;
	fStart = when;
	fTick = 60000000.0 / (bpm * 24);
	// Song position 0 first, so a follower starts at its beginning
	fProducer->SpraySystemCommon(B_SONG_POSITION, 0, 0, when);
	fProducer->SpraySystemRealTime(B_START, when);
	fRunning = true;
	fThread = spawn_thread(ClockThread, "PowerMix MIDI clock", B_REAL_TIME_DISPLAY_PRIORITY,
		this);
	if (fThread >= 0)
		resume_thread(fThread);
	else
		fRunning = false;
#else
	(void)when;
	(void)bpm;
#endif
}

void
MidiClock::Stop()
{
#ifdef POWERMIX_HAVE_MIDI_KIT
	if (!fRunning)
		return;
	fRunning = false;
	status_t result;
	wait_for_thread(fThread, &result);
	fThread = -1;
	if (fProducer != NULL)
		fProducer->SpraySystemRealTime(B_STOP, system_time());
#endif
}

status_t
MidiClock::ClockThread(void* cookie)
{
	static_cast<MidiClock*>(cookie)->Clock();
	return B_OK;
}

void
MidiClock::Clock()
{
#ifdef POWERMIX_HAVE_MIDI_KIT
	// Counted from the start, so the tempo doesn't drift with rounding
	int64 tick = 0;
	while (fRunning) {
		const bigtime_t until = system_time() + kAhead;
		for (;;) {
			const bigtime_t at = fStart + bigtime_t(tick * fTick);
			if (at > until)
				break;
			fProducer->SpraySystemRealTime(B_TIMING_CLOCK, at);
			tick++;
		}
		snooze(std::max<bigtime_t>(1000, kAhead / 2));
	}
#endif
}
