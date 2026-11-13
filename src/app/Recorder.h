// Recorder.h: records the default audio input for a take (BMediaRecorder;
// on Linux, Hokku's, through miniaudio). Whatever format the input gives
// is turned into a clip's: mono 16-bit at 44.1 kHz.
#ifndef POWERMIX_RECORDER_H
#define POWERMIX_RECORDER_H

#include <SupportDefs.h>
#ifndef POWERMIX_NO_MEDIA_KIT
#include <MediaNode.h>
#endif

#include <atomic>
#include <mutex>
#include <vector>

class BMediaRecorder;
struct media_format;

class Recorder {
public:
	Recorder();
	~Recorder();

	// false where there's no Media Kit (Haiku arm64)
	static bool Available();

	status_t Start();
	// Stops and hands over what was recorded
	std::vector<int16> Stop();
	bool IsRecording() const { return fRecording; }
	// The last buffer's peak, 0..1, for the meter
	float Level() const { return fLevel; }
	// Seconds recorded so far
	double Seconds() const;

private:
	static void Record(void* cookie, bigtime_t timestamp, void* data, size_t size,
		const media_format& format);
	status_t Connect();
	void Disconnect();

	BMediaRecorder* fRecorder;
#if !defined(POWERMIX_NO_MEDIA_KIT) && !defined(APP_COSMOE)
	media_node fInput;				// the default audio input, while held
	bool fHoldingInput;
#endif
	std::mutex fLock;
	std::vector<float> fFrames;		// interleaved, as recorded
	int fChannels;
	float fRate;
	std::atomic<bool> fRecording;
	std::atomic<float> fLevel;
};

#endif
