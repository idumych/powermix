// TrackArea.cpp: see TrackArea.h.
#include "TrackArea.h"

#include <Bitmap.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <ScrollBar.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "AudioEngine.h"
#include "Palette.h"
#include "PowerMix.h"

namespace {

// TrackView sets backgr and backgr1 from the screen's palette
const rgb_color kBackground = {200, 200, 200, 255};		// palette 25
const rgb_color kLaneColor = {224, 224, 224, 255};		// palette 28
const float kLaneTop = 30;		// TrackView::Draw: first lane, then height + 8
const float kLaneHeight = 60;	// TrackViewer's field_0x18
const float kClipLeft = 130;	// lanes' clip area, as wide as the ruler
const float kZoomMin = 0.005f, kZoomMax = 1.0f;

enum {
	kMsgTicks = 'bet!',		// TCtrlView's text control
	kMsgBeats = 3000,		// its pop-up, "v" = beats * 1000 + 40
	kMsgHScroll = 'HScr'
};

rgb_color
PaletteColor(int index)
{
	const pmx::Rgb& c = pmx::kSystemPalette[index];
	rgb_color color = {c.r, c.g, c.b, 255};
	return color;
}

// "00:08:00": minutes, seconds, hundredths (TimeView::time_to_string)
BString
TimeString(double seconds)
{
	if (seconds < 0)
		seconds = 0;
	int minutes = int(seconds / 60);
	int secs = int(seconds) % 60;
	int hundredths = int((seconds - floor(seconds)) * 100);
	char text[32];
	snprintf(text, sizeof(text), "%02d:%02d:%02d", minutes, secs, hundredths);
	return text;
}

}	// namespace

// #pragma mark - TimeRuler

class TimeRuler : public BView {
public:
	TimeRuler(BRect frame, TrackArea* area)
		:
		BView(frame, "time", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW),
		fArea(area),
		fTime(0),
		fTracking(false),
		fAnchor(0)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	float X(double time) const
	{
		return float((time - fArea->fViewStart) / fArea->fSecondsPerPixel);
	}

	double Time(float x) const
	{
		return fArea->fViewStart + x * fArea->fSecondsPerPixel;
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		float selStart = 0, selEnd = 0, ticks = 0;
		int beats = 4;
		if (fArea->fEngine->Lock()) {
			const pmx::Project& project = fArea->fEngine->Project();
			selStart = project.selectionStart;
			selEnd = project.selectionEnd;
			ticks = project.ticks;
			beats = std::max<int>(1, project.beatsPerBar);
			fArea->fEngine->Unlock();
		}
		// light outside the time selection, dark inside it
		DrawScale(bounds, false, ticks, beats);
		BRect selection(std::max(bounds.left, X(selStart)), 0,
			std::min(bounds.right, X(selEnd)), bounds.bottom - 1);
		if (selection.IsValid()) {
			BRegion region(selection);
			ConstrainClippingRegion(&region);
			DrawScale(bounds, true, ticks, beats);
			ConstrainClippingRegion(NULL);
		}

		BFont font;
		GetFont(&font);
		font.SetSize(9);
		SetFont(&font);
		SetDrawingMode(B_OP_OVER);
		SetHighColor(230, 230, 230);
		BString now;
		now << int(fTime / 60) << ":";
		now << (int(fTime) % 60 < 10 ? "0" : "") << int(fTime) % 60;
		DrawString(now.String(), BPoint(0, 26));
		SetDrawingMode(B_OP_COPY);

		SetHighColor(40, 40, 40);
		StrokeLine(BPoint(bounds.left, bounds.bottom), BPoint(bounds.right, bounds.bottom));

		// the play position
		float x = X(fTime);
		SetHighColor(200, 0, 0);
		FillTriangle(BPoint(x, 31), BPoint(x - 4, bounds.bottom - 1),
			BPoint(x + 4, bounds.bottom - 1));
	}

