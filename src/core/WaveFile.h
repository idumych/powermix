// WaveFile.h: writes 16-bit stereo 44.1 kHz RIFF WAVE (Save mix as WAVE;
// 3dmiX used BSoundFile with the same format). Portable.
#ifndef POWERMIX_WAVE_FILE_H
#define POWERMIX_WAVE_FILE_H

#include <cstdint>
#include <vector>

// Interleaved left/right frames. Returns 0 on success, -1 on failure.
int WriteWave(const char* path, const std::vector<int16_t>& frames);

#endif
