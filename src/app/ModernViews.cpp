// ModernViews.cpp: see ModernViews.h.
#include "ModernViews.h"

#include <Bitmap.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "AudioEngine.h"
#include "Look.h"
#include "MixerView.h"
#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ModernViews"

using namespace look;

namespace {

enum { kMsgBeatsMenu = 'BtMn' };

// what the mix makes of a place on the floor (Mixer::Prepare)
float
GainDb(float depth)
{
	float distance = (depth + 150.0f) / 200.0f;
	if (distance <= 0.001f)
		return -100;
	return 40.0f * log10f(distance);
}

BString
GainText(float depth)
{
	float db = GainDb(depth);
	BString text;
	if (db <= -99)
		text = B_TRANSLATE("silent");
	else
		text.SetToFormat("%+.1f dB", db);
	return text;
}

float
PanLeft(float x)
{
	float pan = 1.0f - (x + 256.0f) * 0.001953125f;
	float angle = std::max(0.0f, std::min(1.0f, pan * 0.76f + 0.12f)) * 3.1415925f * 0.5f;
	float s = sinf(angle), c = cosf(angle);
	return s / (s + c);
}

BString
PanText(float x)
{
	float left = PanLeft(x);
	float diff = left - (1 - left);
	BString text;
	if (fabsf(diff) < 0.01f)
		text = B_TRANSLATE("centre");
	else if (diff > 0)
		text.SetToFormat(B_TRANSLATE("%d%% left"), int(roundf(diff * 100)));
	else
		text.SetToFormat(B_TRANSLATE("%d%% right"), int(roundf(-diff * 100)));
	return text;
}

// the ear farther away hears it trunc(x / 3.5) frames later
BString
DelayText(float x)
{
	int frames = abs(int(x / 3.5f));
	BString text;
	text.SetToFormat("%.2f ms", frames / 44.1);
	return text;
}

BString
TimeText(double seconds)
{
	if (seconds < 0)
		seconds = 0;
	int minutes = int(seconds / 60);
	int secs = int(seconds) % 60;
	int hundredths = int((seconds - floor(seconds)) * 100);
	char text[32];
	snprintf(text, sizeof(text), "%02d:%02d.%02d", minutes, secs, hundredths);
	return text;
}

void
UseFont(BView* view, const BFont* base, float size)
{
	BFont font(base);
	font.SetSize(size);
	view->SetFont(&font);
}

// Breaks text into lines no wider than width
std::vector<BString>
Wrap(BView* view, const char* text, float width)
{
	std::vector<BString> lines;
	BString rest(text), line;
	while (rest.Length() > 0) {
		int32 space = rest.FindFirst(' ');
		BString word;
		if (space < 0) {
			word = rest;
			rest = "";
		} else {
			rest.CopyInto(word, 0, space);
			rest.Remove(0, space + 1);
		}
		BString candidate = line;
		if (candidate.Length() > 0)
			candidate << " ";
		candidate << word;
		if (line.Length() > 0 && view->StringWidth(candidate.String()) > width) {
			lines.push_back(line);
			line = word;
		} else
			line = candidate;
	}
	if (line.Length() > 0)
		lines.push_back(line);
	return lines;
}

}	// namespace

// #pragma mark - ModernButton

ModernButton::ModernButton(BRect frame, const char* name, const char* label, BMessage* message,
	Icon icon, Kind kind, uint32 resizing)
	:
	BControl(frame, name, label, message, resizing, B_WILL_DRAW),
	fIcon(icon),
	fKind(kind),
	fBackground(kPanel),
	fTracking(false),
	fInside(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}

void
ModernButton::SetGlyph(Icon icon)
{
	if (icon == fIcon)
		return;
	fIcon = icon;
	Invalidate();
}

void
ModernButton::SetBackground(rgb_color color)
{
	if (color == fBackground)
		return;
	fBackground = color;
	Invalidate();
}

static void
DrawIcon(BView* view, ModernButton::Icon icon, BPoint p, rgb_color ink)
{
	view->SetHighColor(ink);
	view->SetPenSize(1.7f);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	auto at = [p](float x, float y) { return BPoint(p.x + x, p.y + y); };
	switch (icon) {
		case ModernButton::kIconStart:
			view->StrokeLine(at(3.5f, 3), at(3.5f, 13));
			view->FillTriangle(at(13, 3), at(5.5f, 8), at(13, 13));
			break;
		case ModernButton::kIconPlay:
			view->FillTriangle(at(4, 2), at(4, 14), at(14, 8));
			break;
		case ModernButton::kIconPause:
			view->FillRect(BRect(at(3.5f, 2.5f), at(6.5f, 13.5f)));
			view->FillRect(BRect(at(9.5f, 2.5f), at(12.5f, 13.5f)));
			break;
		case ModernButton::kIconPlus:
			view->StrokeLine(at(8, 3), at(8, 13));
			// fall through
		case ModernButton::kIconMinus:
			view->StrokeLine(at(3, 8), at(13, 8));
			break;
		case ModernButton::kIconExport:
			view->StrokeLine(at(8, 2), at(8, 10));
			view->StrokeLine(at(4.5f, 6.5f), at(8, 10));
			view->StrokeLine(at(11.5f, 6.5f), at(8, 10));
			view->StrokeLine(at(3, 13.5f), at(13, 13.5f));
			break;
		case ModernButton::kIconReset:
			view->StrokeArc(at(8, 8), 5, 5, 100, 290);
			view->FillTriangle(at(1.5f, 5), at(6.5f, 5.5f), at(3.5f, 9.5f));
			break;
		case ModernButton::kIconChevron:
			view->StrokeLine(at(4, 6), at(8, 10));
			view->StrokeLine(at(8, 10), at(12, 6));
			break;
		case ModernButton::kIconRecord:
			view->FillEllipse(at(8, 8), 5, 5);
			break;
		case ModernButton::kNoIcon:
			break;
	}
	view->SetPenSize(1);
}