	// TimeView::private_draw: labels every 1, 2, 4, 8... seconds (at least
	// 100 pixels apart), eight ticks between, a strip of beat ticks below
	void DrawScale(BRect bounds, bool dark, float ticks, int beats)
	{
		const rgb_color ground = dark ? rgb_color{24, 24, 24, 255} : rgb_color{200, 200, 200, 255};
		const rgb_color ink = dark ? rgb_color{200, 200, 200, 255} : rgb_color{60, 60, 60, 255};
		const rgb_color text = dark ? rgb_color{230, 230, 230, 255} : rgb_color{20, 20, 20, 255};
		SetHighColor(ground);
		FillRect(BRect(bounds.left, 0, bounds.right, bounds.bottom - 1));

		double step = 1;
		while (step / fArea->fSecondsPerPixel < 100 && step < 3600)
			step *= 2;
		BFont font;
		GetFont(&font);
		font.SetSize(10);
		SetFont(&font);
		SetDrawingMode(B_OP_OVER);
		double first = floor(Time(bounds.left) / step) * step;
		for (double t = first; X(t) <= bounds.right; t += step) {
			float x = X(t);
			SetHighColor(ink);
			StrokeLine(BPoint(x, 0), BPoint(x, 20));
			for (int k = 1; k < 8; k++) {
				float minor = X(t + step * k / 8);
				StrokeLine(BPoint(minor, k == 4 ? 13 : 16), BPoint(minor, 20));
			}
			SetHighColor(text);
			DrawString(TimeString(t).String(), BPoint(x + 2, 9));
		}
		SetDrawingMode(B_OP_COPY);

		SetHighColor(dark ? rgb_color{40, 40, 40, 255} : rgb_color{160, 160, 160, 255});
		FillRect(BRect(bounds.left, 28, bounds.right, bounds.bottom - 1));
		if (ticks > 0) {
			double beat = 120.0 / ticks;
			if (beat / fArea->fSecondsPerPixel >= 2) {
				long firstBeat = long(floor(Time(bounds.left) / beat));
				for (long b = std::max(0L, firstBeat); X(b * beat) <= bounds.right; b++) {
					float x = X(b * beat);
					SetHighColor(b % beats == 0 ? rgb_color{255, 255, 255, 255}
						: rgb_color{dark ? uint8(150) : uint8(240), dark ? uint8(150) : uint8(240),
							dark ? uint8(150) : uint8(240), 255});
					StrokeLine(BPoint(x, b % beats == 0 ? 29 : 31), BPoint(x, bounds.bottom - 1));
				}
			}
		}
	}

	// TimeView::ChangeSelection: dragging sets the time selection the mix
	// loops over
	virtual void MouseDown(BPoint where)
	{
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		fTracking = true;
		fAnchor = std::max(0.0, Time(where.x));
		SetSelection(fAnchor, fAnchor);
	}

	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
	{
		if (!fTracking)
			return;
		double t = std::max(0.0, Time(where.x));
		SetSelection(std::min(fAnchor, t), std::max(fAnchor, t));
	}

	virtual void MouseUp(BPoint where)
	{
		fTracking = false;
		float start = 0, end = 0;
		if (fArea->fEngine->Lock()) {
			const pmx::Project& project = fArea->fEngine->Project();
			start = project.selectionStart;
			end = project.selectionEnd;
			fArea->fEngine->Unlock();
		}
		// a click without a drag selects up to the end of the project
		if (end - start < 0.05f && fArea->fEngine->Lock()) {
			float length = fArea->fEngine->Project().Length();
			fArea->fEngine->Unlock();
			SetSelection(start, std::max(start + 0.05f, length));
		}
	}

	void SetSelection(double start, double end)
	{
		if (fArea->fEngine->Lock()) {
			pmx::Project& project = fArea->fEngine->Project();
			project.selectionStart = float(start);
			project.selectionEnd = float(end);
			if (end - start >= 0.05)
				fArea->fEngine->Mixer().SetRange(start, end);
			fArea->fEngine->Unlock();
		}
		Invalidate();
	}

	void SetTime(double time)
	{
		if (time == fTime)
			return;
		fTime = time;
		Invalidate();
	}

private:
	TrackArea* fArea;
	double fTime;
	bool fTracking;
	double fAnchor;
};

