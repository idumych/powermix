// Importer.h: reading a dropped sound with the Media Kit (3dmiX's
// SampleCache file constructor) into a clip's samples.
#ifndef POWERMIX_IMPORTER_H
#define POWERMIX_IMPORTER_H

#include <Entry.h>

#include <vector>

// Mono 16-bit 44.1 kHz samples of the file's first audio track
status_t DecodeSound(const entry_ref& ref, std::vector<int16>& samples);

#endif