void
ModernButton::Draw(BRect updateRect)
{
	BRect b = Bounds();
	SetHighColor(fBackground);
	FillRect(b);
	SetDrawingMode(B_OP_ALPHA);

	const bool on = Value() == B_CONTROL_ON;
	const bool down = fTracking && fInside;
	rgb_color fill, line, ink;
	switch (fKind) {
		case kPrimary:
			fill = down ? Mix(kAccent, kGround, 0.2f) : kAccent;
			line = fill;
			ink = kGround;
			break;
		case kSegment:
			fill = on ? rgb_color{38, 44, 57, 255} : fBackground;
			if (down)
				fill = Mix(fill, kText, 0.08f);
			line = fill;
			ink = on ? kText : kMuted;
			break;
		case kRecord:
			// red while recording; a red dot on grey otherwise
			fill = on ? rgb_color{226, 68, 68, 255} : kRaised;
			if (down)
				fill = Mix(fill, kText, 0.1f);
			line = on ? fill : kBorder;
			// the record button's dot is red; a labelled button's text isn't
			ink = on || (Label() != NULL && Label()[0] != '\0') ? kText
				: rgb_color{255, 107, 107, 255};
			break;
		case kOverlay:
			fill = on ? kAccentDim : rgb_color{14, 17, 25, 255};
			line = on ? kAccentLine : rgb_color{38, 44, 57, 255};
			ink = on ? kAccentText : kText;
			if (down)
				fill = Mix(fill, kText, 0.1f);
			break;
		default:
			fill = on ? kAccentDim : kRaised;
			line = on ? kAccentLine : kBorder;
			ink = on ? kAccentText : kText;
			if (down)
				fill = Mix(fill, kText, 0.1f);
			break;
	}
	if (!IsEnabled())
		ink = Mix(ink, fill, 0.55f);

	if ((fKind == kPrimary || fKind == kRecord) && fabsf(b.Width() - b.Height()) < 1) {
		SetHighColor(fill);
		FillEllipse(b);
		if (fKind == kRecord && !on) {
			SetHighColor(line);
			StrokeEllipse(b);
		}
	} else {
		FillRound(this, b, 7, fill);
		if (fKind != kSegment)
			StrokeRound(this, b, 7, line);
	}

	BFont font(fKind == kSegment && on ? be_bold_font : be_plain_font);
	font.SetSize(12);
	SetFont(&font);
	const char* label = Label();
	const bool hasLabel = label != NULL && label[0] != '\0';
	const float labelWidth = hasLabel ? font.StringWidth(label) : 0;
	const float iconWidth = fIcon != kNoIcon ? 16 : 0;
	const float gap = hasLabel && fIcon != kNoIcon ? 6 : 0;
	float x = floorf((b.Width() - labelWidth - iconWidth - gap) / 2);
	if (fIcon == kIconChevron && hasLabel) {
		// the chevron goes after the label
		SetHighColor(ink);
		SetDrawingMode(B_OP_OVER);
		font_height fh;
		font.GetHeight(&fh);
		DrawString(label, BPoint(x, floorf((b.Height() + fh.ascent - fh.descent) / 2)));
		SetDrawingMode(B_OP_ALPHA);
		DrawIcon(this, fIcon, BPoint(x + labelWidth + gap, floorf(b.Height() / 2) - 7), kMuted);
		return;
	}
	if (fIcon != kNoIcon)
		DrawIcon(this, fIcon, BPoint(x, floorf(b.Height() / 2) - 7.5f), ink);
	if (hasLabel) {
		font_height fh;
		font.GetHeight(&fh);
		SetHighColor(ink);
		SetDrawingMode(B_OP_OVER);
		DrawString(label, BPoint(x + iconWidth + gap,
			floorf((b.Height() + fh.ascent - fh.descent) / 2)));
	}
	SetDrawingMode(B_OP_COPY);
}

void
ModernButton::MouseDown(BPoint where)
{
	if (!IsEnabled())
		return;
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	fTracking = true;
	fInside = true;
	Invalidate();
}

void
ModernButton::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (!fTracking)
		return;
	bool inside = Bounds().Contains(where);
	if (inside != fInside) {
		fInside = inside;
		Invalidate();
	}
}

void
ModernButton::MouseUp(BPoint where)
{
	if (!fTracking)
		return;
	fTracking = false;
	Invalidate();
	if (Bounds().Contains(where))
		Invoke();
}

// #pragma mark - BufferedView

BufferedView::BufferedView(BRect frame, const char* name, uint32 resizing)
	:
	BView(frame, name, resizing, B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
	fBitmap(NULL),
	fPen(NULL)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}

BufferedView::~BufferedView()
{
	delete fBitmap;
}

void
BufferedView::FrameResized(float width, float height)
{
	// a bigger bitmap when needed; the old one serves a smaller view
	if (fBitmap != NULL && fBitmap->Bounds().Width() >= width
		&& fBitmap->Bounds().Height() >= height)
		return;
	delete fBitmap;
	fBitmap = NULL;
	fPen = NULL;
}

void
BufferedView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	bounds.OffsetTo(0, 0);
	if (fBitmap == NULL) {
		fBitmap = new BBitmap(bounds, B_RGB32, true);
		fPen = new BView(bounds, "pen", B_FOLLOW_NONE, 0);
		fBitmap->AddChild(fPen);
	}
	if (!fBitmap->Lock())
		return;
	fPen->SetFont(be_plain_font);
	fPen->SetDrawingMode(B_OP_COPY);
	DrawContent(fPen, bounds);
	fPen->Sync();
	fBitmap->Unlock();
	DrawBitmap(fBitmap, bounds, Bounds());
}

// #pragma mark - ValueField

// The beat grid's Ticks as a number to drag up or down (or scroll); Shift
// for tenths
class ValueField : public BufferedView {
public:
	ValueField(BRect frame)
		:
		BufferedView(frame, "ticks", B_FOLLOW_LEFT | B_FOLLOW_TOP),
		fValue(0),
		fTracking(false),
		fLast(0)
	{
	}

	// From the window: ignored while it's being dragged, when it holds the
	// value and the window's are late
	void SetValue(float value)
	{
		if (fTracking || value == fValue)
			return;
		fValue = value;
		Invalidate();
	}

	virtual void MouseDown(BPoint where)
	{
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		fTracking = true;
		fLast = where.y;
		Invalidate();
	}

	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
	{
		if (!fTracking)
			return;
		float dy = fLast - where.y;
		if (fabsf(dy) < 2)
			return;
		fLast = where.y;
		Change(dy / 2 * ((modifiers() & B_SHIFT_KEY) != 0 ? 0.1f : 1.0f));
	}

	virtual void MouseUp(BPoint where)
	{
		fTracking = false;
		Invalidate();
	}

