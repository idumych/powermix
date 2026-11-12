// Palette.h: 8-bit colour as 3dmiX's 3D view uses it. The view renders
// palette indices (B_CMAP8) and gets translucency from lookup tables built
// on BeOS's system palette, so the clone does the same and converts the
// finished frame to RGB. Portable: no Haiku headers.
#ifndef POWERMIX_PALETTE_H
#define POWERMIX_PALETTE_H

#include <cstdint>

namespace pmx {

struct Rgb {
	uint8_t r, g, b;
};

extern const Rgb kSystemPalette[256];

// The nearest palette entry, through a 15-bit table as
// BScreen::IndexForColor does.
uint8_t IndexForColor(uint8_t r, uint8_t g, uint8_t b);
// Tests only: use another 15-bit inverse map (BeOS R5's own, dumped from
// the reference VM) before ColorTables::Get() is first called.
void SetInverseMap(const uint8_t map[32768]);

// 3dmiX's Gamma table: (int)(255 (i / 255)^(1 / 1.45)).
uint8_t Gamma(int i);

// cmix(): the colour of object component a over background component b
// (TSoundView::BuildMappers).
int ColorMix(int a, int b);

// 32-step intensity ramps for lines (TSoundView's constructor); entry i
// is the colour at intensity i of 31.
struct Ramp {
	uint8_t index[32];
};

enum {
	kRampBlue = 0,		// floor grid
	kRampRed,			// the L and R letters (vector colour 200)
	kRampGreen,			// vector colour 100
	kRampGrey,			// label leader lines
	kRampDepthBlue,		// 20 of them, dimmer to brighter
	kRampCount = kRampDepthBlue + 20
};

// Face colours: 6 hues x 4 levels; mapper (hue * 4 + level) sends a
// palette index to the index of the face's colour over it.
enum {
	kHueRed = 0, kHueGreen, kHueBlue, kHueCyan, kHueMagenta, kHueYellow,
	kHueCount
};

struct Mapper {
	uint8_t index[256];
};

class ColorTables {
public:
	static const ColorTables& Get();

	const Ramp& RampAt(int i) const { return fRamps[i]; }
	const Mapper& MapperAt(int i) const { return fMappers[i]; }

private:
	ColorTables();

	Ramp fRamps[kRampCount];
	Mapper fMappers[kHueCount * 4];
};

}	// namespace pmx

#endif
