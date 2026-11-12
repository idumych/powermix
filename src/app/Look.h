// Look.h: the optional modern look (Display ▸ Modern look): its colours,
// drawing helpers and the settings file that remembers the choice.
#ifndef POWERMIX_LOOK_H
#define POWERMIX_LOOK_H

#include <GraphicsDefs.h>
#include <Rect.h>

class BView;

namespace look {

bool IsModern();
void SetModern(bool modern);

extern const rgb_color kGround;		// behind everything
extern const rgb_color kPanel;		// sidebars, toolbar, timeline
extern const rgb_color kRaised;		// buttons, cards
extern const rgb_color kBorder;
extern const rgb_color kLine;		// separators
extern const rgb_color kText;
extern const rgb_color kSoft;		// secondary text
extern const rgb_color kMuted;		// labels
extern const rgb_color kAccent;
extern const rgb_color kAccentDim;	// pressed and selected grounds
extern const rgb_color kAccentLine;
extern const rgb_color kAccentText;
extern const rgb_color kWarn;
extern const rgb_color kWarnDim;
extern const rgb_color kStage;		// behind the 3D view

// The columns' colours in the 3D view: red, green, blue by track number
rgb_color TrackColor(int index);
rgb_color Mix(rgb_color a, rgb_color b, float t);

void FillRound(BView* view, BRect rect, float radius, rgb_color fill);
void StrokeRound(BView* view, BRect rect, float radius, rgb_color stroke);
// A small capitals label: "TRACKS · 6"
void DrawCaption(BView* view, const char* text, BPoint where, rgb_color color);

}	// namespace look

// B_USER_SETTINGS_DIRECTORY/PowerMix_settings
struct Settings {
	bool modern = false;
	int32 viewMode = 1;		// modern: 0 stage, 1 split, 2 timeline
	BRect classicFrame = BRect(30, 30, 730, 430);	// TSoundApplication's
	BRect modernFrame = BRect(40, 40, 1319, 839);
	int32 inputLatency = 0;	// ms, added to the measured output delay

	void Load();
	void Save() const;
};

#endif