	virtual void MessageReceived(BMessage* message)
	{
		float delta;
		if (message->what == B_MOUSE_WHEEL_CHANGED
			&& message->FindFloat("be:wheel_delta_y", &delta) == B_OK) {
			Change(-delta * ((modifiers() & B_SHIFT_KEY) != 0 ? 0.1f : 1.0f));
			return;
		}
		BufferedView::MessageReceived(message);
	}

protected:
	virtual void DrawContent(BView* v, BRect b)
	{
		v->SetHighColor(kPanel);
		v->FillRect(b);
		v->SetDrawingMode(B_OP_ALPHA);
		FillRound(v, b, 6, fTracking ? kAccentDim : kRaised);
		StrokeRound(v, b, 6, fTracking ? kAccentLine : kBorder);
		BString text;
		if (fValue <= 0)
			text = "–";
		else if (fabsf(fValue - roundf(fValue)) < 0.05f)
			text.SetToFormat("%d", int(roundf(fValue)));
		else
			text.SetToFormat("%.1f", fValue);
		UseFont(v, be_fixed_font, 12);
		font_height fh;
		v->GetFontHeight(&fh);
		v->SetHighColor(kText);
		v->SetDrawingMode(B_OP_OVER);
		v->DrawString(text.String(), BPoint(9, floorf((b.Height() + fh.ascent - fh.descent) / 2)));
		// up and down marks: it can be dragged
		v->SetHighColor(kMuted);
		float x = b.right - 10;
		v->FillTriangle(BPoint(x - 3, 11), BPoint(x + 3, 11), BPoint(x, 7));
		v->FillTriangle(BPoint(x - 3, b.bottom - 11), BPoint(x + 3, b.bottom - 11),
			BPoint(x, b.bottom - 7));
		v->SetDrawingMode(B_OP_COPY);
	}

private:
	void Change(float by)
	{
		float value = std::max(1.0f, std::min(2000.0f, roundf((fValue + by) * 10) / 10));
		if (value == fValue)
			return;
		fValue = value;
		Invalidate();
		BMessage message(kMsgSetTicks);
		message.AddFloat("ticks", value);
		Window()->PostMessage(&message);
	}

	float fValue;
	bool fTracking;
	float fLast;
};

// #pragma mark - MasterBar

// The master volume across: full at the right. Its value is the classic
// slider's, 0 (full) to 100 (silent). Drawn offscreen, so the knob doesn't
// flicker half-drawn while it's dragged.
class MasterBar : public BufferedView {
public:
	MasterBar(BRect frame)
		:
		BufferedView(frame, "master", B_FOLLOW_RIGHT | B_FOLLOW_TOP),
		fValue(0),
		fTracking(false)
	{
	}

	// From the window: ignored while it's being dragged (see ValueField)
	void SetValue(float value)
	{
		if (fTracking || value == fValue)
			return;
		fValue = value;
		Invalidate();
	}

	virtual void MouseDown(BPoint where)
	{
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		fTracking = true;
		MouseMoved(where, 0, NULL);
	}

	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
	{
		if (!fTracking)
			return;
		float f = (where.x - 7) / (Bounds().Width() - 14);
		f = std::max(0.0f, std::min(1.0f, f));
		float value = roundf((1 - f) * 100);
		if (value == fValue)
			return;
		fValue = value;
		Invalidate();
		BMessage message(kMsgMasterChanged);
		message.AddFloat("value", value);
		message.AddBool("toolbar", true);
		Window()->PostMessage(&message);
	}

	virtual void MouseUp(BPoint where)
	{
		fTracking = false;
	}

protected:
	virtual void DrawContent(BView* v, BRect b)
	{
		v->SetHighColor(kPanel);
		v->FillRect(b);
		v->SetDrawingMode(B_OP_ALPHA);
		const float mid = floorf(b.Height() / 2);
		const float x = ThumbX();
		FillRound(v, BRect(7, mid - 2, b.right - 7, mid + 2), 2, kLine);
		FillRound(v, BRect(7, mid - 2, x, mid + 2), 2, kAccent);
		v->SetHighColor(kText);
		v->FillEllipse(BPoint(x, mid), 6, 6);
		v->SetDrawingMode(B_OP_COPY);
	}

private:
	float ThumbX() const
	{
		return 7 + (1 - fValue / 100) * (Bounds().Width() - 14);
	}

	float fValue;
	bool fTracking;
};

// #pragma mark - ToolbarView

ToolbarView::ToolbarView(BRect frame)
	:
	BufferedView(frame, "toolbar", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP),
	fTime(0),
	fLength(0),
	fLeft(0),
	fRight(0),
	fRecording(false),
	fCountingIn(false)
{
	fStart = new ModernButton(BRect(0, 11, 33, 44), "start", "", new BMessage(kMsgSeekStart),
		ModernButton::kIconStart);
	AddChild(fStart);
	fPlay = new ModernButton(BRect(0, 8, 39, 47), "pause", "", new BMessage(kMsgPause),
		ModernButton::kIconPause, ModernButton::kPrimary);
	AddChild(fPlay);
	fRecord = new ModernButton(BRect(0, 11, 33, 44), "record", "", new BMessage(kMsgRecord),
		ModernButton::kIconRecord, ModernButton::kRecord);
	AddChild(fRecord);
	fTicks = new ValueField(BRect(0, 13, 63, 42));
	AddChild(fTicks);
	fBeats = new ModernButton(BRect(0, 13, 57, 42), "beats", "4/4", new BMessage(kMsgBeatsMenu),
		ModernButton::kIconChevron);
	AddChild(fBeats);

	const char* names[3] = {B_TRANSLATE("Stage"), B_TRANSLATE("Split"),
		B_TRANSLATE("Timeline")};
	for (int i = 0; i < 3; i++) {
		BMessage* message = new BMessage(kMsgViewMode);
		message->AddInt32("mode", i);
		fSegments[i] = new ModernButton(BRect(0, 14, 63, 41), names[i], names[i], message,
			ModernButton::kNoIcon, ModernButton::kSegment, B_FOLLOW_RIGHT | B_FOLLOW_TOP);
		fSegments[i]->SetBackground(kRaised);
		AddChild(fSegments[i]);
	}
	fMaster = new MasterBar(BRect(0, 16, 89, 39));
	AddChild(fMaster);
	fExport = new ModernButton(BRect(0, 11, 117, 44), "export", B_TRANSLATE("Save mix"),
		new BMessage(kMsgSaveMixAsWave), ModernButton::kIconExport, ModernButton::kPlain,
		B_FOLLOW_RIGHT | B_FOLLOW_TOP);
	AddChild(fExport);
}

void
ToolbarView::AttachedToWindow()
{
	BufferedView::AttachedToWindow();
	fBeats->SetTarget(this);
	LayoutChildren();
}

void
ToolbarView::FrameResized(float width, float height)
{
	BufferedView::FrameResized(width, height);
	LayoutChildren();
}

bool
ToolbarView::Narrow() const
{
	return Bounds().Width() < 1140;
}

float
ToolbarView::Left() const
{
	return Narrow() ? 52 : 196;
}

float
ToolbarView::TimeLeft() const
{
	return Left() + 138;
}

// after the time, "00:00.00 / 00:00.00", whatever the fonts' widths
float
ToolbarView::TicksLeft() const
{
	BFont big(be_fixed_font), small(be_fixed_font);
	big.SetSize(15);
	small.SetSize(11);
	BFont label(be_plain_font);
	label.SetSize(12);
	// a narrow toolbar leaves out the length
	return ceilf(TimeLeft() + big.StringWidth("00:00.00")
		+ (Narrow() ? 0 : 6 + small.StringWidth("/ 00:00.00"))
		+ 20 + label.StringWidth(B_TRANSLATE("Ticks")) + 8);
}