// #pragma mark - TrackLanes

class TrackLanes : public BView {
public:
	TrackLanes(BRect frame, TrackArea* area)
		:
		BView(frame, "lanes", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
		fArea(area)
	{
		SetViewColor(kBackground);
	}

	float LaneTop(size_t i) const { return kLaneTop + i * (kLaneHeight + 8); }

	float X(double time) const
	{
		return kClipLeft + float((time - fArea->fViewStart) / fArea->fSecondsPerPixel) + 1;
	}

	virtual void Draw(BRect updateRect)
	{
		if (!fArea->fEngine->Lock())
			return;
		const pmx::Project& project = fArea->fEngine->Project();
		const pmx::Mixer& mixer = fArea->fEngine->Mixer();
		for (size_t i = 0; i < project.tracks.size(); i++) {
			BRect lane(0, LaneTop(i), Bounds().right, LaneTop(i) + kLaneHeight);
			if (!lane.Intersects(updateRect))
				continue;
			DrawExtra(i, project.tracks[i], lane, mixer.IsMuted(i), mixer.IsSoloed(i));
			DrawTrack(project.tracks[i], lane, fSelected.count(int(i)) > 0);
		}
		fArea->fEngine->Unlock();
	}

	// TrackView::draw_track_extra: "3-bass" and the M/S box
	void DrawExtra(size_t index, const pmx::Track& track, BRect lane, bool muted, bool soloed)
	{
		const float center = lane.top + kLaneHeight / 2;
		BFont font;
		GetFont(&font);
		font.SetSize(11);
		SetFont(&font);
		SetHighColor(0, 80, 180);
		SetLowColor(kBackground);
		BString name;
		name << int32(std::min<size_t>(index, 99)) << "-" << track.name.c_str();
		font.TruncateString(&name, B_TRUNCATE_END, 96);
		DrawString(name.String(), BPoint(12, center - 1 + 4));

		const float half = 12;
		BRect m(110, center - half, 122, center);
		BRect s(110, center, 122, center + half);
		SetHighColor(muted ? rgb_color{200, 80, 80, 255} : rgb_color{80, 200, 80, 255});
		FillRect(m);
		SetHighColor(soloed ? rgb_color{200, 80, 80, 255} : rgb_color{80, 200, 80, 255});
		FillRect(s);
		SetHighColor(150, 0, 0);
		StrokeRect(BRect(m.left, m.top, s.right, s.bottom));
		StrokeLine(BPoint(m.left, center), BPoint(m.right, center));
		font.SetSize(9);
		SetFont(&font);
		SetHighColor(20, 20, 20);
		SetDrawingMode(B_OP_OVER);
		DrawString("M", BPoint(m.left + 3, center - 2));
		DrawString("S", BPoint(m.left + 4, s.bottom - 2));
		SetDrawingMode(B_OP_COPY);
	}

	// TrackViewer::InternalDraw: the lane, its clips, a bevelled frame
	void DrawTrack(const pmx::Track& track, BRect lane, bool selected)
	{
		BRect box(kClipLeft, lane.top, kClipLeft + 799, lane.top + kLaneHeight);
		BRect visible = box & Bounds();
		if (!visible.IsValid())
			return;
		SetHighColor(selected ? PaletteColor(30) : kLaneColor);
		FillRect(box);
		BRegion clip(box);
		ConstrainClippingRegion(&clip);
		for (const pmx::Clip& c : track.clips)
			DrawClip(c, box);
		ConstrainClippingRegion(NULL);
		SetHighColor(40, 40, 40);
		StrokeRect(box);
		SetHighColor(120, 120, 120);
		StrokeLine(BPoint(box.left + 1, box.bottom - 1), BPoint(box.right - 1, box.bottom - 1));
		StrokeLine(BPoint(box.right - 1, box.top + 1), BPoint(box.right - 1, box.bottom - 1));
	}

	// SimpleObject::Render: the first pass of the loop in red and green, the
	// repeats in grey and white; the loop's end has a red handle below
	void DrawClip(const pmx::Clip& c, BRect box)
	{
		const float spp = fArea->fSecondsPerPixel;
		const float top = box.top + 6, bottom = box.bottom - 8;
		float left = X(c.start), right = X(c.end) - 2;
		if (right < box.left || left > box.right)
			return;
		BRect rect(left + 1, top + 1, right, bottom - 1);
		SetHighColor(PaletteColor(25));
		FillRect(rect);

		const float scale = ((bottom - top) / 2.5f) / 35000.0f;
		const float mid = top + (bottom - top) / 2;
		const float loopLength = c.loopOut - c.loopIn;
		bool repeat = false;
		float t = c.loopIn + 2 * spp;
		int x0 = std::max(int(left) + 1, int(box.left));
		if (x0 > int(left) + 1)
			t += spp * (x0 - (int(left) + 1));
		while (loopLength > 0 && t > c.loopOut) {
			t -= loopLength;
			repeat = true;
		}
		int x1 = std::min(int(right), int(box.right));
		for (int x = x0; x < x1; x++) {
			if (t > c.loopOut && loopLength > 0) {
				repeat = true;
				t -= loopLength;
			}
			int16 lo, hi;
			GetSample(c, t, spp, lo, hi);
			// the minimum is drawn upwards in red, the maximum down in green
			int up = int(scale * lo - 1) + int(mid);
			int down = int(scale * hi + 1) + int(mid);
			SetHighColor(PaletteColor(repeat ? 13 : 45));
			if (up != int(mid))
				StrokeLine(BPoint(x, up), BPoint(x, mid));
			SetHighColor(PaletteColor(repeat ? 30 : 72));
			StrokeLine(BPoint(x, mid), BPoint(x, down));
			t += spp;
		}
		// frame: 120 and 80 above and left, 192 below and right
		SetHighColor(PaletteColor(15));
		StrokeLine(BPoint(left, top), BPoint(left, bottom));
		StrokeLine(BPoint(left, top), BPoint(right, top));
		SetHighColor(PaletteColor(10));
		StrokeLine(BPoint(left + 1, top), BPoint(left + 1, bottom - 1));
		StrokeLine(BPoint(left, top + 1), BPoint(right - 1, top + 1));
		// the darker ends of the box
		SetHighColor(PaletteColor(20));
		FillRect(BRect(left + 2, top + 2, left + 3, bottom - 2));
		FillRect(BRect(right - 5, top + 2, right - 2, bottom - 2));
		SetHighColor(PaletteColor(24));
		StrokeLine(BPoint(left, bottom - 1), BPoint(right - 1, bottom - 1));
		StrokeLine(BPoint(left, bottom), BPoint(right - 1, bottom));
		StrokeLine(BPoint(right, top), BPoint(right, bottom));
		StrokeLine(BPoint(right - 1, top), BPoint(right - 1, bottom));

		// the loop's end and its handle
		float loopEnd = X(c.start + loopLength);
		if (loopLength > 0 && loopEnd < right) {
			SetHighColor(PaletteColor(13));
			StrokeLine(BPoint(loopEnd, top + 2), BPoint(loopEnd, bottom - 2));
		}
		SetHighColor(PaletteColor(45));
		float h = std::min(loopEnd, right);
		FillTriangle(BPoint(h, bottom + 1), BPoint(h - 4, bottom + 5), BPoint(h + 4, bottom + 5));

		// the source's name, centred at the bottom
		if (right - left > 30 && !c.source.empty()) {
			BFont font;
			GetFont(&font);
			font.SetSize(right - left < 60 ? 9 : 10);
			SetFont(&font);
			BString name(c.source.c_str());
			font.TruncateString(&name, B_TRUNCATE_END, right - left - 8);
			SetHighColor(0, 0, 220);
			SetDrawingMode(B_OP_OVER);
			DrawString(name.String(), BPoint((left + right - font.StringWidth(name.String())) / 2,
				bottom - 4));
			SetDrawingMode(B_OP_COPY);
		}
	}

	// SampleCache::GetSample: the extremes of every (n / 8 + 1)th sample in
	// the column (n / 2 + 1 with Display ▸ Fast display), at least -1 and 1
	static void GetSample(const pmx::Clip& c, float time, float span, int16& lo, int16& hi)
	{
		lo = -1;
		hi = 1;
		if (time < 0)
			return;
		const int count = int(c.samples.size());
		int at = int(time * pmx::kSampleRate);
		if (count < at)
			return;
		int n = int(span * pmx::kSampleRate);
		if (n == 0)
			n = 1;
		int step = int(int16((n >> (sFastDisplay ? 1 : 3)) + 1));
		int last = count - step - 1;
		int end = std::min(n + at, last);
		if (last < at)
			at = last;
		do {
			at += step;
			if (at < 0 || at >= count)
				break;
			int16 s = c.samples[size_t(at)];
			if (hi < s)
				hi = s;
			if (s < lo)
				lo = s;
		} while (at < end);
	}

	static bool sFastDisplay;

	// do_mute_solo and selecting a track by its name
	virtual void MouseDown(BPoint where)
	{
		if (!fArea->fEngine->Lock())
			return;
		size_t count = fArea->fEngine->Project().tracks.size();
		int lane = -1;
		for (size_t i = 0; i < count; i++) {
			if (where.y >= LaneTop(i) && where.y <= LaneTop(i) + kLaneHeight)
				lane = int(i);
		}
		pmx::Mixer& mixer = fArea->fEngine->Mixer();
		bool changed = false;
		if (lane >= 0 && where.x >= 110 && where.x <= 122) {
			float center = LaneTop(size_t(lane)) + kLaneHeight / 2;
			if (where.y < center)
				mixer.SetMuted(size_t(lane), !mixer.IsMuted(size_t(lane)));
			else
				mixer.SetSoloed(size_t(lane), !mixer.IsSoloed(size_t(lane)));
			changed = true;
		}
		fArea->fEngine->Unlock();
		if (changed) {
			Window()->PostMessage(kMsgMuteSoloChanged);
		} else if (lane >= 0 && where.x < 110) {
			uint32 modifiers = 0;
			Window()->CurrentMessage()->FindInt32("modifiers", (int32*)&modifiers);
			if ((modifiers & B_SHIFT_KEY) == 0)
				fSelected.clear();
			if (fSelected.count(lane) && (modifiers & B_SHIFT_KEY) != 0)
				fSelected.erase(lane);
			else
				fSelected.insert(lane);
			BMessage message(kMsgTrackSelection);
			for (int t : fSelected)
				message.AddInt32("track", t);
			Window()->PostMessage(&message);
		}
		Invalidate();
	}

	std::set<int> fSelected;

private:
	TrackArea* fArea;
};

bool TrackLanes::sFastDisplay = false;

// #pragma mark - ZoomBar

// ZoomControl: a black-to-grey ramp with a red mark; drag to zoom
class ZoomBar : public BView {
public:
	ZoomBar(BRect frame, TrackArea* area)
		:
		BView(frame, "zoom", B_FOLLOW_LEFT | B_FOLLOW_BOTTOM, B_WILL_DRAW),
		fArea(area),
		fTracking(false)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	float MarkX() const
	{
		float f = log(fArea->Zoom() / kZoomMin) / log(kZoomMax / kZoomMin);
		return 4 + f * (Bounds().Width() - 8);
	}

