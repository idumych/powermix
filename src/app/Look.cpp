// Look.cpp: see Look.h.
#include "Look.h"

#include <File.h>
#include <FindDirectory.h>
#include <Font.h>
#include <Message.h>
#include <Path.h>
#include <Screen.h>
#include <String.h>
#include <View.h>

#include <algorithm>

namespace look {

static bool sModern = false;

bool IsModern() { return sModern; }
void SetModern(bool modern) { sModern = modern; }

const rgb_color kGround = {11, 13, 17, 255};
const rgb_color kPanel = {16, 19, 26, 255};
const rgb_color kRaised = {24, 28, 36, 255};
const rgb_color kBorder = {42, 48, 64, 255};
const rgb_color kLine = {29, 36, 51, 255};
const rgb_color kText = {230, 233, 239, 255};
const rgb_color kSoft = {195, 201, 212, 255};
const rgb_color kMuted = {138, 147, 163, 255};
const rgb_color kAccent = {91, 155, 255, 255};
const rgb_color kAccentDim = {23, 36, 66, 255};
const rgb_color kAccentLine = {47, 79, 143, 255};
const rgb_color kAccentText = {169, 198, 255, 255};
const rgb_color kWarn = {255, 200, 87, 255};
const rgb_color kWarnDim = {58, 42, 18, 255};
const rgb_color kStage = {11, 15, 26, 255};

rgb_color
TrackColor(int index)
{
	static const rgb_color kColors[3] = {
		{255, 122, 107, 255}, {61, 220, 151, 255}, {91, 155, 255, 255}
	};
	return kColors[(index % 3 + 3) % 3];
}

rgb_color
Mix(rgb_color a, rgb_color b, float t)
{
	rgb_color c;
	c.red = uint8(a.red + (b.red - a.red) * t);
	c.green = uint8(a.green + (b.green - a.green) * t);
	c.blue = uint8(a.blue + (b.blue - a.blue) * t);
	c.alpha = 255;
	return c;
}

void
FillRound(BView* view, BRect rect, float radius, rgb_color fill)
{
	view->SetHighColor(fill);
	view->FillRoundRect(rect, radius, radius);
}

void
StrokeRound(BView* view, BRect rect, float radius, rgb_color stroke)
{
	view->SetHighColor(stroke);
	view->StrokeRoundRect(rect, radius, radius);
}

void
DrawCaption(BView* view, const char* text, BPoint where, rgb_color color)
{
	BFont font(be_bold_font);
	font.SetSize(10);
	view->SetFont(&font);
	view->SetHighColor(color);
	BString upper(text);
	upper.ToUpper();
	view->SetDrawingMode(B_OP_OVER);
	// a little tracking between the letters
	float x = where.x;
	for (int32 i = 0; i < upper.Length();) {
		int32 n = 1;
		while (i + n < upper.Length() && (upper.ByteAt(i + n) & 0xc0) == 0x80)
			n++;
		BString letter;
		upper.CopyInto(letter, i, n);
		view->DrawString(letter.String(), BPoint(x, where.y));
		x += font.StringWidth(letter.String()) + 0.8f;
		i += n;
	}
	view->SetDrawingMode(B_OP_COPY);
}

}	// namespace look

static bool
SettingsPath(BPath& path)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path, true) != B_OK)
		return false;
	return path.Append("PowerMix_settings") == B_OK;
}

static BRect
OnScreen(BRect frame, BRect fallback)
{
	BRect screen = BScreen().Frame();
	if (!frame.IsValid() || frame.Width() < 200 || !screen.Intersects(frame))
		return fallback;
	return frame;
}

void
Settings::Load()
{
	// the modern window fills most of a small screen
	BRect screen = BScreen().Frame();
	float width = std::min(1280.0f, screen.Width() - 40);
	float height = std::min(800.0f, screen.Height() - 60);
	modernFrame.Set(20, 30, 20 + width - 1, 30 + height - 1);
	BPath path;
	if (!SettingsPath(path))
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage message;
	if (file.InitCheck() != B_OK || message.Unflatten(&file) != B_OK)
		return;
	message.FindBool("modern", &modern);
	message.FindInt32("view", &viewMode);
	message.FindInt32("input latency", &inputLatency);
	message.FindBool("click", &click);
	message.FindBool("count-in", &countIn);
	if (viewMode < 0 || viewMode > 2)
		viewMode = 1;
	BRect frame;
	if (message.FindRect("classic frame", &frame) == B_OK)
		classicFrame = OnScreen(frame, classicFrame);
	if (message.FindRect("modern frame", &frame) == B_OK)
		modernFrame = OnScreen(frame, modernFrame);
}

void
Settings::Save() const
{
	BPath path;
	if (!SettingsPath(path))
		return;
	BMessage message('PmxS');
	message.AddBool("modern", modern);
	message.AddInt32("view", viewMode);
	message.AddInt32("input latency", inputLatency);
	message.AddBool("click", click);
	message.AddBool("count-in", countIn);
	message.AddRect("classic frame", classicFrame);
	message.AddRect("modern frame", modernFrame);
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		message.Flatten(&file);
}