void
ToolbarView::LayoutChildren()
{
	const float x0 = Left();
	const float w = Bounds().Width();
	fStart->MoveTo(x0, 11);
	fPlay->MoveTo(x0 + 42, 8);
	fRecord->MoveTo(x0 + 90, 11);
	fTicks->MoveTo(TicksLeft(), 13);
	fBeats->MoveTo(TicksLeft() + 70, 13);

	// a narrow toolbar's Save mix is the icon alone
	fExport->ResizeTo(Narrow() ? 33 : 117, 33);
	fExport->SetLabel(Narrow() ? "" : B_TRANSLATE("Save mix"));
	float x = w - 14 - fExport->Bounds().Width();
	fExport->MoveTo(x, 11);
	fMeter = BRect(x - 14 - 48, 22, x - 14, 34);
	x = fMeter.left - 14 - fMaster->Bounds().Width();
	fMaster->MoveTo(x, 16);
	x -= 22 + 18 + 3 * 64 + 6;
	for (int i = 0; i < 3; i++)
		fSegments[i]->MoveTo(x + 3 + i * 64, 14);
	Invalidate();
}

void
ToolbarView::DrawContent(BView* v, BRect b)
{
	v->SetHighColor(kPanel);
	v->FillRect(b);
	v->SetHighColor(kLine);
	v->StrokeLine(BPoint(b.left, b.bottom), BPoint(b.right, b.bottom));
	v->SetDrawingMode(B_OP_ALPHA);

	// the mark: three columns on a floor
	BPoint m(14, 15);
	v->SetHighColor(31, 58, 138);
	BPoint floorShape[4] = {BPoint(m.x + 2, m.y + 21), BPoint(m.x + 9, m.y + 15),
		BPoint(m.x + 19, m.y + 15), BPoint(m.x + 24, m.y + 21)};
	v->FillPolygon(floorShape, 4);
	FillRound(v, BRect(m.x + 6, m.y + 7, m.x + 10, m.y + 19), 1, TrackColor(0));
	FillRound(v, BRect(m.x + 11.5f, m.y + 3, m.x + 15.5f, m.y + 18), 1, TrackColor(1));
	FillRound(v, BRect(m.x + 17, m.y + 9, m.x + 21, m.y + 20), 1, TrackColor(2));

	v->SetDrawingMode(B_OP_OVER);
	if (!Narrow()) {
		UseFont(v, be_bold_font, 14);
		v->SetHighColor(kText);
		v->DrawString("PowerMix", BPoint(50, 26));
		UseFont(v, be_plain_font, 12);
		v->SetHighColor(kMuted);
		BString project = fProject.Length() > 0 ? fProject : BString(B_TRANSLATE("Untitled"));
		BFont font;
		v->GetFont(&font);
		font.TruncateString(&project, B_TRUNCATE_MIDDLE, Left() - 64);
		v->DrawString(project.String(), BPoint(50, 42));
	}

	// the time, red while recording
	UseFont(v, be_fixed_font, 15);
	v->SetHighColor(fRecording ? rgb_color{255, 107, 107, 255} : kText);
	BString now = fCountingIn ? BString(B_TRANSLATE("count-in")) : TimeText(fTime);
	v->DrawString(now.String(), BPoint(TimeLeft(), 33));
	float after = TimeLeft() + v->StringWidth(now.String()) + 6;
	if (!Narrow()) {
		UseFont(v, be_fixed_font, 11);
		v->SetHighColor(kMuted);
		BString total("/ ");
		total << TimeText(fLength);
		v->DrawString(total.String(), BPoint(after, 33));
	}

	UseFont(v, be_plain_font, 12);
	v->SetHighColor(kMuted);
	v->DrawString(B_TRANSLATE("Ticks"), BPoint(TicksLeft() - 8
		- v->StringWidth(B_TRANSLATE("Ticks")), 32));

	// the segmented switch's ground
	BRect group = fSegments[0]->Frame() | fSegments[2]->Frame();
	group.InsetBy(-3, -3);
	FillRound(v, group, 9, kRaised);
	StrokeRound(v, group, 9, kBorder);

	// a speaker before the master volume
	BRect master = fMaster->Frame();
	float sx = master.left - 20, sy = floorf((master.top + master.bottom) / 2);
	v->SetHighColor(kMuted);
	BPoint speaker[6] = {BPoint(sx, sy - 3), BPoint(sx + 4, sy - 3), BPoint(sx + 9, sy - 7),
		BPoint(sx + 9, sy + 7), BPoint(sx + 4, sy + 3), BPoint(sx, sy + 3)};
	v->FillPolygon(speaker, 6);
	v->SetPenSize(1.5f);
	v->StrokeArc(BPoint(sx + 10, sy), 5, 5, -50, 100);
	v->SetPenSize(1);

	// the meter: left above right, green turning yellow near the top
	for (int ch = 0; ch < 2; ch++) {
		BRect bar(fMeter.left, fMeter.top + ch * 8, fMeter.right, fMeter.top + ch * 8 + 4);
		FillRound(v, bar, 2, kLine);
		float level = std::min(1.0f, ch == 0 ? fLeft : fRight);
		if (level <= 0)
			continue;
		float right = bar.left + level * bar.Width();
		for (float x = bar.left; x <= right; x++) {
			float f = (x - bar.left) / bar.Width();
			v->SetHighColor(f < 0.7f ? rgb_color{61, 220, 151, 255}
				: Mix(rgb_color{61, 220, 151, 255}, kWarn, std::min(1.0f, (f - 0.7f) / 0.2f)));
			v->StrokeLine(BPoint(x, bar.top + (x < bar.left + 1 ? 1 : 0)),
				BPoint(x, bar.bottom - (x < bar.left + 1 ? 1 : 0)));
		}
	}
	v->SetDrawingMode(B_OP_COPY);
}

void
ToolbarView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgBeatsMenu) {
		BPopUpMenu* menu = new BPopUpMenu("beats", false, false);
		for (int beats = 3; beats <= 7; beats++) {
			BMessage* item = new BMessage(kMsgSetBeats);
			item->AddInt32("beats", beats);
			BString label;
			label << beats << "/4";
			BMenuItem* menuItem = new BMenuItem(label.String(), item);
			menuItem->SetMarked(label == fBeats->Label());
			menu->AddItem(menuItem);
		}
		menu->SetTargetForItems(Window());
		menu->SetAsyncAutoDestruct(true);
		menu->Go(ConvertToScreen(fBeats->Frame().LeftBottom() + BPoint(0, 2)), true, false,
			true);
		return;
	}
	BufferedView::MessageReceived(message);
}

void
ToolbarView::SetProject(const char* name)
{
	fProject = name;
	Invalidate(BRect(0, 0, Left(), Bounds().bottom));
}

