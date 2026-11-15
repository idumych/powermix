// MidiClock.h: MIDI clock for programs that follow PowerMix, such as
// Hydrogen: a Midi Kit output, "PowerMix output", that sends Start when a
// take begins (after the count-in), 24 clocks a beat at the click's tempo
// while it records, and Stop when it ends. Connect a program's MIDI input
// to it (in Hydrogen: Settings ▸ MIDI, input "PowerMix output", with MIDI
// clock and transport input on). Haiku only; elsewhere it does nothing.
#ifndef POWERMIX_MIDI_CLOCK_H
#define POWERMIX_MIDI_CLOCK_H

#include <OS.h>

#include <atomic>

class BMidiLocalProducer;

class MidiClock {
public:
	MidiClock();
	~MidiClock();

	static bool Available();

	// Start at `when` (system time), then clock at `bpm` until Stop()
	void Start(bigtime_t when, double bpm);
	void Stop();
	bool IsRunning() const { return fRunning; }

private:
	static status_t ClockThread(void* cookie);
	void Clock();

	BMidiLocalProducer* fProducer;
	thread_id fThread;
	std::atomic<bool> fRunning;
	bigtime_t fStart;
	double fTick;		// microseconds between clocks
};

#endif
