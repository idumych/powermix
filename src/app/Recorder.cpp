// Recorder.cpp: see Recorder.h.
#include "Recorder.h"

#ifndef POWERMIX_NO_MEDIA_KIT
#include <MediaRecorder.h>
#include <MediaRoster.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Edit.h"

Recorder::Recorder()
	:
	fRecorder(NULL),
#if !defined(POWERMIX_NO_MEDIA_KIT) && !defined(APP_COSMOE)
	fHoldingInput(false),
#endif
	fChannels(1),
	fRate(44100),
	fRecording(false),
	fLevel(0)
{
}

Recorder::~Recorder()
{
	if (fRecording)
		Stop();
#ifndef POWERMIX_NO_MEDIA_KIT
	delete fRecorder;
#endif
}

bool
Recorder::Available()
{
#ifdef POWERMIX_NO_MEDIA_KIT
	return false;
#else
	return true;
#endif
}

status_t
Recorder::Start()
{
#ifdef POWERMIX_NO_MEDIA_KIT
	return B_NOT_SUPPORTED;
#else
	if (fRecording)
		return B_OK;
	{
		std::lock_guard<std::mutex> guard(fLock);
		fFrames.clear();
		fFrames.reserve(44100 * 2 * 60);
	}
	delete fRecorder;
	fRecorder = new BMediaRecorder("PowerMix take", B_MEDIA_RAW_AUDIO);
	status_t status = fRecorder->InitCheck();
	if (status != B_OK)
		return status;
	fRecorder->SetHooks(Record, NULL, this);
	status = Connect();
	if (status != B_OK)
		return status;
	fRecording = true;
	status = fRecorder->Start();
	if (status != B_OK) {
		fRecording = false;
		Disconnect();
	}
	return status;
#endif
}

#ifndef POWERMIX_NO_MEDIA_KIT
// BMediaRecorder::Connect(format) on Haiku hooks up to the system mixer's
// output (what's playing), which the sound card already has: connect to a
// free output of the default audio input instead, as SoundRecorder does,
// in the format it gives. Hokku's Connect(format) opens the capture device.
status_t
Recorder::Connect()
{
	media_format format;
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio = media_raw_audio_format::wildcard;
	fRecorder->SetAcceptedFormat(format);
#ifdef APP_COSMOE
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
	format.u.raw_audio.channel_count = 1;
	format.u.raw_audio.frame_rate = 44100;
	status_t status = fRecorder->Connect(format);
	if (status != B_OK) {
		// the input couldn't give that: take what it gives
		format.u.raw_audio = media_raw_audio_format::wildcard;
		status = fRecorder->Connect(format);
	}
	return status;
#else
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return B_MEDIA_SYSTEM_FAILURE;
	status_t status = roster->GetAudioInput(&fInput);
	if (status != B_OK)
		return status;
	fHoldingInput = true;
	media_output output;
	int32 count = 0;
	status = roster->GetFreeOutputsFor(fInput, &output, 1, &count,
		B_MEDIA_RAW_AUDIO);
	if (status == B_OK && count < 1)
		status = B_BUSY;				// another program is recording
	if (status == B_OK) {
		format.u.raw_audio = output.format.u.raw_audio;
		status = fRecorder->Connect(fInput, &output, &format);
	}
	if (status != B_OK)
		Disconnect();
	return status;
#endif
}

void
Recorder::Disconnect()
{
	if (fRecorder != NULL && fRecorder->IsConnected())
		fRecorder->Disconnect();
#ifndef APP_COSMOE
	if (fHoldingInput) {
		BMediaRoster::Roster()->ReleaseNode(fInput);
		fHoldingInput = false;
	}
#endif
}
#endif

std::vector<int16>
Recorder::Stop()
{
	std::vector<int16> samples;
#ifndef POWERMIX_NO_MEDIA_KIT
	if (!fRecording || fRecorder == NULL)
		return samples;
	fRecorder->Stop();
	Disconnect();
	fRecording = false;
	fLevel = 0;
	std::lock_guard<std::mutex> guard(fLock);
	const size_t frames = fFrames.size() / std::max(1, fChannels);
	samples = pmx::ToClipSamples(fFrames.data(), frames, fChannels, fRate);
	fFrames.clear();
	fFrames.shrink_to_fit();
#endif
	return samples;
}

double
Recorder::Seconds() const
{
	Recorder* self = const_cast<Recorder*>(this);
	std::lock_guard<std::mutex> guard(self->fLock);
	return fRate > 0 ? fFrames.size() / double(std::max(1, fChannels)) / fRate : 0;
}

// The record hook, on the input's thread: one buffer to floats
void
Recorder::Record(void* cookie, bigtime_t timestamp, void* data, size_t size,
	const media_format& format)
{
#ifndef POWERMIX_NO_MEDIA_KIT
	Recorder* self = static_cast<Recorder*>(cookie);
	if (!self->fRecording)
		return;
	const media_multi_audio_format& raw = format.u.raw_audio;
	size_t samples = 0;
	float peak = 0;
	std::lock_guard<std::mutex> guard(self->fLock);
	self->fChannels = std::max<int>(1, int(raw.channel_count));
	self->fRate = raw.frame_rate > 0 ? raw.frame_rate : 44100;
	auto add = [&](float v) {
		self->fFrames.push_back(v);
		peak = std::max(peak, std::fabs(v));
	};
	switch (raw.format) {
		case media_raw_audio_format::B_AUDIO_SHORT:
		{
			samples = size / 2;
			const int16* in = static_cast<const int16*>(data);
			for (size_t i = 0; i < samples; i++)
				add(in[i] / 32768.0f);
			break;
		}
		case media_raw_audio_format::B_AUDIO_FLOAT:
		{
			samples = size / 4;
			const float* in = static_cast<const float*>(data);
			for (size_t i = 0; i < samples; i++)
				add(in[i]);
			break;
		}
		case media_raw_audio_format::B_AUDIO_INT:
		{
			samples = size / 4;
			const int32* in = static_cast<const int32*>(data);
			for (size_t i = 0; i < samples; i++)
				add(in[i] / 2147483648.0f);
			break;
		}
		case media_raw_audio_format::B_AUDIO_UCHAR:
		{
			samples = size;
			const uint8* in = static_cast<const uint8*>(data);
			for (size_t i = 0; i < samples; i++)
				add((in[i] - 128) / 128.0f);
			break;
		}
		case media_raw_audio_format::B_AUDIO_CHAR:
		{
			samples = size;
			const int8* in = static_cast<const int8*>(data);
			for (size_t i = 0; i < samples; i++)
				add(in[i] / 128.0f);
			break;
		}
		default:
			break;
	}
	self->fLevel = peak;
#endif
}