void
ToolbarView::SetTime(double time, double length)
{
	if (time == fTime && length == fLength)
		return;
	fTime = time;
	fLength = length;
	Invalidate(BRect(TimeLeft() - 4, 10, TicksLeft() - 40, 46));
}

// StatusView::SetLevel's input, shown with a falling peak
void
ToolbarView::SetLevels(float left, float right)
{
	// the classic meter's full scale
	const float scale = 1.0f / 24000;
	float l = std::max(left * scale, fLeft * 0.85f);
	float r = std::max(right * scale, fRight * 0.85f);
	if (fabsf(l - fLeft) < 0.004f && fabsf(r - fRight) < 0.004f)
		return;
	fLeft = l;
	fRight = r;
	Invalidate(fMeter.InsetByCopy(-1, -1));
}

void
ToolbarView::SetPaused(bool paused)
{
	fPlay->SetGlyph(paused ? ModernButton::kIconPlay : ModernButton::kIconPause);
}

void
ToolbarView::SetViewMode(int mode)
{
	for (int i = 0; i < 3; i++)
		fSegments[i]->SetValue(i == mode ? B_CONTROL_ON : B_CONTROL_OFF);
}

void
ToolbarView::SetTicks(float ticks)
{
	fTicks->SetValue(ticks);
}

void
ToolbarView::SetBeats(int beats)
{
	BString label;
	label << beats << "/4";
	fBeats->SetLabel(label.String());
}

void
ToolbarView::SetRecording(bool recording, bool countingIn, float level)
{
	fRecord->SetValue(recording ? B_CONTROL_ON : B_CONTROL_OFF);
	if (recording != fRecording || countingIn != fCountingIn) {
		fRecording = recording;
		fCountingIn = countingIn;
		Invalidate(BRect(TimeLeft() - 4, 10, TicksLeft() - 40, 46));
	}
}

void
ToolbarView::SetMaster(float value)
{
	fMaster->SetValue(value);
}

// #pragma mark - TrackListView

namespace {

const float kListHeader = 44;
const float kRowHeight = 48;
const float kRowGap = 4;

struct RowInfo {
	BString name;
	bool muted, soloed, selected;
	float level, depth;
};

}	// namespace

TrackListView::TrackListView(BRect frame, AudioEngine* engine, MixerView* mixer,
	const RecordState* record)
	:
	BufferedView(frame, "track list", B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM),
	fEngine(engine),
	fMixer(mixer),
	fRecord(record),
	fScroll(0)
{
	ModernButton* add = new ModernButton(BRect(frame.Width() - 40, 9, frame.Width() - 12, 37),
		"new track", "", new BMessage(kMsgNewTrack), ModernButton::kIconPlus);
	AddChild(add);
}

BRect
TrackListView::RowFrame(int index) const
{
	float top = kListHeader + index * (kRowHeight + kRowGap) - fScroll;
	return BRect(8, top, Bounds().right - 8, top + kRowHeight - 1);
}

BRect
TrackListView::MuteFrame(BRect row) const
{
	float mid = floorf((row.top + row.bottom) / 2);
	return BRect(row.right - 62, mid - 13, row.right - 37, mid + 12);
}

BRect
TrackListView::ArmFrame(BRect row) const
{
	float mid = floorf((row.top + row.bottom) / 2);
	return BRect(row.right - 92, mid - 13, row.right - 67, mid + 12);
}

BRect
TrackListView::SoloFrame(BRect row) const
{
	float mid = floorf((row.top + row.bottom) / 2);
	return BRect(row.right - 32, mid - 13, row.right - 7, mid + 12);
}

void
TrackListView::DrawContent(BView* v, BRect b)
{
	std::vector<RowInfo> rows;
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		const pmx::Mixer& mixer = fEngine->Mixer();
		pmx::Scene& scene = fMixer->Scene();
		for (size_t i = 0; i < project.tracks.size(); i++) {
			RowInfo row;
			row.name = project.tracks[i].name.c_str();
			row.muted = mixer.IsMuted(i);
			row.soloed = mixer.IsSoloed(i);
			row.selected = int(i) < scene.CountTracks() && scene.IsSelected(int(i));
			row.level = mixer.TrackLevel(i);
			float x = 0;
			row.depth = project.tracks[i].y;
			if (int(i) < scene.CountTracks())
				scene.GetPosition(int(i), x, row.depth);
			rows.push_back(row);
		}
		fEngine->Unlock();
	}

	v->SetHighColor(kPanel);
	v->FillRect(b);
	v->SetHighColor(kLine);
	v->StrokeLine(BPoint(b.right, b.top), BPoint(b.right, b.bottom));
	v->SetDrawingMode(B_OP_ALPHA);

	BString caption;
	caption.SetToFormat(B_TRANSLATE("Tracks · %d"), int(rows.size()));
	DrawCaption(v, caption.String(), BPoint(14, 28), kMuted);

	BRegion clip(BRect(0, kListHeader - 4, b.right - 1, b.bottom));
	v->ConstrainClippingRegion(&clip);
	float bottom = kListHeader;
	for (size_t i = 0; i < rows.size(); i++) {
		const RowInfo& row = rows[i];
		BRect r = RowFrame(int(i));
		bottom = r.bottom;
		if (r.bottom < 0 || r.top > b.bottom)
			continue;
		const rgb_color color = TrackColor(int(i));
		if (row.selected) {
			FillRound(v, r, 10, rgb_color{23, 32, 54, 255});
			StrokeRound(v, r, 10, kAccentLine);
		}
		const float mid = floorf((r.top + r.bottom) / 2);
		FillRound(v, BRect(r.left + 10, mid - 5, r.left + 19, mid + 4), 3,
			row.muted ? Mix(color, kPanel, 0.6f) : color);

		BRect arm = ArmFrame(r), mute = MuteFrame(r), solo = SoloFrame(r);
		const bool armed = fRecord != NULL && fRecord->armed == int(i);
		const float textRight = arm.left - 8;
		// the name above; the level bar and the gain from depth below
		UseFont(v, be_bold_font, 12);
		v->SetHighColor(row.muted ? kMuted : kText);
		BString name = row.name;
		BFont font;
		v->GetFont(&font);
		font.TruncateString(&name, B_TRUNCATE_END, textRight - (r.left + 28));
		v->SetDrawingMode(B_OP_OVER);
		v->DrawString(name.String(), BPoint(r.left + 28, mid - 3));
		BString gain = row.muted ? BString(B_TRANSLATE("muted")) : GainText(row.depth);
		BFont small(be_fixed_font);
		small.SetSize(9);
		const float gainWidth = small.StringWidth(gain.String());
		v->SetFont(&small);
		v->SetHighColor(kMuted);
		v->DrawString(gain.String(), BPoint(textRight - gainWidth, mid + 12));
		v->SetDrawingMode(B_OP_ALPHA);

		BRect bar(r.left + 28, mid + 6, textRight - gainWidth - 6, mid + 9);
		FillRound(v, bar, 1.5f, kLine);
		float f = row.muted ? 0 : std::max(0.0f, std::min(1.0f, (row.level - 2) / 13));
		if (f > 0)
			FillRound(v, BRect(bar.left, bar.top, bar.left + f * bar.Width(), bar.bottom), 1.5f,
				Mix(color, kPanel, 0.15f));

		// R (armed for recording), M, S
		UseFont(v, be_bold_font, 10);
		const bool states[3] = {armed, row.muted, row.soloed};
		const BRect chips[3] = {arm, mute, solo};
		const char* letters[3] = {"R", "M", "S"};
		const rgb_color red = {255, 107, 107, 255};
		for (int k = 0; k < 3; k++) {
			rgb_color fill = kRaised, ink = kMuted, line = kBorder;
			if (states[k] && k == 0) {
				const bool live = fRecord->recording;
				fill = live ? rgb_color{226, 68, 68, 255} : rgb_color{60, 22, 26, 255};
				ink = live ? kText : red;
				line = live ? fill : Mix(red, kPanel, 0.55f);
			} else if (states[k] && k == 1) {
				fill = kWarnDim;
				ink = kWarn;
				line = Mix(kWarn, kPanel, 0.6f);
			} else if (states[k]) {
				fill = kAccentDim;
				ink = kAccentText;
				line = kAccentLine;
			}
			FillRound(v, chips[k], 6, fill);
			StrokeRound(v, chips[k], 6, line);
			v->SetHighColor(ink);
			v->SetDrawingMode(B_OP_OVER);
			float w = v->StringWidth(letters[k]);
			v->DrawString(letters[k], BPoint(floorf((chips[k].left + chips[k].right - w) / 2) + 1,
				chips[k].bottom - 8));
			v->SetDrawingMode(B_OP_ALPHA);
		}
	}

	UseFont(v, be_plain_font, 11);
	v->SetHighColor(kMuted);
	v->SetDrawingMode(B_OP_OVER);
	const char* hint = rows.empty()
		? B_TRANSLATE("Drop sound files here, or open a project, to start a mix.")
		: B_TRANSLATE("Drop sound files here or on a lane to add them.");
	float y = bottom + 24;
	for (const BString& line : Wrap(v, hint, b.Width() - 32)) {
		v->DrawString(line.String(), BPoint(14, y));
		y += 15;
	}
	v->ConstrainClippingRegion(NULL);
	v->SetDrawingMode(B_OP_COPY);
}

