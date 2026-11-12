// Importer.cpp: see Importer.h.
#include "Importer.h"

#ifndef POWERMIX_NO_MEDIA_KIT
#	include <MediaFile.h>
#	include <MediaTrack.h>
#endif

#include <Path.h>

#include "Edit.h"
#include "WaveReader.h"

status_t
DecodeSound(const entry_ref& ref, std::vector<int16>& samples)
{
	// WAVE ourselves first: Haiku's MS ADPCM decoder mangles the R5 sounds
	BPath path(&ref);
	pmx::DecodedSound sound;
	if (path.InitCheck() == B_OK && pmx::ReadWave(path.Path(), sound)) {
		samples = pmx::ToClipSamples(sound.samples.data(), sound.Frames(), sound.channels,
			sound.rate);
		return samples.empty() ? B_ERROR : B_OK;
	}
#ifdef POWERMIX_NO_MEDIA_KIT
	return B_NOT_SUPPORTED;	// no Media Kit on Haiku arm64: WAVE only
#else
	BMediaFile file(&ref);
	status_t status = file.InitCheck();
	if (status != B_OK)
		return status;
	for (int32 i = 0; i < file.CountTracks(); i++) {
		BMediaTrack* track = file.TrackAt(i);
		if (track == NULL)
			continue;
		media_format format;
		format.type = B_MEDIA_RAW_AUDIO;
		format.u.raw_audio = media_raw_audio_format::wildcard;
		format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
		if (track->DecodedFormat(&format) != B_OK || format.type != B_MEDIA_RAW_AUDIO) {
			file.ReleaseTrack(track);
			continue;
		}
		const media_raw_audio_format& raw = format.u.raw_audio;
		const int channels = int(raw.channel_count > 0 ? raw.channel_count : 1);
		const size_t sampleSize = raw.format & 0xf;
		std::vector<char> buffer(raw.buffer_size > 0 ? raw.buffer_size : 65536);
		std::vector<float> decoded;
		int64 frames = 0;
		while (track->ReadFrames(buffer.data(), &frames) == B_OK && frames > 0) {
			size_t count = size_t(frames) * size_t(channels);
			for (size_t k = 0; k < count; k++) {
				const char* p = buffer.data() + k * sampleSize;
				float v = 0;
				switch (raw.format) {
					case media_raw_audio_format::B_AUDIO_FLOAT:
						v = *reinterpret_cast<const float*>(p);
						break;
					case media_raw_audio_format::B_AUDIO_INT:
						v = *reinterpret_cast<const int32*>(p) / 2147483648.0f;
						break;
					case media_raw_audio_format::B_AUDIO_SHORT:
						v = *reinterpret_cast<const int16*>(p) / 32768.0f;
						break;
					case media_raw_audio_format::B_AUDIO_UCHAR:
						v = (*reinterpret_cast<const uint8*>(p) - 128) / 128.0f;
						break;
					case media_raw_audio_format::B_AUDIO_CHAR:
						v = *reinterpret_cast<const int8*>(p) / 128.0f;
						break;
				}
				decoded.push_back(v);
			}
		}
		file.ReleaseTrack(track);
		samples = pmx::ToClipSamples(decoded.data(), decoded.size() / size_t(channels), channels,
			raw.frame_rate > 0 ? raw.frame_rate : 44100);
		return samples.empty() ? B_ERROR : B_OK;
	}
	return B_BAD_TYPE;
#endif
}