	virtual void Draw(BRect updateRect)
	{
		BRect b = Bounds();
		SetHighColor(255, 255, 255);
		FillRect(b);
		for (float x = 4; x < b.right - 4; x++) {
			uint8 v = uint8(std::min(220.0f, 20 + (x - 4) / (b.Width() - 8) * 220));
			SetHighColor(v, v, v);
			float h = 1 + (x - 4) / (b.Width() - 8) * 4;
			StrokeLine(BPoint(x, b.top + 7 - h), BPoint(x, b.top + 7 + h));
		}
		SetHighColor(200, 0, 0);
		float x = MarkX();
		StrokeLine(BPoint(x, b.top + 1), BPoint(x, b.bottom - 1));
		SetHighColor(150, 150, 150);
		StrokeRect(b);
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
		float f = (where.x - 4) / (Bounds().Width() - 8);
		f = std::max(0.0f, std::min(1.0f, f));
		fArea->SetZoom(kZoomMin * pow(kZoomMax / kZoomMin, f));
	}

	virtual void MouseUp(BPoint where)
	{
		fTracking = false;
	}

private:
	TrackArea* fArea;
	bool fTracking;
};

// THSB: scrolls time
class TimeScrollBar : public BScrollBar {
public:
	TimeScrollBar(BRect frame, TrackArea* area)
		:
		BScrollBar(frame, "_HSB_", NULL, 0, 1000, B_HORIZONTAL),
		fArea(area)
	{
		SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	}