void
TrackListView::MouseDown(BPoint where)
{
	int count = 0;
	if (fEngine->Lock()) {
		count = int(fEngine->Project().tracks.size());
		fEngine->Unlock();
	}
	for (int i = 0; i < count; i++) {
		BRect row = RowFrame(i);
		if (!row.Contains(where) || where.y < kListHeader - 4)
			continue;
		BMessage message(kMsgSelectTrack);
		if (ArmFrame(row).InsetByCopy(-2, -2).Contains(where))
			message.what = kMsgToggleArm;
		else if (MuteFrame(row).InsetByCopy(-2, -2).Contains(where))
			message.what = kMsgToggleMute;
		else if (SoloFrame(row).InsetByCopy(-2, -2).Contains(where))
			message.what = kMsgToggleSolo;
		else
			message.AddBool("extend", (modifiers() & B_SHIFT_KEY) != 0);
		message.AddInt32("track", i);
		Window()->PostMessage(&message);
		return;
	}
}

void
TrackListView::MessageReceived(BMessage* message)
{
	float delta;
	if (message->what == B_MOUSE_WHEEL_CHANGED
		&& message->FindFloat("be:wheel_delta_y", &delta) == B_OK) {
		int count = 0;
		if (fEngine->Lock()) {
			count = int(fEngine->Project().tracks.size());
			fEngine->Unlock();
		}
		float content = kListHeader + count * (kRowHeight + kRowGap) + 60;
		fScroll = std::max(0.0f, std::min(std::max(0.0f, content - Bounds().Height()),
			fScroll + delta * 30));
		Invalidate();
		return;
	}
	if (message->WasDropped() && message->HasRef("refs")) {
		BMessage drop(*message);
		drop.what = kMsgDropRefs;
		drop.AddInt32("lane", -1);
		drop.AddFloat("x", 0);
		drop.AddFloat("y", 100);
		Window()->PostMessage(&drop);
		return;
	}
	BufferedView::MessageReceived(message);
}

// #pragma mark - InspectorView

namespace {

const float kPadTop = 58;
const float kCardHeight = 50;

}	// namespace

InspectorView::InspectorView(BRect frame, AudioEngine* engine, MixerView* mixer,
	const RecordState* record)
	:
	BufferedView(frame, "inspector", B_FOLLOW_RIGHT | B_FOLLOW_TOP_BOTTOM),
	fEngine(engine),
	fMixer(mixer),
	fRecord(record),
	fDragging(-1)
{
	fMute = new ModernButton(BRect(0, 0, 60, 33), "mute", B_TRANSLATE("Mute"),
		new BMessage(kMsgToggleMute));
	fSolo = new ModernButton(BRect(0, 0, 60, 33), "solo", B_TRANSLATE("Solo"),
		new BMessage(kMsgToggleSolo));
	fArm = new ModernButton(BRect(0, 0, 60, 33), "arm", B_TRANSLATE("Arm"),
		new BMessage(kMsgToggleArm), ModernButton::kNoIcon, ModernButton::kRecord);
	fEdit = new ModernButton(BRect(0, 0, 60, 33), "edit", B_TRANSLATE("Edit sound"),
		new BMessage(kMsgEditClip));
	fRecordMoves = new ModernButton(BRect(0, 0, 60, 33), "record moves",
		B_TRANSLATE("Record moves"), new BMessage(kMsgRecordMoves), ModernButton::kNoIcon,
		ModernButton::kRecord);
	fClearMoves = new ModernButton(BRect(0, 0, 60, 33), "clear moves", B_TRANSLATE("Clear moves"),
		new BMessage(kMsgClearMoves));
	AddChild(fMute);
	AddChild(fSolo);
	AddChild(fArm);
	AddChild(fRecordMoves);
	AddChild(fClearMoves);
	AddChild(fEdit);
	LayoutChildren();
}

BRect
InspectorView::PadFrame() const
{
	const float w = Bounds().Width() - 32;
	return BRect(16, kPadTop, 16 + w, kPadTop + w);
}

