// Palette.cpp: the BeOS/Haiku 8-bit system palette and colour lookup.
// The palette is Haiku's kSystemPalette (headers/private/interface/
// Palette.h, MIT licence), which is BeOS's B_CMAP8 palette.
#include "Palette.h"

#include <cmath>
#include <cstring>

namespace pmx {

const Rgb kSystemPalette[256] = {
	{0, 0, 0}, {8, 8, 8}, {16, 16, 16}, {24, 24, 24},
	{32, 32, 32}, {40, 40, 40}, {48, 48, 48}, {56, 56, 56},
	{64, 64, 64}, {72, 72, 72}, {80, 80, 80}, {88, 88, 88},
	{96, 96, 96}, {104, 104, 104}, {112, 112, 112}, {120, 120, 120},
	{128, 128, 128}, {136, 136, 136}, {144, 144, 144}, {152, 152, 152},
	{160, 160, 160}, {168, 168, 168}, {176, 176, 176}, {184, 184, 184},
	{192, 192, 192}, {200, 200, 200}, {208, 208, 208}, {216, 216, 216},
	{224, 224, 224}, {232, 232, 232}, {240, 240, 240}, {248, 248, 248},
	{0, 0, 255}, {0, 0, 229}, {0, 0, 204}, {0, 0, 179},
	{0, 0, 154}, {0, 0, 129}, {0, 0, 105}, {0, 0, 80},
	{0, 0, 55}, {0, 0, 30}, {255, 0, 0}, {228, 0, 0},
	{203, 0, 0}, {178, 0, 0}, {153, 0, 0}, {128, 0, 0},
	{105, 0, 0}, {80, 0, 0}, {55, 0, 0}, {30, 0, 0},
	{0, 255, 0}, {0, 228, 0}, {0, 203, 0}, {0, 178, 0},
	{0, 153, 0}, {0, 128, 0}, {0, 105, 0}, {0, 80, 0},
	{0, 55, 0}, {0, 30, 0}, {0, 152, 51}, {255, 255, 255},
	{203, 255, 255}, {203, 255, 203}, {203, 255, 152}, {203, 255, 102},
	{203, 255, 51}, {203, 255, 0}, {152, 255, 255}, {152, 255, 203},
	{152, 255, 152}, {152, 255, 102}, {152, 255, 51}, {152, 255, 0},
	{102, 255, 255}, {102, 255, 203}, {102, 255, 152}, {102, 255, 102},
	{102, 255, 51}, {102, 255, 0}, {51, 255, 255}, {51, 255, 203},
	{51, 255, 152}, {51, 255, 102}, {51, 255, 51}, {51, 255, 0},
	{255, 152, 255}, {255, 152, 203}, {255, 152, 152}, {255, 152, 102},
	{255, 152, 51}, {255, 152, 0}, {0, 102, 255}, {0, 102, 203},
	{203, 203, 255}, {203, 203, 203}, {203, 203, 152}, {203, 203, 102},
	{203, 203, 51}, {203, 203, 0}, {152, 203, 255}, {152, 203, 203},
	{152, 203, 152}, {152, 203, 102}, {152, 203, 51}, {152, 203, 0},
	{102, 203, 255}, {102, 203, 203}, {102, 203, 152}, {102, 203, 102},
	{102, 203, 51}, {102, 203, 0}, {51, 203, 255}, {51, 203, 203},
	{51, 203, 152}, {51, 203, 102}, {51, 203, 51}, {51, 203, 0},
	{255, 102, 255}, {255, 102, 203}, {255, 102, 152}, {255, 102, 102},
	{255, 102, 51}, {255, 102, 0}, {0, 102, 152}, {0, 102, 102},
	{203, 152, 255}, {203, 152, 203}, {203, 152, 152}, {203, 152, 102},
	{203, 152, 51}, {203, 152, 0}, {152, 152, 255}, {152, 152, 203},
	{152, 152, 152}, {152, 152, 102}, {152, 152, 51}, {152, 152, 0},
	{102, 152, 255}, {102, 152, 203}, {102, 152, 152}, {102, 152, 102},
	{102, 152, 51}, {102, 152, 0}, {51, 152, 255}, {51, 152, 203},
	{51, 152, 152}, {51, 152, 102}, {51, 152, 51}, {51, 152, 0},
	{230, 134, 0}, {255, 51, 203}, {255, 51, 152}, {255, 51, 102},
	{255, 51, 51}, {255, 51, 0}, {0, 102, 51}, {0, 102, 0},
	{203, 102, 255}, {203, 102, 203}, {203, 102, 152}, {203, 102, 102},
	{203, 102, 51}, {203, 102, 0}, {152, 102, 255}, {152, 102, 203},
	{152, 102, 152}, {152, 102, 102}, {152, 102, 51}, {152, 102, 0},
	{102, 102, 255}, {102, 102, 203}, {102, 102, 152}, {102, 102, 102},
	{102, 102, 51}, {102, 102, 0}, {51, 102, 255}, {51, 102, 203},
	{51, 102, 152}, {51, 102, 102}, {51, 102, 51}, {51, 102, 0},
	{255, 0, 255}, {255, 0, 203}, {255, 0, 152}, {255, 0, 102},
	{255, 0, 51}, {255, 175, 19}, {0, 51, 255}, {0, 51, 203},
	{203, 51, 255}, {203, 51, 203}, {203, 51, 152}, {203, 51, 102},
	{203, 51, 51}, {203, 51, 0}, {152, 51, 255}, {152, 51, 203},
	{152, 51, 152}, {152, 51, 102}, {152, 51, 51}, {152, 51, 0},
	{102, 51, 255}, {102, 51, 203}, {102, 51, 152}, {102, 51, 102},
	{102, 51, 51}, {102, 51, 0}, {51, 51, 255}, {51, 51, 203},
	{51, 51, 152}, {51, 51, 102}, {51, 51, 51}, {51, 51, 0},
	{255, 203, 102}, {255, 203, 152}, {255, 203, 203}, {255, 203, 255},
	{0, 51, 152}, {0, 51, 102}, {0, 51, 51}, {0, 51, 0},
	{203, 0, 255}, {203, 0, 203}, {203, 0, 152}, {203, 0, 102},
	{203, 0, 51}, {255, 227, 70}, {152, 0, 255}, {152, 0, 203},
	{152, 0, 152}, {152, 0, 102}, {152, 0, 51}, {152, 0, 0},
	{102, 0, 255}, {102, 0, 203}, {102, 0, 152}, {102, 0, 102},
	{102, 0, 51}, {102, 0, 0}, {51, 0, 255}, {51, 0, 203},
	{51, 0, 152}, {51, 0, 102}, {51, 0, 51}, {51, 0, 0},
	{255, 203, 51}, {255, 203, 0}, {255, 255, 0}, {255, 255, 51},
	{255, 255, 102}, {255, 255, 152}, {255, 255, 203}, {255, 255, 255},
};

namespace {

// The 15-bit inverse map as Haiku's app_server builds it for BScreen
// (PaletteConverter): each 5-bit colour expanded with << 3, matched with a
// weighted ("psycho-visual") distance.
struct InverseMap {
	uint8_t index[32768];