	virtual void ValueChanged(float value)
	{
		BScrollBar::ValueChanged(value);
		fArea->ScrollTime(value * fArea->Zoom());
	}

private:
	TrackArea* fArea;
};

// #pragma mark - TrackArea

TrackArea::TrackArea(BRect frame, AudioEngine* engine)
	:
	BView(frame, "tracks", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
	fEngine(engine),
	fRotation(3.1415925f),
	fSecondsPerPixel(1.0f / 14),
	fViewStart(0)
{
	SetViewColor(kBackground);
	const float width = frame.Width();
	// TWaveWindow: iVar16 = window height - 34; this view is 50 taller
	const float bottom = frame.Height() - 50 - 34;

	fTicks = new BTextControl(BRect(15, 9, 65, 22), "Ticks:", "Ticks:", "",
		new BMessage(kMsgTicks), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	fTicks->SetDivider(fTicks->StringWidth("Ticks:") + 2);
	AddChild(fTicks);

	BPopUpMenu* menu = new BPopUpMenu("beats");
	for (int beats = 3; beats <= 7; beats++) {
		BMessage* message = new BMessage(kMsgBeats);
		message->AddInt32("v", beats * 1000 + 40);
		BString label;
		label << beats << "/4";
		menu->AddItem(new BMenuItem(label.String(), message));
	}
	menu->ItemAt(1)->SetMarked(true);
	fBeats = new BMenuField(BRect(68, 8, 111, 26), "beats", NULL, menu,
		B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BFont font(be_plain_font);
	font.SetSize(10);
	fBeats->SetFont(&font);
	fBeats->MenuBar()->SetFont(&font);
	AddChild(fBeats);

	fRuler = new TimeRuler(BRect(130, 1, width, 40), this);
	AddChild(fRuler);
	fLanes = new TrackLanes(BRect(0, 41, width - 14, bottom), this);
	AddChild(fLanes);
	fVertical = new BScrollBar(BRect(width - 13, 41, width + 1, bottom + 1), "_VSB_",
		fLanes, 0, 500, B_VERTICAL);
	fVertical->SetResizingMode(B_FOLLOW_RIGHT | B_FOLLOW_TOP_BOTTOM);
	fVertical->SetSteps(12, 50);
	AddChild(fVertical);
	fZoom = new ZoomBar(BRect(0, bottom + 1, 109, bottom + 15), this);
	AddChild(fZoom);
	fHorizontal = new TimeScrollBar(BRect(110, bottom + 1, width - 13, bottom + 15), this);
	AddChild(fHorizontal);
}

void
TrackArea::AttachedToWindow()
{
	BView::AttachedToWindow();
	fTicks->SetTarget(this);
	fBeats->Menu()->SetTargetForItems(this);
}

// TCtrlView::Draw: two lines along the bottom and the turning triangle
void
TrackArea::Draw(BRect updateRect)
{
	SetHighColor(130, 130, 140);
	StrokeLine(BPoint(0, 40 - 1), BPoint(129, 40 - 1));
	SetHighColor(160, 160, 160);
	StrokeLine(BPoint(0, 40), BPoint(129, 40));
	DrawTriangle();
}

// TCtrlView::transform: the triangle turns about (9, 17)
void
TrackArea::DrawTriangle()
{
	SetHighColor(kBackground);
	FillRect(BRect(1, 4, 16, 32));
	BPoint points[3] = {BPoint(4, 10), BPoint(14, 17), BPoint(4, 24)};
	const float angle = fRotation;
	for (BPoint& p : points) {
		float x = p.x - 9, y = p.y - 17;
		p.x = x * cosf(angle) - y * sinf(angle) + 9;
		p.y = x * sinf(angle) + y * cosf(angle) + 17;
	}
	SetHighColor(20, 20, 20);
	FillPolygon(points, 3);
}

void
TrackArea::SetControlsRotation(float angle)
{
	fRotation = angle;
	Invalidate(BRect(1, 4, 16, 32));
}

void
TrackArea::MouseDown(BPoint where)
{
	// TCtrlView::MouseDown: the triangle shows and hides the controls
	if (where.x < 15 && where.y < 40)
		Window()->PostMessage(kMsgShowHideControls);
}

void
TrackArea::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgTicks:
			if (fEngine->Lock()) {
				pmx::Project& project = fEngine->Project();
				project.ticksText = fTicks->Text();
				project.ticks = float(atof(fTicks->Text()));
				fEngine->Unlock();
			}
			fRuler->Invalidate();
			break;
		case kMsgBeats:
		{
			int32 v;
			if (message->FindInt32("v", &v) == B_OK && fEngine->Lock()) {
				fEngine->Project().beatsPerBar = v / 1000;
				fEngine->Unlock();
			}
			fRuler->Invalidate();
			break;
		}
		default:
			BView::MessageReceived(message);
	}
}

void
TrackArea::ProjectChanged()
{
	size_t count = 0;
	float length = 0;
	BString ticks;
	int beats = 4;
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		count = project.tracks.size();
		length = project.Length();
		ticks = project.ticksText.c_str();
		beats = project.beatsPerBar;
		fEngine->Unlock();
	}
	fTicks->SetText(ticks.String());
	BMenuItem* item = fBeats->Menu()->ItemAt(std::max(0, std::min(4, beats - 3)));
	if (item != NULL)
		item->SetMarked(true);
	fLanes->fSelected.clear();
	// TrackView::LoadMaster: 30 + (height + 8) per track
	fVertical->SetRange(0, std::max(0.0f, kLaneTop + count * (kLaneHeight + 8)
		- fLanes->Bounds().Height()));
	fHorizontal->SetRange(0, std::max(0.0f, length / fSecondsPerPixel));
	fLanes->Invalidate();
	fRuler->Invalidate();
}

void
TrackArea::SetFastDisplay(bool fast)
{
	TrackLanes::sFastDisplay = fast;
	fLanes->Invalidate();
}

bool
TrackArea::FastDisplay() const
{
	return TrackLanes::sFastDisplay;
}

void
TrackArea::RedrawLanes()
{
	fLanes->Invalidate();
}

void
TrackArea::SetTime(double time)
{
	fRuler->SetTime(time);
}

void
TrackArea::SetSelection(const BMessage& tracks)
{
	fLanes->fSelected.clear();
	int32 track;
	for (int32 i = 0; tracks.FindInt32("track", i, &track) == B_OK; i++)
		fLanes->fSelected.insert(track);
	fLanes->Invalidate();
}

void
TrackArea::SetZoom(float secondsPerPixel)
{
	if (secondsPerPixel == fSecondsPerPixel)
		return;
	fSecondsPerPixel = secondsPerPixel;
	float length = 0;
	if (fEngine->Lock()) {
		length = fEngine->Project().Length();
		fEngine->Unlock();
	}
	fHorizontal->SetRange(0, std::max(0.0f, length / fSecondsPerPixel));
	fHorizontal->SetValue(fViewStart / fSecondsPerPixel);
	fZoom->Invalidate();
	fLanes->Invalidate();
	fRuler->Invalidate();
}

void
TrackArea::ScrollTime(float seconds)
{
	fViewStart = seconds;
	fLanes->Invalidate();
	fRuler->Invalidate();
}