void
InspectorView::LayoutChildren()
{
	BRect pad = PadFrame();
	const float top = pad.bottom + 26 + 2 * kCardHeight + 8 + 14;
	// Mute, Solo, Arm; Edit sound below
	const float w = pad.Width();
	const float third = floorf((w - 16) / 3);
	ModernButton* row[3] = {fMute, fSolo, fArm};
	for (int i = 0; i < 3; i++) {
		row[i]->ResizeTo(i < 2 ? third : w - 2 * third - 16, 33);
		row[i]->MoveTo(pad.left + i * (third + 8), top);
	}
	fEdit->ResizeTo(w, 33);
	fEdit->MoveTo(pad.left, top + 41);
	// the floor automation: write moves, clear this track's
	const float half = floorf((w - 8) / 2);
	fRecordMoves->ResizeTo(half, 33);
	fRecordMoves->MoveTo(pad.left, top + 82);
	fClearMoves->ResizeTo(w - half - 8, 33);
	fClearMoves->MoveTo(pad.left + half + 8, top + 82);
}

void
InspectorView::FrameResized(float width, float height)
{
	BufferedView::FrameResized(width, height);
	LayoutChildren();
}

// The floor is +-256 each way; the listener is at the bottom (the mix is
// louder the larger the depth)
BPoint
InspectorView::PadPoint(float x, float y) const
{
	BRect pad = PadFrame();
	return BPoint(pad.left + (x + 256) / 512 * pad.Width(),
		pad.top + (y + 256) / 512 * pad.Height());
}

void
InspectorView::FloorPoint(BPoint where, float& x, float& y) const
{
	BRect pad = PadFrame();
	x = std::max(-250.0f, std::min(250.0f, (where.x - pad.left) / pad.Width() * 512 - 256));
	y = std::max(-250.0f, std::min(250.0f, (where.y - pad.top) / pad.Height() * 512 - 256));
}

int
InspectorView::Selected() const
{
	pmx::Scene& scene = fMixer->Scene();
	for (int i = 0; i < scene.CountTracks(); i++) {
		if (scene.IsSelected(i))
			return i;
	}
	return -1;
}

void
InspectorView::Refresh()
{
	const int selected = Selected();
	BString signature;
	bool muted = false, soloed = false;
	size_t clips = 0;
	size_t pathPoints = 0;
	BString name;
	pmx::Scene& scene = fMixer->Scene();
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		if (selected >= 0 && size_t(selected) < project.tracks.size()) {
			muted = fEngine->Mixer().IsMuted(size_t(selected));
			soloed = fEngine->Mixer().IsSoloed(size_t(selected));
			clips = project.tracks[size_t(selected)].clips.size();
			name = project.tracks[size_t(selected)].name.c_str();
			pathPoints = project.tracks[size_t(selected)].path.size();
		}
		signature << int32(project.tracks.size()) << "|";
		for (const pmx::Track& t : project.tracks)
			signature << int32(t.path.size()) << ",";
		fEngine->Unlock();
	}
	const bool armed = selected >= 0 && fRecord->armed == selected;
	signature << int32(fMixer->RecordMoves()) << "|";
	signature << selected << "|" << int32(muted) << int32(soloed) << int32(armed) << "|"
		<< int32(clips) << "|" << name;
	for (int i = 0; i < scene.CountTracks(); i++) {
		float x, y;
		scene.GetPosition(i, x, y);
		signature << "|" << int32(x) << "," << int32(y);
	}
	if (signature == fSignature)
		return;
	fSignature = signature;

	const bool have = selected >= 0;
	fMute->SetEnabled(have);
	fSolo->SetEnabled(have);
	fArm->SetEnabled(have);
	fArm->SetValue(armed ? B_CONTROL_ON : B_CONTROL_OFF);
	BMessage arm(kMsgToggleArm);
	arm.AddInt32("track", selected);
	fArm->SetMessage(new BMessage(arm));
	fEdit->SetEnabled(have && clips > 0);
	fRecordMoves->SetValue(fMixer->RecordMoves() ? B_CONTROL_ON : B_CONTROL_OFF);
	fClearMoves->SetEnabled(have && pathPoints > 0);
	BMessage clear(kMsgClearMoves);
	clear.AddInt32("track", selected);
	fClearMoves->SetMessage(new BMessage(clear));
	fMute->SetValue(muted ? B_CONTROL_ON : B_CONTROL_OFF);
	fSolo->SetValue(soloed ? B_CONTROL_ON : B_CONTROL_OFF);
	BMessage mute(kMsgToggleMute), solo(kMsgToggleSolo), edit(kMsgEditClip);
	mute.AddInt32("track", selected);
	solo.AddInt32("track", selected);
	edit.AddInt32("track", selected);
	edit.AddInt32("clip", 0);
	fMute->SetMessage(new BMessage(mute));
	fSolo->SetMessage(new BMessage(solo));
	fEdit->SetMessage(new BMessage(edit));
	Invalidate();
}

