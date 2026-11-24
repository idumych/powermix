// Recorder.cpp: see Recorder.h.
#include "Recorder.h"

#ifndef POWERMIX_NO_MEDIA_KIT
#include <MediaRecorder.h>
#include <MediaRoster.h>
#endif

#include <OS.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Edit.h"

Recorder::Recorder()
	:
	fRecorder(NULL),
#ifndef POWERMIX_NO_MEDIA_KIT
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

void
Recorder::SetSource(const char* node, const char* output)
{
	fSourceNode = node != NULL ? node : "";
	fSourceOutput = output != NULL ? output : "";
}

#ifndef POWERMIX_NO_MEDIA_KIT
// Programs killed while their nodes were registered leave entries behind
// whose port is gone: they answer every query with an error (and a
// "Bad port ID" line on stderr). Looking at the port skips them quietly.
static bool
NodeIsAlive(const media_node& node)
{
	port_info info;
	return get_port_info(node.port, &info) == B_OK;
}

// Running producers' free raw audio outputs, but not the audio input
// (that's the default) or PowerMix's own nodes. Killed programs can
// leave nodes behind whose port is gone: they don't answer and are left
// out.
std::vector<std::pair<BString, BString>>
Recorder::Sources()
{
	std::vector<std::pair<BString, BString>> sources;
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return sources;
	media_node input;
	media_node_id inputId = -1;
	if (roster->GetAudioInput(&input) == B_OK) {
		inputId = input.node;
		roster->ReleaseNode(input);
	}
	live_node_info nodes[64];
	int32 count = 64;
	if (roster->GetLiveNodes(nodes, &count, NULL, NULL, NULL, B_BUFFER_PRODUCER) != B_OK)
		return sources;
	for (int32 i = 0; i < count; i++) {
		if (nodes[i].node.node == inputId || strncmp(nodes[i].name, "PowerMix", 8) == 0
			|| !NodeIsAlive(nodes[i].node))
			continue;
		media_output outputs[16];
		int32 outputCount = 0;
		if (roster->GetFreeOutputsFor(nodes[i].node, outputs, 16, &outputCount,
				B_MEDIA_RAW_AUDIO) != B_OK)
			continue;
		for (int32 j = 0; j < outputCount; j++)
			sources.push_back(std::make_pair(BString(nodes[i].name), BString(outputs[j].name)));
	}
	return sources;
}

// The first live node of that name with a free output of that name (any
// free output when no name is given). B_NAME_NOT_FOUND: no such node runs;
// B_BUSY: it does, but its output is connected elsewhere.
status_t
Recorder::FindSource(BMediaRoster* roster, const BString& nodeName, const BString& outputName,
	media_node& node, media_output& output)
{
	live_node_info nodes[16];
	int32 count = 16;
	status_t status = roster->GetLiveNodes(nodes, &count, NULL, NULL, nodeName.String(),
		B_BUFFER_PRODUCER);
	if (status != B_OK)
		return status;
	bool found = false;
	for (int32 i = 0; i < count; i++) {
		if (!NodeIsAlive(nodes[i].node))
			continue;
		found = true;
		media_output outputs[16];
		int32 outputCount = 0;
		if (roster->GetFreeOutputsFor(nodes[i].node, outputs, 16, &outputCount,
				B_MEDIA_RAW_AUDIO) != B_OK)
			continue;
		for (int32 j = 0; j < outputCount; j++) {
			if (outputName.Length() == 0 || outputName == outputs[j].name) {
				if (roster->GetNodeFor(nodes[i].node.node, &node) != B_OK)
					continue;
				output = outputs[j];
				return B_OK;
			}
		}
	}
	return found ? B_BUSY : B_NAME_NOT_FOUND;
}
#else
std::vector<std::pair<BString, BString>>
Recorder::Sources()
{
	return std::vector<std::pair<BString, BString>>();
}

#endif

#ifndef POWERMIX_NO_MEDIA_KIT
// BMediaRecorder::Connect(format) on Haiku hooks up to the system mixer's
// output (what's playing), which the sound card already has: connect to a
// free output of the default audio input instead, as SoundRecorder does,
// in the format it gives. Hokku's roster offers the input and the programs
// playing sound the same way.
status_t
Recorder::Connect()
{
	media_format format;
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio = media_raw_audio_format::wildcard;
	fRecorder->SetAcceptedFormat(format);
	BMediaRoster* roster = BMediaRoster::Roster();
	if (roster == NULL)
		return B_MEDIA_SYSTEM_FAILURE;
	media_output output;
	status_t status = B_OK;
	if (fSourceNode.Length() > 0) {
		status = FindSource(roster, fSourceNode, fSourceOutput, fInput, output);
		if (status != B_OK)
			return status;
		fHoldingInput = true;
	} else {
		status = roster->GetAudioInput(&fInput);
		if (status != B_OK)
			return status;
		fHoldingInput = true;
		int32 count = 0;
		status = roster->GetFreeOutputsFor(fInput, &output, 1, &count,
			B_MEDIA_RAW_AUDIO);
		if (status == B_OK && count < 1)
			status = B_BUSY;				// another program is recording
	}
	if (status == B_OK) {
		format.u.raw_audio = output.format.u.raw_audio;
		status = fRecorder->Connect(fInput, &output, &format);
	}
	if (status != B_OK)
		Disconnect();
	return status;
}

void
Recorder::Disconnect()
{
	if (fRecorder != NULL && fRecorder->IsConnected())
		fRecorder->Disconnect();
	if (fHoldingInput) {
		BMediaRoster::Roster()->ReleaseNode(fInput);
		fHoldingInput = false;
	}
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
	std::lock_guard<std::mutex> guard(fLock);
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
