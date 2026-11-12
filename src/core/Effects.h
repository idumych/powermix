// Effects.h: the sample editor's filters and edits (3dmiX's EditView:
// do_zero, do_volume, do_maxvolume, do_fade, do_echo, do_chorus, do_metal,
// do_compress, do_cut/copy/paste/clear) and the clip stretches behind
// Shift- and Option-resizing (SampleCache::pitch_down_soft and
// compress_soft). Each works on frames [from, to) of a sound. Portable.
#ifndef POWERMIX_EFFECTS_H
#define POWERMIX_EFFECTS_H

#include <cstdint>
#include <vector>

namespace pmx {

typedef std::vector<int16_t> Sound;

void Zero(Sound& s, size_t from, size_t to);
void ChangeVolume(Sound& s, size_t from, size_t to, float percent);	// 0..200
void MaxVolume(Sound& s, size_t from, size_t to);
void Fade(Sound& s, size_t from, size_t to, float finalPercent);		// from 100% to this
void Echo(Sound& s, size_t from, size_t to, float strength, float delayMs);
// 3dmiX takes the Strength slider (0..100) as the chorus delay in ms
void Chorus(Sound& s, size_t from, size_t to, float strength);
void Metal(Sound& s, size_t from, size_t to);
void Compress(Sound& s, size_t from, size_t to, float strength);

// Edits; the clipboard holds the copied frames
void CopyFrames(const Sound& s, size_t from, size_t to, Sound& clipboard);
void DeleteFrames(Sound& s, size_t from, size_t to);
void InsertFrames(Sound& s, size_t at, const Sound& frames);

// The whole sound made `factor` times as long: resampled (pitch changes)
// or stretched in time (pitch kept)
Sound Resample(const Sound& s, double factor);
Sound TimeStretch(const Sound& s, double factor);

}	// namespace pmx

#endif