void
InspectorView::DrawContent(BView* v, BRect b)
{
	const int selected = Selected();
	pmx::Scene& scene = fMixer->Scene();
	BString name;
	size_t clips = 0;
	std::vector<std::vector<pmx::PathPoint> > paths;
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		if (selected >= 0 && size_t(selected) < project.tracks.size()) {
			name = project.tracks[size_t(selected)].name.c_str();
			clips = project.tracks[size_t(selected)].clips.size();
		}
		for (const pmx::Track& t : project.tracks)
			paths.push_back(t.path);
		fEngine->Unlock();
	}
	const bool selectedMoves = selected >= 0 && size_t(selected) < paths.size()
		&& !paths[size_t(selected)].empty();

	v->SetHighColor(kPanel);
	v->FillRect(b);
	v->SetHighColor(kLine);
	v->StrokeLine(BPoint(b.left, b.top), BPoint(b.left, b.bottom));
	v->SetDrawingMode(B_OP_ALPHA);

	// the title
	if (selected >= 0) {
		FillRound(v, BRect(16, 18, 29, 31), 4, TrackColor(selected));
		UseFont(v, be_bold_font, 16);
		v->SetHighColor(kText);
		BString title = name;
		BFont font;
		v->GetFont(&font);
		font.TruncateString(&title, B_TRUNCATE_END, b.Width() - 110);
		v->SetDrawingMode(B_OP_OVER);
		v->DrawString(title.String(), BPoint(38, 30));
		UseFont(v, be_plain_font, 12);
		v->SetHighColor(kMuted);
		BString count;
		if (clips == 1)
			count = B_TRANSLATE("1 clip");
		else
			count.SetToFormat(B_TRANSLATE("%d clips"), int(clips));
		if (selectedMoves)
			count << " · " << B_TRANSLATE("moves");
		v->DrawString(count.String(), BPoint(b.right - 16 - v->StringWidth(count.String()), 30));
	} else {
		UseFont(v, be_bold_font, 14);
		v->SetHighColor(kMuted);
		v->SetDrawingMode(B_OP_OVER);
		v->DrawString(B_TRANSLATE("No track selected"), BPoint(16, 30));
	}
	v->SetDrawingMode(B_OP_ALPHA);
	DrawCaption(v, B_TRANSLATE("Place on the floor"), BPoint(16, kPadTop - 8), kMuted);

	// the pad: the floor from above
	BRect pad = PadFrame();
	FillRound(v, pad, 12, kStage);
	BRegion clip(pad.InsetByCopy(1, 1));
	v->ConstrainClippingRegion(&clip);
	v->SetHighColor(26, 36, 64);
	for (int k = 1; k < 8; k++) {
		float x = pad.left + k * pad.Width() / 8, y = pad.top + k * pad.Height() / 8;
		v->StrokeLine(BPoint(x, pad.top), BPoint(x, pad.bottom));
		v->StrokeLine(BPoint(pad.left, y), BPoint(pad.right, y));
	}
	v->SetHighColor(kAccentLine);
	float cx = floorf((pad.left + pad.right) / 2);
	v->StrokeLine(BPoint(cx, pad.top), BPoint(cx, pad.bottom));
	// each track's moves, as a line over the floor
	for (size_t i = 0; i < paths.size(); i++) {
		if (paths[i].size() < 2)
			continue;
		rgb_color color = TrackColor(int(i));
		color.alpha = int(i) == selected ? 230 : 110;
		v->SetHighColor(color);
		v->SetPenSize(int(i) == selected ? 2 : 1.2f);
		const size_t step = std::max<size_t>(1, paths[i].size() / 400);
		BPoint last = PadPoint(paths[i][0].x, paths[i][0].y);
		for (size_t k = step; k < paths[i].size(); k += step) {
			BPoint p = PadPoint(paths[i][k].x, paths[i][k].y);
			v->StrokeLine(last, p);
			last = p;
		}
		v->SetPenSize(1);
	}
	for (int i = 0; i < scene.CountTracks(); i++) {
		if (i == selected)
			continue;
		float x, y;
		scene.GetPosition(i, x, y);
		v->SetHighColor(TrackColor(i));
		v->FillEllipse(PadPoint(x, y), 5, 5);
	}
	if (selected >= 0) {
		float x, y;
		scene.GetPosition(selected, x, y);
		BPoint p = PadPoint(x, y);
		v->SetHighColor(Mix(kAccent, kStage, 0.55f));
		v->FillEllipse(p, 11, 11);
		v->SetHighColor(TrackColor(selected));
		v->FillEllipse(p, 7, 7);
	}
	v->ConstrainClippingRegion(NULL);
	StrokeRound(v, pad, 12, kLine);
	const char* listener = B_TRANSLATE("Listener");
	BFont captionFont(be_bold_font);
	captionFont.SetSize(10);
	float lw = captionFont.StringWidth(BString(listener).ToUpper().String()) + 8;
	DrawCaption(v, listener, BPoint(cx - lw / 2, pad.bottom + 15), kMuted);

	// what the mix makes of it
	float x = 0, y = 0;
	if (selected >= 0)
		scene.GetPosition(selected, x, y);
	const char* labels[4] = {B_TRANSLATE("Gain from depth"), B_TRANSLATE("Pan"),
		B_TRANSLATE("Ear delay"), B_TRANSLATE("Position")};
	BString position;
	position.SetToFormat("%d, %d", int(x), int(y));
	BString values[4] = {GainText(y), PanText(x), DelayText(x), position};
	const float cardWidth = floorf((pad.Width() - 8) / 2);
	for (int k = 0; k < 4; k++) {
		float left = pad.left + (k % 2) * (cardWidth + 8);
		float top = pad.bottom + 26 + (k / 2) * (kCardHeight + 8);
		BRect card(left, top, left + cardWidth, top + kCardHeight - 1);
		FillRound(v, card, 10, rgb_color{21, 25, 34, 255});
		StrokeRound(v, card, 10, kLine);
		v->SetDrawingMode(B_OP_OVER);
		UseFont(v, be_plain_font, 11);
		v->SetHighColor(kMuted);
		v->DrawString(labels[k], BPoint(left + 10, top + 18));
		UseFont(v, be_fixed_font, 13);
		v->SetHighColor(selected >= 0 ? kText : kMuted);
		v->DrawString(selected >= 0 ? values[k].String() : "–", BPoint(left + 10, top + 38));
		v->SetDrawingMode(B_OP_ALPHA);
	}

	// how it works, at the bottom if there's room
	const float buttonsBottom = fClearMoves->Frame().bottom;
	UseFont(v, be_plain_font, 11);
	std::vector<BString> lines = Wrap(v, selected >= 0
		? B_TRANSLATE("Nearer is louder; left and right pan and delay the far ear. "
			"With Record moves on, drag the track while the mix plays to make it move.")
		: B_TRANSLATE("Click a track in the list, on the stage or on the floor plan "
			"above to see where it sits in the mix."), pad.Width() - 24);
	const float noteHeight = lines.size() * 15 + 18;
	if (b.bottom - 16 - noteHeight > buttonsBottom + 14) {
		BRect note(pad.left, b.bottom - 16 - noteHeight, pad.right, b.bottom - 16);
		FillRound(v, note, 10, rgb_color{21, 25, 34, 255});
		StrokeRound(v, note, 10, kLine);
		v->SetHighColor(kMuted);
		v->SetDrawingMode(B_OP_OVER);
		float ty = note.top + 21;
		for (const BString& line : lines) {
			v->DrawString(line.String(), BPoint(note.left + 12, ty));
			ty += 15;
		}
	}
	v->SetDrawingMode(B_OP_COPY);
}

void
InspectorView::MouseDown(BPoint where)
{
	BRect pad = PadFrame();
	if (!pad.Contains(where))
		return;
	pmx::Scene& scene = fMixer->Scene();
	// a dot under the pointer, else the selected track jumps there
	int hit = -1;
	float best = 12 * 12;
	for (int i = 0; i < scene.CountTracks(); i++) {
		float x, y;
		scene.GetPosition(i, x, y);
		BPoint p = PadPoint(x, y);
		float d = (p.x - where.x) * (p.x - where.x) + (p.y - where.y) * (p.y - where.y);
		if (d <= best) {
			best = d;
			hit = i;
		}
	}
	const int selected = Selected();
	if (hit >= 0 && hit != selected) {
		BMessage message(kMsgSelectTrack);
		message.AddInt32("track", hit);
		Window()->PostMessage(&message);
	}
	fDragging = hit >= 0 ? hit : selected;
	if (fDragging < 0)
		return;
	fMixer->BeginMove(fDragging);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	if (hit < 0)
		MouseMoved(where, 0, NULL);
}

void
InspectorView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (fDragging < 0)
		return;
	float x, y;
	FloorPoint(where, x, y);
	fMixer->MoveTrack(fDragging, x, y);
	Refresh();
}

void
InspectorView::MouseUp(BPoint where)
{
	if (fDragging >= 0)
		fMixer->EndMove();
	fDragging = -1;
}