	static int Distance(int r1, int g1, int b1, int r2, int g2, int b2)
	{
		int rd = r1 - r2, gd = g1 - g2, bd = b1 - b2;
		int rmean = (r1 + r2) / 2;
		return (((512 + rmean) * rd * rd) >> 8) + 4 * gd * gd
			+ (((767 - rmean) * bd * bd) >> 8);
	}

	InverseMap()
	{
		for (int c = 0; c < 32768; c++) {
			int r = ((c >> 10) & 31) << 3, g = ((c >> 5) & 31) << 3, b = (c & 31) << 3;
			int best = 0, bestDistance = 1 << 30;
			// 255 is B_TRANSPARENT_MAGIC_CMAP8, never chosen
			for (int i = 0; i < 255; i++) {
				int distance = Distance(r, g, b, kSystemPalette[i].r, kSystemPalette[i].g,
					kSystemPalette[i].b);
				if (distance < bestDistance) {
					bestDistance = distance;
					best = i;
				}
			}
			index[c] = uint8_t(best);
		}
	}
};

int Trunc(double v) { return int(v); }

}	// namespace

InverseMap& Map()
{
	static InverseMap sMap;
	return sMap;
}

uint8_t
IndexForColor(uint8_t r, uint8_t g, uint8_t b)
{
	return Map().index[(r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)];
}

void
SetInverseMap(const uint8_t map[32768])
{
	memcpy(Map().index, map, 32768);
}

uint8_t
Gamma(int i)
{
	static uint8_t sTable[256];
	static bool sReady = false;
	if (!sReady) {
		for (int k = 0; k < 256; k++)
			sTable[k] = uint8_t(Trunc(255.0 * pow(k / 255.0, 1.0 / 1.45)));
		sReady = true;
	}
	return sTable[i & 0xff];
}

int
ColorMix(int a, int b)
{
	int product = a * b;
	if (product < 0)
		product += 0x1ff;
	int v = Trunc(double((product >> 9) + Trunc(b / 1.9) + Trunc(a * 0.95)) * 0.8);
	return v > 255 ? 255 : v;
}

const ColorTables&
ColorTables::Get()
{
	static const ColorTables sTables;
	return sTables;
}

ColorTables::ColorTables()
{
	auto color = [](int r, int g, int b) {
		return IndexForColor(Gamma(r), Gamma(g), Gamma(b));
	};
	for (int i = 0; i < 32; i++) {
		int bright = Trunc(7.0 * i);
		fRamps[kRampBlue].index[i] = color(0, 0, bright);
		fRamps[kRampRed].index[i] = color(bright, 0, 0);
		fRamps[kRampGreen].index[i] = color(i, Trunc(2.6 * i), i);
		int grey = Trunc(2.2857142857142856 * i);
		fRamps[kRampGrey].index[i] = color(grey, grey, grey);
		for (int k = 0; k < 20; k++) {
			int b = Trunc((k + 8) / 3.3 * i) & 0xff;
			fRamps[kRampDepthBlue + k].index[i] = IndexForColor(0, 0, Gamma(Gamma(b)));
		}
	}

	// BuildMappers: red/green/blue at 55, 93, 131, 229; cyan, magenta and
	// yellow at 35, 73, 111, 229
	for (int hue = 0; hue < kHueCount; hue++) {
		for (int level = 0; level < 4; level++) {
			int c = level == 3 ? 229 : (hue < 3 ? 55 : 35) + 38 * level;
			int cr = 0, cg = 0, cb = 0;
			switch (hue) {
				case kHueRed: cr = c; break;
				case kHueGreen: cg = c; break;
				case kHueBlue: cb = c; break;
				case kHueCyan: cg = cb = c; break;
				case kHueMagenta: cr = cb = c; break;
				case kHueYellow: cr = cg = c; break;
			}
			Mapper& mapper = fMappers[hue * 4 + level];
			for (int i = 0; i < 256; i++) {
				const Rgb& under = kSystemPalette[i];
				mapper.index[i] = color(ColorMix(cr, under.r), ColorMix(cg, under.g),
					ColorMix(cb, under.b));
			}
		}
	}
}

}	// namespace pmx
